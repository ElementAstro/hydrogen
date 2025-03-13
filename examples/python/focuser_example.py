# examples/python/focuser_example.py
import sys
import time
import os

# 将生成的模块路径添加到Python搜索路径
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build/python'))

import pyastrocomm as pac
import pydevices

# 创建一个派生的调焦器类
class MyCustomFocuser(pydevices.PyFocuser):
    def __init__(self, device_id, manufacturer="Custom", model="PiFocuser"):
        super().__init__(device_id, manufacturer, model)
        self.custom_property = "Custom property"
        
    def start(self):
        # 在启动前添加自定义逻辑
        print("Starting custom focuser...")
        self.set_property("custom_feature", True)
        return super().start()
        
    def get_device_info(self):
        # 增强设备信息
        info = super().get_device_info()
        info["customInfo"] = {
            "creator": "Python Example",
            "version": "1.0.0",
            "features": ["custom_move", "extra_precision"]
        }
        return info
        
    def custom_move(self, target, speed_factor=1.0):
        """自定义移动函数"""
        print(f"Custom move to {target} with speed factor {speed_factor}")
        self.set_speed(int(5 * speed_factor))
        self.move_absolute(target)
        return True

# 初始化日志
pac.init_logger("python_focuser.log", pac.LogLevel.DEBUG)

# 创建调焦器设备
focuser = MyCustomFocuser("python-focuser", "PythonFocuser", "AutoFocus-Pro")

try:
    # 连接到服务器
    if not focuser.connect("localhost", 8000):
        print("Failed to connect to server")
        sys.exit(1)
    
    # 注册设备
    if not focuser.register_device():
        print("Failed to register device")
        sys.exit(1)
    
    # 启动设备
    if not focuser.start():
        print("Failed to start device")
        sys.exit(1)
    
    print("Focuser device started and registered successfully")
    
    # 调用自定义方法
    focuser.custom_move(5000, 1.2)
    
    # 测试基本功能
    time.sleep(2)
    
    print("Moving to position 8000")
    focuser.move_absolute(8000)
    time.sleep(5)
    
    print("Moving relative -2000 steps")
    focuser.move_relative(-2000)
    time.sleep(5)
    
    print("Enabling temperature compensation")
    focuser.set_temperature_compensation(True, 15.0)
    
    # 运行消息循环
    print("Running message loop, press Ctrl+C to exit")
    focuser.run()
    
except KeyboardInterrupt:
    print("Interrupted by user")
except Exception as e:
    print(f"Error: {e}")
finally:
    # 清理资源
    focuser.stop()
    focuser.disconnect()