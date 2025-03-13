#!/usr/bin/env python3
import astrocomm
import time
import sys

# 初始化日志
astrocomm.init_logger("py_switch.log", astrocomm.LogLevel.INFO)
astrocomm.log_info("Python Switch Example Starting")

# 创建开关设备
device_id = "python-switch"
if len(sys.argv) > 1:
    device_id = sys.argv[1]

switch_device = astrocomm.device.Switch(device_id, "Python", "PySwitch 8-port")

# 添加开关
switch_device.add_switch("power1", astrocomm.device.SwitchType.TOGGLE, astrocomm.device.SwitchState.OFF)
switch_device.add_switch("power2", astrocomm.device.SwitchType.TOGGLE, astrocomm.device.SwitchState.OFF)
switch_device.add_switch("reset", astrocomm.device.SwitchType.MOMENTARY, astrocomm.device.SwitchState.OFF)
switch_device.add_switch("dew_heater", astrocomm.device.SwitchType.TOGGLE, astrocomm.device.SwitchState.OFF)

# 创建开关组
switch_device.create_switch_group("all_power", ["power1", "power2"])
switch_device.create_switch_group("all_heaters", ["dew_heater"])

# 连接到服务器
host = "localhost"
port = 8000
if len(sys.argv) > 2:
    host = sys.argv[2]
if len(sys.argv) > 3:
    port = int(sys.argv[3])

print(f"Connecting to server at {host}:{port}")
if not switch_device.connect(host, port):
    print("Failed to connect to server")
    sys.exit(1)

# 注册设备
if not switch_device.register_device():
    print("Failed to register device")
    switch_device.disconnect()
    sys.exit(1)

# 启动设备
if not switch_device.start():
    print("Failed to start device")
    switch_device.disconnect()
    sys.exit(1)

print(f"Switch device {device_id} started and registered successfully")
print("Press Ctrl+C to exit")

try:
    # 运行消息循环
    switch_device.run()
except KeyboardInterrupt:
    print("Interrupted by user")
finally:
    # 清理
    switch_device.stop()
    switch_device.disconnect()
    print("Device stopped and disconnected")