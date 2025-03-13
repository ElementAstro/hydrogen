import astrocomm
import time
import random

class MyCustomSwitch(astrocomm.device.Switch):
    def __init__(self, device_id):
        super().__init__(device_id, "PythonCustom", "AdvancedSwitch")
        # 初始化额外属性
        self.temperature = 20.0
        self.set_property("temperature", self.temperature)
        
        # 添加开关
        self.add_switch("main_power", astrocomm.device.SwitchType.TOGGLE)
        self.add_switch("aux_power", astrocomm.device.SwitchType.TOGGLE)
        
        # 注册自定义命令
        self.register_command_handler("GET_TEMPERATURE", self.handle_get_temp)
        self.register_command_handler("SET_TEMPERATURE", self.handle_set_temp)
    
    def start(self):
        result = super().start()
        if result:
            # 启动温度更新线程
            import threading
            self.running = True
            self.update_thread = threading.Thread(target=self.update_temp)
            self.update_thread.daemon = True
            self.update_thread.start()
        return result
    
    def stop(self):
        self.running = False
        if hasattr(self, 'update_thread'):
            self.update_thread.join(timeout=1.0)
        super().stop()
    
    def update_temp(self):
        while self.running:
            # 模拟温度变化
            self.temperature += random.uniform(-0.5, 0.5)
            self.set_property("temperature", self.temperature)
            
            # 发送温度变更事件
            if random.random() < 0.2:  # 20%概率发送事件
                event = astrocomm.common.EventMessage("TEMPERATURE_CHANGED")
                event.set_details({"temperature": self.temperature})
                self.send_event(event)
            
            time.sleep(2)
    
    def handle_get_temp(self, cmd, response):
        response.set_status("SUCCESS")
        response.set_details({"temperature": self.temperature})
    
    def handle_set_temp(self, cmd, response):
        params = cmd.get_parameters()
        if "temperature" in params:
            self.temperature = params["temperature"]
            self.set_property("temperature", self.temperature)
            response.set_status("SUCCESS")
        else:
            response.set_status("ERROR")
            response.set_details({"message": "Missing temperature parameter"})

# 创建和使用自定义设备
if __name__ == "__main__":
    # 初始化日志
    astrocomm.init_logger("custom_switch.log")
    
    # 创建自定义设备
    device = MyCustomSwitch("python-custom-switch")
    
    # 连接到服务器
    if not device.connect("localhost", 8000):
        print("Failed to connect to server")
        exit(1)
    
    # 注册设备
    if not device.register_device():
        print("Failed to register device")
        device.disconnect()
        exit(1)
    
    # 启动设备
    if not device.start():
        print("Failed to start device")
        device.disconnect()
        exit(1)
    
    print("Custom device started. Press Ctrl+C to exit")
    
    try:
        # 运行消息循环
        device.run()
    except KeyboardInterrupt:
        print("Interrupted by user")
    finally:
        device.stop()
        device.disconnect()
        print("Device stopped and disconnected")