#include "Arduino.h"
#include "pinout.h"
#include "motor_control.h"

static const uint8_t motorInPins[] = {
    0,              // index 0 unused (motors are 1-based)
    MOTOR1_IN_PIN,  // MOTOR_1
    MOTOR2_IN_PIN,  // MOTOR_2
    MOTOR3_IN_PIN,  // MOTOR_3
    MOTOR4_IN_PIN,  // MOTOR_4
};

static const uint8_t motorCtrlPins[] = {
    0,               // index 0 unused
    MOTOR1_CTRL_PIN, // MOTOR_1
    MOTOR2_CTRL_PIN, // MOTOR_2
    MOTOR3_CTRL_PIN, // MOTOR_3
    MOTOR4_CTRL_PIN, // MOTOR_4
};

void motorInit(void)
{
  for (int i = MOTOR_1; i <= MOTOR_4; ++i)
  {
    pinMode(motorInPins[i], OUTPUT);
    pinMode(motorCtrlPins[i], OUTPUT);

    // Default to off when starting.
    digitalWrite(motorInPins[i], LOW);
    digitalWrite(motorCtrlPins[i], LOW);
  }
  Serial1.println(F("===Motors initialized\n"));
}

void motorSet(MotorId motor, bool on)
{
  if (motor < MOTOR_1 || motor > MOTOR_4)
  {
    return;
  }

  digitalWrite(motorInPins[motor], on ? HIGH : LOW);
  digitalWrite(motorCtrlPins[motor], on ? HIGH : LOW);
}

void motorSetAll(bool on)
{
  for (int i = MOTOR_1; i <= MOTOR_4; ++i)
  {
    motorSet((MotorId)i, on);
  }
}
