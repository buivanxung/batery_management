#include "bat_charge.h"
#include "stm8_comm.h"
#include "logger.h"

// Debug slot 4 specifically
#define DEBUG_SLOT4 1

#if DEBUG_SLOT4
  #define LOG_SLOT4(fmt, ...) logPrintf("[SLOT4] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_SLOT4(fmt, ...) do {} while(0)
#endif

/* ===== POGO_CTR Pin Table ===== */
const uint8_t POGO_CTR_PINS[NUM_BATTERY_SLOTS] = {
    POGO1_CTR_PIN,  // Slot 0
    POGO2_CTR_PIN,  // Slot 1
    POGO3_CTR_PIN,  // Slot 2
    POGO4_CTR_PIN,  // Slot 3
    POGO5_CTR_PIN,  // Slot 4
    POGO6_CTR_PIN,  // Slot 5
    POGO7_CTR_PIN,  // Slot 6
    POGO8_CTR_PIN,  // Slot 7
};

/* ===== Static Variables ===== */
static ChargeManager_t chargeManager = {
    .currentCharging = 0xFF,
    .isEnabled = false
};

/* ===== Helper Functions ===== */

static void chargeEnablePin(uint8_t slot)
{
    if (slot >= MOTOR_COUNT) return;
    // 1. Enable POGO hardware path via direct GPIO
    digitalWrite(POGO_CTR_PINS[slot], HIGH);
    // 2. Tell STM8 to keep its PIN_PW_KEY ON for N minutes via UART
    stm8SetPwTimer(slot, CHARGE_TIMER_MINUTES);
    if (slot == 4) {
        LOG_SLOT4("enablePin: GPIO HIGH + PWTimer %u minutes", CHARGE_TIMER_MINUTES);
    }
}

static void chargeDisablePin(uint8_t slot)
{
    if (slot >= MOTOR_COUNT) return;
    // 1. Cancel timer on STM8 (turns PIN_PW_KEY OFF)
    stm8SetPwTimer(slot, 0);
    // 2. Disable POGO hardware path
    digitalWrite(POGO_CTR_PINS[slot], LOW);
    if (slot == 4) {
        LOG_SLOT4("disablePin: GPIO LOW + PWTimer cancelled");
    }
}

static void chargeInitPins()
{
    // Init POGO_CTR GPIO pins (direct STM32 outputs, idle HIGH = enabled)
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        pinMode(POGO_CTR_PINS[i], OUTPUT);
        digitalWrite(POGO_CTR_PINS[i], LOW);  // disabled until charging starts
    }
    // Init single-wire UART communication with STM8 boards via 74HC4051 mux
    stm8CommInit();
    LOG_SLOT4("GPIO pin initialized");
    logPrintln("[CHARGE] POGO GPIO + STM8 UART comm initialized");
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

    if (slot >= MOTOR_COUNT)
    {
        bat.voltage = 0.0f;
        bat.percent = 0;
        bat.isPresent = false;
        bat.status = BAT_STATUS_ABSENT;
        return bat;
    }

    // Query battery status from the STM8 via single-wire UART over 74HC4051 mux
    Stm8Status_t s = stm8GetStatus(slot);

    if (slot == 4) {
        LOG_SLOT4("Query result: ok=%d, batMv=%u, pwLocked=%d, errCode=%d", 
                  s.ok, s.batMv, s.pwLocked, s.errCode);
    }

    if (!s.ok)
    {
        if (slot == 9) {
            LOG_SLOT4("FAILED to query");

            // Debug aid: probe adjacent channels to detect physical/index mismatch.
            Stm8Status_t s3 = stm8GetStatus(3);
            LOG_SLOT4("Probe slot 3: ok=%d, batMv=%u, pwLocked=%d, errCode=%d",
                      s3.ok, s3.batMv, s3.pwLocked, s3.errCode);

            Stm8Status_t s5 = stm8GetStatus(5);
            LOG_SLOT4("Probe slot 5: ok=%d, batMv=%u, pwLocked=%d, errCode=%d",
                      s5.ok, s5.batMv, s5.pwLocked, s5.errCode);

            if (s3.ok && !s5.ok) {
                LOG_SLOT4("Hint: battery may be on index slot 3 (human slot #4)");
            } else if (s5.ok && !s3.ok) {
                LOG_SLOT4("Hint: battery may be on index slot 5");
            }
        }
        bat.voltage = 0.0f;
        bat.percent = 0;
        bat.isPresent = false;
        bat.status = BAT_STATUS_ABSENT;
        return bat;
    }

    bat.voltage   = s.batMv / 1000.0f;
    bat.isPresent = (s.batMv >= (uint16_t)(BAT_PRESENT_MIN_VOLTAGE * 1000.0f));
    bat.status    = batteryStatus(bat.voltage);
    // Xác định đang sạc dựa vào chargeManager.currentCharging
    bool isCharging = (chargeManager.currentCharging == slot);
    bat.percent   = bat.isPresent ? batteryPercentWithCharging(bat.voltage, isCharging) : 0;
    
    if (slot == 4) {
        LOG_SLOT4("Status: voltage=%.2fV, percent=%u, present=%d, status=%d", 
                  bat.voltage, bat.percent, bat.isPresent, bat.status);
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
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        logPrintf("[SCAN] STM32 scanning slot %u\n", i + 1);
        slots[i] = chargeGetBatterySlot(i);
    }

    // Ưu tiên chọn các slot có phần trăm 0% trước
    uint8_t zeroSlots[2] = {0xFF, 0xFF};
    uint8_t zeroCount = 0;
    for (uint8_t i = 0; i < MOTOR_COUNT && zeroCount < 2; i++) {
        if (slots[i].isPresent && slots[i].percent == 0) {
            zeroSlots[zeroCount++] = i;
        }
    }
    if (zeroCount == 2) {
        candidates[0] = zeroSlots[0];
        candidates[1] = zeroSlots[1];
        return;
    } else if (zeroCount == 1) {
        candidates[0] = zeroSlots[0];
        // Tìm slot nhỏ tiếp theo (khác slot 0%)
        uint8_t minSlot = 0xFF;
        uint8_t minPercent = 101;
        for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
            if (!slots[i].isPresent || i == zeroSlots[0]) continue;
            if (slots[i].percent < minPercent) {
                minPercent = slots[i].percent;
                minSlot = i;
            }
        }
        candidates[1] = minSlot;
        return;
    }
    // Nếu không có slot nào 0%, chọn 2 slot phần trăm thấp nhất như cũ
    uint8_t minSlot[2] = {0xFF, 0xFF};
    uint8_t minPercent[2] = {101, 101};
    for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
        if (!slots[i].isPresent) continue;
        uint8_t pct = slots[i].percent;
        if (pct < minPercent[0]) {
            minPercent[1] = minPercent[0];
            minSlot[1] = minSlot[0];
            minPercent[0] = pct;
            minSlot[0] = i;
        } else if (pct < minPercent[1]) {
            minPercent[1] = pct;
            minSlot[1] = i;
        }
    }
    candidates[0] = minSlot[0];
    candidates[1] = minSlot[1];
}

void chargeStartSlot(uint8_t slot)
{
    if (slot >= MOTOR_COUNT)
        return;

    // Stop current charging
    if (chargeManager.currentCharging != 0xFF)
    {
        if (slot == 4) {
            LOG_SLOT4("Previous slot %u stopped", chargeManager.currentCharging);
        }
        chargeDisablePin(chargeManager.currentCharging);
    }

    // Start new slot
    chargeEnablePin(slot);
    chargeManager.currentCharging = slot;
    logPrintf("[CHARGE] START charging slot %u\n", slot + 1);
    if (slot == 4) {
        LOG_SLOT4("CHARGING START - PIN enabled");
    }
}

void chargeStopAll()
{
    if (chargeManager.currentCharging != 0xFF)
    {
        if (chargeManager.currentCharging == 4) {
            LOG_SLOT4("CHARGING STOP");
        }
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
    BatterySlot_t slots[NUM_BATTERY_SLOTS];
    uint8_t candidates[2];

    logPrintln("[CHARGE] Task started");
    chargeManagerInit();
    chargeManagerSetEnabled(true);

    uint8_t scanSlot = 0;
    while (1)
    {
        // --- To keep original logic, continue below ---
        if (!chargeManager.isEnabled)
        {
            vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
            continue;
        }

        // Read all battery levels via UART from each STM8
        chargeGetCandidates(slots, candidates);

        logPrintln("[CHARGE] ===== Charging Status =====");
        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            if (slots[i].isPresent)
            {
                logPrintf("[CHARGE] Slot[%u]: %.2fV %3u%% %s\n",
                          i + 1, slots[i].voltage, slots[i].percent,
                          chargeIsCharging(i) ? "[CHARGING]" : "");
            }
            else
            {
                logPrintf("[CHARGE] Slot[%u]: NO BATTERY\n", i + 1);
            }
        }

        // Luôn bật sạc cho 2 slot có phần trăm thấp nhất (nếu có pin)
        if (candidates[0] != 0xFF && slots[candidates[0]].isPresent)
            chargeStartSlot(candidates[0]);
        if (candidates[1] != 0xFF && slots[candidates[1]].isPresent)
            chargeStartSlot(candidates[1]);

        vTaskDelay(pdMS_TO_TICKS(CHARGE_POLL_INTERVAL));
    }
}
