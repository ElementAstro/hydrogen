import sys
import time
import threading
import numpy as np
import pyastrodevice as astro

class CustomTelescope(astro.Telescope):
    
    def __init__(self, device_id, manufacturer="Python Custom", model="PySky 1000"):
        super().__init__(device_id, manufacturer, model)

        self.set_property("observer_name", "Python Observer")
        self.set_property("location", {"latitude": 40.0, "longitude": -74.0, "elevation": 100.0})
        self.set_property("firmware_version", "1.0.0-py")
        
        self.register_command_handler("CUSTOM_COMMAND", self.handle_custom_command)
        self.register_command_handler("CALCULATE_FIELD", self.handle_calculate_field)
        
        self.is_custom_tracking = False
        self.custom_tracking_thread = None
        self.running = False
    
    def start(self):
        result = super().start()
        
        if result:
            astro.log_info(f"Custom telescope {self.get_device_id()} started", "CustomTelescope")
            self.running = True
            
            self.custom_tracking_thread = threading.Thread(target=self.custom_task)
            self.custom_tracking_thread.daemon = True
            self.custom_tracking_thread.start()
        
        return result
    
    def stop(self):
        self.running = False
        
        if self.custom_tracking_thread and self.custom_tracking_thread.is_alive():
            self.custom_tracking_thread.join(2.0) 
        
        super().stop()
        astro.log_info(f"Custom telescope {self.get_device_id()} stopped", "CustomTelescope")
    
    def custom_task(self):
        counter = 0
        while self.running:
            time.sleep(5.0)
            
            counter += 1
            self.send_event("CUSTOM_STATUS_UPDATE", {
                "counter": counter,
                "timestamp": astro.get_iso_timestamp(),
                "randomValue": np.random.random()
            })
            
            self.set_property("ambient_temperature", round(15.0 + 5.0 * np.random.random(), 1))
            self.set_property("humidity", round(50.0 + 20.0 * np.random.random(), 1))
            
            astro.log_debug(f"Custom task iteration {counter}", "CustomTelescope")
    
    def handle_custom_command(self, cmd, response):
        astro.log_info(f"Received custom command: {cmd}", "CustomTelescope")
        
        if "parameters" not in cmd or not cmd["parameters"]:
            response["status"] = "ERROR"
            response["details"] = {"message": "Missing parameters"}
            return
        
        params = cmd["parameters"]
        operation = params.get("operation", "")
        
        if operation == "ROTATE":
            angle = params.get("angle", 0)
            axis = params.get("axis", "RA")
            
            response["status"] = "SUCCESS"
            response["details"] = {
                "message": f"Rotated {angle} degrees around {axis} axis",
                "executionTime": astro.get_iso_timestamp()
            }
            
            self.send_event("CUSTOM_OPERATION", {
                "operation": "ROTATE",
                "angle": angle,
                "axis": axis,
                "status": "COMPLETED"
            })
        else:
            response["status"] = "ERROR"
            response["details"] = {"message": f"Unknown operation: {operation}"}
    
    def handle_calculate_field(self, cmd, response):
        params = cmd["parameters"]
        
        fov_width = params.get("fov_width", 1.0) 
        fov_height = params.get("fov_height", 0.75) 
        
        ra_prop = self.get_property("ra")
        dec_prop = self.get_property("dec")
        
        star_density = 100 
        field_area = fov_width * fov_height
        star_count = int(field_area * star_density * (1 + 0.2 * np.random.random()))
        
        # 构建响应
        response["status"] = "SUCCESS"
        response["details"] = {
            "field_center": {"ra": ra_prop, "dec": dec_prop},
            "field_size": {"width": fov_width, "height": fov_height},
            "star_count": star_count,
            "limiting_magnitude": 14.5 + 0.5 * np.random.random(),
            "calculation_time": astro.get_iso_timestamp()
        }
        
        self.send_event("FIELD_CALCULATED", response["details"])


def main():
    astro.init_logger("custom_telescope.log", astro.LogLevel.DEBUG)
    astro.log_info("Starting custom Python telescope device", "Main")
    
    host = "localhost"
    port = 8000
    device_id = "python-telescope"
    
    if len(sys.argv) > 1:
        host = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    if len(sys.argv) > 3:
        device_id = sys.argv[3]
    
    print(f"Connecting to server at {host}:{port}")
    print(f"Device ID: {device_id}")
    
    try:
        telescope = CustomTelescope(device_id)
        
        if not telescope.connect(host, port):
            print("Failed to connect to server")
            sys.exit(1)
        
        if not telescope.register_device():
            print("Failed to register device")
            sys.exit(1)
        
        if not telescope.start():
            print("Failed to start device")
            sys.exit(1)
        
        print("Python telescope device started and registered successfully")
        print("Press Ctrl+C to exit")
        
        telescope.run()
    
    except KeyboardInterrupt:
        print("Interrupted by user")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'telescope' in locals():
            telescope.stop()
            telescope.disconnect()
            print("Device stopped and disconnected")


if __name__ == "__main__":
    main()