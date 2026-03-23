#include "bat_handle.h"
#include "bat_man.h"

void muxInit()
{
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);

    pinMode(MUX_SIG, INPUT_ANALOG);
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

    delayMicroseconds(5); // ⚠️ rất quan trọng

    return analogRead(MUX_SIG);
}

void muxReadAll(uint16_t *values)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        values[i] = muxRead(i);
    }
}

void adcTask(void *pvParameters)
{
    uint16_t adcValues[8];

    while (1)
    {
        muxReadAll(adcValues);

        Serial1.println("ADC:");

        for (int i = 0; i < 8; i++)
        {
            Serial1.print("CH");
            Serial1.print(i);
            Serial1.print(": ");
            Serial1.println(adcValues[i]);
        }

        Serial1.println("----------------");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}