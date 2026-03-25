#include "logger.h"
#include <stdarg.h>

QueueHandle_t logQueue = NULL;

/* ===================== TASK ===================== */
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

/* ===================== INIT ===================== */
void loggerInit()
{
    logQueue = xQueueCreate(20, sizeof(LogMessage_t));

    if (!logQueue)
    {
        while (1)
            ;
    }

    xTaskCreate(loggerTask, "LOGGER", 512, NULL, 1, NULL);
}

/* ===================== CORE ===================== */
static void logSend(const char *str)
{
    if (!logQueue)
        return;

    LogMessage_t msg;

    strncpy(msg.message, str, LOG_BUFFER_SIZE - 1);
    msg.message[LOG_BUFFER_SIZE - 1] = '\0';

    if (xQueueSend(logQueue, &msg, 0) != pdPASS)
    {
        // Queue full → fallback trực tiếp (tránh mất log)
        Serial1.print("LOG DROP: ");
        Serial1.println(msg.message);
    }
}

/* ===================== API ===================== */
void logPrint(const char *str)
{
    logSend(str);
}

void logPrintln(const char *str)
{
    char buf[LOG_BUFFER_SIZE];

    snprintf(buf, sizeof(buf), "%s\r\n", str);
    logSend(buf);
}

void logPrintf(const char *fmt, ...)
{
    char buf[LOG_BUFFER_SIZE];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    logSend(buf);
}