#include <Arduino.h>
#include <STM32FreeRTOS.h>

typedef enum
{
  CMD_LED_ON,
  CMD_LED_OFF,
  CMD_LED_TOGGLE,
  CMD_PRINT,
} CommandType;

typedef struct
{
  CommandType cmd;
  uint32_t param;
} Message_t;

QueueHandle_t queueCmd;

// Task handle (optional)
TaskHandle_t ledTaskHandle;
TaskHandle_t commTaskHandle;

void ledTask(void *pvParameters)
{
  Message_t msg;

   pinMode(PF0, OUTPUT);

  while (1)
  {
    if (xQueueReceive(queueCmd, &msg, portMAX_DELAY) == pdPASS)
    {
      switch (msg.cmd)
      {
      case CMD_LED_ON:
        digitalWrite(PF0, HIGH);
        break;

      case CMD_LED_OFF:
        digitalWrite(PF0, LOW);
        break;

      case CMD_LED_TOGGLE:
        digitalWrite(PF0, !digitalRead(PF0));
        break;

      default:
        break;
      }
    }
    vTaskDelay(100);
  }
}
void commTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    if (xQueueReceive(queueCmd, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_PRINT)
      {
        Serial.println("PRINT CMD received");
      }
    }
    vTaskDelay(100);
  }
}

void mainTask(void *pvParameters)
{
  Message_t msg;
  pinMode(PF0, OUTPUT);
  while (1)
  {
    // Ví dụ gửi lệnh toggle LED
    // msg.cmd = CMD_LED_TOGGLE;
    // xQueueSend(queueCmd, &msg, portMAX_DELAY);

     digitalWrite(PF0, HIGH);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // // gửi lệnh print
    // msg.cmd = CMD_PRINT;
    // xQueueSend(queueCmd, &msg, portMAX_DELAY);
     digitalWrite(PF0, LOW);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup()
{
   __HAL_RCC_GPIOF_CLK_ENABLE();
  Serial.begin(115200);

  // Tạo queue (10 message)
  queueCmd = xQueueCreate(10, sizeof(Message_t));

  // Tạo task
  xTaskCreate(mainTask, "MAIN", 256, NULL, 2, NULL);
  xTaskCreate(ledTask, "LED", 256, NULL, 1, &ledTaskHandle);
  xTaskCreate(commTask, "COMM", 256, NULL, 1, &commTaskHandle);

  // Start scheduler
  vTaskStartScheduler();
}

void loop()
{
}