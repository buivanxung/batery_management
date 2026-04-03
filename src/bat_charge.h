#pragma once

#include <cstdint>
#include <STM32FreeRTOS.h>
#include "pinout.h"
#include "bat_handle.h"

/* ===== Charging Management Constants ===== */
#define NUM_BATTERY_SLOTS    8
#define CHARGE_THRESHOLD     20    // Switch charging if slot drops below 20%
#define CHARGE_DEFAULT_SLOT  0     // Default slot to start charging
#define CHARGE_POLL_INTERVAL 1000  // ms - Check battery levels

/* ===== Battery Slot Detection ===== */
#define BAT_PRESENT_MIN_VOLTAGE  3.5f   // Battery connected reads 4.5V through ADC
#define BAT_ABSENT_MAX_VOLTAGE   1.5f   // No battery reads 0.6V through ADC

/* ===== Charging Control Pin Mapping ===== */
static const uint8_t POGO_CTR_PINS[NUM_BATTERY_SLOTS] = {
    POGO1_CTR_PIN,  // Slot 0
    POGO2_CTR_PIN,  // Slot 1
    POGO3_CTR_PIN,  // Slot 2
    POGO4_CTR_PIN,  // Slot 3
    POGO5_CTR_PIN,  // Slot 4
    POGO6_CTR_PIN,  // Slot 5
    POGO7_CTR_PIN,  // Slot 6
    POGO8_CTR_PIN,  // Slot 7
};

/* ===== Battery State Structure ===== */
typedef struct {
    uint8_t  slot;           // Slot number (0-7)
    float    voltage;        // Current voltage reading
    uint8_t  percent;        // Battery percentage (0-100%)
    bool     isPresent;      // Battery connected or not
    BatStatus_t status;      // Current status (OK, LOW, CRITICAL, etc.)
} BatterySlot_t;

/* ===== Charging Manager Structure ===== */
typedef struct {
    uint8_t  currentCharging;   // Currently charging slot (0-7), or 0xFF if none
    uint8_t  candidates[2];     // Two slots with smallest capacity to charge
    uint16_t pollCounter;       // Internal timing counter
    bool     isEnabled;         // Charging manager enabled/disabled
} ChargeManager_t;

/* ===== Function Declarations ===== */

/**
 * @brief Initialize charging manager and pins
 */
void chargeManagerInit();

/**
 * @brief Enable/disable charging management
 */
void chargeManagerSetEnabled(bool enabled);

/**
 * @brief Read battery level from a slot (with voltage detection)
 * @param slot Slot number (0-7)
 * @return BatterySlot_t with voltage, percent, and presence
 */
BatterySlot_t chargeGetBatterySlot(uint8_t slot);

/**
 * @brief Check if battery is present in slot
 * Returns true if ADC reading indicates presence (reading > 3.5V)
 */
bool chargeIsBatteryPresent(uint8_t slot);

/**
 * @brief Get all battery levels and find 2 with lowest capacity
 * @param slots Array of 8 BatterySlot_t to fill with readings
 * @return Array of 2 slot indices with smallest capacity (present batteries only)
 */
void chargeGetCandidates(BatterySlot_t *slots, uint8_t *candidates);

/**
 * @brief Start charging a specific slot
 * @param slot Slot number (0-7)
 */
void chargeStartSlot(uint8_t slot);

/**
 * @brief Stop all charging
 */
void chargeStopAll();

/**
 * @brief Get current charging slot
 * @return Slot number (0-7) or 0xFF if no charging
 */
uint8_t chargeGetCurrent();

/**
 * @brief Check if slot is currently charging
 */
bool chargeIsCharging(uint8_t slot);

/**
 * @brief Main charging manager task
 * - Reads all battery levels
 * - Selects 2 candidates with smallest capacity
 * - Starts charging the lowest
 * - Switches if another drops below threshold
 * - Not allowed to charge 2 slots simultaneously
 */
void chargeTask(void *pvParameters);
