#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Flask Board GUI Tool for Windows - No Command Line Needed!
Wraps flash_board.py with a user-friendly interface
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import subprocess
import threading
import os
import sys
from pathlib import Path
from datetime import datetime

class FlashBoardGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("⚡ Flash Board Tool - Power Banking")
        self.root.geometry("900x700")
        self.root.resizable(False, False)
        self.root.option_add("*Font", ("Segoe UI", 10))
        
        # Style
        style = ttk.Style()
        style.theme_use('clam')
        
        # Variables
        self.motors_var = tk.StringVar(value="8")
        self.port_var = tk.StringVar()
        self.audio_dir = tk.StringVar(value="./out")
        self.process_running = False
        
        self.setup_ui()
        self.bootstrap_dependencies()
        self.refresh_ports()

    def _utf8_env(self):
        """Force UTF-8 so Vietnamese text is shown correctly in logs on Windows."""
        env = os.environ.copy()
        env["PYTHONUTF8"] = "1"
        env["PYTHONIOENCODING"] = "utf-8"
        return env

    def _is_module_available(self, module_import_path):
        """Check whether a Python module can be imported by the current interpreter."""
        result = subprocess.run(
            [sys.executable, "-c", f"import {module_import_path}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=str(Path(__file__).parent)
        )
        return result.returncode == 0

    def _install_package(self, package_name):
        """Install package with pip using the same Python interpreter as this GUI."""
        self.log(f"Đang cài package: {package_name}")
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", "--upgrade", package_name],
            cwd=str(Path(__file__).parent),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=self._utf8_env()
        )
        if result.stdout:
            for line in result.stdout.splitlines():
                if line.strip():
                    self.log(line, timestamp=False)
        return result.returncode == 0

    def bootstrap_dependencies(self):
        """Auto-install runtime dependencies so the GUI can run on fresh machines."""
        self.log("Kiểm tra dependency Python...", timestamp=False)

        if not self._is_module_available("serial"):
            self.log("Thiếu pyserial, tự động cài đặt...", timestamp=False)
            if not self._install_package("pyserial"):
                messagebox.showerror(
                    "Thiếu dependency",
                    "Không thể cài pyserial tự động.\nHãy chạy: python -m pip install pyserial"
                )

        if not self._is_module_available("platformio"):
            self.log("Thiếu platformio, tự động cài đặt...", timestamp=False)
            if not self._install_package("platformio"):
                messagebox.showerror(
                    "Thiếu dependency",
                    "Không thể cài platformio tự động.\nHãy chạy: python -m pip install platformio"
                )
        
    def setup_ui(self):
        """Setup user interface"""
        
        # Header
        header = ttk.Frame(self.root)
        header.pack(fill=tk.X, padx=10, pady=10)
        
        title = ttk.Label(header, text="⚡ FLASH BOARD TOOLS", font=("Arial", 16, "bold"))
        title.pack(side=tk.LEFT)
        
        subtitle = ttk.Label(header, text="Nạp Firmware & Audio cho STM32 + STM8", font=("Segoe UI", 10))
        subtitle.pack(side=tk.LEFT, padx=20)
        
        # Separator
        ttk.Separator(self.root, orient=tk.HORIZONTAL).pack(fill=tk.X, padx=10)
        
        # Settings Frame
        settings_frame = ttk.LabelFrame(self.root, text="⚙️  Cấu Hình", padding=10)
        settings_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Motors selection
        ttk.Label(settings_frame, text="Số Motor:").grid(row=0, column=0, sticky=tk.W, pady=5)
        motor_combo = ttk.Combobox(settings_frame, textvariable=self.motors_var, 
                                   values=["6", "8"], state="readonly", width=15)
        motor_combo.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        # Port selection
        ttk.Label(settings_frame, text="COM Port:").grid(row=1, column=0, sticky=tk.W, pady=5)
        port_frame = ttk.Frame(settings_frame)
        port_frame.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        self.port_combo = ttk.Combobox(port_frame, textvariable=self.port_var, state="readonly", width=15)
        self.port_combo.pack(side=tk.LEFT)
        
        refresh_btn = ttk.Button(port_frame, text="🔄 Refresh", command=self.refresh_ports, width=10)
        refresh_btn.pack(side=tk.LEFT, padx=5)
        
        # Audio directory
        ttk.Label(settings_frame, text="Audio Dir:").grid(row=2, column=0, sticky=tk.W, pady=5)
        audio_frame = ttk.Frame(settings_frame)
        audio_frame.grid(row=2, column=1, sticky=tk.W+tk.E, padx=5)
        
        audio_entry = ttk.Entry(audio_frame, textvariable=self.audio_dir, width=35)
        audio_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        
        browse_btn = ttk.Button(audio_frame, text="Browse...", command=self.browse_audio, width=12)
        browse_btn.pack(side=tk.LEFT, padx=5)
        
        # Action Buttons Frame
        buttons_frame = ttk.LabelFrame(self.root, text="🎬 Thao Tác", padding=10)
        buttons_frame.pack(fill=tk.X, padx=10, pady=10)
        
        btn_grid = ttk.Frame(buttons_frame)
        btn_grid.pack(fill=tk.X)
        
        # Action buttons
        self.build_btn = ttk.Button(btn_grid, text="🔨 Build Only", command=self.build_only, width=18)
        self.build_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        self.upload_btn = ttk.Button(btn_grid, text="⬆️ Upload Only", command=self.upload_only, width=18)
        self.upload_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        self.build_upload_btn = ttk.Button(btn_grid, text="🚀 Build + Upload", command=self.build_and_upload, width=18)
        self.build_upload_btn.pack(side=tk.LEFT, padx=5, pady=5)

        self.stm8_btn = ttk.Button(btn_grid, text="🔐 STM8 Upload", command=self.stm8_unlock_upload, width=20)
        self.stm8_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Advanced options frame
        adv_frame = ttk.LabelFrame(self.root, text="📋 Tùy Chọn Nâng Cao", padding=10)
        adv_frame.pack(fill=tk.X, padx=10, pady=10)
        
        row2_frame = ttk.Frame(adv_frame)
        row2_frame.pack(fill=tk.X)
        
        self.skip_fw_var = tk.BooleanVar()
        self.skip_fw_check = ttk.Checkbutton(row2_frame, text="Bỏ qua Firmware", variable=self.skip_fw_var)
        self.skip_fw_check.pack(side=tk.LEFT, padx=5)
        
        self.skip_audio_var = tk.BooleanVar()
        self.skip_audio_check = ttk.Checkbutton(row2_frame, text="Bỏ qua Audio", variable=self.skip_audio_var)
        self.skip_audio_check.pack(side=tk.LEFT, padx=5)
        
        self.flash_audio_btn = ttk.Button(row2_frame, text="💾 Flash Audio Only", command=self.flash_audio_only, width=20)
        self.flash_audio_btn.pack(side=tk.LEFT, padx=5)
        
        # Output/Log Frame
        log_frame = ttk.LabelFrame(self.root, text="📝 Output Log", padding=10)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(log_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Text widget
        self.output_text = tk.Text(log_frame, height=15, yscrollcommand=scrollbar.set,
                                    font=("Consolas", 10), wrap=tk.WORD)
        self.output_text.pack(fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.output_text.yview)
        
        # Status bar
        status_frame = ttk.Frame(self.root)
        status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.status_var = tk.StringVar(value="✓ Sẵn sàng")
        self.status_label = ttk.Label(status_frame, textvariable=self.status_var, 
                                      foreground="green", font=("Segoe UI", 10))
        self.status_label.pack(side=tk.LEFT)
        
        self.progress_var = tk.StringVar(value="")
        self.progress_label = ttk.Label(status_frame, textvariable=self.progress_var)
        self.progress_label.pack(side=tk.RIGHT)
    
    def refresh_ports(self):
        """Refresh available COM ports"""
        try:
            import serial.tools.list_ports as list_ports
        except ImportError:
            self.port_combo['values'] = ["Thiếu pyserial"]
            self.port_var.set("Thiếu pyserial")
            self.log("✗ Không thể đọc COM port: thiếu pyserial", timestamp=False)
            return

        ports = []
        for port, desc, hwid in list_ports.comports():
            ports.append(f"{port} ({desc})")
        
        self.port_combo['values'] = ports if ports else ["Không tìm thấy port"]
        
        if ports and not self.port_var.get():
            self.port_combo.current(0)
        
        self.log(f"✓ Tìm thấy {len(ports)} COM port", timestamp=False)
    
    def browse_audio(self):
        """Browse for audio directory"""
        folder = filedialog.askdirectory(title="Chọn thư mục chứa audio files")
        if folder:
            self.audio_dir.set(folder)
            self.log(f"Audio dir: {folder}", timestamp=False)
    
    def log(self, message, timestamp=True):
        """Add message to output log"""
        if timestamp:
            time_str = datetime.now().strftime("%H:%M:%S")
            message = f"[{time_str}] {message}"
        
        self.output_text.insert(tk.END, message + "\n")
        self.output_text.see(tk.END)
        self.root.update()
    
    def get_port_name(self):
        """Extract port name from combo value"""
        port_value = self.port_var.get()
        if port_value:
            return port_value.split(" ")[0]
        return None
    
    def disable_buttons(self):
        """Disable action buttons"""
        self.build_btn.config(state=tk.DISABLED)
        self.upload_btn.config(state=tk.DISABLED)
        self.build_upload_btn.config(state=tk.DISABLED)
        self.stm8_btn.config(state=tk.DISABLED)
        self.flash_audio_btn.config(state=tk.DISABLED)
        self.process_running = True
    
    def enable_buttons(self):
        """Enable action buttons"""
        self.build_btn.config(state=tk.NORMAL)
        self.upload_btn.config(state=tk.NORMAL)
        self.build_upload_btn.config(state=tk.NORMAL)
        self.stm8_btn.config(state=tk.NORMAL)
        self.flash_audio_btn.config(state=tk.NORMAL)
        self.process_running = False
    
    def run_command(self, cmd_list, title):
        """Run command in thread"""
        def _run():
            try:
                self.disable_buttons()
                self.status_var.set(f"⏳ Đang {title}...")
                self.status_label.config(foreground="blue")
                self.log(f"\n{'='*60}")
                self.log(f"Lệnh: {' '.join(cmd_list)}")
                self.log(f"{'='*60}\n")
                
                process = subprocess.Popen(
                    cmd_list,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    cwd=str(Path(__file__).parent),
                    encoding="utf-8",
                    errors="replace",
                    env=self._utf8_env()
                )
                
                for line in process.stdout:
                    if line.strip():
                        self.log(line.rstrip(), timestamp=False)
                
                return_code = process.wait()
                
                self.log(f"\n{'='*60}")
                if return_code == 0:
                    self.log(f"✓ {title} thành công!")
                    self.log(f"{'='*60}\n")
                    self.status_var.set(f"✓ {title} thành công")
                    self.status_label.config(foreground="green")
                else:
                    self.status_var.set(f"✗ {title} thất bại")
                    self.status_label.config(foreground="red")
                    self.log(f"✗ {title} thất bại (code: {return_code})")
                    self.log(f"{'='*60}\n")
                
            except Exception as e:
                self.log(f"✗ Lỗi: {e}")
                self.status_var.set(f"✗ Lỗi: {str(e)}")
                self.status_label.config(foreground="red")
            finally:
                self.enable_buttons()
        
        thread = threading.Thread(target=_run, daemon=True)
        thread.start()

    def platformio_cmd(self, *args):
        """Build a PlatformIO command that works even when `platformio` is not on PATH."""
        return [sys.executable, "-m", "platformio", *args]
    
    def build_only(self):
        """Build firmware only"""
        motors = self.motors_var.get()
        if not motors:
            messagebox.showerror("Lỗi", "Vui lòng chọn số motor!")
            return
        
        env = f"nucleo_g070rb_{motors}motor"
        cmd = self.platformio_cmd("run", "-e", env)
        self.run_command(cmd, "Build")
    
    def upload_only(self):
        """Upload firmware only"""
        motors = self.motors_var.get()
        port = self.get_port_name()
        
        if not motors:
            messagebox.showerror("Lỗi", "Vui lòng chọn số motor!")
            return
        if not port:
            messagebox.showerror("Lỗi", "Vui lòng chọn COM port!")
            return
        
        cmd = [sys.executable, "flash_board.py", "--motors", motors, "--port", port, "--skip-audio"]
        self.run_command(cmd, "Upload firmware")
    
    def build_and_upload(self):
        """Build and upload"""
        motors = self.motors_var.get()
        port = self.get_port_name()
        
        if not motors:
            messagebox.showerror("Lỗi", "Vui lòng chọn số motor!")
            return
        if not port:
            messagebox.showerror("Lỗi", "Vui lòng chọn COM port!")
            return
        
        cmd = [sys.executable, "flash_board.py", "--motors", motors, "--port", port, "--skip-audio"]
        self.run_command(cmd, "Build & Upload")

    def stm8_unlock_upload(self):
        """Run STM8 default flow: upload firmware."""
        batch_path = Path(__file__).parent / "RUN_FLASH_GUI.bat"
        if not batch_path.exists():
            messagebox.showerror("Lỗi", f"Không tìm thấy file: {batch_path}")
            return

        cmd = ["cmd", "/c", str(batch_path), "stm8", "--no-pause"]
        self.run_command(cmd, "STM8 Upload")
    
    def flash_audio_only(self):
        """Flash audio files only"""
        motors = self.motors_var.get()
        port = self.get_port_name()
        audio_dir = self.audio_dir.get()
        
        if not motors:
            messagebox.showerror("Lỗi", "Vui lòng chọn số motor!")
            return
        if not port:
            messagebox.showerror("Lỗi", "Vui lòng chọn COM port!")
            return
        if not os.path.isdir(audio_dir):
            messagebox.showerror("Lỗi", f"Thư mục không tồn tại: {audio_dir}")
            return
        
        cmd = [sys.executable, "flash_board.py", "--motors", motors, "--port", port, 
               "--audio-dir", audio_dir, "--skip-firmware"]
        self.run_command(cmd, "Flash audio")

def main():
    root = tk.Tk()
    app = FlashBoardGUI(root)
    root.mainloop()

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
