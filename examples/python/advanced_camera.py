import sys
import time
import threading
import numpy as np
from PIL import Image, ImageDraw
import pyastrodevice as astro

class AdvancedCamera(astro.DeviceBase):
    """高级相机设备实现，从设备基类继承"""
    
    def __init__(self, device_id, manufacturer="Python Imaging", model="PyCamera Pro"):
        # 调用C++父类构造函数
        super().__init__(device_id, "CAMERA", manufacturer, model)
        
        # 相机属性
        self.width = 1936
        self.height = 1096
        self.pixel_size = 5.86
        self.bit_depth = 16
        self.gain = 0
        self.exposure_time = 0.0
        self.cooler_temp = 20.0
        self.sensor_temp = 20.0
        self.cooler_power = 0
        self.cooler_enabled = False
        self.is_exposing = False
        self.image_ready = False
        self.image_data = None
        
        # 初始化属性
        self.set_property("width", self.width)
        self.set_property("height", self.height)
        self.set_property("pixel_size", self.pixel_size)
        self.set_property("bit_depth", self.bit_depth)
        self.set_property("gain", self.gain)
        self.set_property("offset", 10)
        self.set_property("sensor_temperature", self.sensor_temp)
        self.set_property("cooler_temperature", self.cooler_temp)
        self.set_property("cooler_power", self.cooler_power)
        self.set_property("cooler_enabled", self.cooler_enabled)
        self.set_property("exposure_time", self.exposure_time)
        self.set_property("exposing", self.is_exposing)
        self.set_property("image_ready", self.image_ready)
        self.set_property("connected", False)
        
        # 设备功能
        self.capabilities = ["EXPOSURE", "COOLING", "READOUT"]
        
        # 注册命令处理器
        self.register_command_handler("START_EXPOSURE", self.handle_start_exposure)
        self.register_command_handler("ABORT_EXPOSURE", self.handle_abort_exposure)
        self.register_command_handler("SET_COOLER", self.handle_set_cooler)
        self.register_command_handler("GET_IMAGE", self.handle_get_image)
        
        # 线程控制
        self.running = False
        self.update_thread = None
        self.exposure_thread = None
        self.exposure_lock = threading.Lock()
    
    def start(self):
        """启动相机设备"""
        if super().start():
            self.running = True
            self.set_property("connected", True)
            
            # 启动更新线程
            self.update_thread = threading.Thread(target=self.update_loop)
            self.update_thread.daemon = True
            self.update_thread.start()
            
            astro.log_info(f"Advanced camera {self.get_device_id()} started", "AdvancedCamera")
            return True
        return False
    
    def stop(self):
        """停止相机设备"""
        self.running = False
        
        # 等待线程结束
        if self.update_thread and self.update_thread.is_alive():
            self.update_thread.join(2.0)
        
        with self.exposure_lock:
            if self.exposure_thread and self.exposure_thread.is_alive():
                self.exposure_thread.join(2.0)
        
        self.set_property("connected", False)
        super().stop()
        astro.log_info(f"Advanced camera {self.get_device_id()} stopped", "AdvancedCamera")
    
    def update_loop(self):
        """温度和状态更新循环"""
        while self.running:
            time.sleep(1.0)
            
            # 更新温度
            if self.cooler_enabled:
                # 模拟温度变化，逐渐接近目标温度
                temp_diff = self.cooler_temp - self.sensor_temp
                if abs(temp_diff) > 0.1:
                    # 根据温差调整冷却功率
                    self.cooler_power = min(100, max(0, int(abs(temp_diff) * 10)))
                    # 按冷却功率调整温度
                    self.sensor_temp += np.sign(temp_diff) * self.cooler_power * 0.01
                else:
                    self.cooler_power = 10  # 维持温度所需的功率
                    self.sensor_temp = self.cooler_temp
            else:
                # 冷却器关闭，温度逐渐回到环境温度
                self.cooler_power = 0
                ambient_temp = 20.0
                if abs(self.sensor_temp - ambient_temp) > 0.1:
                    self.sensor_temp += (ambient_temp - self.sensor_temp) * 0.05
            
            # 发送更新的属性
            self.set_property("sensor_temperature", round(self.sensor_temp, 2))
            self.set_property("cooler_power", self.cooler_power)
    
    def start_exposure(self, exposure_time, is_light=True):
        """开始曝光"""
        with self.exposure_lock:
            if self.is_exposing:
                astro.log_warning("Exposure already in progress", "AdvancedCamera")
                return False
            
            self.exposure_time = exposure_time
            self.is_exposing = True
            self.image_ready = False
            self.set_property("exposing", True)
            self.set_property("exposure_time", exposure_time)
            self.set_property("image_ready", False)
            
            # 启动曝光线程
            self.exposure_thread = threading.Thread(target=self.exposure_process, args=(exposure_time, is_light))
            self.exposure_thread.daemon = True
            self.exposure_thread.start()
            
            astro.log_info(f"Started exposure: {exposure_time}s", "AdvancedCamera")
            return True
    
    def abort_exposure(self):
        """中止曝光"""
        with self.exposure_lock:
            if not self.is_exposing:
                return False
            
            self.is_exposing = False
            self.set_property("exposing", False)
            
            # 发送中止事件
            self.send_event("EXPOSURE_ABORTED", {
                "timestamp": astro.get_iso_timestamp(),
                "exposure_time": self.exposure_time
            })
            
            astro.log_info("Exposure aborted", "AdvancedCamera")
            return True
    
    def exposure_process(self, exposure_time, is_light):
        """曝光过程"""
        try:
            start_time = time.time()
            
            # 发送开始曝光事件
            self.send_event("EXPOSURE_STARTED", {
                "timestamp": astro.get_iso_timestamp(),
                "exposure_time": exposure_time
            })
            
            # 模拟曝光进度
            while self.is_exposing and (time.time() - start_time) < exposure_time:
                elapsed = time.time() - start_time
                progress = elapsed / exposure_time
                
                # 发送进度事件
                self.send_event("EXPOSURE_PROGRESS", {
                    "progress": progress,
                    "elapsed": elapsed,
                    "remaining": exposure_time - elapsed
                })
                
                time.sleep(0.5)
            
            # 检查是否被中止
            if not self.is_exposing:
                return
            
            # 模拟图像读出
            self.send_event("READING_OUT", {"timestamp": astro.get_iso_timestamp()})
            time.sleep(1.0)  # 模拟读出时间
            
            # 生成模拟图像
            self.generate_image(is_light)
            
            # 设置曝光完成状态
            self.is_exposing = False
            self.image_ready = True
            self.set_property("exposing", False)
            self.set_property("image_ready", True)
            
            # 发送曝光完成事件
            self.send_event("EXPOSURE_COMPLETE", {
                "timestamp": astro.get_iso_timestamp(),
                "exposure_time": exposure_time,
                "is_light": is_light,
                "image_size": {
                    "width": self.width,
                    "height": self.height
                }
            })
            
            astro.log_info("Exposure completed", "AdvancedCamera")
        
        except Exception as e:
            astro.log_error(f"Error during exposure: {e}", "AdvancedCamera")
            self.is_exposing = False
            self.set_property("exposing", False)
            self.send_event("EXPOSURE_ERROR", {
                "timestamp": astro.get_iso_timestamp(),
                "error": str(e)
            })
    
    def generate_image(self, is_light):
        """生成模拟图像"""
        # 创建模拟图像
        if is_light:
            # 为光照图像创建星场
            img = Image.new('L', (self.width, self.height), 1000)  # 背景天光
            draw = ImageDraw.Draw(img)
            
            # 添加随机星星
            num_stars = int(200 + np.random.random() * 300)
            for _ in range(num_stars):
                x = int(np.random.random() * self.width)
                y = int(np.random.random() * self.height)
                brightness = int(np.random.pareto(2.5) * 10000)
                size = 1 + int(np.log(1 + brightness / 1000))
                
                # 绘制星星
                draw.ellipse((x-size, y-size, x+size, y+size), fill=min(65535, brightness + 1000))
            
            # 添加噪声
            noise_level = int(20 + 10 * self.gain)
            noise = np.random.normal(0, noise_level, (self.height, self.width)).astype(np.int16)
            img_array = np.array(img, dtype=np.int16)
            img_array = np.clip(img_array + noise, 0, 65535).astype(np.uint16)
            
            # 存储图像数据
            self.image_data = img_array.tobytes()
        else:
            # 黑场
            noise_level = int(5 + 5 * self.gain)
            dark_current = int(2 * self.exposure_time * np.exp(0.1 * (self.sensor_temp + 20)))
            
            img_array = np.random.normal(dark_current, noise_level, (self.height, self.width))
            img_array = np.clip(img_array, 0, 65535).astype(np.uint16)
            
            # 存储图像数据
            self.image_data = img_array.tobytes()
    
    def set_cooler(self, enabled, temperature=None):
        """设置冷却器状态"""
        old_enabled = self.cooler_enabled
        old_temp = self.cooler_temp
        
        self.cooler_enabled = enabled
        if temperature is not None:
            self.cooler_temp = max(-30, min(50, temperature))
        
        self.set_property("cooler_enabled", enabled)
        self.set_property("cooler_temperature", self.cooler_temp)
        
        # 发送冷却器变更事件
        self.send_event("COOLER_CHANGED", {
            "timestamp": astro.get_iso_timestamp(),
            "enabled": {
                "current": enabled,
                "previous": old_enabled
            },
            "temperature": {
                "current": self.cooler_temp,
                "previous": old_temp
            }
        })
        
        astro.log_info(f"Cooler set to: enabled={enabled}, temp={self.cooler_temp}", "AdvancedCamera")
        return True
    
    # 命令处理器
    def handle_start_exposure(self, cmd, response):
        """处理开始曝光命令"""
        params = cmd["parameters"]
        
        if "duration" not in params:
            response["status"] = "ERROR"
            response["details"] = {"message": "Missing required parameter: duration"}
            return
        
        duration = float(params["duration"])
        is_light = params.get("light", True)
        
        if self.start_exposure(duration, is_light):
            response["status"] = "SUCCESS"
            response["details"] = {
                "exposure_time": duration,
                "started_at": astro.get_iso_timestamp(),
                "estimated_completion": astro.get_iso_timestamp()  # 这里应该计算实际完成时间
            }
        else:
            response["status"] = "ERROR"
            response["details"] = {"message": "Failed to start exposure, camera busy"}
    
    def handle_abort_exposure(self, cmd, response):
        """处理中止曝光命令"""
        if self.abort_exposure():
            response["status"] = "SUCCESS"
            response["details"] = {"message": "Exposure aborted successfully"}
        else:
            response["status"] = "ERROR"
            response["details"] = {"message": "No exposure in progress to abort"}
    
    def handle_set_cooler(self, cmd, response):
        """处理设置冷却器命令"""
        params = cmd["parameters"]
        
        if "enabled" not in params:
            response["status"] = "ERROR"
            response["details"] = {"message": "Missing required parameter: enabled"}
            return
        
        enabled = params["enabled"]
        temperature = params.get("temperature")
        
        if self.set_cooler(enabled, temperature):
            response["status"] = "SUCCESS"
            response["details"] = {
                "cooler_enabled": enabled,
                "cooler_temperature": self.cooler_temp,
                "sensor_temperature": self.sensor_temp
            }
        else:
            response["status"] = "ERROR"
            response["details"] = {"message": "Failed to set cooler"}
    
    def handle_get_image(self, cmd, response):
        """处理获取图像命令"""
        if not self.image_ready or not self.image_data:
            response["status"] = "ERROR"
            response["details"] = {"message": "No image available"}
            return
        
        # 在实际实现中，这里会将图像数据转为可传输格式(如base64)
        # 或通过其他协议提供下载链接
        response["status"] = "SUCCESS"
        response["details"] = {
            "image_ready": True,
            "width": self.width,
            "height": self.height,
            "bit_depth": self.bit_depth,
            "size_bytes": len(self.image_data),
            "download_url": f"http://server/images/{self.get_device_id()}/latest",
            "metadata": {
                "exposure_time": self.exposure_time,
                "gain": self.gain,
                "temperature": self.sensor_temp,
                "timestamp": astro.get_iso_timestamp()
            }
        }


def main():
    # 初始化日志
    astro.init_logger("advanced_camera.log", astro.LogLevel.DEBUG)
    astro.log_info("Starting advanced Python camera device", "Main")
    
    # 解析命令行参数
    host = "localhost"
    port = 8000
    device_id = "python-camera"
    
    if len(sys.argv) > 1:
        host = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    if len(sys.argv) > 3:
        device_id = sys.argv[3]
    
    print(f"Connecting to server at {host}:{port}")
    print(f"Device ID: {device_id}")
    
    try:
        # 创建相机设备
        camera = AdvancedCamera(device_id)
        
        # 连接到服务器
        if not camera.connect(host, port):
            print("Failed to connect to server")
            sys.exit(1)
        
        # 注册设备
        if not camera.register_device():
            print("Failed to register device")
            sys.exit(1)
        
        # 启动设备
        if not camera.start():
            print("Failed to start device")
            sys.exit(1)
        
        print("Python camera device started and registered successfully")
        print("Press Ctrl+C to exit")
        
        # 运行消息循环
        camera.run()
    
    except KeyboardInterrupt:
        print("Interrupted by user")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'camera' in locals():
            camera.stop()
            camera.disconnect()
            print("Device stopped and disconnected")


if __name__ == "__main__":
    main()