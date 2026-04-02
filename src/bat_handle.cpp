#include "bat_handle.h"
#include "bat_man.h"
#include "logger.h"

SemaphoreHandle_t adcMutex;

void muxInit()
{
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);

    pinMode(MUX_SIG, INPUT_ANALOG);

    logPrintln("muxInit initialized");
}

void muxSelect(uint8_t channel)
{
    digitalWrite(MUX_S0, channel & 0x01);
    digitalWrite(MUX_S1, (channel >> 1) & 0x01);
    digitalWrite(MUX_S2, (channel >> 2) & 0x01);
}

uint16_t muxRead(uint8_t channel)
{
    muxSelect(channel);

    delayMicroseconds(50); // increased: 5→50µs for better MUX settling + less switching noise

    return analogRead(MUX_SIG);
}

// ADC calibration constants
static constexpr float ADC_VREF = 3.3f;       // STM32 ADC reference voltage (V)
static constexpr float ADC_MAX_COUNTS = 4095.0f;

// Divider for battery to ADC input (example 100k + 68k)
// V_adc = V_bat * (R_bottom/(R_top+R_bottom))
// V_bat = V_adc * ((R_top+R_bottom)/R_bottom)
static constexpr float R_TOP = 100000.0f;
static constexpr float R_BOTTOM = 68000.0f;
static constexpr float BAT_MULT = (R_TOP + R_BOTTOM) / R_BOTTOM;

float adcValueToBatteryVoltage(uint16_t adcValue)
{
    float vAdc = (adcValue / ADC_MAX_COUNTS) * ADC_VREF;
    return vAdc * BAT_MULT;
}

void muxReadAll(uint16_t *values)
{
    xSemaphoreTake(adcMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < 8; i++)
    {
        values[i] = muxRead(i);
    }
    xSemaphoreGive(adcMutex);
}

void adcTask(void *pvParameters)
{
    uint16_t adcValues[8];

    logPrintln("adcTask started");

    while (1)
    {
        muxReadAll(adcValues);

        // logPrintln("ADC:");

        for (int i = 0; i < 8; i++)
        {
            float vBat = adcValueToBatteryVoltage(adcValues[i]);
            // logPrintf("CH%d raw=%u bat=%.3fV\n", i, adcValues[i], vBat);

            // if (vBat < BAT_UNDERVOLTAGE)
            //     logPrintln("WARNING: UNDER-VOLTAGE");
            // else if (vBat > BAT_OVERVOLTAGE)
            //     logPrintln("WARNING: OVER-VOLTAGE");
        }

        // logPrintln("----------------");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}