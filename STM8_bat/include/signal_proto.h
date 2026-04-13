#ifndef SIGNAL_PROTO_H
#define SIGNAL_PROTO_H

#include <Arduino.h>

void signalProtoInit(void);
void signalProtoPoll(bool pwLocked, uint16_t batMv);

/**
 * Call once per main loop iteration to enforce the remote N-minute PW_KEY timer.
 * Turns PIN_PW_KEY LOW automatically when the timer expires.
 */
void signalProtoUpdatePwTimer(void);

#endif // SIGNAL_PROTO_H
