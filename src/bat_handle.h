#include <cstdint>
#include <STM32FreeRTOS.h>
#include "pinout.h"

extern SemaphoreHandle_t adcMutex;

static constexpr float BAT_UNDERVOLTAGE = 3.2f;
static constexpr float BAT_OVERVOLTAGE = 4.3f;

void muxInit();
void muxSelect(uint8_t channel);
uint16_t muxRead(uint8_t channel);
void muxReadAll(uint16_t *values);
float adcValueToBatteryVoltage(uint16_t adcValue);
void adcTask(void *pvParameters);