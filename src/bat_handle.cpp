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

    // Set 12-bit ADC resolution (0–4095) to match ADC_MAX_COUNTS constant
    // Default Arduino is 10-bit (0–1023) → wrong conversion!
    analogReadResolution(12);

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
static constexpr float ADC_VREF       = 3.3f;
static constexpr float ADC_MAX_COUNTS = 4095.0f;

// Circuit model: V_BAT → D5(1N4148) → R5(22Ω) → MUX → POINT_RET → PA0
//                                                         R65(10kΩ) pulls to +5V
//
// Because R_source (R5+MUX_Ron ≈ 150Ω) << R65 (10kΩ):
//   V_PA0 ≈ V_BAT - V_DIODE  (diode offset model, not multiplier)
//   V_BAT = V_PA0 + V_DIODE
//
// LIMITATION: V_PA0 max is 3.3V → V_BAT max readable = 3.3+0.65 = ~3.95V
//   Batteries above ~4.0V → ADC saturates at 4095 → flagged as BAT_STATUS_OVER_WARN
//
// Calibrate DIODE_DROP by:
//   1. Measure real battery voltage with multimeter
//   2. Read ADC value from serial log
//   3. DIODE_DROP = V_bat_multimeter - (adc_raw/4095.0 * 3.3)
static constexpr float DIODE_DROP   = 0.65f;   // 1N4148 forward voltage (V) - calibrate if needed
static constexpr float ADC_SATURATE = 4090.0f;  // treat as saturated above this

float adcValueToBatteryVoltage(uint16_t adcValue)
{
    if (adcValue >= (uint16_t)ADC_SATURATE)
        return 4.0f;  // Saturated: battery ≥ 4.0V, can't measure exact value

    float vAdc = (adcValue / ADC_MAX_COUNTS) * ADC_VREF;
    return vAdc + DIODE_DROP;
}

// Li-ion battery percentage: 3.0V = 0%, 4.2V = 100%
uint8_t batteryPercent(float voltage)
{
    if (voltage >= BAT_FULL)  return 100;
    if (voltage <= BAT_EMPTY) return 0;
    return (uint8_t)(((voltage - BAT_EMPTY) / (BAT_FULL - BAT_EMPTY)) * 100.0f);
}

BatStatus_t batteryStatus(float voltage)
{
    if (voltage >= 4.0f)  return BAT_STATUS_OVER_WARN;  // ADC saturated, can't measure
    if (voltage > BAT_OVERVOLTAGE)  return BAT_STATUS_OVER;
    if (voltage < BAT_UNDERVOLTAGE) return BAT_STATUS_CRITICAL;
    if (voltage < 3.2f)             return BAT_STATUS_LOW;
    return BAT_STATUS_OK;
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
            float vBat   = adcValueToBatteryVoltage(adcValues[i]);
            uint8_t pct  = batteryPercent(vBat);
            BatStatus_t st = batteryStatus(vBat);
            const char *stStr = (st == BAT_STATUS_OVER_WARN) ? ">4.0V(sat)" :
                                (st == BAT_STATUS_OVER)     ? "OVER" :
                                (st == BAT_STATUS_CRITICAL) ? "CRITICAL" :
                                (st == BAT_STATUS_LOW)      ? "LOW" : "OK";
            logPrintf("BAT[%d] raw=%4u  %.3fV  %3d%%  %s\n",
                      i, adcValues[i], vBat, pct, stStr);

            // if (vBat < BAT_UNDERVOLTAGE)
            //     logPrintln("WARNING: UNDER-VOLTAGE");
            // else if (vBat > BAT_OVERVOLTAGE)
            //     logPrintln("WARNING: OVER-VOLTAGE");
        }

        // logPrintln("----------------");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}