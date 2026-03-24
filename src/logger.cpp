#include "logger.h"

QueueHandle_t logQueue;

void loggerTask(void *pvParameters)
{
    LogMessage_t msg;

    while (1)
    {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            Serial1.print(msg.message);
        }
    }
}

void loggerInit()
{
    logQueue = xQueueCreate(10, sizeof(LogMessage_t));
    if (!logQueue)
    {
        // Error, but can't log yet
        while (1)
            ;
    }

    xTaskCreate(loggerTask, "LOGGER", 256, NULL, 1, NULL);
}

void logPrint(const char *str)
{
    LogMessage_t msg;
    strncpy(msg.message, str, LOG_BUFFER_SIZE - 1);
    msg.message[LOG_BUFFER_SIZE - 1] = '\0';

    xQueueSend(logQueue, &msg, 0);
}

void logPrint(const __FlashStringHelper *str)
{
    String s = String(str);
    logPrint(s.c_str());
}

void logPrintln(const char *str)
{
    LogMessage_t msg;
    size_t len = strlen(str);
    if (len < LOG_BUFFER_SIZE - 2)
    {
        strcpy(msg.message, str);
        strcat(msg.message, "\n");
    }
    else
    {
        strncpy(msg.message, str, LOG_BUFFER_SIZE - 2);
        msg.message[LOG_BUFFER_SIZE - 2] = '\0';
        strcat(msg.message, "\n");
    }

    xQueueSend(logQueue, &msg, 0);
}

void logPrintln(const __FlashStringHelper *str)
{
    String s = String(str);
    logPrintln(s.c_str());
}

void logPrint(int num)
{
    char buf[16];
    itoa(num, buf, 10);
    logPrint(buf);
}

void logPrintln(int num)
{
    char buf[16];
    itoa(num, buf, 10);
    logPrintln(buf);
}