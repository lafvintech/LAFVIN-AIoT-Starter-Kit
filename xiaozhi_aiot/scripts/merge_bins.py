#!/usr/bin/env python3
"""
ESP32-S3 固件合并脚本
将多个bin文件合并成一个完整的固件文件
"""

import os
import sys
import argparse
from pathlib import Path

class BinMerger:
    def __init__(self):
        # ESP32-S3 默认flash大小 (16MB)
        self.flash_size = 16 * 1024 * 1024
        
        # 默认烧录地址映射 (基于你的烧录信息)
        self.default_layout = [
            {"address": 0x0, "file": "bootloader/bootloader.bin", "description": "引导加载程序"},
            {"address": 0x8000, "file": "partition_table/partition-table.bin", "description": "分区表"},
            {"address": 0xd000, "file": "ota_data_initial.bin", "description": "OTA数据初始化"},
            {"address": 0x10000, "file": "srmodels/srmodels.bin", "description": "语音识别模型"},
            {"address": 0x100000, "file": "xiaozhi.bin", "description": "主应用程序"}
        ]
    
    def read_bin_file(self, file_path):
        """读取bin文件"""
        try:
            with open(file_path, 'rb') as f:
                return f.read()
        except FileNotFoundError:
            print(f"错误: 找不到文件 {file_path}")
            return None
        except Exception as e:
            print(f"错误: 读取文件 {file_path} 失败: {e}")
            return None
    
    def merge_bins(self, layout, output_file, base_dir="build"):
        """合并多个bin文件"""
        print("开始合并固件文件...")
        print(f"Flash大小: {self.flash_size / 1024 / 1024:.1f}MB")
        print("-" * 60)
        
        # 创建空的flash内容 (填充0xFF)
        flash_content = bytearray([0xFF] * self.flash_size)
        
        total_size = 0
        
        for item in layout:
            address = item["address"]
            file_path = os.path.join(base_dir, item["file"])
            description = item.get("description", "")
            
            print(f"处理: {description}")
            print(f"  地址: 0x{address:06X}")
            print(f"  文件: {file_path}")
            
            # 读取bin文件
            bin_data = self.read_bin_file(file_path)
            if bin_data is None:
                print(f"  状态: 跳过 (文件不存在)")
                print()
                continue
            
            file_size = len(bin_data)
            print(f"  大小: {file_size} 字节 ({file_size/1024:.1f} KB)")
            
            # 检查地址范围
            if address + file_size > self.flash_size:
                print(f"  错误: 文件超出flash范围!")
                return False
            
            # 检查重叠
            end_address = address + file_size
            if address < total_size:
                print(f"  警告: 可能存在地址重叠!")
            
            # 写入flash内容
            flash_content[address:address + file_size] = bin_data
            total_size = max(total_size, end_address)
            
            print(f"  状态: 成功")
            print()
        
        # 截取有效内容 (去掉末尾的空白区域)
        # 找到最后一个非0xFF字节
        last_byte_pos = self.flash_size - 1
        while last_byte_pos > 0 and flash_content[last_byte_pos] == 0xFF:
            last_byte_pos -= 1
        
        # 保留一些padding
        final_size = ((last_byte_pos + 4096) // 4096) * 4096  # 4KB对齐
        final_content = flash_content[:final_size]
        
        # 写入输出文件
        try:
            with open(output_file, 'wb') as f:
                f.write(final_content)
            
            print(f"合并完成!")
            print(f"输出文件: {output_file}")
            print(f"文件大小: {len(final_content)} 字节 ({len(final_content)/1024/1024:.2f} MB)")
            print(f"Flash使用率: {len(final_content)/self.flash_size*100:.1f}%")
            
            return True
            
        except Exception as e:
            print(f"错误: 写入输出文件失败: {e}")
            return False
    
    def create_layout_from_config(self, config_file):
        """从配置文件创建布局"""
        import json
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                config = json.load(f)
            return config.get("layout", self.default_layout)
        except Exception as e:
            print(f"错误: 读取配置文件失败: {e}")
            return None
    
    def show_layout(self, layout):
        """显示当前布局"""
        print("当前固件布局:")
        print("-" * 60)
        for item in layout:
            address = item["address"]
            file_path = item["file"]
            description = item.get("description", "")
            print(f"0x{address:06X}: {file_path:30} # {description}")
        print("-" * 60)

def main():
    parser = argparse.ArgumentParser(description="ESP32-S3 固件合并工具")
    parser.add_argument("-o", "--output", default="merged_firmware.bin", 
                       help="输出文件名 (默认: merged_firmware.bin)")
    parser.add_argument("-d", "--dir", default="build", 
                       help="bin文件目录 (默认: build)")
    parser.add_argument("-c", "--config", 
                       help="布局配置文件 (JSON格式)")
    parser.add_argument("--show-layout", action="store_true", 
                       help="显示当前布局并退出")
    parser.add_argument("--flash-size", type=int, default=16, 
                       help="Flash大小 (MB, 默认: 16)")
    
    args = parser.parse_args()
    
    merger = BinMerger()
    merger.flash_size = args.flash_size * 1024 * 1024
    
    # 确定布局
    if args.config:
        layout = merger.create_layout_from_config(args.config)
        if layout is None:
            sys.exit(1)
    else:
        layout = merger.default_layout
    
    # 显示布局
    if args.show_layout:
        merger.show_layout(layout)
        return
    
    # 执行合并
    success = merger.merge_bins(layout, args.output, args.dir)
    if not success:
        sys.exit(1)

if __name__ == "__main__":
    main()
