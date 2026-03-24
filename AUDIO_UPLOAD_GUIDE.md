# Hướng Dẫn Nạp Audio Vào Flash Qua UART

## Tổng Quan
Hướng dẫn này giải thích cách nạp file audio (MP3) vào flash memory của STM32 qua giao tiếp UART. Quá trình bao gồm 3 bước chính:
1. Chuyển đổi MP3 sang định dạng PCM 8-bit
2. Nạp file PCM vào flash qua UART
3. Phát audio từ flash

## Yêu Cầu Hệ Thống
- **FFmpeg**: Để chuyển đổi audio
  ```bash
  sudo apt install ffmpeg  # Ubuntu/Debian
  ```
- **Python 3**: Để chạy script nạp
- **STM32 Nucleo G070RB**: Đã nạp firmware battery management
- **Kết nối UART**: Serial1 (PA9/PA10) @ 115200 baud

## Bước 1: Chuyển Đổi Audio

### Chuyển Đổi Đơn Lẻ
```bash
python3 convert_audio.py --input_dir ./audio --output_dir ./audio/audio8
```

### Chuyển Đổi Thủ Công (Nếu Cần)
```bash
ffmpeg -y -i input.mp3 -acodec pcm_s8 -f s8 -ac 1 -ar 8000 output.pcm
```

**Thông số chuyển đổi:**
- Codec: pcm_s8 (8-bit signed PCM)
- Kênh: 1 (mono)
- Tần số: 8000 Hz

## Bước 2: Nạp File Vào Flash

### Nạp Một File
1. Kết nối STM32 với máy tính qua USB
2. Mở terminal và chạy:
   ```bash
   python3 upload_audio.py <filename.pcm>
   ```
   Ví dụ:
   ```bash
   python3 upload_audio.py audio/audio8/xinchao.pcm
   ```

### Nạp Nhiều File (Batch)
```bash
python3 batch_upload_audio.py --dir ./audio/audio8
```

**Quy trình nạp:**
1. Script gửi lệnh `store <tên_file> <kích_thước>` qua UART
2. MCU chuẩn bị nhận dữ liệu
3. Script gửi dữ liệu nhị phân theo từng chunk
4. MCU lưu vào flash filesystem

## Bước 3: Kiểm Tra và Phát Audio

### Kiểm Tra File Đã Nạp
1. Mở terminal serial (115200 baud)
2. Gửi lệnh: `list`
   ```
   Files in flash:
   xinchao.pcm (9600 bytes)
   khay1.pcm (8000 bytes)
   ...
   ```

### Phát Audio
```
play xinchao
```

**Lệnh CLI có sẵn:**
- `help` - Hiển thị tất cả lệnh
- `list` - Liệt kê file trong flash
- `store <name> <size>` - Nạp file (thường dùng bởi script)
- `play <name>` - Phát file audio
- `format` - Xóa toàn bộ flash
- `mon` - Giám sát ADC và voltage
- `mem` - Hiển thị heap memory

## Xử Lý Sự Cố

### Lỗi "Flash FAIL"
- Kiểm tra kết nối SPI flash (W25Q32)
- Kiểm tra chân SPI: CS=PA8, SCK=PA5, MISO=PA6, MOSI=PA7

### Lỗi "Play FAIL"
- Kiểm tra tên file chính xác (phân biệt hoa thường)
- Đảm bảo file đã được nạp thành công
- Kiểm tra flash filesystem chưa bị hỏng

### Lỗi Timeout Khi Nạp
- Kiểm tra kết nối UART ổn định
- Giảm tốc độ truyền nếu cần
- Đảm bảo MCU không bị watchdog reset

### Audio Phát Không Đúng
- Đảm bảo file PCM đúng định dạng (8-bit signed, 8000Hz, mono)
- Kiểm tra PWM output trên PA4
- Thử nạp lại file

## Cấu Trúc File
```
batery_management/
├── audio/                    # Thư mục MP3 gốc
│   ├── xinchao.mp3
│   └── khay1.mp3
├── audio/audio8/            # Thư mục PCM đã chuyển đổi
│   ├── xinchao.pcm
│   └── khay1.pcm
├── convert_audio.py         # Script chuyển đổi
├── upload_audio.py          # Script nạp đơn lẻ
└── batch_upload_audio.py    # Script nạp hàng loạt
```

## Lưu Ý Quan Trọng
- **Định dạng bắt buộc**: Chỉ PCM 8-bit signed, mono, 8000Hz
- **Kích thước file**: Tối đa ~32KB cho mỗi file (giới hạn flash)
- **Tên file**: Không dùng ký tự đặc biệt, phân biệt hoa thường
- **UART**: 115200 baud, không parity, 1 stop bit
- **Timeout**: Script có timeout 10 giây cho mỗi file

## Kiểm Tra Hoạt Động
Sau khi nạp thành công:
1. `list` - Xem file trong flash
2. `play <tên_file>` - Phát và nghe audio
3. `mon` - Giám sát hệ thống
4. `mem` - Kiểm tra memory usage

Nếu gặp vấn đề, kiểm tra log UART để debug.</content>
<parameter name="filePath">AUDIO_UPLOAD_GUIDE.md