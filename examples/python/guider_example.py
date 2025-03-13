# examples/python/guider_example.py
import sys
import time
import os
import threading

# 将生成的模块路径添加到Python搜索路径
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build/python'))

import pyastrocomm as pac
import pydevices

# 创建一个派生的导星器类
class PiGuider(pydevices.PyGuider):
    def __init__(self, device_id, manufacturer="PiGuider", model="PHD3"):
        super().__init__(device_id, manufacturer, model)
        self.connected_camera = None
        self.dither_sequence = [1.5, 2.0, 2.5, 2.0, 1.5]  # 自定义抖动序列
        self.dither_index = 0
        
    def start(self):
        print("Starting PiGuider guide camera...")
        result = super().start()
        
        # 添加额外属性
        self.set_property("pythonGuider", True)
        self.set_property("algorithm", "Neural-PID")
        self.set_property("dither_sequence", self.dither_sequence)
        
        # 设置导星参数
        self.set_aggressiveness(0.8, 0.6)  # RA更积极，DEC更保守
        self.set_guide_rate(0.5, 0.5)
        self.set_calibrated_pixel_scale(1.2)  # 角秒/像素
        
        return result
        
    # 添加自定义方法
    def auto_calibrate_and_guide(self):
        """自动校准然后开始导星"""
        print("Starting automatic calibration...")
        self.start_calibration()
        
        # 监控校准状态的线程
        def monitor_calibration():
            while True:
                time.sleep(1)
                # 这里假设状态是内部枚举，实际中需要从设备获取
                state_prop = self.get_property("calibrationState")
                if state_prop == "COMPLETED":
                    print("Calibration completed, starting guiding...")
                    self.start_guiding()
                    break
                elif state_prop == "FAILED":
                    print("Calibration failed!")
                    break
                
        thread = threading.Thread(target=monitor_calibration)
        thread.daemon = True
        thread.start()
    
    def auto_dither(self):
        """按照预设序列自动抖动"""
        amount = self.dither_sequence[self.dither_index]
        self.dither_index = (self.dither_index + 1) % len(self.dither_sequence)
        print(f"Auto dithering with amount {amount}")
        self.dither(amount, True)

# 初始化日志
pac.init_logger("python_guider.log", pac.LogLevel.DEBUG)

# 创建导星器设备
guider = PiGuider("python-guider")

try:
    # 连接到服务器
    if not guider.connect("localhost", 8000):
        print("Failed to connect to server")
        sys.exit(1)
    
    # 注册设备
    if not guider.register_device():
        print("Failed to register device")
        sys.exit(1)
    
    # 启动设备
    if not guider.start():
        print("Failed to start device")
        sys.exit(1)
    
    print("Guider device started and registered successfully")
    
    # 测试自定义方法
    print("Starting auto calibration and guiding...")
    guider.auto_calibrate_and_guide()
    
    # 等待校准和导星开始
    time.sleep(30)
    
    # 测试抖动
    print("Testing auto dither...")
    for _ in range(3):
        guider.auto_dither()
        time.sleep(10)
    
    # 运行消息循环
    print("Running message loop, press Ctrl+C to exit")
    guider.run()
    
except KeyboardInterrupt:
    print("Interrupted by user")
except Exception as e:
    print(f"Error: {e}")
finally:
    # 确保停止导星
    if guider.get_property("state") == "GUIDING":
        guider.stop_guiding()
    
    # 清理资源
    guider.stop()
    guider.disconnect()