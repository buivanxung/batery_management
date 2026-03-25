#pragma once
#include <Arduino.h>
#include <STM32FreeRTOS.h>

#define LOG_BUFFER_SIZE 64

typedef struct
{
    char message[LOG_BUFFER_SIZE];
} LogMessage_t;

void loggerInit();
void logPrint(const char *str);
void logPrintln(const char *str);
void logPrintf(const char *fmt, ...);