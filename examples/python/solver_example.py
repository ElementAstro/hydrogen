# examples/python/solver_example.py
import sys
import time
import os
import numpy as np
from PIL import Image

# 将生成的模块路径添加到Python搜索路径
sys.path.append(os.path.join(os.path.dirname(__file__), '../../build/python'))

import pyastrocomm as pac
import pydevices

# 创建一个派生的解析器类
class AIPlateSolver(pydevices.PySolver):
    def __init__(self, device_id, manufacturer="AIPlateSolver", model="DeepSolve"):
        super().__init__(device_id, manufacturer, model)
        self.local_catalog_path = "/path/to/star/catalog"
        self.ai_model_loaded = False
        
    def start(self):
        print("Starting AI Plate Solver...")
        result = super().start()
        
        if result:
            # 模拟加载AI模型
            print("Loading deep learning model for plate solving...")
            time.sleep(2)  # 模拟加载延迟
            self.ai_model_loaded = True
            
            # 设置解析参数
            params = {
                "fovMin": 5,
                "fovMax": 120,
                "scaleMin": 0.5,
                "scaleMax": 5,
                "useDistortion": True,
                "downsample": 2
            }
            self.set_parameters(params)
            
            # 设置解析器路径和选项
            self.set_solver_path("/usr/local/bin/astrometrynet")
            self.set_solver_options({
                "use_gpu": "true",
                "backend": "pytorch",
                "precision": "high"
            })
            
            # 添加自定义属性
            self.set_property("ai_model", "StarNet v2")
            self.set_property("catalog", "GAIA DR3")
            self.set_property("ai_model_loaded", self.ai_model_loaded)
            
        return result
    
    # 添加自定义方法
    def load_and_solve_image(self, image_path, hint_ra=None, hint_dec=None):
        """加载图像文件并解析"""
        try:
            # 如果支持PIL，打开图像并转换为numpy数组
            img = Image.open(image_path).convert('L')  # 转为灰度
            width, height = img.size
            img_array = np.array(img).flatten().astype(np.uint8)
            
            # 设置位置提示（如果有）
            params = {}
            if hint_ra is not None:
                params["raHint"] = hint_ra
            if hint_dec is not None:
                params["decHint"] = hint_dec
            
            if params:
                self.set_parameters(params)
                
            print(f"Solving image: {image_path} ({width}x{height})")
            self.solve(img_array, width, height)
            
            return True
        except Exception as e:
            print(f"Error loading image: {e}")
            return False
    
    def get_solution_as_dict(self):
        """获取最后的解析结果作为Python字典"""
        solution = self.get_last_solution()
        if not solution or "success" not in solution or not solution["success"]:
            return None
        
        # 格式化为更友好的字典
        result = {
            "ra_hours": solution["ra"],
            "dec_degrees": solution["dec"],
            "ra_formatted": solution["ra_hms"],
            "dec_formatted": solution["dec_dms"],
            "pixel_scale": solution["pixelScale"],
            "field_width_arcmin": solution["fieldWidth"],
            "field_height_arcmin": solution["fieldHeight"],
            "rotation_degrees": solution["rotation"],
            "stars_detected": solution["starCount"],
            "solve_time_seconds": solution["solveTime"]
        }
        
        if "stars" in solution:
            result["stars"] = solution["stars"]
            
        return result

# 初始化日志
pac.init_logger("python_solver.log", pac.LogLevel.DEBUG)

# 创建解析器设备
solver = AIPlateSolver("python-solver")

try:
    # 连接到服务器
    if not solver.connect("localhost", 8000):
        print("Failed to connect to server")
        sys.exit(1)
    
    # 注册设备
    if not solver.register_device():
        print("Failed to register device")
        sys.exit(1)
    
    # 启动设备
    if not solver.start():
        print("Failed to start device")
        sys.exit(1)
    
    print("Plate solver device started and registered successfully")
    
    # 模拟从文件解析
    print("\nSolving from file...")
    solver.solve_from_file("/path/to/image.fits")
    
    # 等待解析完成
    solving = True
    while solving:
        time.sleep(1)
        state = solver.get_property("state")
        progress = solver.get_property("progress")
        print(f"Solving state: {state}, progress: {progress}%")
        solving = (state == "SOLVING")
    
    # 获取结果
    solution = solver.get_solution_as_dict()
    if solution:
        print("\nSolution found:")
        for key, value in solution.items():
            if key != "stars":  # 太长不打印
                print(f"  {key}: {value}")
        print(f"  Number of stars: {len(solution.get('stars', []))}")
    else:
        print("\nSolving failed or no solution available")
    
    # 监听解析器消息
    print("\nRunning message loop, press Ctrl+C to exit")
    solver.run()
    
except KeyboardInterrupt:
    print("Interrupted by user")
except Exception as e:
    print(f"Error: {e}")
finally:
    # 清理资源
    solver.stop()
    solver.disconnect()