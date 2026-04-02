# Developer's Implementation Guide

## Table of Contents
1. [Project Structure](#project-structure)
2. [Code Flow](#code-flow)
3. [Adding New Features](#adding-new-features)
4. [Common Patterns](#common-patterns)
5. [Testing](#testing)
6. [Optimization](#optimization)

---

## Project Structure

### Directory Tree
```
battery_manager/
├── .pio/                    # PlatformIO cache
├── .vscode/                 # VS Code settings
├── src/                     # Main source code
│   ├── bat_man.h           # Core types (Message_t, CommandType)
│   ├── bat_handle.h        # Battery struct defs
│   ├── pinout.h            # Pin definitions
│   ├── command_uart.cpp    # CLI interface
│   ├── audio_dac.cpp/h     # Audio PWM playback
│   ├── motor_control.cpp   # Motor control
│   ├── flash_files.cpp/h   # SPI flash FS
│   ├── logger.cpp/h        # Logging system
│   └── system_clock.c      # Clock setup
├── include/                 # External headers
├── lib/                     # Project libs (if any)
├── test/                    # Test files
├── audio/                   # Original MP3 files
├── out/                     # Compiled PCM files
├── platformio.ini          # Build config
├── gen_hex.py              # Hex generation
├── flash_board.py          # Main upload script
└── docs/                   # Documentation
    ├── DETAILED_DOCUMENTATION.md
    ├── QUICK_REFERENCE.md
    └── (this file)
```

### Key Files Purpose

| File | Lines | Purpose |
|------|-------|---------|
| bat_man.h | 50 | Message structures, config |
| command_uart.cpp | 400+ | CLI parser & commands |
| audio_dac.cpp | 300+ | PWM audio playback |
| flash_files.cpp | 500+ | SPI flash FS |
| motor_control.cpp | 100+ | GPIO motor control |
| logger.cpp | 200+ | Queue-based logging |

---

## Code Flow

### 1. Startup Flow

```
STM32 Reset
    ↓
setup() (Arduino entry)
    ├→ pinMode() calls for all GPIO
    ├→ Serial1.begin(115200)
    ├→ SPI.begin()
    ├→ flashFsInit()
    ├→ motorInit()
    ├→ audioDacInit()
    ├→ muxInit()
    └→ Create FreeRTOS tasks:
        ├→ commTask (UART CLI)
        ├→ audioTask (audio queue)
        ├→ motorTask (motor queue)
        ├→ ledTask (LED status)
        ├→ adcTask (battery monitor)
        └→ loggerTask (logging)
    ↓
vTaskStartScheduler()
    ↓
FreeRTOS running...
```

### 2. Command Execution Flow

```
User input: "play khay1.pcm\r\n"
    ↓
commTask reads from Serial1
    ↓
Character accumulated in buffer
    ↓
\r or \n received
    ↓
cli_tokenize("play",  "khay1.pcm")
    ↓
cli_execute() searches command table
    ↓
cmd_play() called with argc=2, argv=["play", "khay1.pcm"]
    ↓
Create Message_t msg
    ├→ msg.cmd = CMD_PLAY_AUDIO
    ├→ strcpy(msg.name, "khay1.pcm")
    └→ msg.param = 0
    ↓
safeQueueSend(_queueAudio, &msg)
    ↓
audioTask receives message
    ├→ audioDacPlayFile(_flash, "khay1.pcm")
    │   ├→ flashFsReadFile() locate file
    │   ├→ Setup timer interrupt (TIM2)
    │   ├→ Read blocks from flash
    │   ├→ ISR outputs PWM samples
    │   └→ Return when done
    ├→ Clear queue message
    └→ Continue waiting for next message
```

### 3. Audio Playback Flow

```
audioDacPlayFile("khay1.pcm")
    ↓
flashFsGetFileInfo()
    ├→ Search file table
    ├→ Get file address & length
    └→ Validate
    ↓
Open file for reading
    ↓
Enable TIM2 interrupt (16kHz)
    ↓
Main loop: read flash blocks
    ├→ SPI read 256 bytes
    ├→ Queue for interrupt
    └→ Continue...
    ↓
TIM2_IRQHandler (called at 16kHz)
    ├→ Get next sample byte (0-255)
    ├→ Convert to PWM duty (0-100%)
    ├→ Update TIM2_CCR1
    └→ Return
    ↓
When file exhausted:
    ├→ Disable timer interrupt
    ├→ Return from function
    └→ audioTask continues
```

### 4. File Upload Flow

```
User: "store khay1.pcm 24902\r\n"
    ↓
cmd_store() called
    ├→ Set uploadMode = true
    ├→ Set silentMode = true
    ├→ Output "OK\r\n"
    ├→ Output "READY\r\n"
    └→ Return
    ↓
commTask loop continues
    ↓
if (uploadMode)
    ├→ flashFsWriteFileFromSerial_PRO()
    │   ├→ Receive binary frames (262 bytes each)
    │   ├→ Validate CRC
    │   ├→ Write to flash
    │   ├→ Send ACK/NACK
    │   └→ Continue until uploadLength bytes received
    ├→ Set uploadMode = false
    ├→ Set silentMode = false
    ├→ Output "DONE\r\n"
    └→ Output prompt ">\r\n"
```

### 5. Battery Monitoring Flow

```
adcTask loop
    ↓
for (ch = 0 to 7)
    ├→ muxSelect(ch)
    ├→ muxRead(ch) - get ADC value
    ├→ adcValueToBatteryVoltage() - convert
    ├→ batteryPercent() - calculate %
    ├→ batteryStatus() - get status
    ├→ logPrintf("CH%d: %.3fV")
    └→ Check thresholds:
        ├→ If over 4.3V → alert
        ├→ If under 3.0V → alert
        └→ Otherwise OK
    ↓
vTaskDelay(pdMS_TO_TICKS(1000))
    ↓
Repeat
```

---

## Adding New Features

### Pattern 1: Add New Command

**Step 1:** Define command function
```cpp
// In command_uart.cpp, add:
void cmd_newcmd(int argc, char **argv) {
    if (argc < 2) {
        logPrintln("Usage: newcmd <arg>");
        return;
    }
    
    // Parse argument
    int param = atoi(argv[1]);
    
    // Execute
    logPrintln("newcmd executed");
}
```

**Step 2:** Register in command table
```cpp
// In cli_table[]:
{"newcmd", cmd_newcmd, "newcmd <arg> - Description"},
```

**Step 3:** Test via UART
```
> newcmd 42
newcmd executed
>
```

### Pattern 2: Add New Task

**Step 1:** Create task function
```cpp
// In new_module.cpp
void myNewTask(void *pvParameters) {
    logPrintln("myNewTask started");
    
    while (1) {
        // Do periodic work
        
        logPrintln("myNewTask tick");
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second
    }
}
```

**Step 2:** Create in main
```cpp
// In setup() or equivalent:
xTaskCreate(
    myNewTask,          // Task function
    "MyTask",           // Task name
    512,                // Stack size (bytes)
    NULL,               // Parameters
    2,                  // Priority
    NULL                // Handle
);
```

### Pattern 3: Add New Message Type

**Step 1:** Define command type
```cpp
// In bat_man.h, add to CommandType enum:
typedef enum {
    // ... existing ...
    CMD_NEW_ACTION,
    // ... others ...
} CommandType;
```

**Step 2:** Create handler task
```cpp
void newHandlerTask(void *pvParameters) {
    QueueHandle_t q = (QueueHandle_t)pvParameters;
    Message_t msg;
    
    while (1) {
        if (xQueueReceive(q, &msg, portMAX_DELAY)) {
            switch (msg.cmd) {
                case CMD_NEW_ACTION:
                    // Handle action
                    logPrintln("New action!");
                    break;
                default:
                    break;
            }
        }
    }
}
```

**Step 3:** Send message from CLI
```cpp
void cmd_newaction(int argc, char **argv) {
    Message_t msg;
    msg.cmd = CMD_NEW_ACTION;
    msg.param = 0;
    safeQueueSend(_queueNew, &msg);
}
```

### Pattern 4: Add Audio Processing

**Modify audio_dac.cpp:**
```cpp
void TIM2_IRQHandler(void) {
    // Timer called at 16kHz
    
    // Get next sample
    uint8_t sample = getNextSample();  // 0-255
    
    // Apply processing:
    // 1. Volume control
    sample = (sample * volume) / 100;  // 0-100% volume
    
    // 2. Filter
    static uint16_t filtered = 0;
    filtered = (filtered * 3 + sample) / 4;  // Simple low-pass
    
    // 3. Output
    TIM2->CCR1 = filtered;  // 0-255 → PWM duty
    
    // Clear interrupt
    TIM2->SR &= ~TIM_SR_CC1IF;
}
```

### Pattern 5: Add External Sensor

**Step 1:** Create sensor header
```cpp
// include/my_sensor.h
#pragma once

void sensorInit(void);
float sensorRead(void);
```

**Step 2:** Implement sensor module
```cpp
// src/my_sensor.cpp
#include "my_sensor.h"
#include "bat_man.h"

#define SENSOR_PIN PA0

void sensorInit(void) {
    pinMode(SENSOR_PIN, INPUT);
}

float sensorRead(void) {
    int raw = analogRead(SENSOR_PIN);
    float voltage = raw * 3.3f / 1024.0f;
    return voltage;
}
```

**Step 3:** Use in main
```cpp
setup() {
    sensorInit();
}

void sensorTask(void *pvParameters) {
    while (1) {
        float value = sensorRead();
        logPrintf("Sensor: %.2fV\n", value);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Common Patterns

### Thread-Safe Queue Send
```cpp
void safeQueueSend(QueueHandle_t q, Message_t *msg) {
    if (xQueueSend(q, msg, pdMS_TO_TICKS(100)) != pdPASS) {
        logPrintln("Queue send failed!");
    }
}
```

### Convert ADC to Voltage
```cpp
float adcToVoltage(uint16_t adcValue) {
    // 12-bit ADC: 0-4095
    // 3.3V ref
    return (adcValue / 4095.0f) * 3.3f;
}
```

### GPIO Toggle
```cpp
void togglePin(uint32_t pin) {
    static uint8_t state = LOW;
    state = !state;
    digitalWrite(pin, state);
}
```

### Timer Setup (TIM2)
```cpp
void TimerInit(void) {
    // Enable TIM2 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    // Configure timer:
    // PSC = 64MHz / 16kHz = 4000 - 1
    TIM2->PSC = 3999;
    TIM2->ARR = 0;  // Overflow = no limit
    
    // Enable interrupt
    TIM2->DIER |= TIM_DIER_UIE;
    
    // Start timer
    TIM2->CR1 |= TIM_CR1_CEN;
}
```

### Mutex Protection
```cpp
void protectedFunction(void) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
        // Critical section
        criticalWork();
        xSemaphoreGive(mutex);
    }
}
```

---

## Testing

### Unit Testing Strategy

**1. Test Individual Components**
```cpp
// Test audio_dac in isolation
#include "audio_dac.h"

void test_audioDac(void) {
    audioDacInit();
    
    // Generate test file
    uint8_t testData[100] = {/* sine wave */};
    
    // Play
    bool result = audioDacPlayFile(nullptr, "test.pcm");
    
    assert(result == true);
    assert(audioDacIsPlaying() == false);  // Should be done
}
```

**2. Integration Testing**
```cpp
// Test CLI → audio flow
void test_play_command(void) {
    Message_t msg;
    msg.cmd = CMD_PLAY_AUDIO;
    strcpy(msg.name, "khay1.pcm");
    
    xQueueSend(audioQueue, &msg, 0);
    vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for playback
    
    // Verify playback occurred
}
```

**3. Module Testing via CLI**
```
# Test motor
> motor 1 on
> motor 1 off
> motor 1 on

# Test audio
> play khay1.pcm
> beep 2000

# Test battery
> mon

# Test flash
> list
> format
```

### Debug Output

Add debug prints:
```cpp
#define DEBUG 1

#if DEBUG
#define dbg(...) logPrintf(__VA_ARGS__)
#else
#define dbg(...)
#endif

void myFunction(void) {
    dbg("Starting...\n");
    // code
    dbg("Result: %d\n", result);
}
```

---

## Optimization

### Memory Optimization

**Check usage:**
```cpp
// In a task:
void logMemory(void) {
    uint8_t buffer[256];
    size_t freeHeap = xPortGetFreeHeapSize();
    logPrintf("Free heap: %d bytes\n", freeHeap);
}
```

**Reduce stack usage:**
```cpp
// BAD: Large stack allocation
void process_data(void) {
    uint8_t buffer[10000];  // Stack overflow!
    // ...
}

// GOOD: Use heap or global
uint8_t buffer[10000];  // Global or heap allocated
void process_data(void) {
    // Use buffer
}
```

**Optimize strings:**
```cpp
// BAD: Many string copies
const char str[] = "This is a very long string";
logPrintln(str);
logPrintln(str);

// GOOD: Use string reference
#define STATUS_MSG "This is a very long string"
logPrintln(STATUS_MSG);
```

### Speed Optimization

**Optimize loops:**
```cpp
// BAD: Repeated condition check
for (int i = 0; i < strlen(data); i++) {  // strlen called 1000x
    process(data[i]);
}

// GOOD: Cache length
int len = strlen(data);
for (int i = 0; i < len; i++) {
    process(data[i]);
}
```

**Use hardware features:**
```cpp
// Use DMA for SPI transfers:
HAL_SPI_Transmit_DMA(&hspi1, buffer, size);

// Use timer for PWM instead of software:
TIM2->CCR1 = dutyCycle;  // Hardware PWM
// Instead of: digitalWrite() in loop
```

### Power Optimization

**Use task delays appropriately:**
```cpp
// GOOD: MCU sleeps between checks
void adcTask(void *pvParameters) {
    while (1) {
        readADC();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep 1 second
    }
}

// Use STOP mode when not needed:
HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
```

---

## Best Practices

### 1. Always Use Mutex for Shared Resources
```cpp
extern SemaphoreHandle_t adcMutex;

void getADCValue(void) {
    xSemaphoreTake(adcMutex, portMAX_DELAY);
    uint16_t value = HAL_ADC_Read();
    xSemaphoreGive(adcMutex);
    return value;
}
```

### 2. Error Handling
```cpp
bool readFromFlash(uint32_t addr, uint8_t *buffer, uint16_t len) {
    if (!buffer || len == 0) {
        logPrintln("ERR: Invalid params");
        return false;
    }
    
    if (!flash.readByteArray(addr, buffer, len)) {
        logPrintln("ERR: Flash read failed");
        return false;
    }
    
    return true;
}
```

### 3. Resource Cleanup
```cpp
void task_with_resources(void *pvParameters) {
    QueueHandle_t q = xQueueCreate(10, sizeof(Message_t));
    
    while (1) {
        // Use queue
    }
    
    vQueueDelete(q);  // Cleanup before exit
}
```

### 4. Documentation
```cpp
/// Calculate battery percentage from voltage
/// \param voltage Battery voltage in volts (3.0-4.2V)
/// \return Battery percentage (0-100)
uint8_t batteryPercent(float voltage) {
    // Implementation
}
```

---

**Version:** 1.0  
**Last Updated:** April 2, 2026
