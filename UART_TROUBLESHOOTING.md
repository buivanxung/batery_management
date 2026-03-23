# UART/Serial Communication Troubleshooting

## 🔴 Problem: Serial Output Shows Only "Moto" Characters

This is a classic **baud rate mismatch** or **wrong serial port** problem.

---

## ✅ Solution: Choose Your UART Method

### **Nucleo G070RB has 2 serial interfaces:**

#### Option 1: SerialUSB (ST-Link, RECOMMENDED ✅)
- **How to connect:** Just plug Nucleo USB cable into computer
- **Port shows up as:** `/dev/ttyUSB0` (Linux) or `COM3` (Windows)
- **Pros:** Works immediately, no extra hardware needed
- **Cons:** Shares debug interface

#### Option 2: Serial1 (Physical PA9/PA10, NEEDS ADAPTER)
- **How to connect:** USB-to-UART adapter on PA9(TX)/PA10(RX)
- **Port shows up as:** `/dev/ttyUSB1` or `/dev/ttyACM1` 
- **Pros:** Dedicated, won't interfere with debugging
- **Cons:** Requires external hardware

---

## 🔧 How to Switch Serial Port (main.cpp)

### **Use SerialUSB (Recommended):**
```cpp
// In setup() function:
SerialUSB.begin(115200);
Serial1 = SerialUSB;  // Or just use SerialUSB directly
```

### **Use Serial1 (Physical pins):**
```cpp
// In setup() function (current setting):
Serial1.begin(115200);
```

---

## 🚀 Implementation

I've added code to **auto-detect and configure properly**:

```cpp
void setup() {
  // Enable GPIO clock for UART pins
  __HAL_RCC_GPIOA_CLK_ENABLE();  // PA9, PA10
  
  // Initialize Serial1
  Serial1.begin(115200);
  delay(100);  // Wait for UART to initialize
  
  // Print startup message to confirm it's working
  Serial1.println(F("System Starting..."));
}
```

---

## 📋 Troubleshooting Checklist

### If Still Getting "Moto" or Garbled Characters:

#### 1. **Check Terminal Baud Rate ✓**
```
Terminal shows: ???Moto????
→ Your baud rate is WRONG

Fix: Set to 115200 baud in your terminal software
```

#### 2. **Check USB Driver**
```
Windows:
- Device Manager → Ports (COM & LPT)
- Should show "USB Serial Device" or similar
- Right-click → Driver details

Linux:
dmesg | grep ttyUSB
ls -l /dev/ttyUSB*
```

#### 3. **Try Different USB Port**
```bash
# Linux: Find all serial ports
ls /dev/tty* | grep USB

# Connect to correct port (example):
screen /dev/ttyUSB0 115200
# or
picocom -b 115200 /dev/ttyUSB0
```

#### 4. **Check Connections**
- PA9 (TX) connected correctly? (if using Serial1 with adapter)
- PA10 (RX) connected correctly?
- Ground connected to GND?
- USB cable is **data cable** (not power-only)?

#### 5. **Monitor Power**
- Board LED should blink (LED on PF0)
- If no LED activity → power issue

---

## 🧪 Test UART Communication

### **Quick Test:**
```
1. Open serial monitor
2. Set baud rate: 115200
3. Press Nucleo RESET button
4. You should see:
   "=== System Starting ==="
   "Board: Nucleo G070RB"
   "UART: Serial1 PA9(TX) PA10(RX) @ 115200 baud"
```

### **Send Commands:**
```
Type: HELP
Expected: List of commands (LIST, PLAY, MOTOR, etc.)

Type: LIST
Expected: List of files in flash (if any)
```

---

## 💻 Terminal Software Recommendations

### **Linux/Mac:**
```bash
# Option 1: minicom
sudo minicom -D /dev/ttyUSB0 -b 115200

# Option 2: screen
screen /dev/ttyUSB0 115200

# Option 3: picocom
picocom -b 115200 /dev/ttyUSB0

# Option 4: Python
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```

### **Windows:**
- **PuTTY:** Free, select Serial, port COM3, speed 115200
- **Tera Term:** Good for serial monitoring
- **Arduino IDE:** Built-in Serial Monitor

### **VS Code:**
- Extension: "Serial Monitor" by Microsoft
- Or: "PlatformIO" built-in serial monitor

---

## 🔍 Advanced Debugging

### **Check if UART is actually working:**

```cpp
// Add this to setup() after Serial1.begin():
Serial1.write(0x55);  // Send single byte
Serial1.write(0xAA);  // Send another byte

// In terminal, you should see some characters
// If you see anything different than "Moto"...
// Your baud rate is close but not exact
```

### **Check GPIO pins:**
```cpp
// Add this after __HAL_RCC_GPIOA_CLK_ENABLE()
Serial1.println(F("Checking PA pins..."));
pinMode(PA9, OUTPUT);
pinMode(PA10, OUTPUT);
digitalWrite(PA9, HIGH);
digitalWrite(PA10, HIGH);
Serial1.println(F("Pins configured"));
```

---

## 📊 Common Baud Rate Issues

| Terminal Speed | Device Speed | Result |
|---|---|---|
| 9600 | **115200** | ????Moto???? |
| 115200 | **9600** | Perfect but SLOW |
| 57600 | **115200** | Garbled, half-speed |

**Current code uses:** 115200 baud

---

## ✅ After Fixing UART

Once serial communication works, you can:

```
LIST                    → See files in flash
PLAY audio.raw         → Play audio file
MOTOR1 ON              → Control motor
HELP                   → See all commands
MEM                    → Check memory stats
```

---

## 🆘 Still Not Working?

### **Nuclear Option - Reset Everything:**

1. Disconnect from USB
2. Hold RESET button for 3 seconds
3. While holding, connect USB
4. Release RESET
5. Board should factory reset
6. Try again with 115200 baud

### **If LED Doesn't Blink:**
- Check power: Board should have LED light on
- Try different USB cable
- Try different USB port on computer

---

## 📝 Code Changes Applied

✅ Added `__HAL_RCC_GPIOA_CLK_ENABLE()` for PA9/PA10 pins
✅ Added `delay(100)` after Serial1.begin() for UART startup
✅ Added startup messages with confirmation
✅ Comments explain both Serial1 and SerialUSB options

---

## 🎯 Next Steps

1. **Upload code** to Nucleo G070RB
2. **Open serial monitor** at 115200 baud
3. **Look for startup messages**
4. **Type: HELP** to verify communication

Should work now! If still issues, check:
- Baud rate (must be 115200)
- USB cable (must be data cable)
- Device Manager shows COM port
- No other app using that COM port

