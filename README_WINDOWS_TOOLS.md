# 🪟 HƯỚNG DẪN WINDOWS - CÁC BATCH FILES VÀ GUI TOOL

**Cho người dùng Windows - Không cần dùng Command Line!**

---

## 📂 CÁC FILE GIÚP BẠN

### **1. RUN_FLASH_GUI.bat** ⭐ (RECOMMENDED)
**Cách dùng:** Double-click file này

**Chức năng:**
- Mở giao diện đồ họa (GUI)
- Chọn số motor, COM port, thư mục audio bằng dropdown
- Click nút để build / upload / flash
- Xem output log realtime

**Lợi ích:**
- ✓ Không cần gõ lệnh command line
- ✓ Dễ hiểu cho người mới
- ✓ Tất cả trong 1 cửa sổ
- ✓ Khó gặp lỗi typo

---

### **2. BUILD_ONLY.bat**
**Cách dùng:** Double-click file này

**Chức năng:** Chỉ build firmware (biên dịch code)

**Quy trình:**
```
1. Nhập số motor (6 hoặc 8)
2. Build tự động
3. Chờ [SUCCESS] hoặc [ERROR]
```

**Khi nào dùng:**
- Lần đầu setup
- Sửa code & muốn kiểm tra có lỗi không
- Chỉ muốn biên dịch, chưa upload

---

### **3. UPLOAD_ONLY.bat**
**Cách dùng:** Double-click file này

**Chức năng:** Chỉ upload firmware lên board

**Quy trình:**
```
1. Nhập số motor (6 hoặc 8)
2. Nhập COM port (xem Device Manager)
3. Cắm board vào USB
4. Upload tự động
5. Board khởi động lại
```

**Khi nào dùng:**
- Build đã xong, chỉ cần upload
- Muốn upload code mới mà không rebuild

---

## 🖱️ CÁCH DÙNG GUI TOOL (RUN_FLASH_GUI.bat)

### Bước 1: Chạy Chương Trình
1. Tìm file **RUN_FLASH_GUI.bat** trong thư mục project
2. **Double-click** vào file
3. Cửa sổ GUI sẽ mở lên

### Bước 2: Cấu Hình
**Ở section "⚙️ Cấu Hình":**
- **Số Motor:** Dropdown chọn 6 hoặc 8
- **COM Port:** Dropdown hiển thị các port có sẵn
  - Nếu không thấy → Click "🔄 Refresh"
  - Vẫn không thấy → Cắm board USB vào → Refresh lại
- **Audio Dir:** Chọn thư mục chứa file .pcm
  - Click "Browse..." để chọn

### Bước 3: Chọn Thao Tác

#### ✅ Để Build:
1. Click nút **"🔨 Build Only"**
2. Chờ output log chạy xong
3. Nếu thấy **"✓ Build thành công"** → OK

#### ✅ Để Upload Firmware:
1. Cắm board USB vào máy
2. Click "🔄 Refresh" nếu cần
3. Chọn COM port
4. Click nút **"⬆️ Upload Only"** (hoặc "🚀 Build + Upload")
5. Chờ xong → Thấy **"✓ Upload thành công"**
6. Board sẽ nhấp nháy LED lần rồi khởi động

#### ✅ Để Flash Audio:
1. Upload firmware lên board xong (xem trên)
2. Chọn thư mục Audio
3. Click nút **"💾 Flash Audio Only"**
4. Chờ xong → Các file audio được save vào board

#### ✅ Để Build + Upload + Flash (All-in-One):
1. Cắm board
2. Chọn số motor, COM port, audio dir
3. Bỏ tick "Bỏ qua Firmware" & "Bỏ qua Audio"
4. Click nút **"🚀 Build + Upload"**
5. Chờ đến **✓ Hoàn thành**

### Bước 4: Xem Output Log
- Output sẽ hiển thị ở phần **"📝 Output Log"** dưới cùng
- Mỗi dòng có timestamp `[HH:MM:SS]`
- Scroll xuống để xem dòng mới nhất

### Bước 5: Kiểm Tra Kết Quả
- Nhìn vào **Status Bar** dưới cùng:
  - 🟢 **Green "✓"** = Thành công
  - 🔵 **Blue "⏳"** = Đang chạy
  - 🔴 **Red "✗"** = Lỗi

---

## 🔄 WORKFLOW HÀNG NGÀY

### Lần Đầu Setup (30 phút)
```
1. Cài Python → Cài PlatformIO → Cài ST-Link driver
2. Double-click RUN_FLASH_GUI.bat
3. Chọn 8 motor, COM3, ./out folder
4. Click "🚀 Build + Upload"
5. Chờ ✓ xong
6. Done!
```

### Hàng Ngày (5 phút)
```
1. Sửa code trong VS Code
2. Save (Ctrl+S)
3. Double-click RUN_FLASH_GUI.bat
4. Click "⬆️ Upload Only" (bypass build)
5. Done!
```

### Nếu Chỉ Build
```
Double-click BUILD_ONLY.bat → Enter → Done
```

### Nếu Chỉ Upload
```
Double-click UPLOAD_ONLY.bat → Enter → Done
```

---

## ⚙️ TÙY CHỌN NÂNG CAO (Ở GUI Tool)

### "Bỏ qua Firmware"
- Tick → Sẽ **không** build & upload firmware
- Chỉ flash audio files
- **Dùng khi:** Firmware đã upload, chỉ muốn cập nhật audio

### "Bỏ qua Audio"
- Tick → Sẽ **không** flash audio
- Chỉ build & upload firmware
- **Dùng khi:** Chỉ muốn cập nhật code, không cần audio

### "💾 Flash Audio Only"
- Click → Flash audio files mà không cần rebuild firmware
- **Dùng khi:** Code không thay, chỉ thay audio

---

## 🆘 LỖI THƯỜNG GẶP & CÁCH FIX

### Lỗi 1: "Python not found"
```
❌ Error: Could not find Python
   hoặc 'python' is not recognized

✅ Fix:
   1. Cài Python từ https://python.org
   2. TÍK vào "Add Python to PATH"
   3. Restart Windows
   4. Chạy lại .bat file
```

### Lỗi 2: "COM Port not found"
```
❌ Error: Could not open port COM3

✅ Fix:
   1. Cắm board vào USB
   2. Đợi 2 giây
   3. Click "🔄 Refresh" ở GUI
   4. Kiểm tra Device Manager (Win+R → devmgmt.msc)
   5. Xem port tên gì (COM3, COM4, etc.)
   6. Chọn port đó ở GUI
```

### Lỗi 3: "STLink not responding"
```
❌ Error: STLink not responding

✅ Fix:
   1. Cài lại ST-Link driver từ:
      https://www.st.com/en/development-tools/stlink-v2.html
   2. Restart Windows
   3. Chạy lại .bat file
```

### Lỗi 4: Build Error (Code Syntax)
```
❌ error: 'digitalWrite' was not declared

✅ Fix:
   1. Sửa typo trong code (.cpp file)
   2. Save (Ctrl+S)
   3. Build lại
```

### Lỗi 5: "Audio directory not found"
```
❌ Error: Folder C:\path\to\audio not found

✅ Fix:
   1. Click "Browse..." ở GUI
   2. Chọn thư mục có file .pcm
   3. Thử lại
```

---

## 📋 CHECKLIST BEFORE RUNNING

- [ ] Python cài xong & restart Windows
- [ ] VS Code + PlatformIO cài
- [ ] ST-Link driver cài & restart Windows
- [ ] Board Nucleo kết nối & Device Manager thấy port
- [ ] File RUN_FLASH_GUI.bat & flash_board_gui.py trong project folder
- [ ] Audio files (.pcm) trong thư mục ./out

---

## 🎬 QUICK START (3 BƯỚC)

### Step 1: Chuẩn Bị
```
□ Cài Python + VS Code + ST-Link driver
□ Cắm board vào USB
□ Ngả folder project trong VS Code
```

### Step 2: Build
```
□ Double-click BUILD_ONLY.bat
□ Nhập 8 (hoặc 6)
□ Chờ ✓ [SUCCESS]
```

### Step 3: Upload
```
□ Double-click UPLOAD_ONLY.bat
□ Nhập 8
□ Nhập COM3 (hoặc cái bạn thấy ở Device Manager)
□ Chờ ✓ [SUCCESS]
□ Board sẽ nhấp nháy & khởi động
```

**XONG!** 🎉

---

## 🔍 CHỌN CÁCH NÀO TỐT NHẤT?

| Tình Huống | Dùng File/Tool Nào |
|-----------|------------------|
| **Lần đầu setup** | RUN_FLASH_GUI.bat (đầy đủ nhất) |
| **Chỉ muốn build** | BUILD_ONLY.bat (nhanh) |
| **Chỉ muốn upload** | UPLOAD_ONLY.bat (nhanh) |
| **Muốn xem realtime log** | RUN_FLASH_GUI.bat |
| **Không bao giờ dùng terminal** | RUN_FLASH_GUI.bat |
| **Người dùng lần đầu** | RUN_FLASH_GUI.bat |

---

## 📞 CẬP NHẬT & HỖ TRỢ

### Nếu .bat file không chạy
- Double-click file → không gì xảy ra?
- Chuột phải → "Run as Administrator"
- Hoặc sửa mặc định: chuột phải → "Open with" → Chọn Command Prompt

### Nếu GUI tool bị crash
- Xem error message ở command prompt
- Cài lại: `pip install pyserial`
- Restart Windows

### Nếu cần cập nhật tool
- Xóa `flash_board_gui.py`
- Download phiên bản mới từ project
- Chạy lại

---

## 🚀 NEXT STEPS

Khi đã upload code thành công:
1. Mở "Serial Monitor" trong VS Code
2. Xem debug message từ board
3. Sửa code thêm features
4. Build + Upload lại

**Chúc bạn thành công! 🎊**

*Created: April 7, 2026*
