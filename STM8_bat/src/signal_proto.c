#include <Arduino.h>
#include "config.h"
#include "signal_proto.h"

#define FRAME_SOF         0xA5
#define CMD_GET_STATUS    0x01
#define CMD_SET_PW_KEY    0x02
#define CMD_SET_PW_TIMER  0x03   // payload: 2 bytes = minutes (big-endian); 0 = cancel
#define CMD_GET_DATA      0x04   // Get millivolts + locked status
#define CMD_ACK_MASK      0x80
#define CMD_NACK          0x7F

#define ERR_BAD_CMD       0x01
#define ERR_BAD_LEN       0x02
#define ERR_BAD_VALUE     0x03
#define ERR_LOCKED        0x04
#define ERR_BAD_CRC       0x05

#define MAX_PAYLOAD_LEN   8
#define INTER_BYTE_US     3000UL  // max wait between bytes within one frame
#define SOF_WAIT_US      12000UL  // wider start-bit window to reduce missed frames

static uint8_t rxPayload[MAX_PAYLOAD_LEN];

// Remote PW_KEY timer (set by CMD_SET_PW_TIMER from STM32)
static unsigned long pwTimerStartMs    = 0;
static unsigned long pwTimerDurationMs = 0;
static bool          pwTimerActive     = false;

// Bit-bang UART receive on PIN_SIGNAL_OUT (PD5).
// Pin must be INPUT_PULLUP. Hardware UART1 must NOT be running (PD5 would
// stay HIGH when idle and fight incoming LOW signals from STM32).
static bool uartReadByte(uint8_t *outByte, unsigned long timeoutUs) {
  const unsigned long bitUs = 1000000UL / SIGNAL_UART_BAUD;

  if (timeoutUs == 0) {
    if (digitalRead(PIN_SIGNAL_OUT) != LOW) return false;
  } else {
    unsigned long t0 = micros();
    while (digitalRead(PIN_SIGNAL_OUT) != LOW) {
      if (micros() - t0 >= timeoutUs) return false;
    }
  }

  // Centre of first data bit
  delayMicroseconds(bitUs + (bitUs / 2));
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(PIN_SIGNAL_OUT) == HIGH) value |= (1U << i);
    delayMicroseconds(bitUs);
  }
  delayMicroseconds(bitUs); // consume stop bit
  *outByte = value;
  return true;
}

// Bit-bang UART transmit on PIN_SIGNAL_OUT (PD5). Pin must be OUTPUT.
static void uartWriteByte(uint8_t value) {
  const unsigned long bitUs = 1000000UL / SIGNAL_UART_BAUD;

  digitalWrite(PIN_SIGNAL_OUT, LOW);            // start bit
  delayMicroseconds(bitUs);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_SIGNAL_OUT, (value & 0x01U) ? HIGH : LOW);
    delayMicroseconds(bitUs);
    value >>= 1;
  }
  digitalWrite(PIN_SIGNAL_OUT, HIGH);           // stop bit
  delayMicroseconds(bitUs);
}

static uint8_t frameCrc(uint8_t cmd, uint8_t len, const uint8_t *payload) {
  uint8_t crc = FRAME_SOF ^ cmd ^ len;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= payload[i];
  }
  return crc;
}

static void sendFrame(uint8_t cmd, uint8_t len, const uint8_t *payload) {
  uint8_t crc = frameCrc(cmd, len, payload);

  // Turnaround: let STM32 finish its last stop bit and switch PA0 to INPUT
  delayMicroseconds(2000);

  // Switch to OUTPUT, assert idle HIGH briefly before sending
  pinMode(PIN_SIGNAL_OUT, OUTPUT);
  digitalWrite(PIN_SIGNAL_OUT, HIGH);
  delayMicroseconds(500);

  uartWriteByte(FRAME_SOF);
  uartWriteByte(cmd);
  uartWriteByte(len);
  for (uint8_t i = 0; i < len; i++) {
    uartWriteByte(payload[i]);
  }
  uartWriteByte(crc);

  // Back to INPUT_PULLUP: ready to receive next command
  pinMode(PIN_SIGNAL_OUT, INPUT_PULLUP);
}

static void sendNack(uint8_t reqCmd, uint8_t errorCode) {
  uint8_t payload[2] = {reqCmd, errorCode};
  sendFrame(CMD_NACK, 2, payload);
}

static void sendStatusAck(uint8_t reqCmd, bool pwLocked, uint16_t batMv) {
  bool realOn = (digitalRead(PIN_IP_LED_BAT_STATUS) == HIGH);
  uint8_t payload[4];
  payload[0] = pwLocked ? 1 : 0;
  payload[1] = realOn ? 1 : 0;
  payload[2] = (uint8_t)(batMv >> 8);   // high byte
  payload[3] = (uint8_t)(batMv & 0xFF); // low byte
  sendFrame(reqCmd | CMD_ACK_MASK, 4, payload);
}

static void sendDataAck(uint8_t reqCmd, uint16_t batMv, bool pwLocked) {
  uint8_t payload[3];
  payload[0] = (uint8_t)(batMv >> 8);   // millivolts high byte
  payload[1] = (uint8_t)(batMv & 0xFF); // millivolts low byte
  payload[2] = pwLocked ? 1 : 0;        // locked status
  sendFrame(reqCmd | CMD_ACK_MASK, 3, payload);
}

static void handleRequest(uint8_t cmd, uint8_t len, bool pwLocked, uint16_t batMv) {
  if (cmd == CMD_GET_STATUS) {
    if (len != 0) {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  if (cmd == CMD_GET_DATA) {
    if (len != 0) {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    sendDataAck(cmd, batMv, pwLocked);
    return;
  }

  if (cmd == CMD_SET_PW_KEY) {
    if (len != 1) {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }

    if (rxPayload[0] > 1) {
      sendNack(cmd, ERR_BAD_VALUE);
      return;
    }

    if (pwLocked) {
      sendNack(cmd, ERR_LOCKED);
      return;
    }

    // Cancel any running timer before manual override
    pwTimerActive = false;
    digitalWrite(PIN_PW_KEY, rxPayload[0] ? PW_KEY_ON : PW_KEY_OFF);
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  if (cmd == CMD_SET_PW_TIMER) {
    if (len != 2) {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    if (pwLocked) {
      sendNack(cmd, ERR_LOCKED);
      return;
    }
    uint16_t minutes = ((uint16_t)rxPayload[0] << 8) | rxPayload[1];
    if (minutes == 0) {
      pwTimerActive = false;
      digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
    } else {
      pwTimerStartMs    = millis();
      pwTimerDurationMs = (unsigned long)minutes * 60000UL;
      pwTimerActive     = true;
      digitalWrite(PIN_PW_KEY, PW_KEY_ON);
    }
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  sendNack(cmd, ERR_BAD_CMD);
}

void signalProtoInit(void) {
  // Bit-bang UART on PD5 (PIN_SIGNAL_OUT). Hardware UART1 must NOT run —
  // its TX idle-HIGH would fight incoming LOW signals from STM32 via the MUX.
  // Set PD6 (UART1 RX) to INPUT_PULLUP so it does not drive the shared node.
  pinMode(PD6, INPUT_PULLUP);
  pinMode(PIN_SIGNAL_OUT, INPUT_PULLUP);
  pwTimerActive = false;
}

void signalProtoUpdatePwTimer(void) {
  if (!pwTimerActive) return;
  if ((millis() - pwTimerStartMs) >= pwTimerDurationMs) {
    pwTimerActive = false;
    digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
  }
}

void signalProtoPoll(bool pwLocked, uint16_t batMv) {
  uint8_t byteIn, cmd, len, crc;

  // Wait briefly for SOF start bit (non-blocking with short timeout)
  if (!uartReadByte(&byteIn, SOF_WAIT_US) || byteIn != FRAME_SOF) return;

  // SOF confirmed — receive the rest of the frame with inter-byte timeout
  if (!uartReadByte(&cmd, INTER_BYTE_US)) return;
  if (!uartReadByte(&len, INTER_BYTE_US)) return;
  if (len > MAX_PAYLOAD_LEN) { sendNack(cmd, ERR_BAD_LEN); return; }

  for (uint8_t i = 0; i < len; i++) {
    if (!uartReadByte(&rxPayload[i], INTER_BYTE_US)) return;
  }

  if (!uartReadByte(&crc, INTER_BYTE_US)) return;

  if (crc != frameCrc(cmd, len, rxPayload)) {
    sendNack(cmd, ERR_BAD_CRC);
    return;
  }

  handleRequest(cmd, len, pwLocked, batMv);
}
