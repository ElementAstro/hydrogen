---
type: "manual"
---

Build the entire project from scratch and systematically fix all compilation errors, dependency issues, and build failures that are encountered during the process. This should include:

1. First, analyze the project structure and identify the build system being used (e.g., CMake, Make, npm, cargo, etc.)
2. Install any missing dependencies or packages required for the build
3. Execute the appropriate build commands for this project
4. When build errors occur, diagnose each error carefully and implement the necessary fixes:
   - Fix missing includes/imports
   - Resolve dependency version conflicts
   - Correct syntax errors
   - Address configuration issues
   - Fix linking problems
5. After each fix, re-run the build to verify the issue is resolved
6. Continue this process iteratively until the project builds successfully without any errors
7. Verify the final build by running any available tests or validation steps

The goal is to achieve a complete, successful build of the project with all issues resolved.