#include "Arduino.h"
#include "pinout.h"
#include "motor_control.h"
#include "logger.h"

static const uint8_t motorInPins[] = {
    0,             // index 0 unused (motors are 1-based)
    MOTOR1_IN_PIN, // MOTOR_1
    MOTOR2_IN_PIN, // MOTOR_2
    MOTOR3_IN_PIN, // MOTOR_3
    MOTOR4_IN_PIN, // MOTOR_4
    MOTOR5_IN_PIN, // MOTOR_5
    MOTOR6_IN_PIN, // MOTOR_6
    MOTOR7_IN_PIN, // MOTOR_7
    MOTOR8_IN_PIN, // MOTOR_8
};

static const uint8_t motorCtrlPins[] = {
    0,               // index 0 unused
    MOTOR1_CTRL_PIN, // MOTOR_1
    MOTOR2_CTRL_PIN, // MOTOR_2
    MOTOR3_CTRL_PIN, // MOTOR_3
    MOTOR4_CTRL_PIN, // MOTOR_4
    MOTOR5_CTRL_PIN, // MOTOR_5
    MOTOR6_CTRL_PIN, // MOTOR_6
    MOTOR7_CTRL_PIN, // MOTOR_7
    MOTOR8_CTRL_PIN, // MOTOR_8
};

static const uint8_t chargeCtrlPins[] = {
    0,             // index 0 unused
    POGO1_CTR_PIN, // MOTOR_1
    POGO2_CTR_PIN, // MOTOR_2
    POGO3_CTR_PIN, // MOTOR_3
    POGO4_CTR_PIN, // MOTOR_4
    POGO5_CTR_PIN, // MOTOR_5
    POGO6_CTR_PIN, // MOTOR_6
    POGO7_CTR_PIN, // MOTOR_7
    POGO8_CTR_PIN, // MOTOR_8
};

void motorInit(void)
{
  for (int i = MOTOR_1; i <= MOTOR_7; ++i)
  {
    pinMode(motorInPins[i], INPUT_PULLDOWN);
    pinMode(motorCtrlPins[i], OUTPUT);
    pinMode(chargeCtrlPins[i], OUTPUT);

    // Default to off when starting.
    digitalWrite(chargeCtrlPins[i], LOW);
    digitalWrite(motorCtrlPins[i], LOW);
  }
  logPrintln("===Motors initialized");
}

void motorSet(MotorId motor, bool on)
{
  uint32_t tickstart = HAL_GetTick();
  logPrintf("===Motors time start %ld motor %d curent value %d\n", tickstart, motor, digitalRead(motorInPins[motor]));
  if (motor < MOTOR_1 || motor > MOTOR_8)
  {
    return;
  }
  if (on)
  {
    if (digitalRead(motorInPins[motor]))
      return;
    digitalWrite(motorCtrlPins[motor], on ? HIGH : LOW);
    while (digitalRead(motorInPins[motor]) == 0 && tickstart + 4000 > HAL_GetTick())
    {
      vTaskDelay(10);
    }
    digitalWrite(motorCtrlPins[motor], LOW);
  }
  else
  {
    if (!digitalRead(motorInPins[motor]))
      return;
    digitalWrite(motorCtrlPins[motor], HIGH);
    while (digitalRead(motorInPins[motor]) == 1 && tickstart + 4000 > HAL_GetTick())
    {
      vTaskDelay(10);
    }
    digitalWrite(motorCtrlPins[motor], LOW);
  }
  logPrintf("===Motors time end %ld\n", HAL_GetTick());
  if (tickstart + 4000 < HAL_GetTick())
    logPrintf("===Motors %d control %s fail\n", motor, on ? "on" : "off");
  else
    logPrintf("===Motors %d control %s ok\n", motor, on ? "on" : "off");
}

void motorSetAll(bool on)
{
  for (int i = MOTOR_1; i <= MOTOR_4; ++i)
  {
    motorSet((MotorId)i, on);
  }
}
