# Charging Management System Documentation

## Overview

The charging management system (`bat_charge.h` / `bat_charge.cpp`) implements intelligent battery charging logic for the 8-slot battery management system. It ensures safe, efficient charging with the following features:

1. **Single-Slot Charging**: Maximum one slot can charge at any given time
2. **Automatic Detection**: Detects battery presence via voltage thresholds
3. **Smart Selection**: Always charges the 2 slots with lowest capacity
4. **Dynamic Switching**: Switches charging if a battery suddenly drops below 20%
5. **Voltage Monitoring**: Real-time battery voltage and percentage monitoring

---

## Hardware Connection

### Charging Control Pins (MCU_CTR)
Each of the 8 battery slots has a dedicated control pin in the MCU:

| Slot | Control Pin | Location |
|------|------------|----------|
| 0 | POGO1_CTR_PIN (PC8) | pos1_ctr |
| 1 | POGO2_CTR_PIN (PC6) | pos2_ctr |
| 2 | POGO3_CTR_PIN (PD9) | pos3_ctr |
| 3 | POGO4_CTR_PIN (PC2) | pos4_ctr |
| 4 | POGO5_CTR_PIN (PC0) | pos5_ctr |
| 5 | POGO6_CTR_PIN (PB14) | pos6_ctr |
| 6 | POGO7_CTR_PIN (PC12) | pos7_ctr |
| 7 | POGO8_CTR_PIN (PC13) | pos8_ctr |

- **HIGH**: Enable charging
- **LOW**: Disable charging (standby)

### Battery Voltage Sensing (POINT_CTR)
Battery voltage is read via an 8-channel ADC multiplexer (MUX) connected to PA0:

- **MUX Control**: Pins A11 (MUX_S0), A12 (MUX_S1), A15 (MUX_S2)
- **MUX Signal**: PA0 (analog input)
- **Circuit**: Battery → Diode (1N4148) → R5 (22Ω) → MUX → PA0 (with R65 10kΩ pull-up to +5V)

#### Voltage Detection Thresholds
- **Battery Present**: Reads 4.5V @ ADC (after circuit conversion)
- **No Battery**: Reads 0.6V @ ADC
- **Algorithm**:
  - If voltage ≥ 3.5V: Battery is present
  - If voltage < 1.5V: No battery

---

## Data Structures

### `BatterySlot_t`
Represents the state of a single battery slot:

```c
typedef struct {
    uint8_t  slot;           // Slot number (0-7)
    float    voltage;        // Current voltage reading (V)
    uint8_t  percent;        // Battery percentage (0-100%)
    bool     isPresent;      // Battery connected or not
    BatStatus_t status;      // Current status (OK, LOW, CRITICAL, etc.)
} BatterySlot_t;
```

### `ChargeManager_t`
Internal state of the charging manager:

```c
typedef struct {
    uint8_t  currentCharging;   // Currently charging slot (0-7), or 0xFF if none
    uint8_t  candidates[2];     // Two slots with smallest capacity
    uint16_t pollCounter;       // Internal timing counter
    bool     isEnabled;         // Charging manager enabled/disabled
} ChargeManager_t;
```

---

## API Reference

### Initialization

#### `chargeManagerInit()`
Initialize charging system and configure all control pins as outputs.
- Called automatically by `chargeTask()` on startup
- Sets all charging pins to LOW (disabled state)

**Example:**
```c
chargeManagerInit();  // Done automatically in chargeTask()
```

---

### Configuration

#### `chargeManagerSetEnabled(bool enabled)`
Enable or disable the charging management system.
- When enabled: System actively manages charging
- When disabled: All charging is stopped
- Call `chargeStopAll()` internally if disabling

**Parameters:**
- `enabled`: `true` to enable, `false` to disable

**Example:**
```c
chargeManagerSetEnabled(true);   // Start charging management
chargeManagerSetEnabled(false);  // Stop all charging
```

---

### Reading Battery Status

#### `BatterySlot_t chargeGetBatterySlot(uint8_t slot)`
Read voltage, percentage, and presence status of a specific slot.

**Parameters:**
- `slot`: Slot number (0-7)

**Returns:** `BatterySlot_t` structure with:
- `voltage`: Battery voltage in Volts
- `percent`: Battery percentage (0-100%)
- `isPresent`: `true` if battery detected
- `status`: Battery status enum

**Example:**
```c
BatterySlot_t bat = chargeGetBatterySlot(0);
if (bat.isPresent) {
    printf("Slot 0: %.2fV at %d%%\n", bat.voltage, bat.percent);
}
```

---

#### `bool chargeIsBatteryPresent(uint8_t slot)`
Quick check if battery is present in a slot.

**Parameters:**
- `slot`: Slot number (0-7)

**Returns:**
- `true`: Battery detected (voltage ≥ 3.5V)
- `false`: No battery

**Example:**
```c
if (chargeIsBatteryPresent(2)) {
    logPrintln("Battery in slot 2");
}
```

---

#### `void chargeGetCandidates(BatterySlot_t *slots, uint8_t *candidates)`
Read all 8 slots and identify the 2 with lowest capacity.

**Parameters:**
- `slots`: Array of 8 `BatterySlot_t` (output - filled with readings)
- `candidates`: Array of 2 slot indices (output - lowest capacities)

**Notes:**
- Only considers slots with batteries present
- Fills `candidates` with slot numbers (0-7) or 0xFF if less than 2 batteries
- First candidate (`candidates[0]`) is the lowest
- Second candidate (`candidates[1]`) is the second-lowest

**Example:**
```c
BatterySlot_t slots[8];
uint8_t candidates[2];
chargeGetCandidates(slots, candidates);

printf("Candidates: slot %u (%u%%), slot %u (%u%%)\n",
       candidates[0], slots[candidates[0]].percent,
       candidates[1], slots[candidates[1]].percent);
```

---

### Controlling Charging

#### `void chargeStartSlot(uint8_t slot)`
Start charging a specific slot. Automatically stops charging of any currently-charging slot.

**Parameters:**
- `slot`: Slot number to begin charging (0-7)

**Behavior:**
1. Sets current charging slot to LOW (disabled)
2. Sets new slot to HIGH (enabled)
3. Updates `chargeManager.currentCharging`
4. Logs operation to serial

**Example:**
```c
chargeStartSlot(3);  // Start charging slot 3
```

---

#### `void chargeStopAll()`
Stop all charging immediately.

**Behavior:**
- Disables the currently charging slot pin
- Sets `chargeManager.currentCharging = 0xFF`

**Example:**
```c
chargeStopAll();  // Emergency stop
```

---

### Status Queries

#### `uint8_t chargeGetCurrent()`
Get the slot number currently being charged.

**Returns:**
- Slot number (0-7): Currently charging
- `0xFF` (255): No slot is charging

**Example:**
```c
uint8_t current = chargeGetCurrent();
if (current != 0xFF) {
    printf("Currently charging slot %u\n", current);
} else {
    printf("No charging\n");
}
```

---

#### `bool chargeIsCharging(uint8_t slot)`
Check if a specific slot is currently being charged.

**Parameters:**
- `slot`: Slot number to check (0-7)

**Returns:**
- `true`: This slot is currently charging
- `false`: This slot is not charging

**Example:**
```c
if (chargeIsCharging(0)) {
    logPrintln("Slot 0 is charging");
}
```

---

## Charging Algorithm

The `chargeTask()` runs continuously and implements the following logic:

### 1. **Initialization**
```
On startup:
- Initialize all charging control pins (set to LOW/off)
- Set currentCharging = 0xFF (no charging)
- Enable charging manager
```

### 2. **Main Loop** (runs every 1000ms)
```
POLL:
  1. Read all 8 battery slots
     - Get voltage via ADC multiplexer
     - Calculate percentage (0-100%)
     - Detect presence (voltage threshold)

  2. Find 2 candidates with lowest capacity
     - Only consider slots with batteries present
     - If < 2 batteries: stop charging

  3. IF not currently charging:
       - Start charging the lowest candidate
       
     ELSE (currently charging):
       - Check if current slot still has battery
         - If battery removed: switch to lowest candidate
         - If battery dropped below 20%:
           - AND another candidate is lower:
           - SWITCH to that candidate

  4. Wait 1000ms, go to POLL
```

### 3. **Switching Logic**
The system will switch charging from Slot A to Slot B if:
- Slot A's battery is removed
- Slot B's percentage < 20%
- Slot B's percentage < Slot A's percentage

This ensures critical batteries get charged first.

---

## Constants and Configuration

Located in `bat_charge.h`:

```c
#define NUM_BATTERY_SLOTS    8      // Total slots
#define CHARGE_THRESHOLD     20     // Switch if slot drops below 20%
#define CHARGE_DEFAULT_SLOT  0      // Default starting slot
#define CHARGE_POLL_INTERVAL 1000   // Check every 1000ms

#define BAT_PRESENT_MIN_VOLTAGE  3.5f   // Battery detected if ≥ 3.5V
#define BAT_ABSENT_MAX_VOLTAGE   1.5f   // No battery if < 1.5V
```

### Changing Configuration

To adjust charging behavior, modify these constants:

**Example: Check every 2 seconds instead of 1 second**
```c
#define CHARGE_POLL_INTERVAL 2000
```

**Example: Switch at 10% instead of 20%**
```c
#define CHARGE_THRESHOLD 10
```

---

## Integration with Existing Code

### Main Task Creation
In `main.cpp`, the charge task is created alongside other tasks:

```c
xTaskCreate(chargeTask, "CHARGE", 512, NULL, 2, NULL);
```

- **Priority**: 2 (same as main task, higher than UI tasks)
- **Stack**: 512 bytes
- **Name**: "CHARGE"

### ADC Mutex Synchronization
The charging system uses the existing `adcMutex` to safely read battery voltages. This prevents conflicts with the `adcTask()` which also reads the multiplexer.

### Log Output
All operations are logged to the serial UART:

```
[CHARGE] Manager initialized
[CHARGE] Pins initialized
[CHARGE] ===== Charging Status =====
[CHARGE] Slot[0]: 4.20V 100% status=0 
[CHARGE] Slot[1]: 3.85V  78% status=0 [CHARGING]
[CHARGE] Slot[2]: 3.20V  20% status=1
[CHARGE] Slot[3]: NO BATTERY
...
[CHARGE] Candidates: [1]=78% [2]=20%
[CHARGE] START charging slot 1
```

---

## Usage Examples

### Example 1: Check which slots have batteries
```c
BatterySlot_t slots[8];
uint8_t candidates[2];
chargeGetCandidates(slots, candidates);

for (int i = 0; i < 8; i++) {
    if (slots[i].isPresent) {
        logPrintf("Slot %d: %.2fV (%d%%)\n", 
                  i, slots[i].voltage, slots[i].percent);
    }
}
```

### Example 2: Manual charging override
```c
// Stop current charging
chargeStopAll();

// Start charging a specific slot
chargeStartSlot(5);

// Wait 5 seconds
vTaskDelay(pdMS_TO_TICKS(5000));

// Resume automatic charging
chargeManagerSetEnabled(true);
```

### Example 3: Check charging status
```c
uint8_t current = chargeGetCurrent();
if (current != 0xFF) {
    logPrintf("Currently charging slot %u\n", current);
    
    BatterySlot_t bat = chargeGetBatterySlot(current);
    logPrintf("Voltage: %.2fV, Capacity: %u%%\n", 
              bat.voltage, bat.percent);
}
```

---

## Troubleshooting

### Problem: Charging not working
**Check:**
1. Verify POGO_CTR_PIN are defined correctly in `pinout.h`
2. Confirm GPIO pins are toggling with multimeter (HIGH when charging, LOW otherwise)
3. Check `chargeTask` priority is adequate (priority 2)
4. Verify battery voltage is being read correctly by examining ADC task output

### Problem: Wrong slot selected for charging
**Check:**
1. Battery voltage thresholds (are batteries actually present?)
2. ADC readings are correct (compare with multimeter)
3. DIODE_DROP constant in `bat_handle.cpp` is calibrated properly
4. Verify candidate selection logic: lowest=first, second-lowest=second

### Problem: Switching too frequently
**Solution:**
- Increase `CHARGE_POLL_INTERVAL` to reduce checking frequency
- Increase `CHARGE_THRESHOLD` to 25 or 30%
- Add smoothing: average last N readings instead of single reading

---

## Design Notes

### Why Single-Slot Charging?
- Limited charger output capacity
- Safer voltage/current regulation
- Simpler hardware requirements
- Easier to implement fault detection

### Why 20% Threshold?
- Li-ion batteries are safer when not over-discharged
- 20% = ~3.2V for typical Li-ion, avoiding critical zone
- Balances between discharging flexibility and safety

### Why 2 Candidates?
- Allows planning: know which slot will charge next
- Prevents "hunting" (rapid switching between same 2 slots)
- Gives system time to stabilize
- Extensible to queue larger than 2 slots

---

## Future Enhancements

1. **Charging Profiles**: Different charging rates for different battery types
2. **Temperature Monitoring**: Stop charging if battery gets too hot
3. **Charge Counting**: Track total charge cycles per slot
4. **Load Balancing**: Distribute charge to equalize final capacity
5. **EEPROM Logging**: Save charging history to flash storage
6. **User Commands**: UART CLI to start/stop/configure charging

