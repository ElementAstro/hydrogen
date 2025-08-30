#!/usr/bin/env python3
"""
Hydrogen Automatic ASCOM/INDI Compatibility Example

This example demonstrates how to use the automatic ASCOM/INDI compatibility
system from Python, enabling seamless protocol integration for devices.
"""

import sys
import time
import os
import threading
from typing import Optional

# Add the build directory to Python path
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build'))

try:
    import pyhydrogen as hydrogen
except ImportError:
    print("Error: pyhydrogen module not found. Make sure to build the Python bindings first.")
    print("Run: cmake --build build --target pyhydrogen")
    sys.exit(1)

class CompatibilityDemo:
    """Demonstrates automatic ASCOM/INDI compatibility features"""
    
    def __init__(self):
        self.devices = {}
        self.running = False
        
    def initialize_system(self):
        """Initialize the automatic compatibility system"""
        print("🚀 Initializing Hydrogen Automatic ASCOM/INDI Compatibility System")
        
        # Set logging level
        hydrogen.set_log_level("info")
        
        # Initialize the compatibility system
        hydrogen.init_compatibility_system(
            enable_auto_discovery=True,
            enable_ascom=True,
            enable_indi=True,
            indi_base_port=7624
        )
        
        print("✅ Compatibility system initialized")
        
    def create_compatible_devices(self):
        """Create devices with automatic ASCOM/INDI compatibility"""
        print("\n📷 Creating devices with automatic compatibility...")
        
        try:
            # Create camera with automatic compatibility
            self.devices['camera'] = hydrogen.create_compatible_camera(
                "demo_camera", "ZWO", "ASI294MM Pro"
            )
            print("  ✅ Camera created with ASCOM/INDI compatibility")
            
            # Create telescope with automatic compatibility
            self.devices['telescope'] = hydrogen.create_compatible_telescope(
                "demo_telescope", "Celestron", "CGX-L"
            )
            print("  ✅ Telescope created with ASCOM/INDI compatibility")
            
            # Create focuser with automatic compatibility
            self.devices['focuser'] = hydrogen.create_compatible_focuser(
                "demo_focuser", "ZWO", "EAF"
            )
            print("  ✅ Focuser created with ASCOM/INDI compatibility")
            
            # Create dome with automatic compatibility
            self.devices['dome'] = hydrogen.create_compatible_dome(
                "demo_dome", "Generic", "Observatory Dome"
            )
            print("  ✅ Dome created with ASCOM/INDI compatibility")
            
            # Create observing conditions with automatic compatibility
            self.devices['weather'] = hydrogen.create_compatible_observing_conditions(
                "demo_weather", "Generic", "Weather Station"
            )
            print("  ✅ Weather station created with ASCOM/INDI compatibility")
            
        except Exception as e:
            print(f"  ❌ Error creating devices: {e}")
            return False
            
        return True
        
    def demonstrate_device_operations(self):
        """Demonstrate basic device operations"""
        print("\n🔧 Demonstrating device operations...")
        
        try:
            # Camera operations
            camera = self.devices.get('camera')
            if camera:
                print("  📷 Camera operations:")
                camera.set_property("coolerOn", True)
                camera.set_property("temperature", -10.0)
                print(f"    - Cooler enabled, target temperature: {camera.get_property('temperature')}°C")
                
                # Start a short exposure
                camera.start_exposure(1.0)  # 1 second exposure
                print("    - Started 1-second exposure")
                
            # Telescope operations
            telescope = self.devices.get('telescope')
            if telescope:
                print("  🔭 Telescope operations:")
                telescope.set_property("tracking", True)
                print("    - Tracking enabled")
                
                # Set target coordinates (example: M42)
                telescope.set_property("targetRA", 5.59)  # 5h 35m
                telescope.set_property("targetDec", -5.39)  # -5° 23'
                print("    - Target set to M42 (Orion Nebula)")
                
            # Focuser operations
            focuser = self.devices.get('focuser')
            if focuser:
                print("  🎯 Focuser operations:")
                current_pos = focuser.get_property("position")
                new_pos = current_pos + 100
                focuser.set_property("targetPosition", new_pos)
                print(f"    - Moving from {current_pos} to {new_pos} steps")
                
            # Dome operations
            dome = self.devices.get('dome')
            if dome:
                print("  🏠 Dome operations:")
                dome.open_shutter()
                dome.slew_to_azimuth(180.0)  # Point south
                print("    - Opening shutter and slewing to south")
                
            # Weather station operations
            weather = self.devices.get('weather')
            if weather:
                print("  🌤️ Weather station operations:")
                weather.refresh()
                temp = weather.get_temperature()
                humidity = weather.get_humidity()
                print(f"    - Temperature: {temp}°C, Humidity: {humidity}%")
                
        except Exception as e:
            print(f"  ❌ Error in device operations: {e}")
            
    def demonstrate_protocol_transparency(self):
        """Demonstrate transparent protocol access"""
        print("\n🔄 Demonstrating protocol transparency...")
        
        try:
            camera = self.devices.get('camera')
            if camera:
                print("  📷 Camera protocol transparency:")
                
                # The same property accessed through different protocols
                # Note: In the actual implementation, you would access the bridge
                # For this demo, we'll show the concept
                
                # Internal protocol (direct device access)
                internal_temp = camera.get_property("temperature")
                print(f"    - Internal protocol temperature: {internal_temp}°C")
                
                # ASCOM protocol (would be CCDTemperature)
                print("    - ASCOM protocol: Property automatically mapped to CCDTemperature")
                
                # INDI protocol (would be CCD_TEMPERATURE)
                print("    - INDI protocol: Property automatically mapped to CCD_TEMPERATURE")
                
                print("    - All protocols stay synchronized automatically! 🎯")
                
        except Exception as e:
            print(f"  ❌ Error demonstrating protocol transparency: {e}")
            
    def show_system_statistics(self):
        """Show compatibility system statistics"""
        print("\n📊 System Statistics:")
        
        try:
            stats = hydrogen.get_compatibility_statistics()
            
            print(f"  📈 Total devices: {stats.total_devices}")
            print(f"  🔌 ASCOM-enabled devices: {stats.ascom_devices}")
            print(f"  🌐 INDI-enabled devices: {stats.indi_devices}")
            print(f"  ⏱️ System uptime: {stats.uptime.total_seconds():.1f} seconds")
            
            if stats.device_type_count:
                print("  📋 Device types:")
                for device_type, count in stats.device_type_count.items():
                    print(f"    - {device_type}: {count}")
                    
        except Exception as e:
            print(f"  ❌ Error getting statistics: {e}")
            
    def simulate_ascom_client(self):
        """Simulate an ASCOM client connecting to devices"""
        print("\n🔌 Simulating ASCOM client connections...")
        
        print("  🎯 ASCOM clients can now connect to:")
        for device_name, device in self.devices.items():
            device_type = device.get_device_type()
            device_id = device.get_device_id()
            print(f"    - {device_type} ({device_id}) via ASCOM COM interface")
            
        print("  📝 Example ASCOM client code (VB.NET/C#):")
        print("    Dim camera As Object = CreateObject(\"Hydrogen.Camera.demo_camera\")")
        print("    camera.Connected = True")
        print("    camera.StartExposure(5.0, True)")
        
    def simulate_indi_client(self):
        """Simulate an INDI client connecting to devices"""
        print("\n🌐 Simulating INDI client connections...")
        
        print("  🎯 INDI clients can now connect to:")
        for device_name, device in self.devices.items():
            device_type = device.get_device_type()
            device_id = device.get_device_id()
            print(f"    - {device_type} ({device_id}) via INDI XML protocol on port 7624+")
            
        print("  📝 Example INDI client code (Python):")
        print("    import PyIndi")
        print("    client = PyIndi.BaseClient()")
        print("    client.setServer(\"localhost\", 7624)")
        print("    client.connectServer()")
        
    def cleanup(self):
        """Clean up resources"""
        print("\n🧹 Cleaning up...")
        
        try:
            # Stop all devices
            for device_name, device in self.devices.items():
                try:
                    device.stop()
                    print(f"  ✅ Stopped {device_name}")
                except Exception as e:
                    print(f"  ⚠️ Error stopping {device_name}: {e}")
                    
            # Shutdown compatibility system
            hydrogen.shutdown_compatibility_system()
            print("  ✅ Compatibility system shutdown")
            
        except Exception as e:
            print(f"  ❌ Error during cleanup: {e}")
            
    def run_demo(self):
        """Run the complete demonstration"""
        print("=" * 60)
        print("🌟 Hydrogen Automatic ASCOM/INDI Compatibility Demo")
        print("=" * 60)
        
        try:
            # Initialize system
            self.initialize_system()
            
            # Create devices
            if not self.create_compatible_devices():
                return
                
            # Wait a moment for initialization
            time.sleep(1)
            
            # Demonstrate operations
            self.demonstrate_device_operations()
            
            # Show protocol transparency
            self.demonstrate_protocol_transparency()
            
            # Show statistics
            self.show_system_statistics()
            
            # Simulate client connections
            self.simulate_ascom_client()
            self.simulate_indi_client()
            
            print("\n🎉 Demo completed successfully!")
            print("\n💡 Key Benefits:")
            print("  ✅ Zero code changes required for existing devices")
            print("  ✅ Automatic ASCOM/INDI protocol support")
            print("  ✅ Transparent property synchronization")
            print("  ✅ Universal client compatibility")
            print("  ✅ Real-time protocol bridging")
            
        except KeyboardInterrupt:
            print("\n⏹️ Demo interrupted by user")
        except Exception as e:
            print(f"\n❌ Demo error: {e}")
        finally:
            self.cleanup()

def main():
    """Main function"""
    demo = CompatibilityDemo()
    demo.run_demo()

if __name__ == "__main__":
    main()
