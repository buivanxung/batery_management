#pragma once

#include <stdint.h>

#ifndef MOTOR_COUNT
#define MOTOR_COUNT 8
#endif

/// Motor identifiers (1-based).
typedef enum
{
  MOTOR_1 = 1,
  MOTOR_2,
  MOTOR_3,
  MOTOR_4,
  MOTOR_5,
  MOTOR_6,
  MOTOR_7,
  MOTOR_8,
} MotorId;

/// Initialize motor GPIO pins (must be called once before using motorSet()).
void motorInit(void);

/// Set the given motor on/off.
/// motor: MOTOR_1..MOTOR_4
/// on: true -> ON (output HIGH), false -> OFF (output LOW)
void motorSet(MotorId motor, bool on);

/// Turn all motors on/off (convenience helper).
void motorSetAll(bool on);
