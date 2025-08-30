#!/usr/bin/env python3
"""
Complete Observatory Example with Automatic ASCOM/INDI Compatibility

This example demonstrates a complete observatory setup using all device types
with automatic ASCOM/INDI compatibility, showing how they work together.
"""

import sys
import time
import os
import threading
import json
from datetime import datetime, timedelta
from typing import Dict, Any, Optional

# Add the build directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build'))

try:
    import pyhydrogen as hydrogen
except ImportError:
    print("Error: pyhydrogen module not found. Make sure to build the Python bindings first.")
    print("Run: cmake --build build --target pyhydrogen")
    sys.exit(1)

class CompleteObservatory:
    """Complete observatory with all device types and automatic compatibility"""
    
    def __init__(self):
        self.devices = {}
        self.running = False
        self.observation_thread = None
        self.weather_monitoring_thread = None
        
    def initialize_observatory(self):
        """Initialize the complete observatory system"""
        print("🏛️ Initializing Complete Observatory System")
        print("=" * 50)
        
        # Set logging level
        hydrogen.set_log_level("info")
        
        # Initialize the compatibility system
        print("🚀 Starting automatic ASCOM/INDI compatibility system...")
        hydrogen.init_compatibility_system(
            enable_auto_discovery=True,
            enable_ascom=True,
            enable_indi=True,
            indi_base_port=7624
        )
        print("✅ Compatibility system initialized")
        
        # Create all observatory devices with automatic compatibility
        self.create_observatory_devices()
        
        # Setup device coordination
        self.setup_device_coordination()
        
        print("🎉 Observatory initialization complete!")
        
    def create_observatory_devices(self):
        """Create all observatory devices with automatic compatibility"""
        print("\n📡 Creating observatory devices...")
        
        device_configs = [
            ("camera", "create_compatible_camera", "ZWO", "ASI6200MM Pro", "📷"),
            ("telescope", "create_compatible_telescope", "Celestron", "CGX-L", "🔭"),
            ("focuser", "create_compatible_focuser", "ZWO", "EAF", "🎯"),
            ("dome", "create_compatible_dome", "Technical Innovations", "ProDome", "🏠"),
            ("weather", "create_compatible_observing_conditions", "Boltwood", "Cloud Sensor II", "🌤️"),
        ]
        
        for device_name, create_func, manufacturer, model, icon in device_configs:
            try:
                device_id = f"observatory_{device_name}"
                create_function = getattr(hydrogen, create_func)
                self.devices[device_name] = create_function(device_id, manufacturer, model)
                print(f"  {icon} {device_name.title()}: {manufacturer} {model} ✅")
            except Exception as e:
                print(f"  ❌ Failed to create {device_name}: {e}")
                
        # Create safety monitor (if available)
        try:
            safety_monitor = hydrogen.SafetyMonitor("observatory_safety", "Generic", "SafetyMonitor")
            safety_monitor.initialize_device()
            safety_monitor.start_device()
            self.devices['safety'] = safety_monitor
            print("  🛡️ Safety Monitor: Generic SafetyMonitor ✅")
        except Exception as e:
            print(f"  ⚠️ Safety monitor not available: {e}")
            
        # Create cover calibrator (if available)
        try:
            cover_cal = hydrogen.CoverCalibrator("observatory_cover", "Alnitak", "Flip-Flat")
            cover_cal.initialize_device()
            cover_cal.start_device()
            self.devices['cover'] = cover_cal
            print("  📦 Cover Calibrator: Alnitak Flip-Flat ✅")
        except Exception as e:
            print(f"  ⚠️ Cover calibrator not available: {e}")
            
    def setup_device_coordination(self):
        """Setup coordination between devices"""
        print("\n🔗 Setting up device coordination...")
        
        try:
            # Setup dome slaving to telescope
            if 'dome' in self.devices and 'telescope' in self.devices:
                dome = self.devices['dome']
                dome.slave_to_telescope(True)
                print("  🏠➡️🔭 Dome slaved to telescope")
                
            # Setup safety monitoring
            if 'safety' in self.devices:
                safety = self.devices['safety']
                
                # Add safety checks for all devices
                for device_name in self.devices:
                    if device_name != 'safety':
                        safety.add_safety_check(f"{device_name}_connected", True)
                        
                print("  🛡️ Safety monitoring configured")
                
            # Setup weather monitoring thresholds
            if 'weather' in self.devices:
                weather = self.devices['weather']
                
                # Set conservative safety thresholds
                weather.set_safety_threshold(hydrogen.SensorType.WIND_SPEED, 0, 15)  # Max 15 m/s
                weather.set_safety_threshold(hydrogen.SensorType.HUMIDITY, 0, 85)    # Max 85%
                weather.set_safety_threshold(hydrogen.SensorType.CLOUD_COVER, 0, 30) # Max 30%
                weather.enable_safety_monitoring(True)
                
                print("  🌤️ Weather safety thresholds configured")
                
        except Exception as e:
            print(f"  ⚠️ Error in device coordination: {e}")
            
    def start_weather_monitoring(self):
        """Start continuous weather monitoring"""
        def weather_monitor():
            while self.running:
                try:
                    if 'weather' in self.devices:
                        weather = self.devices['weather']
                        weather.refresh()
                        
                        # Check if conditions are safe
                        if not weather.is_safe_for_observing():
                            print("⚠️ Weather conditions unsafe - taking protective actions")
                            self.emergency_shutdown()
                            
                    time.sleep(30)  # Check every 30 seconds
                except Exception as e:
                    print(f"❌ Weather monitoring error: {e}")
                    time.sleep(60)
                    
        self.weather_monitoring_thread = threading.Thread(target=weather_monitor, daemon=True)
        self.weather_monitoring_thread.start()
        print("🌤️ Weather monitoring started")
        
    def emergency_shutdown(self):
        """Emergency shutdown sequence"""
        print("🚨 EMERGENCY SHUTDOWN INITIATED")
        
        try:
            # Close dome shutter
            if 'dome' in self.devices:
                self.devices['dome'].close_shutter()
                print("  🏠 Dome shutter closed")
                
            # Park telescope
            if 'telescope' in self.devices:
                telescope = self.devices['telescope']
                if hasattr(telescope, 'park'):
                    telescope.park()
                    print("  🔭 Telescope parked")
                    
            # Close dust cover
            if 'cover' in self.devices:
                self.devices['cover'].close_cover()
                print("  📦 Dust cover closed")
                
            # Stop any ongoing exposures
            if 'camera' in self.devices:
                camera = self.devices['camera']
                if hasattr(camera, 'abort_exposure'):
                    camera.abort_exposure()
                    print("  📷 Camera exposure aborted")
                    
        except Exception as e:
            print(f"❌ Error during emergency shutdown: {e}")
            
    def run_observation_sequence(self):
        """Run a complete observation sequence"""
        print("\n🌟 Starting Observation Sequence")
        print("=" * 40)
        
        try:
            # Pre-observation checks
            if not self.pre_observation_checks():
                print("❌ Pre-observation checks failed")
                return False
                
            # Open observatory
            self.open_observatory()
            
            # Setup imaging
            self.setup_imaging()
            
            # Run imaging sequence
            self.run_imaging_sequence()
            
            # Close observatory
            self.close_observatory()
            
            print("✅ Observation sequence completed successfully")
            return True
            
        except Exception as e:
            print(f"❌ Observation sequence error: {e}")
            self.emergency_shutdown()
            return False
            
    def pre_observation_checks(self):
        """Perform pre-observation safety and readiness checks"""
        print("🔍 Performing pre-observation checks...")
        
        checks_passed = 0
        total_checks = 0
        
        # Check weather conditions
        if 'weather' in self.devices:
            total_checks += 1
            weather = self.devices['weather']
            weather.refresh()
            
            if weather.is_safe_for_observing():
                print("  ✅ Weather conditions safe")
                checks_passed += 1
            else:
                print("  ❌ Weather conditions unsafe")
                
        # Check safety monitor
        if 'safety' in self.devices:
            total_checks += 1
            safety = self.devices['safety']
            
            if safety.is_safe():
                print("  ✅ Safety monitor reports safe")
                checks_passed += 1
            else:
                print("  ❌ Safety monitor reports unsafe")
                
        # Check device connectivity
        for device_name, device in self.devices.items():
            total_checks += 1
            try:
                # Check if device is responsive
                device_info = device.get_device_info()
                print(f"  ✅ {device_name.title()} connected and responsive")
                checks_passed += 1
            except Exception as e:
                print(f"  ❌ {device_name.title()} not responsive: {e}")
                
        success_rate = (checks_passed / total_checks) * 100 if total_checks > 0 else 0
        print(f"📊 Pre-observation checks: {checks_passed}/{total_checks} passed ({success_rate:.1f}%)")
        
        return success_rate >= 80  # Require 80% success rate
        
    def open_observatory(self):
        """Open the observatory for observing"""
        print("🏠 Opening observatory...")
        
        # Unpark telescope
        if 'telescope' in self.devices:
            telescope = self.devices['telescope']
            if hasattr(telescope, 'unpark'):
                telescope.unpark()
                print("  🔭 Telescope unparked")
                
        # Open dome shutter
        if 'dome' in self.devices:
            dome = self.devices['dome']
            dome.open_shutter()
            print("  🏠 Dome shutter opened")
            
            # Slew dome to telescope position
            if 'telescope' in self.devices:
                telescope = self.devices['telescope']
                tel_az = telescope.get_property("azimuth")
                dome.slew_to_azimuth(tel_az)
                print(f"  🏠 Dome slewed to telescope azimuth: {tel_az:.1f}°")
                
        # Open dust cover
        if 'cover' in self.devices:
            cover = self.devices['cover']
            cover.open_cover()
            print("  📦 Dust cover opened")
            
    def setup_imaging(self):
        """Setup imaging parameters"""
        print("📷 Setting up imaging...")
        
        if 'camera' in self.devices:
            camera = self.devices['camera']
            
            # Enable cooling
            camera.set_property("coolerOn", True)
            camera.set_property("targetTemperature", -10.0)
            print("  ❄️ Camera cooling enabled (-10°C target)")
            
            # Set imaging parameters
            camera.set_property("binning", 1)
            camera.set_property("gain", 100)
            print("  📷 Imaging parameters set (1x1 binning, gain 100)")
            
        if 'focuser' in self.devices:
            focuser = self.devices['focuser']
            
            # Move to known good focus position
            focuser.set_property("targetPosition", 25000)
            print("  🎯 Focuser moved to position 25000")
            
    def run_imaging_sequence(self):
        """Run the actual imaging sequence"""
        print("📸 Running imaging sequence...")
        
        if 'camera' not in self.devices:
            print("  ❌ No camera available for imaging")
            return
            
        camera = self.devices['camera']
        
        # Take a series of images
        exposures = [
            ("Light", 300, 5),    # 5x 5-minute light frames
            ("Dark", 300, 3),     # 3x 5-minute dark frames
            ("Flat", 1, 10),      # 10x 1-second flat frames
        ]
        
        for frame_type, exposure_time, count in exposures:
            print(f"  📸 Taking {count}x {exposure_time}s {frame_type} frames...")
            
            for i in range(count):
                try:
                    # Check weather before each exposure
                    if 'weather' in self.devices:
                        weather = self.devices['weather']
                        weather.refresh()
                        if not weather.is_safe_for_observing():
                            print("    ⚠️ Weather turned unsafe, stopping sequence")
                            return
                            
                    # Take exposure
                    if frame_type == "Flat" and 'cover' in self.devices:
                        # Turn on calibrator for flats
                        cover = self.devices['cover']
                        cover.calibrator_on(50)  # 50% brightness
                        
                    camera.start_exposure(exposure_time)
                    print(f"    📷 Exposure {i+1}/{count} started ({exposure_time}s)")
                    
                    # Wait for exposure to complete
                    time.sleep(exposure_time + 2)  # Add 2 seconds for readout
                    
                    # Save image
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = f"{frame_type}_{timestamp}_{i+1:03d}.fits"
                    camera.save_image(filename)
                    print(f"    💾 Saved: {filename}")
                    
                    if frame_type == "Flat" and 'cover' in self.devices:
                        # Turn off calibrator
                        cover = self.devices['cover']
                        cover.calibrator_off()
                        
                except Exception as e:
                    print(f"    ❌ Error in exposure {i+1}: {e}")
                    
    def close_observatory(self):
        """Close the observatory after observing"""
        print("🏠 Closing observatory...")
        
        # Turn off camera cooling
        if 'camera' in self.devices:
            camera = self.devices['camera']
            camera.set_property("coolerOn", False)
            print("  ❄️ Camera cooling disabled")
            
        # Close dust cover
        if 'cover' in self.devices:
            cover = self.devices['cover']
            cover.close_cover()
            print("  📦 Dust cover closed")
            
        # Park telescope
        if 'telescope' in self.devices:
            telescope = self.devices['telescope']
            if hasattr(telescope, 'park'):
                telescope.park()
                print("  🔭 Telescope parked")
                
        # Close dome shutter
        if 'dome' in self.devices:
            dome = self.devices['dome']
            dome.close_shutter()
            print("  🏠 Dome shutter closed")
            
    def show_system_status(self):
        """Show complete system status"""
        print("\n📊 Observatory System Status")
        print("=" * 40)
        
        # Show compatibility statistics
        try:
            stats = hydrogen.get_compatibility_statistics()
            print(f"🔌 ASCOM/INDI Compatibility:")
            print(f"  📈 Total devices: {stats.total_devices}")
            print(f"  🔌 ASCOM-enabled: {stats.ascom_devices}")
            print(f"  🌐 INDI-enabled: {stats.indi_devices}")
            print(f"  ⏱️ System uptime: {stats.uptime.total_seconds():.1f}s")
        except Exception as e:
            print(f"❌ Error getting compatibility stats: {e}")
            
        # Show device status
        print(f"\n📡 Device Status:")
        for device_name, device in self.devices.items():
            try:
                device_info = device.get_device_info()
                device_type = device.get_device_type()
                print(f"  {self.get_device_icon(device_type)} {device_name.title()}: Online ✅")
            except Exception as e:
                print(f"  ❌ {device_name.title()}: Offline ({e})")
                
        # Show weather conditions
        if 'weather' in self.devices:
            try:
                weather = self.devices['weather']
                weather.refresh()
                
                print(f"\n🌤️ Weather Conditions:")
                print(f"  🌡️ Temperature: {weather.get_temperature():.1f}°C")
                print(f"  💧 Humidity: {weather.get_humidity():.1f}%")
                print(f"  💨 Wind Speed: {weather.get_wind_speed():.1f} m/s")
                print(f"  ☁️ Cloud Cover: {weather.get_cloud_cover():.1f}%")
                print(f"  🛡️ Safe for observing: {'Yes' if weather.is_safe_for_observing() else 'No'}")
            except Exception as e:
                print(f"❌ Error getting weather data: {e}")
                
    def get_device_icon(self, device_type):
        """Get emoji icon for device type"""
        icons = {
            "CAMERA": "📷",
            "TELESCOPE": "🔭", 
            "FOCUSER": "🎯",
            "DOME": "🏠",
            "OBSERVING_CONDITIONS": "🌤️",
            "SAFETY_MONITOR": "🛡️",
            "COVER_CALIBRATOR": "📦"
        }
        return icons.get(device_type, "📡")
        
    def cleanup(self):
        """Clean up observatory resources"""
        print("\n🧹 Shutting down observatory...")
        
        self.running = False
        
        # Stop monitoring threads
        if self.weather_monitoring_thread and self.weather_monitoring_thread.is_alive():
            self.weather_monitoring_thread.join(timeout=5)
            
        # Stop all devices
        for device_name, device in self.devices.items():
            try:
                device.stop_device()
                print(f"  ✅ Stopped {device_name}")
            except Exception as e:
                print(f"  ⚠️ Error stopping {device_name}: {e}")
                
        # Shutdown compatibility system
        try:
            hydrogen.shutdown_compatibility_system()
            print("  ✅ Compatibility system shutdown")
        except Exception as e:
            print(f"  ⚠️ Error shutting down compatibility system: {e}")
            
    def run_observatory(self):
        """Run the complete observatory demonstration"""
        try:
            # Initialize
            self.initialize_observatory()
            self.running = True
            
            # Start monitoring
            self.start_weather_monitoring()
            
            # Show initial status
            self.show_system_status()
            
            # Run observation sequence
            print(f"\n⏰ Starting observation in 5 seconds...")
            time.sleep(5)
            
            self.run_observation_sequence()
            
            # Final status
            self.show_system_status()
            
            print("\n🎉 Observatory demonstration completed successfully!")
            
        except KeyboardInterrupt:
            print("\n⏹️ Observatory demonstration interrupted by user")
        except Exception as e:
            print(f"\n❌ Observatory error: {e}")
        finally:
            self.cleanup()

def main():
    """Main function"""
    print("🌟 Hydrogen Complete Observatory Example")
    print("Demonstrating automatic ASCOM/INDI compatibility")
    print("with a full observatory setup")
    print()
    
    observatory = CompleteObservatory()
    observatory.run_observatory()

if __name__ == "__main__":
    main()
