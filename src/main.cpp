#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <SPI.h>
#include <SPIMemory.h>

#include "pinout.h"
#include "motor_control.h"
#include "flash_files.h"
#include "audio_dac.h"

typedef enum
{
  CMD_LED_ON,
  CMD_LED_OFF,
  CMD_LED_TOGGLE,
  CMD_PRINT,
  CMD_PLAY_AUDIO,
  CMD_MOTOR_SET,
} CommandType;

typedef struct
{
  CommandType cmd;
  uint32_t param;
  char name[16];  // for audio file name
  bool on;        // for motor on/off
} Message_t;

QueueHandle_t queueLed;
QueueHandle_t queueComm;
QueueHandle_t queueAudio;
QueueHandle_t queueMotor;

SPIFlash *flash;
HardwareSerial Serial1(SERIAL1_RX_PIN, SERIAL1_TX_PIN);

// Global state for button control
volatile uint8_t currentMotor = 1;  // 1-4
volatile uint32_t lastButtonPress = 0;

// Task handle (optional)
TaskHandle_t ledTaskHandle;
TaskHandle_t commTaskHandle;
TaskHandle_t audioTaskHandle;
TaskHandle_t motorTaskHandle;
TaskHandle_t buttonTaskHandle;

void ledTask(void *pvParameters)
{
  Message_t msg;

  // Configure status LEDs.
  pinMode(LED_BLINK_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);

  while (1)
  {
    if (xQueueReceive(queueLed, &msg, portMAX_DELAY) == pdPASS)
    {
      switch (msg.cmd)
      {
      case CMD_LED_ON:
        digitalWrite(LED_BLINK_PIN, HIGH);
        break;

      case CMD_LED_OFF:
        digitalWrite(LED_BLINK_PIN, LOW);
        break;

      case CMD_LED_TOGGLE:
        digitalWrite(LED_BLINK_PIN, !digitalRead(LED_BLINK_PIN));
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
  // Simple command interface via Serial1:
  //   LIST           - list files stored in flash
  //   PLAY <name>    - play an 8-bit PCM file stored in flash
  //   HELP           - show this help message

  // Print startup message to verify UART is working
  Serial1.println(F("\n=== COMM Task Started ==="));
  Serial1.println(F("Type HELP for commands\n"));

  while (1)
  {
    if (Serial1.available())
    {
      String line = Serial1.readStringUntil('\n');
      line.trim();

      if (line.length() == 0)
      {
        vTaskDelay(10);
        continue;
      }

      if (line.equalsIgnoreCase("LIST"))
      {
        flashFsListFiles(flash);
      }
      else if (line.equalsIgnoreCase("MEM"))
      {
        // Report memory stats
        uint32_t freeHeap = xPortGetFreeHeapSize();
        uint32_t flashCapacity = flash->getCapacity();

        Serial1.println(F("Memory Stats:"));
        Serial1.print(F("  RAM Free: "));
        Serial1.print(freeHeap);
        Serial1.println(F(" bytes"));
        Serial1.print(F("  Flash Capacity: "));
        Serial1.print(flashCapacity / 1024);
        Serial1.println(F(" KB"));
      }
      else if (line.startsWith("PLAY "))
      {
        String name = line.substring(5);
        name.trim();
        Message_t msg;
        msg.cmd = CMD_PLAY_AUDIO;
        strncpy(msg.name, name.c_str(), sizeof(msg.name));
        msg.name[sizeof(msg.name) - 1] = '\0';
        xQueueSend(queueAudio, &msg, portMAX_DELAY);
        Serial1.print(F("Queued play: "));
        Serial1.println(name);
      }
      else if (line.startsWith("MOTOR"))
      {
        // Parse MOTOR1 ON/OFF, MOTOR2 ON/OFF, etc.
        String rest = line.substring(5);
        rest.trim();
        int spaceIndex = rest.indexOf(' ');
        if (spaceIndex <= 0)
        {
          Serial1.println(F("Usage: MOTOR<n> ON|OFF"));
        }
        else
        {
          String motorStr = rest.substring(0, spaceIndex);
          String stateStr = rest.substring(spaceIndex + 1);
          stateStr.trim();
          motorStr.toUpperCase();
          stateStr.toUpperCase();

          MotorId motor = MOTOR_1;
          if (motorStr == "1") motor = MOTOR_1;
          else if (motorStr == "2") motor = MOTOR_2;
          else if (motorStr == "3") motor = MOTOR_3;
          else if (motorStr == "4") motor = MOTOR_4;
          else
          {
            Serial1.println(F("Invalid motor: use 1-4"));
            continue;
          }

          bool on = false;
          if (stateStr == "ON") on = true;
          else if (stateStr == "OFF") on = false;
          else
          {
            Serial1.println(F("Invalid state: use ON or OFF"));
            continue;
          }

          Message_t msg;
          msg.cmd = CMD_MOTOR_SET;
          msg.param = (uint32_t)motor;
          msg.on = on;
          xQueueSend(queueMotor, &msg, portMAX_DELAY);
          Serial1.print(F("Queued motor "));
          Serial1.print(motorStr);
          Serial1.print(on ? F(" ON") : F(" OFF"));
          Serial1.println();
        }
      }
      else if (line.equalsIgnoreCase("HELP"))
      {
        Serial1.println(F("Commands:"));
        Serial1.println(F("  LIST"));
        Serial1.println(F("  MEM"));
        Serial1.println(F("  PLAY <name>"));
        Serial1.println(F("  MOTOR<n> ON|OFF  (n=1-4)"));
        Serial1.println(F("  STORE <name> <length>    (then send <length> raw bytes)"));
      }
      else if (line.startsWith("STORE "))
      {
        String rest = line.substring(6);
        rest.trim();
        int spaceIndex = rest.indexOf(' ');
        if (spaceIndex <= 0)
        {
          Serial1.println(F("Usage: STORE <name> <length>"));
        }
        else
        {
          String name = rest.substring(0, spaceIndex);
          String lenStr = rest.substring(spaceIndex + 1);
          uint32_t length = (uint32_t)lenStr.toInt();
          if (length == 0)
          {
            Serial1.println(F("Invalid length"));
          }
          else
          {
            Serial1.print(F("Send "));
            Serial1.print(length);
            Serial1.println(F(" bytes now..."));
            if (flashFsWriteFileFromSerial(flash, name.c_str(), length))
            {
              Serial1.println(F("STORE OK"));
            }
            else
            {
              Serial1.println(F("STORE FAILED"));
            }
          }
        }
      }
      else
      {
        Serial1.println(F("Unknown command. Type HELP"));
      }
    }

    vTaskDelay(50);
  }
}

void audioTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    if (xQueueReceive(queueAudio, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_PLAY_AUDIO)
      {
        Serial1.print(F("Playing: "));
        Serial1.println(msg.name);
        
        // Note: audioDacPlayFile() is BLOCKING - this task will wait until playback completes
        if (!audioDacPlayFile(flash, msg.name))
        {
          Serial1.print(F("Failed to play: "));
          Serial1.println(msg.name);
        }
        else
        {
          Serial1.println(F("Playback complete"));
        }
      }
    }
    vTaskDelay(10);
  }
}

void motorTask(void *pvParameters)
{
  Message_t msg;

  while (1)
  {
    if (xQueueReceive(queueMotor, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == CMD_MOTOR_SET)
      {
        MotorId motor = (MotorId)msg.param;
        motorSet(motor, msg.on);
        Serial1.print(F("Motor "));
        Serial1.print(msg.param);
        Serial1.print(msg.on ? F(" ON") : F(" OFF"));
        Serial1.println();
      }
    }
    vTaskDelay(10);
  }
}

void buttonTask(void *pvParameters)
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Assume pullup, button grounds when pressed

  uint32_t pressStart = 0;
  bool wasPressed = false;
  uint8_t pressCount = 0;
  uint32_t lastPressTime = 0;

  while (1)
  {
    bool isPressed = (digitalRead(BUTTON_PIN) == LOW);  // LOW when pressed
    uint32_t now = millis();

    if (isPressed && !wasPressed)
    {
      // Button just pressed
      pressStart = now;
      wasPressed = true;
      lastPressTime = now;
      pressCount++;
      if (pressCount > 4) pressCount = 1;  // Cycle 1-4
      currentMotor = pressCount;
      // Set motor
      for (uint8_t m = 1; m <= 4; ++m)
      {
        Message_t msg;
        msg.cmd = CMD_MOTOR_SET;
        msg.param = m;
        msg.on = (m == currentMotor);
        xQueueSend(queueMotor, &msg, 0);  // Non-blocking
      }
      Serial1.print(F("Motor set to "));
      Serial1.println(currentMotor);
    }
    else if (!isPressed && wasPressed)
    {
      // Button just released
      uint32_t pressDuration = now - pressStart;
      if (pressDuration >= 2000)  // Hold for 2s to confirm
      {
        Serial1.print(F("Confirmed motor "));
        Serial1.println(currentMotor);
        // Reset to 1 after confirm
        currentMotor = 1;
        for (uint8_t m = 1; m <= 4; ++m)
        {
          Message_t msg;
          msg.cmd = CMD_MOTOR_SET;
          msg.param = m;
          msg.on = (m == 1);
          xQueueSend(queueMotor, &msg, 0);
        }
        Serial1.println(F("Reset to motor 1"));
        pressCount = 0;
      }
      wasPressed = false;
    }

    // Timeout: if no press for 5s, reset to motor 1
    if (now - lastPressTime > 5000 && currentMotor != 1)
    {
      currentMotor = 1;
      for (uint8_t m = 1; m <= 4; ++m)
      {
        Message_t msg;
        msg.cmd = CMD_MOTOR_SET;
        msg.param = m;
        msg.on = (m == 1);
        xQueueSend(queueMotor, &msg, 0);
      }
      Serial1.println(F("Timeout: reset to motor 1"));
    }

    vTaskDelay(50);
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
  }
}

void setup()
{
  // Ensure GPIO port clocks are enabled for all used pins.
  // Update these if you change LED/USART/SPI pins in pinout.h.
  __HAL_RCC_GPIOA_CLK_ENABLE();  // PA9, PA10 for UART1 + SPI pins
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  // Wait a bit for clocks to settle
  delayMicroseconds(100);

  // OPTION 1: Use SerialUSB (recommended on Nucleo - through ST-Link)
  // Uncomment below and comment out Serial1.begin() if you want ST-Link UART
  // SerialUSB.begin(115200);
  
  // OPTION 2: Use Serial1 (physical PA9/PA10 pins - needs external adapter)
  Serial1.begin(115200);
  
  // Small delay to ensure Serial is ready
  delay(100);
  
  Serial1.println(F("\n\n=== System Starting ==="));
  Serial1.println(F("Board: Nucleo G070RB"));
  Serial1.println(F("UART: Serial1 PA9(TX) PA10(RX) @ 115200 baud\n"));

  // Initialize SPI with the pins used on the board.
  SPI.begin();
  flash = new SPIFlash(FLASH_SPI_CS_PIN);
  
  if (!flash->begin())
  {
    Serial1.println(F("ERROR: Flash initialization failed"));
    return;
  }

  // Initialize a very small filesystem in SPI flash so we can store/play files.
  if (!flashFsInit(flash))
  {
    Serial1.println(F("ERROR: Flash filesystem init failed"));
    return;
  }

  motorInit();
  audioDacInit();

  queueLed = xQueueCreate(10, sizeof(Message_t));
  if (queueLed == NULL) { Serial1.println(F("ERROR: LED queue creation failed")); return; }
  
  queueComm = xQueueCreate(10, sizeof(Message_t));
  if (queueComm == NULL) { Serial1.println(F("ERROR: COMM queue creation failed")); return; }
  
  queueAudio = xQueueCreate(5, sizeof(Message_t));
  if (queueAudio == NULL) { Serial1.println(F("ERROR: AUDIO queue creation failed")); return; }
  
  queueMotor = xQueueCreate(5, sizeof(Message_t));
  if (queueMotor == NULL) { Serial1.println(F("ERROR: MOTOR queue creation failed")); return; }

  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, HIGH);

  // Tạo task (increased stack size to prevent overflow)
  xTaskCreate(mainTask, "MAIN", 512, NULL, 2, NULL);
  xTaskCreate(ledTask, "LED", 512, NULL, 1, &ledTaskHandle);
  xTaskCreate(commTask, "COMM", 768, NULL, 1, &commTaskHandle);  // COMM needs more stack for strings
  xTaskCreate(audioTask, "AUDIO", 1024, NULL, 1, &audioTaskHandle);  // AUDIO uses buffers
  xTaskCreate(motorTask, "MOTOR", 512, NULL, 1, &motorTaskHandle);
  xTaskCreate(buttonTask, "BUTTON", 512, NULL, 1, &buttonTaskHandle);

  // Start scheduler
  vTaskStartScheduler();
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}