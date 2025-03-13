# examples/python/filter_wheel_example.py
import sys
import time
import os

# 将生成的模块路径添加到Python搜索路径
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build/python'))

import pyastrocomm as pac
import pydevices

# 创建一个派生的滤镜轮类
class NanoFilters(pydevices.PyFilterWheel):
    def __init__(self, device_id, manufacturer="NanoFilters", model="RGB-Pro"):
        super().__init__(device_id, manufacturer, model)
        # 添加自定义滤镜
        self.filter_names = ["Red", "Green", "Blue", "Luminance", "H-Alpha", "O-III", "S-II"]
        # 添加对应的聚焦偏移量
        self.filter_offsets = [0, 0, 0, -10, 50, 60, 55]
        
    def start(self):
        # 自定义启动逻辑
        print("Starting NanoFilters filter wheel...")
        result = super().start()
        if result:
            # 设置滤镜名称和偏移
            self.set_filter_names(self.filter_names)
            self.set_filter_offsets(self.filter_offsets)
            # 添加额外属性
            self.set_property("filterCount", len(self.filter_names))
            self.set_property("manufacturer", "NanoFilters Inc.")
        return result
    
    # 添加自定义方法
    def goto_filter_by_name(self, filter_name):
        """按名字切换到指定滤镜"""
        if filter_name in self.filter_names:
            position = self.filter_names.index(filter_name)
            print(f"Moving to filter: {filter_name} (position {position})")
            self.set_position(position)
            return True
        else:
            print(f"Filter {filter_name} not found!")
            return False

# 初始化日志
pac.init_logger("python_filter_wheel.log", pac.LogLevel.DEBUG)

# 创建滤镜轮设备
filter_wheel = NanoFilters("python-filter-wheel")

try:
    # 连接到服务器
    if not filter_wheel.connect("localhost", 8000):
        print("Failed to connect to server")
        sys.exit(1)
    
    # 注册设备
    if not filter_wheel.register_device():
        print("Failed to register device")
        sys.exit(1)
    
    # 启动设备
    if not filter_wheel.start():
        print("Failed to start device")
        sys.exit(1)
    
    print("Filter wheel device started and registered successfully")
    
    # 测试自定义方法
    print("Moving to H-Alpha filter")
    filter_wheel.goto_filter_by_name("H-Alpha")
    time.sleep(6)
    
    print("Moving to Blue filter")
    filter_wheel.goto_filter_by_name("Blue")
    time.sleep(4)
    
    # 运行消息循环
    print("Running message loop, press Ctrl+C to exit")
    filter_wheel.run()
    
except KeyboardInterrupt:
    print("Interrupted by user")
except Exception as e:
    print(f"Error: {e}")
finally:
    # 清理资源
    filter_wheel.stop()
    filter_wheel.disconnect()