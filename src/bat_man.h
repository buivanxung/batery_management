#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <SPI.h>
#include <SPIMemory.h>

/* ===================== CONFIG ===================== */
#define UART_BAUD 115200  // Changed back to 115200 for compatibility
#define CMD_BUFFER_SIZE 64

/* ===================== TYPES ===================== */
typedef enum
{
    CMD_LED_ON,
    CMD_LED_OFF,
    CMD_LED_TOGGLE,
    CMD_PLAY_AUDIO,
    CMD_MOTOR_SET,
} CommandType;

typedef struct
{
    CommandType cmd;
    uint32_t param;
    char name[16];
    bool on;
} Message_t;

void safeQueueSend(QueueHandle_t q, Message_t *msg);
void commTask(void *pvParameters);
void cli_set_command(QueueHandle_t n_queueLed,
                     QueueHandle_t n_queueAudio,
                     QueueHandle_t n_queueMotor, SPIFlash *n_flas);