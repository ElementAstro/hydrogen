#!/usr/bin/env python3
import astrocomm
import time
import sys

# 初始化日志
astrocomm.init_logger("py_rotator.log", astrocomm.LogLevel.INFO)
astrocomm.log_info("Python Rotator Example Starting")

# 创建旋转器设备
device_id = "python-rotator"
if len(sys.argv) > 1:
    device_id = sys.argv[1]

rotator_device = astrocomm.device.Rotator(device_id, "Python", "PyRotator Pro")

# 连接到服务器
host = "localhost"
port = 8000
if len(sys.argv) > 2:
    host = sys.argv[2]
if len(sys.argv) > 3:
    port = int(sys.argv[3])

print(f"Connecting to server at {host}:{port}")
if not rotator_device.connect(host, port):
    print("Failed to connect to server")
    sys.exit(1)

# 注册设备
if not rotator_device.register_device():
    print("Failed to register device")
    rotator_device.disconnect()
    sys.exit(1)

# 启动设备
if not rotator_device.start():
    print("Failed to start device")
    rotator_device.disconnect()
    sys.exit(1)

print(f"Rotator device {device_id} started and registered successfully")
print("Press Ctrl+C to exit")

try:
    # 运行消息循环
    rotator_device.run()
except KeyboardInterrupt:
    print("Interrupted by user")
finally:
    # 清理
    rotator_device.stop()
    rotator_device.disconnect()
    print("Device stopped and disconnected")