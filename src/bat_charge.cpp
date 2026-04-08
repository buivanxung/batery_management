#include "bat_charge.h"
#include "logger.h"

/* ===== Static Variables ===== */
static ChargeManager_t chargeManager = {
    .currentCharging = 0xFF,  // No charging initially
    .isEnabled = false
};

static uint16_t chargeAdcValues[8];

static void chargeSetAllPinsHigh()
{
    for (uint8_t i = 0; i < NUM_BATTERY_SLOTS; i++)
    {
        digitalWrite(POGO_CTR_PINS[i], HIGH);
    }
}

/* ===== Helper Functions ===== */

/**
 * @brief Enable charging control pin for a slot
 */
static void chargeEnablePin(uint8_t slot)
{
    if (slot >= NUM_BATTERY_SLOTS) return;
    digitalWrite(POGO_CTR_PINS[slot], HIGH);
}

/**
 * @brief Disable charging control pin for a slot
 */
static void chargeDisablePin(uint8_t slot)
{
    if (slot >= NUM_BATTERY_SLOTS) return;
    digitalWrite(POGO_CTR_PINS[slot], LOW);
}

/**
 * @brief Initialize all charging control pins as outputs
 */
static void chargeInitPins()
{
    for (uint8_t i = 0; i < NUM_BATTERY_SLOTS; i++)
    {
        pinMode(POGO_CTR_PINS[i], OUTPUT);
        digitalWrite(POGO_CTR_PINS[i], HIGH);
    }
    logPrintln("[CHARGE] Pins initialized -> all MCU_CTR HIGH");
}

/* ===== Public Functions ===== */

void chargeManagerInit()
{
    chargeInitPins();
    chargeManager.currentCharging = 0xFF;
    chargeManager.isEnabled = false;
    chargeManager.pollCounter = 0;
    logPrintln("[CHARGE] Manager initialized");
}

void chargeManagerSetEnabled(bool enabled)
{
    chargeManager.isEnabled = enabled;
    if (!enabled)
    {
        chargeStopAll();
    }
    logPrintf("[CHARGE] Manager %s\n", enabled ? "ENABLED" : "DISABLED");
}

BatterySlot_t chargeGetBatterySlot(uint8_t slot)
{
    BatterySlot_t bat;
    bat.slot = slot;
    
    if (slot >= NUM_BATTERY_SLOTS)
    {
        bat.voltage = 0.0f;
        bat.percent = 0;
        bat.isPresent = false;
        bat.status = BAT_STATUS_ABSENT;
        return bat;
    }

    // Get ADC value via multiplexer
    xSemaphoreTake(adcMutex, portMAX_DELAY);
    uint16_t adcValue = muxRead(slot);
    xSemaphoreGive(adcMutex);

    // Convert to voltage
    bat.voltage = adcValueToBatteryVoltage(adcValue);
    bat.status = batteryStatus(bat.voltage);
    
    // Detect presence based on voltage reading
    // Battery present: ~4.5V (from the schematic, battery connected reads 4.5V)
    // No battery: ~0.6V
    bat.isPresent = (bat.voltage >= BAT_PRESENT_MIN_VOLTAGE);
    
    if (bat.isPresent)
    {
        bat.percent = batteryPercent(bat.voltage);
    }
    else
    {
        bat.percent = 0;
    }
    
    return bat;
}

bool chargeIsBatteryPresent(uint8_t slot)
{
    BatterySlot_t bat = chargeGetBatterySlot(slot);
    return bat.isPresent;
}

void chargeGetCandidates(BatterySlot_t *slots, uint8_t *candidates)
{
    // Read all 8 slots
    for (uint8_t i = 0; i < NUM_BATTERY_SLOTS; i++)
    {
        slots[i] = chargeGetBatterySlot(i);
    }

    // Find 2 slots with smallest capacity (only those with batteries present)
    uint8_t minSlot[2] = {0xFF, 0xFF};
    uint8_t minPercent[2] = {101, 101};

    for (uint8_t i = 0; i < NUM_BATTERY_SLOTS; i++)
    {
        if (!slots[i].isPresent)
            continue;  // Skip slots without batteries

        uint8_t pct = slots[i].percent;

        // Update two lowest percentages
        if (pct < minPercent[0])
        {
            // Shift first to second
            minPercent[1] = minPercent[0];
            minSlot[1] = minSlot[0];
            // Insert new lowest
            minPercent[0] = pct;
            minSlot[0] = i;
        }
        else if (pct < minPercent[1])
        {
            minPercent[1] = pct;
            minSlot[1] = i;
        }
    }

    candidates[0] = minSlot[0];
    candidates[1] = minSlot[1];
}

void chargeStartSlot(uint8_t slot)
{
    if (slot >= NUM_BATTERY_SLOTS)
        return;

    // Stop current charging
    if (chargeManager.currentCharging != 0xFF)
    {
        chargeDisablePin(chargeManager.currentCharging);
    }

    // Start new slot
    chargeEnablePin(slot);
    chargeManager.currentCharging = slot;
    logPrintf("[CHARGE] START charging slot %u\n", slot);
}

void chargeStopAll()
{
    if (chargeManager.currentCharging != 0xFF)
    {
        chargeDisablePin(chargeManager.currentCharging);
        logPrintf("[CHARGE] STOP charging slot %u\n", chargeManager.currentCharging);
    }
    chargeManager.currentCharging = 0xFF;
}

uint8_t chargeGetCurrent()
{
    return chargeManager.currentCharging;
}

bool chargeIsCharging(uint8_t slot)
{
    return (chargeManager.currentCharging == slot);
}

/* ===== Main Charging Task ===== */

void chargeTask(void *pvParameters)
{
    // User request: keep all MCU_CTR pins HIGH continuously.
    // Disable charge manager auto-switching logic because it toggles these pins.
    chargeManagerInit();
    chargeManagerSetEnabled(false);
    chargeSetAllPinsHigh();
    logPrintln("[CHARGE] FORCE mode: all MCU_CTR pins are HIGH");

    while (1)
    {
        chargeSetAllPinsHigh();
        vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
    }

#if 0
    BatterySlot_t slots[NUM_BATTERY_SLOTS];
    uint8_t candidates[2];

    logPrintln("[CHARGE] Task started");

    // Initialize charging system
    chargeManagerInit();
    chargeManagerSetEnabled(true);

    while (1)
    {
        if (!chargeManager.isEnabled)
        {
            vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
            continue;
        }

        // Read all battery levels
        chargeGetCandidates(slots, candidates);

        logPrintln("[CHARGE] ===== Charging Status =====");
        for (uint8_t i = 0; i < NUM_BATTERY_SLOTS; i++)
        {
            logPrintf("[CHARGE] Slot[%u]: ", i);
            if (slots[i].isPresent)
            {
                logPrintf("%.2fV %3u%% status=%u %s\n",
                          slots[i].voltage, slots[i].percent, slots[i].status,
                          chargeIsCharging(i) ? "[CHARGING]" : "");
            }
            else
            {
                logPrintln("NO BATTERY");
            }
        }

        logPrintf("[CHARGE] Candidates: [%u]=%u%% [%u]=%u%%\n",
                  candidates[0], (candidates[0] != 0xFF) ? slots[candidates[0]].percent : 0,
                  candidates[1], (candidates[1] != 0xFF) ? slots[candidates[1]].percent : 0);

        /* ===== Charging Logic ===== */

        // If no candidates, stop charging
        if (candidates[0] == 0xFF)
        {
            chargeStopAll();
            logPrintln("[CHARGE] No batteries present - charging stopped");
            vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
            continue;
        }

        // If not currently charging, start with lowest capacity
        if (chargeManager.currentCharging == 0xFF)
        {
            chargeStartSlot(candidates[0]);
        }
        else
        {
            // Currently charging - check if we should switch
            uint8_t currentSlot = chargeManager.currentCharging;
            uint8_t currentPercent = slots[currentSlot].percent;

            // Check if current slot is still valid (has battery)
            if (!slots[currentSlot].isPresent)
            {
                logPrintf("[CHARGE] Slot %u battery removed - switching\n", currentSlot);
                chargeStartSlot(candidates[0]);
            }
            // Check if another candidate dropped below threshold while current is being charged
            else if (candidates[1] != 0xFF)
            {
                uint8_t otherCandidate = (candidates[0] == currentSlot) ? candidates[1] : candidates[0];
                uint8_t otherPercent = slots[otherCandidate].percent;

                if (otherPercent < CHARGE_THRESHOLD && otherPercent < currentPercent)
                {
                    logPrintf("[CHARGE] Slot %u dropped to %u%% - switching from slot %u\n",
                              otherCandidate, otherPercent, currentSlot);
                    chargeStartSlot(otherCandidate);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
    }
#endif
}
