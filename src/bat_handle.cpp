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


// Tính phần trăm pin theo trạng thái sạc/thực tế đo
uint8_t batteryPercentWithCharging(float voltage, bool isCharging)
{
    if (isCharging) {
        // Đang sạc: 3857mV = 0%, 4238mV = 100%
        const float vEmpty = 3.857f;
        const float vFull = 4.238f;
        if (voltage >= vFull) return 100;
        if (voltage <= vEmpty) return 0;
        return (uint8_t)(((voltage - vEmpty) / (vFull - vEmpty)) * 100.0f);
    } else {
        // Không sạc: nếu đo trong khoảng 5.39–5.44V thì luôn trả về 0% (pin cạn) hoặc 100% (pin đầy)
        if (voltage >= 5.43f) return 0; // Pin cạn (theo thực tế đo)
        if (voltage <= 5.40f) return 100; // Pin đầy (theo thực tế đo)
        // Ngoài ra fallback về 0
        return 0;
    }
}

BatStatus_t batteryStatus(float voltage)
{
    if (voltage >= 4.0f)  return BAT_STATUS_OVER_WARN;  // ADC saturated, can't measure
    if (voltage > BAT_OVERVOLTAGE)  return BAT_STATUS_OVER;
    if (voltage < BAT_UNDERVOLTAGE) return BAT_STATUS_CRITICAL;
    if (voltage < 3.2f)             return BAT_STATUS_LOW;
    return BAT_STATUS_OK;
}