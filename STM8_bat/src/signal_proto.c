#include <Arduino.h>
#include "config.h"
#include "signal_proto.h"

#define FRAME_SOF         0xA5
#define CMD_GET_STATUS    0x01
#define CMD_SET_PW_KEY    0x02
#define CMD_SET_PW_TIMER  0x03   // payload: 2 bytes = minutes (big-endian); 0 = cancel
#define CMD_GET_DATA      0x04   // Get millivolts + locked status
#define CMD_GET_DIAG      0x05   // Debug counters for protocol diagnosis
#define CMD_ACK_MASK      0x80
#define CMD_NACK          0x7F

#define ERR_BAD_CMD       0x01
#define ERR_BAD_LEN       0x02
#define ERR_BAD_VALUE     0x03
#define ERR_LOCKED        0x04
#define ERR_BAD_CRC       0x05

#define MAX_PAYLOAD_LEN   8
#define INTER_BYTE_US     3000UL  // max wait between bytes within one frame
#define UART_ECHO_TEST_MODE 0

// With fixed 1-wire hardware wiring, bytes transmitted on PD5 can be seen back on PD6.
// Mute RX briefly after each TX frame to avoid parsing our own response as a new request.
#define RX_MUTE_MARGIN_US 3000UL

static uint8_t rxPayload[MAX_PAYLOAD_LEN];

// Protocol diagnostics (all counters saturate at 255).
static uint8_t diagRxOk = 0;
static uint8_t diagCrcErr = 0;
static uint8_t diagTimeoutErr = 0;
static uint8_t diagBadLenErr = 0;
static uint8_t diagBadCmdErr = 0;
static uint8_t diagFramingErr = 0;
static uint8_t diagFalseStartErr = 0;
static uint8_t diagLastCmd = 0;

// Remote PW_KEY timer (set by CMD_SET_PW_TIMER from STM32)
static unsigned long pwTimerStartMs    = 0;
static unsigned long pwTimerDurationMs = 0;
static bool          pwTimerActive     = false;
static bool          pd6PrevHigh       = true;
static unsigned long rxMuteUntilUs     = 0;

static void satInc(uint8_t *v) {
  if (*v < 0xFF) {
    (*v)++;
  }
}

// Hardware UART1 receive with timeout in microseconds.
static bool uartReadByte(uint8_t *outByte, unsigned long timeoutUs) {
  unsigned long t0 = micros();
  while (Serial_available() == 0) {
    if (timeoutUs > 0 && micros() - t0 >= timeoutUs) {
      return false;
    }
  }
  // Debug probe: toggle RED LED each received byte on UART1 RX (PD6).
  digitalWrite(PIN_IP_RED, (digitalRead(PIN_IP_RED) == HIGH) ? LOW : HIGH);
  *outByte = (uint8_t)Serial_read();
  return true;
}

// Hardware UART1 transmit.
static void uartWriteByte(uint8_t value) {
  Serial_write(value);
}

static void uartDiscardRxBuffer(void) {
  while (Serial_available() > 0) {
    (void)Serial_read();
  }
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
  uint8_t frameLen = (uint8_t)(len + 4); // SOF + CMD + LEN + CRC

  // One-wire mode: drive PD5 only while transmitting.
  pinMode(PD5, OUTPUT);

  uartWriteByte(FRAME_SOF);
  uartWriteByte(cmd);
  uartWriteByte(len);
  for (uint8_t i = 0; i < len; i++) {
    uartWriteByte(payload[i]);
  }
  uartWriteByte(crc);

  // Ensure stop bit is fully sent, then release line.
  delayMicroseconds(1200);
  pinMode(PD5, INPUT);

  // At 9600 baud each byte is about 1042us (10 bits). Add margin for safety.
  rxMuteUntilUs = micros() + (unsigned long)frameLen * 1042UL + RX_MUTE_MARGIN_US;
  uartDiscardRxBuffer();
}

static void sendNack(uint8_t reqCmd, uint8_t errorCode) {
  uint8_t payload[2] = {reqCmd, errorCode};
  sendFrame(CMD_NACK, 2, payload);
}

static void sendDiagAck(uint8_t reqCmd) {
  uint8_t payload[8];
  payload[0] = diagRxOk;
  payload[1] = diagCrcErr;
  payload[2] = diagTimeoutErr;
  payload[3] = diagBadLenErr;
  payload[4] = diagBadCmdErr;
  payload[5] = diagFramingErr;
  payload[6] = diagFalseStartErr;
  payload[7] = diagLastCmd;
  sendFrame(reqCmd | CMD_ACK_MASK, 8, payload);
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

  if (cmd == CMD_GET_DIAG) {
    if (len != 0) {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    sendDiagAck(cmd);
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
  satInc(&diagBadCmdErr);
}

void signalProtoInit(void) {
  // Explicitly configure PD5(TX) and PD6(RX) for UART1 hardware.
  // One-wire behavior: keep PD5 released (input) except when sending.
  // PD6 always stays as RX input.
  pinMode(PD5, INPUT);
  pinMode(PD6, INPUT_PULLUP);
  
  // Initialize UART1 at 9600 baud.
  Serial_begin(SIGNAL_UART_BAUD);
  
  pwTimerActive = false;
  diagRxOk = 0;
  diagCrcErr = 0;
  diagTimeoutErr = 0;
  diagBadLenErr = 0;
  diagBadCmdErr = 0;
  diagFramingErr = 0;
  diagFalseStartErr = 0;
  diagLastCmd = 0;
  pd6PrevHigh = (digitalRead(PD6) == HIGH);
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
  unsigned long nowUs = micros();

  if ((long)(nowUs - rxMuteUntilUs) < 0) {
    uartDiscardRxBuffer();
    return;
  }

  // Keep TX released while listening on shared signal line.
  pinMode(PD5, INPUT);

  // Raw electrical probe: toggle RED LED on every falling edge at PD6.
  // This helps distinguish wiring/voltage issues from UART decode issues.
  bool pd6NowHigh = (digitalRead(PD6) == HIGH);
  if (pd6PrevHigh && !pd6NowHigh) {
    digitalWrite(PIN_IP_RED, (digitalRead(PIN_IP_RED) == HIGH) ? LOW : HIGH);
  }
  pd6PrevHigh = pd6NowHigh;

#if UART_ECHO_TEST_MODE
  // Minimal electrical test: echo each received byte back out on PD5.
  if (Serial_available() == 0) return;
  if (!uartReadByte(&byteIn, 0)) return;

  pinMode(PD5, OUTPUT);
  uartWriteByte(byteIn);
  delayMicroseconds(1200);
  pinMode(PD5, INPUT);
  return;
#endif

  // Always listen: if no data in UART RX buffer, return immediately.
  if (Serial_available() == 0) return;

  // Read SOF byte without waiting once data is present.
  if (!uartReadByte(&byteIn, 0) || byteIn != FRAME_SOF) return;

  // SOF confirmed — receive the rest of the frame with inter-byte timeout
  if (!uartReadByte(&cmd, INTER_BYTE_US)) {
    satInc(&diagTimeoutErr);
    return;
  }
  diagLastCmd = cmd;

  if (!uartReadByte(&len, INTER_BYTE_US)) {
    satInc(&diagTimeoutErr);
    return;
  }

  if (len > MAX_PAYLOAD_LEN) {
    sendNack(cmd, ERR_BAD_LEN);
    satInc(&diagBadLenErr);
    return;
  }

  for (uint8_t i = 0; i < len; i++) {
    if (!uartReadByte(&rxPayload[i], INTER_BYTE_US)) {
      satInc(&diagTimeoutErr);
      return;
    }
  }

  if (!uartReadByte(&crc, INTER_BYTE_US)) {
    satInc(&diagTimeoutErr);
    return;
  }

  if (crc != frameCrc(cmd, len, rxPayload)) {
    sendNack(cmd, ERR_BAD_CRC);
    satInc(&diagCrcErr);
    return;
  }

  satInc(&diagRxOk);
  handleRequest(cmd, len, pwLocked, batMv);
}
