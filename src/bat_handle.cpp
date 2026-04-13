#include "bat_handle.h"
#include "bat_man.h"
#include "logger.h"
#include "stm8_comm.h"

SemaphoreHandle_t adcMutex;

void muxInit()
{
    // Initialize 74HC4051 mux and single-wire UART signal line via stm8_comm
    stm8CommInit();
    logPrintln("muxInit: STM8 single-wire comm initialized");
}

void muxSelect(uint8_t channel)
{
    digitalWrite(MUX_S0,  channel & 0x01);
    digitalWrite(MUX_S1, (channel >> 1) & 0x01);
    digitalWrite(MUX_S2, (channel >> 2) & 0x01);
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