# 🚀 COPY PROJECT SANG WINDOWS & BUILD .EXE

## 📋 Danh sách file cần copy

**Cần copy TOÀN BỘ thư mục:** `batery_management/`

Bao gồm các file quan trọng:
- ✅ `flash_board_gui.py` - GUI chính
- ✅ `flash_board.py` - Backend script
- ✅ `build_exe.py` - Script build executable
- ✅ `BUILD_EXE.bat` - Batch file để build (chỉ chạy trên Windows)
- ✅ `src/`, `audio/`, `include/`, `lib/` - Firmware & config

**Không cần copy:**
- ❌ `.git/` - Git repository (to, không cần)
- ❌ `.__pycache__/` - Python cache (tự tạo)
- ❌ `build/`, `dist/` - Build artifacts

---

## 💾 Cách copy sang Windows

### **Cách 1: USB/Drive**
```
1. Cắm USB vào Linux
2. Copy toàn bộ thư mục batery_management/ vào USB
3. Cắm USB vào Windows
4. Paste thư mục vào C:\Projects\ (hoặc nơi bạn muốn)
```

### **Cách 2: Git Commit & Clone trên Windows**
```bash
# Trên Linux:
cd /home/xung/batery_management
git add -A
git commit -m "Prepare for Windows .exe build"
git push

# Trên Windows:
git clone <repository-url> C:\Projects\batery_management
cd C:\Projects\batery_management
```

### **Cách 3: Compress & Download**
```bash
# Trên Linux:
cd /home/xung
tar --exclude='.git' --exclude='__pycache__' -czf batery_management.tar.gz batery_management/

# Copy file batery_management.tar.gz sang Windows
# Dùng 7-Zip hoặc WinRAR để giải nén
```

---

## 🔨 Build .exe trên Windows

Sau khi copy thư mục sang Windows:

### **Bước 1: Chắc chắn có Python**
```
Kiểm tra: Win+R → cmd → gõ: py -3 --version
Nếu không có, cài từ: https://www.python.org/downloads/
  - Nhớ tick ✅ "Add Python to PATH"
```

### **Bước 2: Build .exe**
```
Double-click: BUILD_EXE.bat
```

Hoặc chạy bằng cmd:
```cmd
cd C:\Projects\batery_management
python build_exe.py
```

### **Bước 3: Chờ...**
Script sẽ:
- ✅ Cài PyInstaller
- ✅ Compile GUI
- ⏱️  Chứng có thể mất 2-5 phút tùy CPU

### **Bước 4: Sử dụng .exe**
File output: `dist\FlashBoard-GUI.exe` (~150-200 MB)

**Copy file này sang máy khác (không cần Python/dependencies)**

---

## ✅ Checklist trước copy

- [ ] Sử dụng `git commit` để lưu trữ mã mới nhất
- [ ] Xóa `.git/` nếu muốn giảm kích thước
- [ ] Xóa `build/`, `dist/`, `.spec` files
- [ ] Kiểm tra `flash_board_gui.py` có sẵn

---

## 🆘 Troubleshoot trên Windows

### "Python not found"
→ Cài Python: https://www.python.org/downloads/ (3.9+)

### Build fail: "permission denied"
→ Chạy cmd với **Administrator** (Run as Admin)

### Build fail: "pyserial not found"
→ Script sẽ tự cài, nhưng nếu fail:
```cmd
python -m pip install pyserial
python -m pip install platformio
```

### Antivirus block BUILD_EXE.bat
→ Tạm thời disable antivirus, hoặc:
```cmd
python build_exe.py
```

---

## 📞 Sau khi có .exe

**Bạn có thể:**
1. ✅ Share file `.exe` cho người khác
2. ✅ Chạy trên máy không có Python
3. ✅ Đóng gói với firmware/audio files
4. ✅ Deploy cho end-users

**Lần sau update code:**
- Edit `flash_board_gui.py`
- Rebuild: `BUILD_EXE.bat`
- Copy `dist\FlashBoard-GUI.exe` mới

---

## 📊 Ước tính kích thước

| Item | Size |
|------|------|
| Project folder | ~50 MB |
| Build output (.exe) | ~150-200 MB |
| USB/Drive cần | ~500 MB (safe) |

---

**Ready? Let's go! 🚀**
