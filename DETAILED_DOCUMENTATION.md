# Battery Manager Project - Detailed Technical Documentation

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Hardware Configuration](#hardware-configuration)
4. [Core Components](#core-components)
5. [FreeRTOS Tasks](#freertos-tasks)
6. [Data Structures](#data-structures)
7. [Module Documentation](#module-documentation)
8. [Communication Protocol](#communication-protocol)
9. [Usage Guide](#usage-guide)
10. [Development Guide](#development-guide)

---

## 1. Project Overview

**Name:** Battery Manager with Audio and Motor Control  
**MCU:** STM32G070RB (Nucleo G070RB board)  
**Framework:** Arduino + STM32FreeRTOS  
**Purpose:** Multi-motor control system with:
- Battery voltage monitoring
- Audio playback system
- Motor on/off control
- UART CLI interface
- Persistent audio storage on SPI flash

### Key Features
- ✓ FreeRTOS multi-tasking
- ✓ SPI flash filesystem for audio storage
- ✓ PWM audio DAC (8-bit PCM playback)
- ✓ Battery monitoring with voltage thresholds
- ✓ UART command interface
- ✓ Motor control (6 or 8 motors configuration)

### Specifications
- **Target MCU:** STM32G070RB
- **Flash Memory:** 128KB internal
- **RAM:** 36KB internal
- **External Storage:** W25Q32 SPI Flash (4MB)
- **Audio Sample Rate:** 16 kHz PWM
- **UART Baud:** 115200 bps
- **Motor Count:** 6 or 8 (compile-time config)

---

## 2. Architecture

### System Overview
```
┌─────────────────────────────────────────┐
│      STM32G070RB Nucleo Board           │
├─────────────────────────────────────────┤
│  FreeRTOS Kernel (Multi-Tasking)        │
├─────────────────────────────────────────┤
│                                         │
│  ┌──────────────┐  ┌──────────────┐   │
│  │  commTask    │  │  audioTask   │   │
│  │ (UART CLI)   │  │ (Playback)   │   │
│  └──────────────┘  └──────────────┘   │
│                                         │
│  ┌──────────────┐  ┌──────────────┐   │
│  │  motorTask   │  │  ledTask     │   │
│  │ (Motor ctrl) │  │ (Status LED) │   │
│  └──────────────┘  └──────────────┘   │
│                                         │
│  ┌──────────────┐  ┌──────────────┐   │
│  │  adcTask     │  │  loggerTask  │   │
│  │ (Battery mon)│  │ (Logging)    │   │
│  └──────────────┘  └──────────────┘   │
│                                         │
├─────────────────────────────────────────┤
│  Hardware Interfaces                    │
│  - UART1 (PA9/PA10)                     │
│  - SPI1 (PA5/PA6/PA7) + CS(PA8)         │
│  - GPIO Motors (PB0-PB7, PC*)           │
│  - PWM Audio (PA4/TIM2_CH1)             │
│  - ADC Mux (PA0, PC0/2, PA11/12/15)     │
└─────────────────────────────────────────┘
```

### Communication Flow
```
UART Input (commTask)
    ↓
CLI Parser (command_uart.cpp)
    ↓
Message Queue
    ├→ LED Queue (ledTask)
    ├→ Audio Queue (audioTask)
    ├→ Motor Queue (motorTask)
    └→ ADC Queue (adcTask)
    ↓
Device Control (GPIO/PWM/SPI)
```

---

## 3. Hardware Configuration

### Pin Mapping (pinout.h)

#### SPI Flash (W25Q32)
```
CS   → PA8
SCK  → PA5 (SPI1_SCK)
MISO → PA6 (SPI1_MISO)
MOSI → PA7 (SPI1_MOSI)
```

#### UART Interface (Serial1)
```
TX → PA9
RX → PA10
Baud: 115200
```

#### Audio Output
```
PWM Pin → PA4 (TIM2_CH1)
Sample Rate → 16kHz PWM frequency
Output → RC-filtered 8-bit DAC
```

#### Motor Control (6-motor or 8-motor)
```
Motor1: IN=PB0, CTRL=PB15
Motor2: IN=PB1, CTRL=PC1
Motor3: IN=PB2, CTRL=PC3
Motor4: IN=PB3, CTRL=PC5
Motor5: IN=PB4, CTRL=PC7
Motor6: IN=PB5, CTRL=PC9
Motor7: IN=PB6, CTRL=PC10 (8-motor only)
Motor8: IN=PB7, CTRL=PC11 (8-motor only)
```

#### Battery Monitoring (ADC Mux)
```
Mux Selector: S0→A11, S1→A12, S2→A15
Signal Out   → PA0 (ADC)
8 channels for 8 battery packs
```

#### Status LEDs
```
Blink LED   → PD1
Status LED  → PF0
```

---

## 4. Core Components

### 4.1 Command UART (command_uart.cpp)

**Purpose:** CLI interface for user control

**Key Functions:**
- `cli_tokenize()` - Parse command string into args
- `cli_execute()` - Match and execute commands
- `cmd_*()` - Individual command handlers
- `commTask()` - Main task loop

**Commands:**
```
help              - Show all available commands
led on/off/toggle - Control status LED
play <file>       - Play audio file from flash
motor <1-8> on/off - Control motor
mem               - Show memory usage
list              - List files in FLASH
store <name> <len> - Upload audio file (binary protocol)
format            - Format flash filesystem
mon               - Monitor ADC/battery values
beep [ms]         - Test 1kHz tone (debug)
```

**Upload Protocol (Binary Mode):**
```
PC sends: "store <filename> <length>\r\n"
Board responds: "OK\r\nREADY\r\n"
PC sends binary frames:
  [SOF] [SEQ] [LEN_L] [LEN_H] [DATA...256B] [CRC_L] [CRC_H]
Board responds: ACK (0x06) or NACK (0x15)
Repeat until all bytes sent
```

### 4.2 Audio DAC (audio_dac.cpp/h)

**Purpose:** 8-bit PCM audio playback via PWM

**Function:**
- `audioDacInit()` - Initialize PWM (TIM2_CH1 on PA4)
- `audioDacPlayFile()` - Load and play audio from flash
- `audioDacStop()` - Immediate stop
- `test_beep()` - Generate 1kHz test tone

**Implementation:**
- Uses Timer interrupt (TIM2_IRQHandler) at 16kHz
- Reads bytes from flash sequentially
- Maps 8-bit value (0-255) to PWM duty (0-100%)
- Blocking function (returns after playback completes)

**Output:**
- PWM frequency: 16kHz
- RC filter recommended: R=10k, C=100nF → ~160Hz cutoff
- Connects to audio amplifier or speaker

### 4.3 Flash Filesystem (flash_files.cpp/h)

**Purpose:** Simple filesystem for audio storage

**Layout:**
```
┌──────────────────────────────────────────┐
│ 0x0000-0x0FFF: Filesystem Header (4KB)   │
│  - Magic: 0xA5A5A5A5                     │
│  - File count                            │
│  - File entries (16 max)                 │
├──────────────────────────────────────────┤
│ 0x1000+: Audio Data (192KB)              │
│  - Stored sequentially                   │
│  - Max 16 files                          │
└──────────────────────────────────────────┘
```

**Key Functions:**
- `flashFsInit()` - Initialize/validate filesystem
- `flashFsFormat()` - Erase and reset
- `flashFsListFiles()` - Show file index
- `flashFsWriteFile()` - Write from buffer
- `flashFsWriteFileFromSerial_PRO()` - Receive via UART with binary protocol
- `flashFsReadFile()` - Read into buffer
- `flashFsGetFileInfo()` - Get file address/length

**3 Upload Modes:**
1. `flashFsWriteFile()` - Common, buffer-based
2. `flashFsWriteFileFromSerial_ACK()` - Simple ACK-per-frame
3. `flashFsWriteFileFromSerial_PRO()` - Professional with CRC & retries

### 4.4 Motor Control (motor_control.cpp/h)

**Purpose:** GPIO-based motor on/off control

**Functions:**
- `motorInit()` - Initialize all motor GPIO pins as outputs
- `motorSet(id, on)` - Turn motor on (HIGH) or off (LOW)
- `motorSetAll(on)` - Control all motors together

**Output Levels:**
- ON: GPIO HIGH (motor enabled)
- OFF: GPIO LOW (motor disabled)

**Note:** Motor driver logic depends on external circuit (high/low trigger)

### 4.5 Battery Monitoring (bat_handle.h/adc)

**Purpose:** Multi-channel ADC monitoring via 74HC4051 MUX

**Components:**
- MUX (74HC4051) with 8 channels
- ADC (PA0) reads mux output
- 3-bit control (S0/S1/S2) selects channel
- Resistor divider: 100k/68k (4.2V → 1.64V ADC range)

**Thresholds:**
```
BAT_OVERVOLTAGE:     4.30V (red alert)
BAT_FULL:            4.20V
BAT_FULL_THRESHOLD:  3.80V (ADC_SAT detect)
BAT_EMPTY:           3.00V
BAT_UNDERVOLTAGE:    2.90V (cutoff)
```

**Status Levels:**
- `BAT_STATUS_OK` - 3.0-4.2V
- `BAT_STATUS_LOW` - <3.2V (20%)
- `BAT_STATUS_CRITICAL` - <3.0V
- `BAT_STATUS_OVER` - >4.3V
- `BAT_STATUS_OVER_WARN` - ADC saturated (≥4.0V)
- `BAT_STATUS_ABSENT` - No battery

### 4.6 Logger (logger.cpp)

**Purpose:** Thread-safe logging via queue

**Features:**
- Asynchronous logging queue
- Timestamp with task name
- Silent mode (disable output during binary transfers)
- Error logging always enabled

**Functions:**
- `logInit()` - Start logger task
- `logPrint()/logPrintln()` - Queue message
- `logPrintf()` - Formatted output

---

## 5. FreeRTOS Tasks

### Task Structure
```
Priority (High→Low):
  4. loggerTask    (logging, lowest priority)
  3. adcTask       (battery monitoring)
  2. motorTask     (motor control)
  2. ledTask       (LED blink)
  2. audioTask     (audio playback - blocking)
  1. commTask      (UART CLI, highest priority)
```

### 5.1 commTask (command_uart.cpp:commTask)
- **Priority:** 1 (high)
- **Stack:** Default
- **Purpose:** UART command interface
- **Loop:**
  1. Check if in upload mode → receive binary data
  2. Otherwise, read CLI characters
  3. Parse and execute commands
  4. Send responses
- **Special:** Can enter binary upload mode (silentMode)

### 5.2 audioTask
- **Purpose:** Queue-based audio playback
- **Messages:** CMD_PLAY_AUDIO, CMD_TEST_AUDIO
- **Action:** Call `audioDacPlayFile()` (blocking)
- **Returns:** After playback complete

### 5.3 motorTask
- **Purpose:** Motor control from queue
- **Messages:** CMD_MOTOR_SET
- **Action:** Call `motorSet()`

### 5.4 ledTask
- **Purpose:** Status LED indication
- **Messages:** CMD_LED_ON, CMD_LED_OFF, CMD_LED_TOGGLE
- **Behavior:** Heartbeat blink pattern

### 5.5 adcTask
- **Purpose:** Battery voltage detection
- **Frequency:** Periodic ADC read
- **Output:** Serial log with voltage/percentage/status
- **Protection:** Auto-alert on over/under voltage

### 5.6 loggerTask
- **Purpose:** Deferred console output
- **Method:** Queue-based logging
- **Priority:** Lowest (non-blocking)

---

## 6. Data Structures

### Message_t (bat_man.h)
```c
typedef struct {
    CommandType cmd;      // Command ID
    uint32_t param;       // Parameter (e.g., duration)
    char name[16];        // Filename or motor ID
    bool on;              // On/off state
} Message_t;
```

### CommandType (bat_man.h)
```c
typedef enum {
    CMD_LED_ON,           // Turn LED on
    CMD_LED_OFF,          // Turn LED off
    CMD_LED_TOGGLE,       // Toggle LED
    CMD_PLAY_AUDIO,       // Play audio file
    CMD_MOTOR_SET,        // Set motor state
    CMD_TEST_AUDIO,       // Test beep
} CommandType;
```

### FlashFileEntry (flash_files.h)
```c
struct FlashFileEntry {
    char name[16];        // Filename (null-term)
    uint32_t addr;        // Flash address
    uint32_t length;      // File size
};
```

### FlashFsHeader (flash_files.h)
```c
struct FlashFsHeader {
    uint32_t magic;           // 0xA5A5A5A5
    uint8_t fileCount;        // Number of files
    uint8_t reserved[3];
    FlashFileEntry files[16]; // File index
};
```

### BatStatus_t (bat_handle.h)
```c
typedef enum {
    BAT_STATUS_OK = 0,
    BAT_STATUS_LOW,
    BAT_STATUS_CRITICAL,
    BAT_STATUS_OVER,
    BAT_STATUS_OVER_WARN,
    BAT_STATUS_ABSENT,
} BatStatus_t;
```

---

## 7. Module Documentation

### audio_dac.cpp
**Implements:** 8-bit PCM playback via PWM

**Key Functions:**
```cpp
void audioDacInit()
// Initialize PWM on PA4 (TIM2_CH1)
// Frequency: 16kHz
// Mode: PWM mode 1 (0-100% duty)

bool audioDacPlayFile(SPIFlash *flash, const char *filename)
// Load file from flash and play via PWM
// Blocking: returns after playback complete
// Returns: true if success, false if file not found

bool audioDacIsPlaying()
// Check if currently playing

void audioDacStop()
// Stop playback immediately

uint8_t audioDacGetProgress()
// Get 0-100% progress

extern "C" void TIM2_IRQHandler()
// Timer interrupt handler
// Called at 16kHz to output next sample
```

**Timer Interrupt Flow:**
```
TIM2 interrupt (16kHz)
  → Get next byte from flash buffer
  → Convert to PWM duty (0-255 → 0-100%)
  → Update TIM2_CCR1
  → Continue until done
```

### motor_control.cpp
**Implements:** GPIO-based motor control

**Functions:**
```cpp
void motorInit()
// Configure all motor GPIO pins as outputs
// Set all to LOW initially

void motorSet(MotorId motor, bool on)
// Drive motor GPIO HIGH (on) or LOW (off)
// Example: motorSet(MOTOR_1, true) → PB0=HIGH, PB15=HIGH

void motorSetAll(bool on)
// Toggle all motors simultaneously
```

### flash_files.cpp
**Implements:** SPI flash filesystem

**Key Algorithm:**
1. **Format:** Write magic header + empty file table to 0x0000
2. **Write:** Find free space, append to file table, write data
3. **Read:** Search file table, get address, read from flash
4. **Upload:** Receive bytes from UART, write to flash sequentially

**Upload Protocols:**

**Mode 1 (Simple ACK):**
```
Frame: [SOF] [SEQ] [LEN_L] [LEN_H] [DATA] [CRC_L] [CRC_H]
Response: ACK (0x06) or NACK (0x15)
```

**Mode 2 (Professional/PRO):**
```
Same frame format but with retries and better error handling
Implemented in flashFsWriteFileFromSerial_PRO()
```

### command_uart.cpp
**Implements:** UART CLI interface

**Parser:**
- Tokenizes input on spaces
- Max 8 arguments
- Dispatches to command function

**Key Variables:**
- `uploadMode` - True during binary file upload
- `silentMode` - Suppress logger during binary mode
- `cliEcho` - Echo input characters
- `uploadName` / `uploadLength` - Current upload context

**Command Handlers:**
- `cmd_help()` - Print command list
- `cmd_led()` - LED control
- `cmd_play()` - Audio playback
- `cmd_motor()` - Motor on/off
- `cmd_mem()` - Memory status
- `cmd_list()` - List files in flash
- `cmd_store()` - Initiate file upload
- `cmd_format()` - Format filesystem
- `cmd_mon()` - Battery monitor
- `cmd_beep()` - Test tone

### logger.cpp
**Implements:** Queue-based logging

**Architecture:**
- Creates message queue (loggerQueue)
- Creates loggerTask
- `logPrint()` enqueues message
- loggerTask dequeues and outputs to Serial

**Features:**
- Thread-safe (no race conditions)
- Non-blocking sends
- Timestamps with task name
- Can be disabled (silentMode)

---

## 8. Communication Protocol

### UART CLI Protocol

**Character Mode:**
```
PC → Board: "play xinchao.pcm\r\n"
Board → PC: "play xinchao.pcm\r\n" (echo)
            "> " (prompt)
```

**Binary File Upload (store command):**

```
Phase 1: Command
  PC → Board: "store khay1.pcm 24902\r\n"
  Board → PC: "OK\r\n"
  Board → PC: "READY\r\n"

Phase 2: Binary Transfer
  PC → Board: Binary frame (256-byte chunks)
              [SOF:1] [SEQ:1] [LEN_L:1] [LEN_H:1] [DATA:256] [CRC_L:1] [CRC_H:1]
              Total: 262 bytes per frame
  
  Board → PC: ACK (0x06) or NACK (0x15)

Phase 3: Completion
  After all data:
  Board → PC: "DONE\r\n"
             "> " (prompt)
```

### Upload Frame Format
```
Byte 0     : SOF = 0xAA (start of frame marker)
Byte 1     : SEQ (sequence number, 0-255, wraps around)
Byte 2-3   : LEN (little-endian, actual chunk size)
Byte 4-259 : DATA (up to 256 bytes)
Byte 260-261: CRC16 (CCITT, little-endian)

Example for 100-byte chunk:
  AA 00 64 00 [100 bytes] [CRC_L] [CRC_H]
```

### CRC16 Calculation
```c
uint16_t crc16(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

---

## 9. Usage Guide

### Building the Project

**Prerequisites:**
- PlatformIO installed
- USB ST-Link debugger connected
- STM32G070RB Nucleo board

**Compile:**
```bash
# 6-motor configuration (default)
platformio run -e nucleo_g070rb_6motor

# 8-motor configuration
platformio run -e nucleo_g070rb_8motor
```

**Flash:**
```bash
platformio run -e nucleo_g070rb_6motor --target upload
```

### Using UART CLI

**Open Serial Monitor:**
```bash
platformio device monitor -p COM6 -b 115200
```

**Example Commands:**
```
> help
Commands:
  help - Show help
  led on/off/toggle - Control LED
  play <file> - Play audio
  motor <1-8> on/off - Control motor
  list - Show files
  beep - Test 1kHz tone
  mon - Battery monitor
  > 

> list
Files:
0: khay1.pcm len=24902
1: khay2.pcm len=25718
...

> play xinchao.pcm
Play: xinchao.pcm (add to queue)
> 

> motor 1 on
Motor 1 set ON
> 

> beep 1000
Beep test queued
> 

> mon
Battery Monitor:
CH0 raw=1024 Vbat=3.125V
CH1 raw=1100 Vbat=3.355V
...
```

### Uploading Audio Files

**Using Python Script:**
```bash
# Upload single file
python upload_audio.py COM6 filename.pcm path/to/audio/filename.pcm

# Upload all files with auto-retry
python flash_board.py --motors 6 --port COM6

# Skip firmware, upload audio only
python flash_board.py --motors 6 --port COM6 --skip-firmware
```

**Manual UART Upload:**
```bash
# Interactive shell
python uart_shell.py

# Commands:
# > format
# > store khay1.pcm 24902
# [send 24902 bytes in binary frames]
# > list
```

### Battery Monitoring

**Check battery status:**
```
> mon
ADC Battery Monitor:
CH0 raw=1024 Vbat=3.125V
CH1 raw=1150 Vbat=3.504V
...
Status indicators:
  - 3.0-4.2V = OK (green)
  - <3.0V = CRITICAL (red alert)
  - >4.3V = OVER-VOLTAGE (red alert)
```

---

## 10. Development Guide

### Adding a New Command

**Step 1:** Define handler in command_uart.cpp
```c
void cmd_mycommand(int argc, char **argv) {
    if (argc < 2) {
        logPrintln("Usage: mycommand <arg>");
        return;
    }
    
    // Parse args
    int value = atoi(argv[1]);
    
    // Do work
    logPrintln("My command executed");
}
```

**Step 2:** Add to command table
```c
cli_command_t cli_table[] = {
    ...
    {"mycommand", cmd_mycommand, "mycommand <arg> - Description"},
    ...
};
```

### Adding a New Task

**Step 1:** Create task function
```cpp
void myTask(void *pvParameters) {
    // Initialize
    logPrintln("myTask started");
    
    while (1) {
        // Work
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Step 2:** Create in main.cpp
```cpp
xTaskCreate(myTask, "myTask", 256, NULL, 3, NULL);
```

### Debugging Tips

**Enable logging:**
- Check `loggerTask` is running
- Make sure `silentMode = false`
- Check Serial1 connection

**Monitor performance:**
```
> mem
Heap usage / Memory stats
```

**Test audio:**
```c
// Generate 1kHz test tone
> beep 2000  // 2-second beep
```

**Monitor battery:**
```
> mon
```

---

## Appendix: File Structure

```
battery_manager/
├── platformio.ini              # PlatformIO configuration
├── src/
│   ├── main.cpp               # Entry point (no longer exists, integrated)
│   ├── bat_man.h              # Main header with Message_t, CommandType
│   ├── pinout.h               # Pin definitions
│   ├── command_uart.cpp/h     # UART CLI implementation
│   ├── audio_dac.cpp/h        # PWM audio playback
│   ├── motor_control.cpp/h    # Motor GPIO control
│   ├── flash_files.cpp/h      # SPI flash filesystem
│   ├── bat_handle.h           # Battery monitoring types
│   ├── logger.cpp             # Queue-based logging
│   ├── system_clock.c         # Clock configuration
│   └── [other modules]
├── include/                   # External libraries
├── lib/                       # Project libraries
├── test/                      # Test files
├── out/                       # Compiled *.pcm audio files
├── audio/                     # Original *.mp3 files
└── docs/
    ├── DETAILED_DOCUMENTATION.md  (this file)
    ├── PRODUCTION.md              # Production checklist
    ├── AUDIO_DAC_GUIDE.md         # Audio configuration
    └── ...
```

---

**Document Version:** 1.0  
**Last Updated:** April 2, 2026  
**Author:** Battery Manager Project Team
