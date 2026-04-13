#pragma once

#include <cstdint>
#include <STM32FreeRTOS.h>
#include "pinout.h"

extern SemaphoreHandle_t adcMutex;

// Li-ion battery voltage thresholds
static constexpr float BAT_FULL        = 4.20f;  // 100%
static constexpr float BAT_OVERVOLTAGE = 4.30f;  // danger
static constexpr float BAT_EMPTY       = 3.00f;  // 0%
static constexpr float BAT_UNDERVOLTAGE= 2.90f;  // danger (cutoff)

typedef enum {
    BAT_STATUS_OK        = 0,
    BAT_STATUS_LOW       = 1,  // < 3.2V (20%)
    BAT_STATUS_CRITICAL  = 2,  // < 3.0V
    BAT_STATUS_OVER      = 3,  // > 4.3V
    BAT_STATUS_OVER_WARN = 4,  // ADC saturated (≥4.0V) - cannot measure exact value
    BAT_STATUS_ABSENT    = 5,  // No battery / disconnected
} BatStatus_t;

void muxInit();
void muxSelect(uint8_t channel);
uint8_t batteryPercent(float voltage);
BatStatus_t batteryStatus(float voltage);