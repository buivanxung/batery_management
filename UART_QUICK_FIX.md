# ⚡ Quick Fix for UART Issue - "Moto" Characters

## 🎯 Problem
You're getting garbled characters like "Moto" when connecting to serial port.

## ✅ Solution (3 Steps)

### Step 1: Check Terminal Baud Rate
**Make sure your serial monitor is set to: `115200` baud**

- PuTTY: Connection → Serial → Speed: 115200
- Arduino IDE: Bottom right corner → 115200
- VS Code Serial Monitor: Baud rate selector

### Step 2: Check USB Cable
- Use a **DATA cable** (not power-only)
- Try a different USB port on computer
- Try a different USB cable

### Step 3: Check Device Port
On Windows/Linux, verify the correct COM port:

**Windows:**
```
Device Manager → Ports (COM & LPT)
Look for: "USB Serial Device" or "STM32..."
Use that COM port
```

**Linux:**
```bash
ls /dev/ttyUSB*
# Should show /dev/ttyUSB0 or similar
```

---

## 🔋 Verify It's Working

After connecting at **115200 baud**, you should see:

```
=== System Starting ===
Board: Nucleo G070RB
UART: Serial1 PA9(TX) PA10(RX) @ 115200 baud

=== COMM Task Started ===
Type HELP for commands
```

Then send a command:
```
Type: HELP
You should see list of commands
```

---

## 🚨 If Still Garbled

Try this quick test - send individual characters:

```
Send: L
You should see: L (echoed back)

Send: I
You should see: I

Send: S
You should see: S

Send: T
You should see: T
```

**If this works** → Baud rate is correct, continue with commands

**If this is garbled** → **BAUD RATE IS WRONG**, fix Step 1

---

## 📊 Connection Map (Nucleo G070RB)

```
USB (ST-Link)
    ↓
Nucleo Board
    ↓
PA9  ← TX (pin 5)
PA10 ← RX (pin 6) 
GND  ← Ground (pin 8)
    ↓
Serial Monitor (115200 baud)
```

---

## 💡 Pro Tips

1. **Close other terminal apps** that might use the same COM port
2. **Don't need external adapter** if using ST-Link USB (plug directly)
3. **Reset board** if connection gets stuck:
   - Press RESET button on Nucleo
   - Reconnect serial monitor

---

## 🆘 Last Resort

If nothing works, try **reducing baud rate** (just for testing):

Edit `src/main.cpp`:
```cpp
Serial1.begin(9600);  // Much slower, but might show data
```

Recompile and upload. Then try serial monitor at **9600 baud**.

This will confirm if it's a speed issue or a connection issue.

---

## ✨ After This Works

You can now use all commands:

```
HELP              ← See all commands
LIST              ← List files in flash
PLAY audio.raw    ← Play audio file
MOTOR1 ON         ← Control motor 1
MEM               ← Check memory stats
```

Good luck! 🚀

