# HydrogenVersions.cmake - Dependency version management and conflict resolution
# This module provides version constraints, conflict detection, and resolution
# strategies for dependencies across different package managers.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_VERSIONS_INCLUDED)
    return()
endif()
set(HYDROGEN_VERSIONS_INCLUDED TRUE)

# =============================================================================
# Version Constraint Definitions
# =============================================================================

# Define version constraints for all dependencies
function(hydrogen_define_version_constraints)
    # Core dependency versions with constraints
    set(HYDROGEN_VERSION_CONSTRAINTS "")
    # Format: "package_name|min_version|max_version|preferred_version|critical"
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "Boost|1.82.0|1.85.0|1.84.0|true")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "OpenSSL|3.0.0|3.3.0|3.2.0|true")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "nlohmann_json|3.11.0|3.12.0|3.11.3|false")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "Crow|1.0.0|1.3.0|1.2.0|false")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "ZLIB|1.2.11|1.4.0|1.3.1|false")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "GTest|1.14.0|1.15.0|1.14.0|false")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "pybind11|2.10.0|2.12.0|2.11.1|false")
    list(APPEND HYDROGEN_VERSION_CONSTRAINTS "benchmark|1.8.0|1.9.0|1.8.3|false")
    
    set(HYDROGEN_VERSION_CONSTRAINTS "${HYDROGEN_VERSION_CONSTRAINTS}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Version Parsing and Comparison
# =============================================================================

# Parse a semantic version string into components
function(hydrogen_parse_version version_string major minor patch)
    string(REPLACE "." ";" version_list "${version_string}")
    list(LENGTH version_list version_length)
    
    # Extract major version
    list(GET version_list 0 version_major)
    set(${major} ${version_major} PARENT_SCOPE)
    
    # Extract minor version
    if(version_length GREATER 1)
        list(GET version_list 1 version_minor)
        set(${minor} ${version_minor} PARENT_SCOPE)
    else()
        set(${minor} 0 PARENT_SCOPE)
    endif()
    
    # Extract patch version
    if(version_length GREATER 2)
        list(GET version_list 2 version_patch)
        # Handle patch versions with additional identifiers (e.g., "1-alpha")
        string(REGEX MATCH "^[0-9]+" version_patch_clean "${version_patch}")
        set(${patch} ${version_patch_clean} PARENT_SCOPE)
    else()
        set(${patch} 0 PARENT_SCOPE)
    endif()
endfunction()

# Compare two version strings
# Returns: -1 if version1 < version2, 0 if equal, 1 if version1 > version2
function(hydrogen_compare_versions version1 version2 result)
    hydrogen_parse_version("${version1}" major1 minor1 patch1)
    hydrogen_parse_version("${version2}" major2 minor2 patch2)
    
    # Compare major version
    if(major1 LESS major2)
        set(${result} -1 PARENT_SCOPE)
        return()
    elseif(major1 GREATER major2)
        set(${result} 1 PARENT_SCOPE)
        return()
    endif()
    
    # Compare minor version
    if(minor1 LESS minor2)
        set(${result} -1 PARENT_SCOPE)
        return()
    elseif(minor1 GREATER minor2)
        set(${result} 1 PARENT_SCOPE)
        return()
    endif()
    
    # Compare patch version
    if(patch1 LESS patch2)
        set(${result} -1 PARENT_SCOPE)
        return()
    elseif(patch1 GREATER patch2)
        set(${result} 1 PARENT_SCOPE)
        return()
    endif()
    
    # Versions are equal
    set(${result} 0 PARENT_SCOPE)
endfunction()

# Check if a version satisfies constraints
function(hydrogen_check_version_constraint package_name version constraint_info satisfied)
    list(GET constraint_info 1 min_version)
    list(GET constraint_info 2 max_version)
    list(GET constraint_info 4 is_critical)
    
    set(constraint_satisfied TRUE)
    
    # Check minimum version
    if(min_version)
        hydrogen_compare_versions("${version}" "${min_version}" min_result)
        if(min_result LESS 0)
            set(constraint_satisfied FALSE)
            if(is_critical STREQUAL "true")
                message(WARNING "Hydrogen: ${package_name} version ${version} is below minimum required ${min_version}")
            endif()
        endif()
    endif()
    
    # Check maximum version
    if(max_version AND constraint_satisfied)
        hydrogen_compare_versions("${version}" "${max_version}" max_result)
        if(max_result GREATER 0)
            set(constraint_satisfied FALSE)
            if(is_critical STREQUAL "true")
                message(WARNING "Hydrogen: ${package_name} version ${version} exceeds maximum supported ${max_version}")
            endif()
        endif()
    endif()
    
    set(${satisfied} ${constraint_satisfied} PARENT_SCOPE)
endfunction()

# =============================================================================
# Dependency Version Detection
# =============================================================================

# Get the version of a found package
function(hydrogen_get_package_version package_name version_var)
    set(detected_version "unknown")
    
    # Try common version variables
    set(version_candidates
        "${package_name}_VERSION"
        "${package_name}_VERSION_STRING"
        "${package_name}_MAJOR_VERSION.${package_name}_MINOR_VERSION.${package_name}_PATCH_VERSION"
        "${package_name}_VERSION_MAJOR.${package_name}_VERSION_MINOR.${package_name}_VERSION_PATCH"
    )
    
    foreach(candidate ${version_candidates})
        if(DEFINED ${candidate} AND NOT "${${candidate}}" STREQUAL "")
            set(detected_version "${${candidate}}")
            break()
        endif()
    endforeach()
    
    # Special cases for specific packages
    if(package_name STREQUAL "Boost" AND DEFINED Boost_VERSION_STRING)
        set(detected_version "${Boost_VERSION_STRING}")
    elseif(package_name STREQUAL "OpenSSL" AND DEFINED OPENSSL_VERSION)
        set(detected_version "${OPENSSL_VERSION}")
    elseif(package_name STREQUAL "nlohmann_json" AND DEFINED nlohmann_json_VERSION)
        set(detected_version "${nlohmann_json_VERSION}")
    endif()
    
    set(${version_var} "${detected_version}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Conflict Detection and Resolution
# =============================================================================

# Check for version conflicts across all dependencies
function(hydrogen_check_version_conflicts)
    message(STATUS "Hydrogen: Checking dependency version constraints...")
    
    hydrogen_define_version_constraints()
    
    set(conflicts_found FALSE)
    set(warnings_found FALSE)
    
    foreach(constraint ${HYDROGEN_VERSION_CONSTRAINTS})
        string(REPLACE "|" ";" constraint_list "${constraint}")

        list(GET constraint_list 0 package_name)
        list(GET constraint_list 3 preferred_version)
        list(GET constraint_list 4 is_critical)
        
        # Check if package is found
        if(${package_name}_FOUND)
            hydrogen_get_package_version("${package_name}" current_version)
            
            if(NOT current_version STREQUAL "unknown")
                hydrogen_check_version_constraint("${package_name}" "${current_version}" "${constraint_list}" constraint_satisfied)
                
                if(NOT constraint_satisfied)
                    if(is_critical STREQUAL "true")
                        set(conflicts_found TRUE)
                        message(SEND_ERROR "Hydrogen: Critical version conflict for ${package_name}: found ${current_version}, constraints not satisfied")
                    else()
                        set(warnings_found TRUE)
                        message(WARNING "Hydrogen: Version warning for ${package_name}: found ${current_version}, preferred ${preferred_version}")
                    endif()
                else()
                    message(STATUS "Hydrogen: ${package_name} ${current_version} - OK")
                endif()
            else()
                message(STATUS "Hydrogen: ${package_name} - version unknown")
            endif()
        endif()
    endforeach()
    
    if(conflicts_found)
        message(FATAL_ERROR "Hydrogen: Critical dependency version conflicts detected. Please resolve before continuing.")
    elseif(warnings_found)
        message(STATUS "Hydrogen: Some dependency version warnings detected, but build can continue.")
    else()
        message(STATUS "Hydrogen: All dependency versions satisfy constraints.")
    endif()
endfunction()

# =============================================================================
# Version Compatibility Matrix
# =============================================================================

# Define known compatibility issues between package versions
function(hydrogen_define_compatibility_matrix)
    # Format: "package1|version1|package2|version2|issue_description"
    set(HYDROGEN_COMPATIBILITY_ISSUES "")
    list(APPEND HYDROGEN_COMPATIBILITY_ISSUES "Boost|1.83.0|OpenSSL|3.0.0|Known compilation issues with Boost.Beast and OpenSSL 3.0")
    list(APPEND HYDROGEN_COMPATIBILITY_ISSUES "Crow|1.0.0|Boost|1.85.0|Crow 1.0.0 not tested with Boost 1.85+")
    list(APPEND HYDROGEN_COMPATIBILITY_ISSUES "pybind11|2.10.0|Python|3.12.0|pybind11 2.10.0 has issues with Python 3.12+")

    set(HYDROGEN_COMPATIBILITY_ISSUES "${HYDROGEN_COMPATIBILITY_ISSUES}" PARENT_SCOPE)
endfunction()

# Check for known compatibility issues
function(hydrogen_check_compatibility_issues)
    message(STATUS "Hydrogen: Checking for known compatibility issues...")
    
    hydrogen_define_compatibility_matrix()
    
    foreach(issue ${HYDROGEN_COMPATIBILITY_ISSUES})
        string(REPLACE "|" ";" issue_list "${issue}")

        list(GET issue_list 0 package1)
        list(GET issue_list 1 version1)
        list(GET issue_list 2 package2)
        list(GET issue_list 3 version2)
        list(GET issue_list 4 description)
        
        # Check if both packages are found and versions match the problematic combination
        if(${package1}_FOUND AND ${package2}_FOUND)
            hydrogen_get_package_version("${package1}" current_version1)
            hydrogen_get_package_version("${package2}" current_version2)
            
            hydrogen_compare_versions("${current_version1}" "${version1}" result1)
            hydrogen_compare_versions("${current_version2}" "${version2}" result2)
            
            # Check if we have the problematic combination
            if(result1 EQUAL 0 AND result2 EQUAL 0)
                message(WARNING "Hydrogen: Known compatibility issue detected: ${description}")
            endif()
        endif()
    endforeach()
    
    message(STATUS "Hydrogen: Compatibility check complete.")
endfunction()

# =============================================================================
# Version Resolution Strategies
# =============================================================================

# Suggest version resolution for conflicts
function(hydrogen_suggest_version_resolution package_name current_version)
    hydrogen_define_version_constraints()
    
    foreach(constraint ${HYDROGEN_VERSION_CONSTRAINTS})
        string(REPLACE ";" "\\;" constraint_escaped "${constraint}")
        string(REPLACE ";" ";" constraint_list "${constraint_escaped}")
        
        list(GET constraint_list 0 constraint_package)
        if(constraint_package STREQUAL package_name)
            list(GET constraint_list 1 min_version)
            list(GET constraint_list 2 max_version)
            list(GET constraint_list 3 preferred_version)
            
            message(STATUS "Hydrogen: Version resolution suggestions for ${package_name}:")
            message(STATUS "  Current: ${current_version}")
            message(STATUS "  Minimum: ${min_version}")
            message(STATUS "  Maximum: ${max_version}")
            message(STATUS "  Preferred: ${preferred_version}")
            
            # Provide specific recommendations
            hydrogen_compare_versions("${current_version}" "${min_version}" min_result)
            hydrogen_compare_versions("${current_version}" "${max_version}" max_result)
            
            if(min_result LESS 0)
                message(STATUS "  Recommendation: Upgrade to at least ${min_version}")
            elseif(max_result GREATER 0)
                message(STATUS "  Recommendation: Downgrade to at most ${max_version}")
            else()
                message(STATUS "  Recommendation: Consider using preferred version ${preferred_version}")
            endif()
            
            break()
        endif()
    endforeach()
endfunction()

# =============================================================================
# Main Version Management Function
# =============================================================================

# Perform comprehensive version management
function(hydrogen_manage_versions)
    message(STATUS "Hydrogen: Starting dependency version management...")
    
    # Check version constraints
    hydrogen_check_version_conflicts()
    
    # Check compatibility issues
    hydrogen_check_compatibility_issues()
    
    message(STATUS "Hydrogen: Dependency version management complete.")
endfunction()
