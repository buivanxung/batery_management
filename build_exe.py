#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Build standalone .exe for Flash Board GUI
Creates a single executable file with all dependencies bundled
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    script_dir = Path(__file__).parent
    gui_script = script_dir / "flash_board_gui.py"
    
    # Check if GUI script exists
    if not gui_script.exists():
        print(f"❌ Lỗi: Không tìm thấy {gui_script}")
        return False
    
    print("🔨 Chuẩn bị build standalone .exe...")
    print()
    
    # Step 1: Install PyInstaller if not already installed
    print("📦 Cài đặt PyInstaller...")
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", "-q", "pyinstaller"],
            check=True
        )
        print("✓ PyInstaller ready")
    except subprocess.CalledProcessError:
        print("❌ Lỗi: Không thể cài PyInstaller")
        return False
    
    print()
    
    # Step 2: Build .exe using PyInstaller
    print("🔨 Đang build executable...")
    
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",                    # Single .exe file
        "--windowed",                   # No console window
        "--icon=NONE",                  # No custom icon
        "--name=FlashBoard-GUI",        # Executable name
        "--distpath=./dist",            # Output directory
        "--workpath=./build",           # Build directory  
        "--specpath=./",                # Spec file location
        str(gui_script)                 # Target script
    ]
    
    try:
        result = subprocess.run(cmd, cwd=str(script_dir), check=True)
        print("✓ Build thành công!")
    except subprocess.CalledProcessError as e:
        print(f"❌ Lỗi build: {e}")
        return False
    
    print()
    
    # Step 3: Check output
    exe_path = script_dir / "dist" / "FlashBoard-GUI.exe"
    if exe_path.exists():
        size_mb = exe_path.stat().st_size / (1024 * 1024)
        print(f"✅ Thành công!")
        print(f"📁 File:    {exe_path}")
        print(f"💾 Kích thước: {size_mb:.1f} MB")
        print()
        print("🎉 Bạn có thể di chuyển file .exe sang máy khác mà không cần install gì!")
        return True
    else:
        print(f"❌ Lỗi: Không tìm thấy {exe_path}")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
