# Charging System - Quick Reference

## Summary

The charging management system automatically charges the 2 battery slots with the lowest capacity, one at a time. It intelligently switches slots if a battery suddenly drops below 20%.

---

## Main Features

1. ✅ **One Slot at a Time**: Cannot charge 2 slots simultaneously
2. ✅ **Auto Detection**: Detects battery presence via ADC voltage thresholds
3. ✅ **Smart Selection**: Charges the slot with lowest capacity first
4. ✅ **Dynamic Switching**: Switches to critical low batteries
5. ✅ **Real-time Monitoring**: Continuous voltage and percentage tracking

---

## Quick API

### Start/Stop
```c
chargeManagerSetEnabled(true);   // Enable auto charging
chargeManagerSetEnabled(false);  // Disable all charging
chargeStopAll();                 // Emergency stop
chargeStartSlot(3);              // Charge specific slot
```

### Read Status
```c
BatterySlot_t bat = chargeGetBatterySlot(2);  // Read slot 2
bool present = chargeIsBatteryPresent(4);     // Check if battery exists
uint8_t current = chargeGetCurrent();         // Which slot is charging? (0xFF = none)
bool charging = chargeIsCharging(1);          // Is slot 1 being charged?
```

### Find Candidates
```c
BatterySlot_t slots[8];
uint8_t candidates[2];
chargeGetCandidates(slots, candidates);
// candidates[0] = lowest capacity slot
// candidates[1] = second-lowest capacity slot
```

---

## Hardware Pins

**Control Pins** (Set HIGH to charge, LOW to stop):
- Slot 0: PC8 (POGO1_CTR_PIN)
- Slot 1: PC6 (POGO2_CTR_PIN)
- Slot 2: PD9 (POGO3_CTR_PIN)
- Slot 3: PC2 (POGO4_CTR_PIN)
- Slot 4: PC0 (POGO5_CTR_PIN)
- Slot 5: PB14 (POGO6_CTR_PIN)
- Slot 6: PC12 (POGO7_CTR_PIN)
- Slot 7: PC13 (POGO8_CTR_PIN)

**Voltage Sensing**:
- All slots read via 8-channel MUX on PA0
- Controlled by pins: PA11 (S0), PA12 (S1), PA15 (S2)

---

## Detection Logic

| Voltage Type | ADC Reading | Meaning |
|--------------|------------|---------|
| ≥ 3.5V | Battery voltage | Battery present |
| 3.0V - 3.5V | Low battery | Critical state |
| < 1.5V | Near 0V | No battery |

---

## Log Output

The system logs to UART at 115200 baud:

```
[CHARGE] Manager initialized
[CHARGE] ===== Charging Status =====
[CHARGE] Slot[0]: 4.20V 100% status=0 [CHARGING]
[CHARGE] Slot[1]: 3.85V  78% status=0
[CHARGE] Slot[2]: 3.20V  20% status=1
[CHARGE] Slot[3]: NO BATTERY
...
[CHARGE] Candidates: [0]=100% [1]=78%
[CHARGE] START charging slot 0
```

---

## Configuration

Edit in `bat_charge.h`:

```c
#define CHARGE_POLL_INTERVAL 1000   // Check every 1000ms
#define CHARGE_THRESHOLD     20     // Switch if drops below 20%
#define BAT_PRESENT_MIN_VOLTAGE  3.5f  // Threshold for detection
```

---

## Charging Algorithm (Simplified)

```
EVERY 1 SECOND:
  1. Read all 8 battery voltages
  2. Find 2 slots with lowest percentages
  3. IF not charging:
        Start charging the lowest
     ELSE IF current battery removed:
        Switch to lowest candidate
     ELSE IF another candidate < 20% AND < current:
        Switch to that candidate
```

---

## Example: Manual Control

```c
// Check slot 2
BatterySlot_t bat = chargeGetBatterySlot(2);
if (bat.isPresent) {
    printf("Slot 2: %.2fV at %d%%\n", bat.voltage, bat.percent);
}

// Force charge slot 5 for 10 seconds
chargeManagerSetEnabled(false);      // Disable auto
chargeStartSlot(5);
vTaskDelay(pdMS_TO_TICKS(10000));
chargeManagerSetEnabled(true);       // Resume auto
```

---

## Troubleshooting

| Problem | Check |
|---------|-------|
| Charging not starting | POGO_CTR_PIN definitions, GPIO output mode |
| Wrong slot charging | ADC readings (compare with multimeter) |
| Frequent switching | Increase CHARGE_POLL_INTERVAL or CHARGE_THRESHOLD |
| Battery not detected | ADC voltage ≥ 3.5V? DIODE_DROP calibrated? |

---

## Files

- **Header**: [src/bat_charge.h](src/bat_charge.h)
- **Implementation**: [src/bat_charge.cpp](src/bat_charge.cpp)
- **Full Docs**: [CHARGING_SYSTEM.md](CHARGING_SYSTEM.md)
- **Integration**: Added to [src/main.cpp](src/main.cpp) line ~257

