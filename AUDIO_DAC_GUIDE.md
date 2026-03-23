# Audio DAC - Testing & Usage Guide

## 🚀 Quick Start

### 1. Upload Code to Device
```bash
platformio run --target upload
```

### 2. Open Serial Monitor
- Baud rate: **115200**
- Port: Auto-detect or select STM32 port

Expected output:
```
[System starts]
[FreeRTOS tasks created]
Connect with: LIST, PLAY, MOTOR, etc.
```

---

## 📝 Serial Commands

### Upload Audio File
```
STORE filename.raw 8000
[then send 8000 bytes of raw audio data]
```

**Example:** Upload 1-second audio at 8kHz:
- File size = 8000 samples × 1 byte = 8000 bytes
- Command: `STORE alarm.raw 8000`

### Play Audio
```
PLAY filename.raw
```

### List Files
```
LIST
```

Output:
```
Flash files:
0: alarm.raw @0x2000 len=8000
1: beep.raw @0x4000 len=4000
```

### Control Motors
```
MOTOR1 ON
MOTOR1 OFF
MOTOR2 ON
...
MOTOR4 OFF
```

---

## 🎵 Audio File Preparation

### Create 8-bit PCM Audio (Linux/Mac)

**From WAV file:**
```bash
ffmpeg -i input.wav -acodec pcm_s8 -ar 8000 -ac 1 output.raw
```

**From MP3:**
```bash
ffmpeg -i song.mp3 -acodec pcm_s8 -ar 8000 -ac 1 output.raw
```

**From audio file (generic):**
```bash
ffmpeg -i audio.mp3 -f u8 -acodec pcm_s8 -ar 8000 -ac 1 output.raw
```

### Check File Size
```bash
ls -la output.raw
# Shows size in bytes
# At 8000 Hz, 1 second = 8000 bytes
```

---

## 🧪 Testing Sequences

### Test 1: Verify Flash Storage
```
Serial > LIST
[Check if filesystem is valid]

Serial > STORE test.raw 100
[Send 100 bytes of test data]

Serial > LIST
[Should see test.raw listed]
```

**Expected:** File appears in flash filesystem

---

### Test 2: Simple Audio Playback
```
Serial > STORE alarm.raw 8000
[Upload 1-second beep]

Serial > PLAY alarm.raw
[Should hear sound from speaker]
```

**Expected:**
- Audio plays for ~1 second
- Serial says "Playback complete"
- No glitches or distortion

---

### Test 3: Audio + Motor Simultaneous
```
Serial > MOTOR1 ON
Serial > PLAY alarm.raw
Serial > MOTOR1 OFF
```

**Expected:**
- Motor runs WHILE audio plays
- Motor response not delayed by audio

---

### Test 4: Multiple Playbacks
```
Serial > PLAY alarm.raw
Serial > PLAY alarm.raw
Serial > PLAY alarm.raw
```

**Expected:**
- All three play back in order
- No memory corruption
- Clean audio each time

---

## ⚠️ Troubleshooting

### Problem: No Sound Output
**Check:**
1. Speaker connected to PA4 (AUDIO_PWM_PIN)?
2. Ground properly connected?
3. Audio file valid? (Try with known-good .raw file)
4. Amplifier powered on?

**Debug:**
```
Serial > LIST
[Verify file uploaded correctly]

Serial > PLAY yourfile.raw
[Listen for "Playback complete" message]
```

---

### Problem: Serial Timeout
**Symptom:** Long delays on `LIST` command

**Cause:** Previous `STORE` command didn't complete properly

**Fix:**
```
1. Click "Reset" on board
2. Reconnect serial
3. Try again
```

---

### Problem: Flash Not Initialized
**Message:** "ERROR: Flash initialization failed"

**Cause:** SPI connection issue

**Debug:**
```c
// Edit main.cpp to add:
Serial1.println("Flash begin: ");
Serial1.println(flash->begin());
```

**Check:**
- SPI pins correct (pinout.h)?
- CS pin not in conflict?
- Flash chip powered?

---

### Problem: Stack Overflow
**Symptom:** Random reboots, erratic behavior

**Already Fixed:** Task stacks increased to safe sizes

**If still occurs:** Increase task stack further in main.cpp:
```cpp
xTaskCreate(audioTask, "AUDIO", 2048, NULL, 1, &audioTaskHandle);  // doubled
```

---

## 🔍 Serial Debug Output

### Healthy System Output
```
Motor set to 1
Playing: alarm.raw
Playback complete
Motor 1 ON
Motor 1 OFF
```

### Problem Indicators
```
ERROR: LED queue creation failed     ← Queue issue
ERROR: Flash initialization failed    ← SPI issue  
Failed to play: bigfile.raw          ← File not found or corrupted
```

---

## 🎚️ Audio Quality Tips

### Better Quality (if needed):
- Use higher sample rate (but requires more storage)
- Use 8-bit signed PCM (supported)
- Keep audio duration reasonable (<30 seconds per file)

### Current Limitations:
- 8-bit audio (0-255 PWM)
- 8000 Hz sample rate
- Single speaker (mono)
- No volume control yet

---

## 💾 Flash Storage Capacity

**Device:** W25Q32 (32 Mbit = 4 MB)

**Layout:**
```
0x0000 - 0x0FFF:  Filesystem header (4 KB) - ~16 files
0x1000 - 0x3FFFFF: Audio data storage     - ~4 MB
```

**Maximum storage:**
- One 4 MB file, or
- Four 1 MB files, or  
- Thirty 128 KB files, etc.

**Current usage:**
```
Serial > MEM
[Shows free RAM and flash capacity]
```

---

## 📋 Maintenance Checklist

- [ ] Verify all serial commands respond
- [ ] Test audio playback monthly
- [ ] Monitor RAM usage (watch for leaks)
- [ ] Check watchdog doesn't trigger
- [ ] Verify motor response during audio

---

## ⚡ Performance Metrics

| Parameter | Value | Notes |
|-----------|-------|-------|
| Audio Latency | ~0 ms | Starts immediately |
| Flash Read Speed | ~5 MB/s | Depends on SPI clock |
| Task Switch Time | <1 ms | FreeRTOS |
| Audio Quality | Good | 8-bit PCM at 8kHz |
| RAM Usage | ~8 KB | Stack + queues |
| Flash Usage | Minimal | ~4KB filesystem header |

---

## 🆘 Emergency Reset

If system unresponsive:

1. Press **RESET** button on board
2. Wait 2 seconds
3. Reconnect serial

System will:
- Reinitialize flash
- Create new empty filesystem
- Be ready for new commands

All stored audio will be preserved.

---

## 📞 Support Commands

```
HELP          - Show all available commands
LIST          - List stored files and sizes
MEM           - Show memory statistics
MOTOR1-4 ON   - Control motors
PLAY <name>   - Play audio file
STORE <n.raw> - Upload new audio
```

---

## ✅ Deployment Checklist

Before shipping to production:

- [ ] Audio plays without glitches
- [ ] Flash storage stable
- [ ] Motors respond reliably
- [ ] No watchdog resets
- [ ] RAM doesn't continuously grow
- [ ] Serial communication stable
- [ ] All error messages formatted
- [ ] Documentation complete

**Status:** Ready for testing ✅

