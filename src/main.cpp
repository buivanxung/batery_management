#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <SPI.h>
#include <SPIMemory.h>

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

QueueHandle_t queueLed;
QueueHandle_t queueComm;

SPIFlash *flash;
HardwareSerial Serial1(PA10, PA9);

// Task handle (optional)
TaskHandle_t ledTaskHandle;
TaskHandle_t commTaskHandle;

void ledTask(void *pvParameters)
{
  Message_t msg;

  pinMode(PF0, OUTPUT);
  pinMode(PB15, OUTPUT);

  while (1)
  {
    if (xQueueReceive(queueLed, &msg, portMAX_DELAY) == pdPASS)
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
    if (xQueueReceive(queueComm, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_PRINT)
      {
        Serial1.println("PRINT CMD received");
      }
    }
    vTaskDelay(100);
  }
}

void mainTask(void *pvParameters)
{
  Message_t msg;
  while (1)
  {
    // Ví dụ gửi lệnh toggle LED
    msg.cmd = CMD_LED_TOGGLE;
    xQueueSend(queueLed, &msg, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(1000));

    msg.cmd = CMD_LED_TOGGLE;
    xQueueSend(queueLed, &msg, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup()
{
  __HAL_RCC_GPIOF_CLK_ENABLE();
  Serial1.begin(115200);
  SPI.begin();
  flash = new SPIFlash(PA8);
  flash->begin();
  queueLed = xQueueCreate(10, sizeof(Message_t));
  queueComm = xQueueCreate(10, sizeof(Message_t));
  pinMode(PB15, OUTPUT);
  digitalWrite(PB15, 1);
  // Tạo task
  xTaskCreate(mainTask, "MAIN", 256, NULL, 2, NULL);
  xTaskCreate(ledTask, "LED", 256, NULL, 1, &ledTaskHandle);
  xTaskCreate(commTask, "COMM", 256, NULL, 1, &commTaskHandle);

  // Start scheduler
  vTaskStartScheduler();
}

uint32_t addr = 0;

void loop()
{
  vTaskDelay(portMAX_DELAY);
  if (Serial1.available())
  {
    uint8_t buf[256];
    int len = Serial1.readBytes(buf, sizeof(buf));

    flash->writeByteArray(addr, buf, len);
    addr += len;

    Serial1.write("OK", 2);
  }
}