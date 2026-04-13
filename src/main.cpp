#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <IWatchdog.h>

#include "pinout.h"
#include "motor_control.h"
#include "flash_files.h"
#include "audio_dac.h"
#include "bat_man.h"
#include "bat_handle.h"
#include "bat_charge.h"
#include "logger.h"

/* ===================== GLOBAL ===================== */
QueueHandle_t queueLed;     // UART CLI control
QueueHandle_t queueAudio;
QueueHandle_t queueMotor;

HardwareSerial Serial1(SERIAL1_RX_PIN, SERIAL1_TX_PIN);
SPIFlash flash(FLASH_SPI_CS_PIN, &SPI);

/* ===================== UTILS ===================== */
void safeQueueSend(QueueHandle_t q, Message_t *msg)
{
  if (xQueueSend(q, msg, 0) != pdPASS)
  {
    logPrintln("Queue FULL!");
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

        logPrintf("Motor %d set to %s\n", (int)msg.param, msg.on ? "ON" : "OFF");
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
        logPrint("Play: ");
        logPrintln(msg.name);

        if (!audioDacPlayFile(&flash, msg.name))
        {
          logPrintln("Play FAIL");
        }
        else
        {
          logPrintln("Play DONE");
        }
      }
      else if (msg.cmd == CMD_TEST_AUDIO)
      {
        test_beep();
      }
    }
  }
}

/* ===================== TASK: BUTTON ===================== */
// Single press: cycle khay1.pcm → khay2.pcm → ... → khayN.pcm → khay1.pcm
// Double tap  : play xinchao.pcm
#define DEBOUNCE_MS     50    // debounce thời gian
#define DOUBLE_TAP_MS  400    // khoảng thời gian tối đa giữa 2 lần nhấn

/**
 * @brief Clear all pending audio messages from queue to prevent buffer buildup
 */
static void clearAudioQueue()
{
  Message_t dummy;
  while (xQueueReceive(queueAudio, &dummy, 0) == pdPASS)
  {
    // Keep receiving until queue is empty
  }
}

/**
 * @brief Send audio message, clearing old queue items first
 * Prevents backlog of audio when button is pressed rapidly
 */
static void sendPlay(const char *filename)
{
  Message_t msg;
  msg.cmd = CMD_PLAY_AUDIO;
  strncpy(msg.name, filename, sizeof(msg.name) - 1);
  msg.name[sizeof(msg.name) - 1] = '\0';
  
  // Clear any pending audio to prevent buffer buildup
  clearAudioQueue();
  
  safeQueueSend(queueAudio, &msg);
}

void buttonTask(void *pvParameters)
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  int khayIndex = 1; // 1..MOTOR_COUNT

  bool stableLevel = HIGH;
  bool lastRawLevel = HIGH;
  uint32_t lastRawChangeMs = 0;

  bool waitingSecondTap = false;
  uint32_t firstReleaseMs = 0;

  while (1)
  {
    uint32_t now = millis();
    bool raw = (bool)digitalRead(BUTTON_PIN);

    if (raw != lastRawLevel)
    {
      lastRawLevel = raw;
      lastRawChangeMs = now;
    }

    // Debounced edge detect
    if ((now - lastRawChangeMs) >= DEBOUNCE_MS && stableLevel != raw)
    {
      stableLevel = raw;

      // We only act on release edge for tap counting
      if (stableLevel == HIGH)
      {
        if (waitingSecondTap && (now - firstReleaseMs) <= DOUBLE_TAP_MS)
        {
          waitingSecondTap = false;
          logPrintln("[BTN] Double tap detected -> Play move.pcm");
          sendPlay("move.pcm");
        }
        else
        {
          waitingSecondTap = true;
          firstReleaseMs = now;
        }
      }
    }

    // Timeout for second tap -> treat as single tap
    if (waitingSecondTap && (now - firstReleaseMs) > DOUBLE_TAP_MS)
    {
      waitingSecondTap = false;
      char filename[16];
      snprintf(filename, sizeof(filename), "khay%d.pcm", khayIndex);
      logPrintf("[BTN] Single press -> Play %s\n", filename);
      sendPlay(filename);

      khayIndex++;
      if (khayIndex > MOTOR_COUNT)
        khayIndex = 1;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


void mainTask(void *pvParameters)
{
  Message_t msg;

  // Play xinchao khi boot xong
  vTaskDelay(pdMS_TO_TICKS(500));  // Chờ audioTask sẵn sàng
  sendPlay("xinchao.pcm");

  while (1)
  {
    // Reload watchdog to prevent reset
    IWatchdog.reload();

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

  // Initialize Independent Watchdog (10 second timeout)
  IWatchdog.begin(10000000);

  logPrintln("System Boot");

  /* SPI */
  SPI.setMISO(FLASH_SPI_MISO_PIN);
  SPI.setMOSI(FLASH_SPI_MOSI_PIN);
  SPI.setSCLK(FLASH_SPI_SCK_PIN);
  SPI.begin();

  if (!flash.begin())
  {
    logPrintln("Flash FAIL");
    while (1)
      ;
  }

  if (!flashFsInit(&flash))
  {
    logPrintln("FS FAIL");
    while (1)
      ;
  }

  motorInit();
  audioDacInit();
  muxInit(); // Initialize ADC multiplexer

  /* ADC Mutex */
  adcMutex = xSemaphoreCreateMutex();
  if (!adcMutex)
  {
    logPrintln("ADC Mutex FAIL");
    while (1)
      ;
  }

  /* QUEUE */
  queueLed = xQueueCreate(5, sizeof(Message_t));
  queueAudio = xQueueCreate(5, sizeof(Message_t));
  queueMotor = xQueueCreate(5, sizeof(Message_t));

  if (!queueLed || !queueAudio || !queueMotor)
  {
    logPrintln("Queue FAIL");
    while (1)
      ;
  }

  cli_set_command(queueLed, queueAudio, queueMotor, &flash);

  loggerInit();

  /* TASK */
  xTaskCreate(mainTask,   "MAIN",   256,  NULL, 2, NULL);
  xTaskCreate(ledTask,    "LED",    256,  NULL, 1, NULL);
  xTaskCreate(commTask,   "COMM",   512,  NULL, 1, NULL);
  xTaskCreate(audioTask,  "AUDIO",  1024, NULL, 1, NULL);
  xTaskCreate(motorTask,  "MOTOR",  256,  NULL, 1, NULL);
  xTaskCreate(chargeTask, "CHARGE", 512,  NULL, 2, NULL);
  xTaskCreate(buttonTask, "BUTTON", 256,  NULL, 1, NULL);

  vTaskStartScheduler();
}

void loop()
{
}