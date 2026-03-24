#pragma once

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#define LOG_BUFFER_SIZE 128

typedef struct
{
    char message[LOG_BUFFER_SIZE];
} LogMessage_t;

extern QueueHandle_t logQueue;

void loggerInit();
void logPrint(const char *str);
void logPrint(const __FlashStringHelper *str);
void logPrintln(const char *str);
void logPrintln(const __FlashStringHelper *str);
void logPrint(int num);
void logPrintln(int num);