# Quick Reference Guide - Battery Manager

## 🚀 Quick Start

### Build & Flash
```bash
# Compile for 6 motors
platformio run -e nucleo_g070rb_6motor --target upload

# Upload audio files
python flash_board.py --motors 6 --port COM6
```

### Monitor Serial
```bash
platformio device monitor -p COM6 -b 115200
```

---

## 📟 UART Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `help` | `help` | Show all commands |
| `list` | `list` | List audio files in flash |
| `play` | `play khay1.pcm` | Play audio file |
| `motor` | `motor 1 on` | Turn motor on/off |
| `led` | `led toggle` | Control LED |
| `mon` | `mon` | Monitor battery voltages |
| `beep` | `beep 2000` | Test 1kHz tone (2 sec) |
| `store` | `store name len` | Upload file (binary) |
| `format` | `format` | Format flash filesystem |

---

## 📁 Audio Files Currently Stored

```
0: khay1.pcm (24.9 KB) - ~3.1 sec
1: khay2.pcm (25.7 KB) - ~3.2 sec
2: khay3.pcm (25.8 KB) - ~3.2 sec
3: khay4.pcm (26.0 KB) - ~3.2 sec
4: khay5.pcm (25.6 KB) - ~3.2 sec
5: khay6.pcm (26.9 KB) - ~3.4 sec
6: khay7.pcm (25.3 KB) - ~3.2 sec
7: khay8.pcm (26.6 KB) - ~3.3 sec
8: move.pcm (33.6 KB) - ~4.2 sec
9: xinchao.pcm (16.2 KB) - ~2.0 sec
```

---

## 🔧 Hardware Pinout

### Audio
- **Output:** PA4 (PWM TIM2_CH1)
- **Filter:** RC 10kΩ/100nF
- **Level:** 0-3.3V (PWM)

### Motors
- **Motor 1-8 IN:** PB0-PB7
- **Motor 1-8 CTRL:** PB15, PC1, PC3, PC5, PC7, PC9, PC10, PC11

### SPI Flash
- **CS:** PA8
- **SCK:** PA5
- **MISO:** PA6
- **MOSI:** PA7

### UART
- **TX:** PA9
- **RX:** PA10
- **Baud:** 115200 bps

### Battery (ADC Mux)
- **Signal:** PA0 (ADC input)
- **Select:** A11 (S0), A12 (S1), A15 (S2)
- **Thresholds:**
  - Normal: 3.0-4.2V
  - Low: <3.2V
  - Critical: <3.0V
  - Over: >4.3V

---

## 🎯 Memory Layout

### Flash Filesystem
```
0x00000000 - 0x00000FFF  → Filesystem header (4 KB)
0x00001000 - 0x0002FFFF  → Audio data (192 KB)
Remaining              → Free space
```

### RAM Usage
- **Total:** 36 KB
- **Currently used:** ~19 KB (52%)

---

## ⚙️ Configuration

### Compile-Time Options
```bash
# In platformio.ini:
build_flags = 
    -DARCH_STM32
    -DSERIAL_TX_BUFFER_SIZE=128
    -DMOTOR_COUNT=6          # Change to 8 for 8 motors
```

### Board Selection
```
[env:nucleo_g070rb_6motor]
board = nucleo_g070rb
framework = arduino

[env:nucleo_g070rb_8motor]
board = nucleo_g070rb
framework = arduino
build_flags = -DMOTOR_COUNT=8
```

---

## 🐛 Troubleshooting

### No Serial Response
- Check baud rate: **115200**
- Check USB-UART cable
- Reset board (button or power cycle)

### Audio Not Playing
```
> beep          # Test 1kHz tone first
> play xinchao  # Try simplest file
```
- Check RC filter on PA4
- Check speaker connections

### Upload Fails
```bash
# Use robust uploader
python flash_board.py --motors 6 --port COM6

# Manual retry
python upload_audio_fix.py
```

### Battery Not Reading
```
> mon           # Check all channels
```
- Verify 74HC4051 mux connection
- Check resistor divider (100k/68k)

---

## 📊 Performance Tips

### Optimize Code Size
```bash
# Release build (optimized)
platformio run -e nucleo_g070rb_6motor --target upload
```

### Improve Audio Quality
- Increase AUDIO_SAMPLE_RATE in pinout.h
- Apply better RC filter (lower cutoff)
- Use higher-quality source audio

### Monitor Timing
```c
// In task:
TickType_t start = xTaskGetTickCount();
// ... do work ...
TickType_t elapsed = xTaskGetTickCount() - start;
logPrintf("Task took %d ticks\n", elapsed);
```

---

## 📝 Python Utilities

### play_all_files.py
Play all 10 audio files sequentially
```bash
python play_all_files.py
```

### upload_audio_fix.py
Upload audio with auto-retry and formatting
```bash
python upload_audio_fix.py
```

### uart_shell.py
Interactive UART shell
```bash
python uart_shell.py
> play khay1.pcm
> list
> quit
```

### gen_hex.py
Generate Intel HEX from ELF (post-build)

---

## 📚 Documentation Files

| File | Purpose |
|------|---------|
| `DETAILED_DOCUMENTATION.md` | Complete technical reference |
| `PRODUCTION.md` | Production checklist |
| `PRODUCTION_REVIEW.md` | Review feedback |
| `AUDIO_DAC_GUIDE.md` | Audio configuration guide |
| `UART_QUICK_FIX.md` | UART protocol quirks |
| `CODE_REVIEW_REPORT.md` | Code quality review |

---

## 🔗 Important Links

**Datasheets:**
- STM32G070RB: ARM Cortex-M0+, 64MHz
- W25Q32: 4MB SPI Flash
- 74HC4051: 8-channel analog mux

**Libraries Used:**
- STM32FreeRTOS: Real-time OS
- SPIMemory: Flash driver
- Arduino: Hardware abstraction

---

## 💡 Tips & Tricks

### Reset Board via Serial
```python
import serial, time
ser = serial.Serial('COM6', 115200)
ser.dtr = False
time.sleep(0.5)
ser.dtr = True
ser.close()
```

### List All Files
```
> list
```

### Get Free Flash Space
```
> mem
```

### Test All Motors
```bash
for i in {1..8}; do
  echo "Testing motor $i..."
  echo "motor $i on" > /dev/ttyUSB0
  sleep 1
  echo "motor $i off" > /dev/ttyUSB0
  sleep 0.5
done
```

### Monitor Battery Real-Time
```bash
# Terminal:
watch -n 1 'echo "mon" > /dev/ttyUSB0 && sleep 0.5'
```

---

## 📋 Task Priority Order

1. **commTask** (highest) - UART input response
2. **audioTask** - Audio playback
3. **motorTask** - Motor control
4. **ledTask** - LED status
5. **adcTask** - Battery monitoring
6. **loggerTask** (lowest) - Logging output

---

## 🎨 Extension Ideas

### Add Features
- [ ] Temperature monitoring (add NTC thermistor)
- [ ] Real-time status display (LCD screen)
- [ ] Wireless control (Bluetooth module)
- [ ] SD card support
- [ ] Button interface
- [ ] Alarm system

### Improve Quality
- [ ] Better audio filtering
- [ ] Volume control (PWM duty adjustment)
- [ ] Fade in/out effects
- [ ] Audio mixing
- [ ] Error correction codes

---

**Version:** 1.0  
**Last Updated:** April 2, 2026
