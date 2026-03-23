#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include "pinout.h"
#include "motor_control.h"
#include "flash_files.h"
#include "audio_dac.h"
#include "bat_man.h"

/* ===================== GLOBAL ===================== */
QueueHandle_t queueLed;
QueueHandle_t queueAudio;
QueueHandle_t queueMotor;

HardwareSerial Serial1(SERIAL1_RX_PIN, SERIAL1_TX_PIN);
SPIFlash flash(FLASH_SPI_CS_PIN, &SPI);

/* ===================== UTILS ===================== */
void safeQueueSend(QueueHandle_t q, Message_t *msg)
{
  if (xQueueSend(q, msg, 0) != pdPASS)
  {
    Serial1.println(F("Queue FULL!"));
  }
}

/* ===================== TASK: LED ===================== */
void ledTask(void *pvParameters)
{
  Message_t msg;

  pinMode(LED_STATUS_PIN, OUTPUT);

  while (1)
  {
    if (xQueueReceive(queueLed, &msg, portMAX_DELAY) == pdPASS)
    {
      switch (msg.cmd)
      {
      case CMD_LED_ON:
        digitalWrite(LED_STATUS_PIN, HIGH);
        break;
      case CMD_LED_OFF:
        digitalWrite(LED_STATUS_PIN, LOW);
        break;
      case CMD_LED_TOGGLE:
        digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN));
        break;
      default:
        break;
      }
    }
  }
}

/* ===================== TASK: MOTOR ===================== */
void motorTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    if (xQueueReceive(queueMotor, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_MOTOR_SET)
      {
        motorSet((MotorId)msg.param, msg.on);

        Serial1.print(F("Motor "));
        Serial1.print(msg.param);
        Serial1.println(msg.on ? F(" ON") : F(" OFF"));
      }
    }
  }
}

/* ===================== TASK: AUDIO ===================== */
void audioTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    if (xQueueReceive(queueAudio, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_PLAY_AUDIO)
      {
        Serial1.print(F("Play: "));
        Serial1.println(msg.name);

        if (!audioDacPlayFile(&flash, msg.name))
        {
          Serial1.println(F("Play FAIL"));
        }
        else
        {
          Serial1.println(F("Play DONE"));
        }
      }
    }
  }
}

/* ===================== TASK: MAIN ===================== */
void mainTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    msg.cmd = CMD_LED_TOGGLE;
    safeQueueSend(queueLed, &msg);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* ===================== SETUP ===================== */
void setup()
{
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  Serial1.begin(UART_BAUD);
  delay(100);

  Serial1.println(F("System Boot"));

  /* SPI */
  SPI.setMISO(FLASH_SPI_MISO_PIN);
  SPI.setMOSI(FLASH_SPI_MOSI_PIN);
  SPI.setSCLK(FLASH_SPI_SCK_PIN);
  SPI.begin();

  if (!flash.begin())
  {
    Serial1.println(F("Flash FAIL"));
    while (1)
      ;
  }

  if (!flashFsInit(&flash))
  {
    Serial1.println(F("FS FAIL"));
    while (1)
      ;
  }

  motorInit();
  audioDacInit();

  /* QUEUE */
  queueLed = xQueueCreate(5, sizeof(Message_t));
  queueAudio = xQueueCreate(5, sizeof(Message_t));
  queueMotor = xQueueCreate(5, sizeof(Message_t));

  if (!queueLed || !queueAudio || !queueMotor)
  {
    Serial1.println(F("Queue FAIL"));
    while (1)
      ;
  }

  cli_set_command(queueLed, queueAudio, queueMotor, &flash);

  /* TASK */
  xTaskCreate(mainTask, "MAIN", 512, NULL, 2, NULL);
  xTaskCreate(ledTask, "LED", 256, NULL, 1, NULL);
  xTaskCreate(commTask, "COMM", 1024, NULL, 1, NULL);
  xTaskCreate(audioTask, "AUDIO", 1536, NULL, 1, NULL);
  xTaskCreate(motorTask, "MOTOR", 512, NULL, 1, NULL);

  vTaskStartScheduler();
}

void loop()
{
}