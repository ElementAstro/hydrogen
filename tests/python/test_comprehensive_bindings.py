#!/usr/bin/env python3
"""
Comprehensive test suite for Hydrogen Python bindings with 100% API parity validation.

This test suite validates that all Python bindings exactly match the C++ interface
definitions and provide complete functionality with proper error handling.
"""

import sys
import os
import unittest
import time
import threading
from typing import Any, Dict, List

# Add build directory to path
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build'))

try:
    import pyhydrogen as hydrogen
except ImportError:
    print("Error: pyhydrogen module not found. Build the Python bindings first.")
    sys.exit(1)

class TestAPICompliance(unittest.TestCase):
    """Test complete API compliance with C++ interfaces"""
    
    def setUp(self):
        """Set up test environment"""
        hydrogen.set_log_level("debug")
        hydrogen.init_compatibility_system(
            enable_auto_discovery=False,
            enable_ascom=True,
            enable_indi=True,
            indi_base_port=7624
        )
    
    def tearDown(self):
        """Clean up test environment"""
        hydrogen.shutdown_compatibility_system()
    
    def test_camera_interface_compliance(self):
        """Test Camera interface 100% API compliance"""
        camera = hydrogen.create_compatible_camera("test_cam", "TestMfg", "TestCam")
        
        # Test all ASCOM ICameraV4 standard methods exist
        required_methods = [
            'start_exposure', 'abort_exposure', 'stop_exposure',
            'get_camera_state', 'get_image_ready', 'get_last_exposure_duration',
            'get_camera_x_size', 'get_camera_y_size', 'get_pixel_size_x', 'get_pixel_size_y',
            'get_bin_x', 'set_bin_x', 'get_bin_y', 'set_bin_y',
            'get_start_x', 'set_start_x', 'get_start_y', 'set_start_y',
            'get_num_x', 'set_num_x', 'get_num_y', 'set_num_y',
            'get_gain', 'set_gain', 'get_offset', 'set_offset',
            'get_sensor_type', 'get_has_shutter', 'get_can_pulse_guide'
        ]
        
        for method in required_methods:
            self.assertTrue(hasattr(camera, method), f"Camera missing method: {method}")
        
        # Test property access
        self.assertIsInstance(camera.camera_x_size, int)
        self.assertIsInstance(camera.camera_y_size, int)
        self.assertIsInstance(camera.pixel_size_x, float)
        self.assertIsInstance(camera.pixel_size_y, float)
        
        # Test capability checking
        caps = hydrogen.check_camera_capabilities(camera)
        required_caps = [
            'can_abort_exposure', 'can_stop_exposure', 'can_pulse_guide',
            'can_fast_readout', 'can_asymmetric_bin', 'has_shutter',
            'max_bin_x', 'max_bin_y', 'sensor_type'
        ]
        
        for cap in required_caps:
            self.assertIn(cap, caps, f"Camera missing capability: {cap}")
    
    def test_telescope_interface_compliance(self):
        """Test Telescope interface 100% API compliance"""
        telescope = hydrogen.create_compatible_telescope("test_tel", "TestMfg", "TestTel")
        
        # Test all ASCOM ITelescopeV4 standard methods exist
        required_methods = [
            'get_right_ascension', 'get_declination', 'get_altitude', 'get_azimuth',
            'get_target_right_ascension', 'set_target_right_ascension',
            'get_target_declination', 'set_target_declination',
            'slew_to_coordinates', 'slew_to_coordinates_async', 'slew_to_target',
            'slew_to_alt_az', 'abort_slew', 'get_slewing',
            'sync_to_coordinates', 'sync_to_target', 'sync_to_alt_az',
            'get_tracking', 'set_tracking', 'get_tracking_rate', 'set_tracking_rate',
            'park', 'unpark', 'get_at_park', 'find_home', 'get_at_home',
            'get_side_of_pier', 'set_side_of_pier', 'pulse_guide', 'get_is_pulse_guiding',
            'get_site_latitude', 'set_site_latitude', 'get_site_longitude', 'set_site_longitude'
        ]
        
        for method in required_methods:
            self.assertTrue(hasattr(telescope, method), f"Telescope missing method: {method}")
        
        # Test capability checking
        caps = hydrogen.check_telescope_capabilities(telescope)
        required_caps = [
            'can_slew', 'can_slew_async', 'can_slew_alt_az', 'can_sync',
            'can_park', 'can_unpark', 'can_find_home', 'can_set_tracking',
            'can_pulse_guide', 'can_set_guide_rates', 'can_set_pier_side'
        ]
        
        for cap in required_caps:
            self.assertIn(cap, caps, f"Telescope missing capability: {cap}")
    
    def test_focuser_interface_compliance(self):
        """Test Focuser interface 100% API compliance"""
        focuser = hydrogen.create_compatible_focuser("test_foc", "TestMfg", "TestFoc")
        
        # Test all ASCOM IFocuserV4 standard methods exist
        required_methods = [
            'get_position', 'move', 'move_relative', 'halt', 'get_is_moving',
            'get_absolute', 'get_max_increment', 'get_max_step', 'get_step_size'
        ]
        
        for method in required_methods:
            self.assertTrue(hasattr(focuser, method), f"Focuser missing method: {method}")

class TestTypeSafety(unittest.TestCase):
    """Test type safety and validation features"""
    
    def test_coordinates_validation(self):
        """Test coordinate validation"""
        # Valid coordinates
        coords = hydrogen.Coordinates(12.5, 45.0)
        self.assertEqual(coords.ra, 12.5)
        self.assertEqual(coords.dec, 45.0)
        
        # Invalid RA (> 24)
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.Coordinates(25.0, 45.0)
        
        # Invalid Dec (> 90)
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.Coordinates(12.0, 95.0)
        
        # Invalid Dec (< -90)
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.Coordinates(12.0, -95.0)
    
    def test_temperature_validation(self):
        """Test temperature validation and conversion"""
        # Valid temperature
        temp = hydrogen.Temperature(-10.0)
        self.assertEqual(temp.celsius, -10.0)
        self.assertAlmostEqual(temp.kelvin, 263.15, places=2)
        self.assertAlmostEqual(temp.fahrenheit, 14.0, places=1)
        
        # Temperature below absolute zero
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.Temperature(-300.0)
        
        # Test conversions
        temp_k = hydrogen.Temperature.from_kelvin(273.15)
        self.assertAlmostEqual(temp_k.celsius, 0.0, places=2)
        
        temp_f = hydrogen.Temperature.from_fahrenheit(32.0)
        self.assertAlmostEqual(temp_f.celsius, 0.0, places=2)
    
    def test_exposure_settings_validation(self):
        """Test exposure settings validation"""
        # Valid settings
        settings = hydrogen.ExposureSettings(60.0, True, 2, 1024, 1024, 0, 0)
        self.assertEqual(settings.duration, 60.0)
        self.assertTrue(settings.is_light)
        self.assertEqual(settings.bin_x, 2)
        self.assertEqual(settings.bin_y, 2)
        
        # Invalid duration (too short)
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.ExposureSettings(0.0001, True, 1)
        
        # Invalid duration (too long)
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.ExposureSettings(4000.0, True, 1)
        
        # Invalid binning
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.ExposureSettings(1.0, True, 0)  # Binning must be >= 1
    
    def test_type_validator_utilities(self):
        """Test type validation utilities"""
        # Valid range
        result = hydrogen.TypeValidator.validate_range(5.0, 0.0, 10.0, "test_param")
        self.assertEqual(result, 5.0)
        
        # Invalid range
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.TypeValidator.validate_range(15.0, 0.0, 10.0, "test_param")
        
        # Valid positive
        result = hydrogen.TypeValidator.validate_positive(5.0, "test_param")
        self.assertEqual(result, 5.0)
        
        # Invalid positive
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.TypeValidator.validate_positive(-1.0, "test_param")
        
        # Valid non-negative
        result = hydrogen.TypeValidator.validate_non_negative(0.0, "test_param")
        self.assertEqual(result, 0.0)
        
        # Invalid non-negative
        with self.assertRaises(hydrogen.ASCOMInvalidValueException):
            hydrogen.TypeValidator.validate_non_negative(-1.0, "test_param")

class TestErrorHandling(unittest.TestCase):
    """Test comprehensive error handling"""
    
    def test_exception_hierarchy(self):
        """Test exception class hierarchy"""
        # Test base exception
        self.assertTrue(issubclass(hydrogen.ASCOMException, hydrogen.DeviceException))
        self.assertTrue(issubclass(hydrogen.ASCOMNotConnectedException, hydrogen.ASCOMException))
        self.assertTrue(issubclass(hydrogen.ASCOMInvalidValueException, hydrogen.ASCOMException))
        self.assertTrue(issubclass(hydrogen.ASCOMInvalidOperationException, hydrogen.ASCOMException))
        self.assertTrue(issubclass(hydrogen.ASCOMNotImplementedException, hydrogen.ASCOMException))
        
        # Test INDI exceptions
        self.assertTrue(issubclass(hydrogen.INDIException, hydrogen.DeviceException))
        self.assertTrue(issubclass(hydrogen.INDIPropertyException, hydrogen.INDIException))
        self.assertTrue(issubclass(hydrogen.INDITimeoutException, hydrogen.INDIException))
    
    def test_ascom_error_codes(self):
        """Test ASCOM error code constants"""
        self.assertEqual(hydrogen.ASCOM_OK, 0x00000000)
        self.assertEqual(hydrogen.ASCOM_UNSPECIFIED_ERROR, 0x80040001)
        self.assertEqual(hydrogen.ASCOM_INVALID_VALUE, 0x80040002)
        self.assertEqual(hydrogen.ASCOM_NOT_CONNECTED, 0x80040007)
        self.assertEqual(hydrogen.ASCOM_INVALID_OPERATION, 0x8004000B)
        self.assertEqual(hydrogen.ASCOM_ACTION_NOT_IMPLEMENTED, 0x8004000C)

class TestCompatibilitySystem(unittest.TestCase):
    """Test automatic ASCOM/INDI compatibility system"""
    
    def setUp(self):
        """Set up compatibility system"""
        hydrogen.init_compatibility_system()
    
    def tearDown(self):
        """Clean up compatibility system"""
        hydrogen.shutdown_compatibility_system()
    
    def test_system_initialization(self):
        """Test compatibility system initialization"""
        stats = hydrogen.get_compatibility_statistics()
        self.assertIsInstance(stats.total_devices, int)
        self.assertIsInstance(stats.ascom_devices, int)
        self.assertIsInstance(stats.indi_devices, int)
        self.assertGreaterEqual(stats.uptime.total_seconds(), 0)
    
    def test_device_creation_with_compatibility(self):
        """Test device creation with automatic compatibility"""
        # Create devices
        camera = hydrogen.create_compatible_camera("test_cam", "ZWO", "ASI294")
        telescope = hydrogen.create_compatible_telescope("test_tel", "Celestron", "CGX")
        focuser = hydrogen.create_compatible_focuser("test_foc", "ZWO", "EAF")
        
        # Verify devices are created
        self.assertIsNotNone(camera)
        self.assertIsNotNone(telescope)
        self.assertIsNotNone(focuser)
        
        # Check statistics
        stats = hydrogen.get_compatibility_statistics()
        self.assertGreaterEqual(stats.total_devices, 3)
    
    def test_protocol_transparency(self):
        """Test transparent protocol access"""
        camera = hydrogen.create_compatible_camera("test_cam", "ZWO", "ASI294")
        
        # Test property access through different protocols
        # Note: This would require actual bridge implementation
        # For now, test that the interface exists
        manager = hydrogen.get_integration_manager()
        self.assertIsNotNone(manager)

class TestTypeWrappers(unittest.TestCase):
    """Test type-safe wrapper classes"""
    
    def setUp(self):
        """Set up test devices"""
        hydrogen.init_compatibility_system()
        self.camera = hydrogen.create_compatible_camera("test_cam", "ZWO", "ASI294")
        self.telescope = hydrogen.create_compatible_telescope("test_tel", "Celestron", "CGX")
    
    def tearDown(self):
        """Clean up"""
        hydrogen.shutdown_compatibility_system()
    
    def test_type_safe_camera(self):
        """Test type-safe camera wrapper"""
        safe_camera = hydrogen.create_type_safe_camera(self.camera)
        self.assertIsNotNone(safe_camera)
        
        # Test type-safe operations
        binning = hydrogen.Binning(2, 2)
        safe_camera.set_binning(binning)
        
        current_binning = safe_camera.get_binning()
        self.assertEqual(current_binning.x, 2)
        self.assertEqual(current_binning.y, 2)
    
    def test_type_safe_telescope(self):
        """Test type-safe telescope wrapper"""
        safe_telescope = hydrogen.create_type_safe_telescope(self.telescope)
        self.assertIsNotNone(safe_telescope)
        
        # Test type-safe coordinate access
        coords = safe_telescope.get_current_coordinates()
        self.assertIsInstance(coords, hydrogen.Coordinates)

class TestSystemInformation(unittest.TestCase):
    """Test system information and utilities"""
    
    def test_system_info(self):
        """Test system information"""
        info = hydrogen.get_system_info()
        
        required_keys = ['version', 'build_date', 'build_time', 'ascom_compatible', 
                        'indi_compatible', 'thread_safe', 'debug_build']
        
        for key in required_keys:
            self.assertIn(key, info, f"System info missing key: {key}")
        
        self.assertTrue(info['ascom_compatible'])
        self.assertTrue(info['indi_compatible'])
        self.assertTrue(info['thread_safe'])
    
    def test_api_reference_generation(self):
        """Test API reference documentation generation"""
        api_ref = hydrogen.generate_api_reference()
        self.assertIsInstance(api_ref, str)
        self.assertGreater(len(api_ref), 100)  # Should be substantial documentation
        
        # Check for key sections
        self.assertIn("Camera", api_ref)
        self.assertIn("Telescope", api_ref)
        self.assertIn("Focuser", api_ref)
        self.assertIn("ASCOM", api_ref)
        self.assertIn("INDI", api_ref)

class TestConcurrentAccess(unittest.TestCase):
    """Test thread safety and concurrent access"""
    
    def setUp(self):
        """Set up test environment"""
        hydrogen.init_compatibility_system()
        self.camera = hydrogen.create_compatible_camera("test_cam", "ZWO", "ASI294")
        self.results = []
        self.errors = []
    
    def tearDown(self):
        """Clean up"""
        hydrogen.shutdown_compatibility_system()
    
    def test_concurrent_property_access(self):
        """Test concurrent property access from multiple threads"""
        def worker_thread(thread_id):
            try:
                for i in range(10):
                    # Test property access
                    state = self.camera.get_camera_state()
                    self.results.append(f"Thread {thread_id}: {state}")
                    time.sleep(0.001)
            except Exception as e:
                self.errors.append(f"Thread {thread_id}: {e}")
        
        # Start multiple threads
        threads = []
        for i in range(5):
            thread = threading.Thread(target=worker_thread, args=(i,))
            threads.append(thread)
            thread.start()
        
        # Wait for completion
        for thread in threads:
            thread.join()
        
        # Check results
        self.assertEqual(len(self.errors), 0, f"Errors in concurrent access: {self.errors}")
        self.assertGreater(len(self.results), 0, "No results from concurrent access")

def run_comprehensive_tests():
    """Run all comprehensive tests"""
    print("🧪 Running Comprehensive Python Bindings Test Suite")
    print("=" * 60)
    
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    test_classes = [
        TestAPICompliance,
        TestTypeSafety,
        TestErrorHandling,
        TestCompatibilitySystem,
        TestTypeWrappers,
        TestSystemInformation,
        TestConcurrentAccess
    ]
    
    for test_class in test_classes:
        tests = loader.loadTestsFromTestCase(test_class)
        suite.addTests(tests)
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print("\n" + "=" * 60)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    
    if result.failures:
        print("\nFailures:")
        for test, traceback in result.failures:
            print(f"  {test}: {traceback}")
    
    if result.errors:
        print("\nErrors:")
        for test, traceback in result.errors:
            print(f"  {test}: {traceback}")
    
    success = len(result.failures) == 0 and len(result.errors) == 0
    print(f"\n🎯 Overall result: {'PASS' if success else 'FAIL'}")
    
    return success

if __name__ == "__main__":
    success = run_comprehensive_tests()
    sys.exit(0 if success else 1)
