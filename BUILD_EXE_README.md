# 📦 BUILD STANDALONE .EXE FOR FLASH BOARD GUI

## Mục đích
Tạo một file `.exe` standalone mà có thể chạy trên bất kỳ máy Windows nào **mà không cần cài đặt Python, pyserial, platformio, hay bất kỳ tool nào khác**.

---

## 🚀 Cách Build

### Cách 1: Dùng Batch Script (Recommend)
```
Chỉ cần double-click file: BUILD_EXE.bat
```

Script sẽ:
1. ✅ Kiểm tra Python
2. ✅ Cài PyInstaller tự động
3. ✅ Compile GUI thành .exe
4. ✅ Tạo file `dist/FlashBoard-GUI.exe`

### Cách 2: Dùng Python trực tiếp
```bash
python build_exe.py
```

---

## 📁 Output & Sử dụng

**File đầu ra:** `dist/FlashBoard-GUI.exe`  
**Kích thước:** ~150-200 MB (bao gồm Python + tất cả dependencies)

### Cách sử dụng .exe:
```
1. Tìm file: dist/FlashBoard-GUI.exe
2. Copy file này sang máy khác (USB, email, etc)
3. Double-click để chạy
4. Không cần cài Python, không cần cài pyserial/platformio
5. GUI sẽ tự động cài dependencies lần đầu nếu cần
```

---

## ⚙️ Dependencies được bundle

Những thư viện Python đã được đóng gói vào .exe:
- **tkinter** - GUI framework (có sẵn với Python)
- **pyserial** - đọc/ghi COM port
- **platformio** - build firmware STM32

---

## 🔧 Tùy chỉnh (Advanced)

Muốn chỉnh sửa option build? Edit file `build_exe.py` và tìm section:
```python
cmd = [
    sys.executable, "-m", "PyInstaller",
    # ... các tùy chọn ở đây
]
```

Các option useful:
- `--onefile` - Thành 1 file (chậm load lần đầu)
- `--onedir` - Thành folder (nhanh hơn)
- `--icon=path/icon.ico` - Thêm icon tùy chỉnh

---

## 📝 Lưu ý

1. **Lần đầu chạy slow** - .exe tự unpack dependencies vào RAM, chỉ lần đầu
2. **Antivirus warning?** - Một số antivirus cảnh báo nhưng file là safe (bạn tự build)
3. **Update code?** - Mỗi lần edit `flash_board_gui.py`, phải rebuild .exe
4. **COM port** - Máy nhận phải có driver STM32/CH340 (e.g., CP2102 driver)

---

## ✅ Khi nào rebuild?

**Phải rebuild:**
- ✅ Edit `flash_board_gui.py`
- ✅ Cập nhật dependencies (pyserial, platformio version)

**Không cần rebuild:**
- ❌ Edit `flash_board.py` (file được call inside, auto-loaded)
- ❌ Edit audio/firmware files (loaded dynamically)
- ❌ Edit config files

---

## 🐛 Troubleshoot

### Build fail: "PyInstaller not found"
→ Cài thủ công: `python -m pip install pyinstaller`

### Build fail: "File not found"
→ Chắc chắn file `flash_board_gui.py` tồn tại cùng thư mục

### .exe thay đổi size mỗi lần build
→ Bình thường, do timestamp và metadata khác nhau. Chỉ trong `--onedir` mode size không thay đổi

### Antivirus quarantine .exe
→ Add folder `dist/` vào exclusion list của antivirus

---

## 📞 Support

Nếu có vấn đề, thử:
1. `BUILD_EXE.bat` lại
2. Xóa folder `build/` và `.spec` files, retry
3. Update Python: `py -3 -m pip install --upgrade pip`
