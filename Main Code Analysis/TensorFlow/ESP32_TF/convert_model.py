#!/usr/bin/env python3
"""
将TensorFlow Lite模型转换为C头文件
使用方法: python3 convert_model.py anomaly_detection_model.tflite include/anomaly_model_data.h
"""

import sys
import os

def convert_tflite_to_header(tflite_path, output_path):
    """将tflite文件转换为C头文件"""

    if not os.path.exists(tflite_path):
        print(f"错误: 找不到文件 {tflite_path}")
        return False
    
    # 读取tflite文件
    with open(tflite_path, 'rb') as f:
        model_data = f.read()
    
    file_size = len(model_data)
    print(f"模型大小: {file_size} bytes")
    
    # 创建输出目录
    output_dir = os.path.dirname(output_path)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 生成C头文件
    with open(output_path, 'w') as f:
        f.write("// Auto-generated TensorFlow Lite model data\n")
        f.write("// DO NOT EDIT\n\n")
        f.write("#ifndef ANOMALY_MODEL_DATA_H\n")
        f.write("#define ANOMALY_MODEL_DATA_H\n\n")
        
        f.write("#include <stdint.h>\n\n")
        
        f.write(f"// 模型数据大小: {file_size} bytes\n")
        f.write("const unsigned char anomaly_model_tflite[] = {\n")
        
        # 将二进制数据转换为16进制数组
        for i, byte in enumerate(model_data):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x}")
            if i < len(model_data) - 1:
                f.write(", ")
            if (i + 1) % 16 == 0:
                f.write("\n")
        
        f.write("\n};\n\n")
        f.write(f"const int anomaly_model_tflite_len = {file_size};\n\n")
        f.write("#endif // ANOMALY_MODEL_DATA_H\n")
    
    print(f"成功: 已生成 {output_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("用法: python3 convert_model.py <input.tflite> <output.h>")
        print("示例: python3 convert_model.py anomaly_detection_model.tflite include/anomaly_model_data.h")
        sys.exit(1)
    
    tflite_file = sys.argv[1]
    output_file = sys.argv[2]
    
    if convert_tflite_to_header(tflite_file, output_file):
        sys.exit(0)
    else:
        sys.exit(1)
