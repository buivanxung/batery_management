
#include <Arduino.h>
#include "config.h"
#include "signal_proto.h"

#define FRAME_SOF 0xA5
#define CMD_GET_STATUS 0x01
#define CMD_SET_PW_KEY 0x02
#define CMD_SET_PW_TIMER 0x03 // payload: 2 bytes = minutes (big-endian); 0 = cancel
#define CMD_GET_DATA 0x04     // Get millivolts + locked status
#define CMD_GET_DIAG 0x05     // Debug counters for protocol diagnosis
#define CMD_ACK_MASK 0x80
#define CMD_NACK 0x7F

#define ERR_BAD_CMD 0x01
#define ERR_BAD_LEN 0x02
#define ERR_BAD_VALUE 0x03
#define ERR_LOCKED 0x04
#define ERR_BAD_CRC 0x05

#define MAX_PAYLOAD_LEN 8
#define INTER_BYTE_US 10000UL // max wait between bytes within one frame
#define UART_ECHO_TEST_MODE 0 // Enable echo test mode for debugging

// With fixed 1-wire hardware wiring, bytes transmitted on PD5 can be seen back on PD6.
// Mute RX briefly after each TX frame to avoid parsing our own response as a new request.
#define RX_MUTE_MARGIN_US 50000UL // 50ms: enough for 5 bytes at 2400 baud (~20ms) + margin

// Bit timing for 9600 baud (microseconds per bit)
#define BIT_TIME_US (1000000UL / SIGNAL_UART_BAUD) // ~104us

static void bitbangTxByte(uint8_t val);

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
static unsigned long pwTimerStartMs = 0;
static unsigned long pwTimerDurationMs = 0;
static bool pwTimerActive = false;
static bool pd6PrevHigh = true;
static unsigned long rxMuteUntilUs = 0;

static void satInc(uint8_t *v)
{
  if (*v < 0xFF)
  {
    (*v)++;
  }
}


// BIT-BANG UART (Open-drain TX, required for one-wire half-duplex bus)
// ============================================================================
// Hardware: PD5(TX) --R5(22Ω)-- Signal_out --R6(22Ω)--R7(22Ω)-- PD6(RX)
//           Signal_out --R8(22Ω)-- 74HC4051 -- PA0(STM32) -- R9(10K)-- 5V
//
// TX idle: PD5 = INPUT (high-impedance), line pulled HIGH by STM32's 10K pull-up
// TX LOW:  PD5 = OUTPUT LOW (actively drive low)
// TX HIGH: PD5 = INPUT (release, let pull-up bring it HIGH)
//
// This matches STM32's open-drain bit-bang implementation.
// ============================================================================

// Transmit one byte LSB-first using bit-bang open-drain.
// HIGH = release (INPUT), LOW = drive (OUTPUT LOW)
static void bitbangTxByte(uint8_t val)
{
  // DEBUG: blink BLUE LED when transmitting a byte
  // digitalWrite(PIN_IP_BLUE, (digitalRead(PIN_IP_BLUE) == HIGH) ? LOW : HIGH);

  // Start bit: drive LOW
  pinMode(PD5, OUTPUT);
  digitalWrite(PD5, LOW);
  delayMicroseconds(BIT_TIME_US);

  // Data bits (LSB first)
  for (uint8_t i = 0; i < 8; i++)
  {
    if (val & 0x01)
    {
      pinMode(PD5, INPUT); // Release for HIGH (pull-up makes it 5V)
    }
    else
    {
      pinMode(PD5, OUTPUT);
      digitalWrite(PD5, LOW);
    }
    delayMicroseconds(BIT_TIME_US);
    val >>= 1;
  }

  // Stop bit: release (HIGH)
  pinMode(PD5, INPUT);
  delayMicroseconds(BIT_TIME_US);
}

static bool bitbangRxByte(uint8_t *outByte, unsigned long timeoutUs)
{
  unsigned long t0 = micros();
  while (digitalRead(PD6) != LOW)
  {
    if (timeoutUs > 0 && (micros() - t0) >= timeoutUs)
      return false;
  }

  // Đợi 0.5 bit để vào giữa Start Bit
  delayMicroseconds(BIT_TIME_US / 2);

  // Kiểm tra lại nếu vẫn là LOW thì mới đúng là Start Bit
  if (digitalRead(PD6) != LOW)
    return false;

  uint8_t val = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    delayMicroseconds(BIT_TIME_US); // Nhảy sang giữa bit tiếp theo
    if (digitalRead(PD6) == HIGH)
    {
      val |= (1u << i);
    }
  }

  // Đợi nốt Stop bit
  delayMicroseconds(BIT_TIME_US);
  *outByte = val;
  return true;
}

// Check if a start bit is present (non-blocking)
static bool bitbangRxAvailable(void)
{
  return (digitalRead(PD6) == LOW);
}

static uint8_t frameCrc(uint8_t cmd, uint8_t len, const uint8_t *payload)
{
  uint8_t crc = FRAME_SOF ^ cmd ^ len;
  for (uint8_t i = 0; i < len; i++)
  {
    crc ^= payload[i];
  }
  return crc;
}

static void sendFrame(uint8_t cmd, uint8_t len, const uint8_t *payload)
{
  // DEBUG: (disabled for now - too many LEDs)
  // digitalWrite(PIN_IP_GREEN, (digitalRead(PIN_IP_GREEN) == HIGH) ? LOW : HIGH);

  uint8_t crc = frameCrc(cmd, len, payload);

  // Gửi bằng bit-bang UART (PD5 TX)
  bitbangTxByte(FRAME_SOF);
  bitbangTxByte(cmd);
  bitbangTxByte(len);
  for (uint8_t i = 0; i < len; i++)
  {
    bitbangTxByte(payload[i]);
  }
  bitbangTxByte(crc);

  // Set RX mute period to avoid parsing our own transmitted bytes as incoming frames
  // 5ms should be enough for 5 bytes * ~4ms per byte at 2400 baud + margin
  rxMuteUntilUs = micros() + RX_MUTE_MARGIN_US;
}

static void sendNack(uint8_t reqCmd, uint8_t errorCode)
{
  // // DEBUG: blink RED LED pattern based on error code
  // // Each error code: blink RED that many times
  // for (uint8_t i = 0; i < errorCode; i++)
  // {
  //   digitalWrite(PIN_IP_RED, LOW);
  //   delayMicroseconds(100000); // 100ms
  //   digitalWrite(PIN_IP_RED, HIGH);
  //   delayMicroseconds(100000); // 100ms
  // }

  uint8_t payload[2] = {reqCmd, errorCode};
  sendFrame(CMD_NACK, 2, payload);
}

static void sendDiagAck(uint8_t reqCmd)
{
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

static void sendStatusAck(uint8_t reqCmd, bool pwLocked, uint16_t batMv)
{
  bool realOn = (digitalRead(PIN_IP_LED_BAT_STATUS) == HIGH);
  uint8_t payload[4];
  payload[0] = pwLocked ? 1 : 0;
  payload[1] = realOn ? 1 : 0;
  payload[2] = (uint8_t)(batMv >> 8);   // high byte
  payload[3] = (uint8_t)(batMv & 0xFF); // low byte
  sendFrame(reqCmd | CMD_ACK_MASK, 4, payload);
}

static void sendDataAck(uint8_t reqCmd, uint16_t batMv, bool pwLocked)
{
  uint8_t payload[3];
  payload[0] = (uint8_t)(batMv >> 8);   // millivolts high byte
  payload[1] = (uint8_t)(batMv & 0xFF); // millivolts low byte
  payload[2] = pwLocked ? 1 : 0;        // locked status
  sendFrame(reqCmd | CMD_ACK_MASK, 3, payload);
}

static void handleRequest(uint8_t cmd, uint8_t len, bool pwLocked, uint16_t batMv)
{
  if (cmd == CMD_GET_STATUS)
  {
    if (len != 0)
    {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    // Luôn trả về ACK cho GET_STATUS, không kiểm tra pwLocked
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  if (cmd == CMD_GET_DATA)
  {
    if (len != 0)
    {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    sendDataAck(cmd, batMv, pwLocked);
    return;
  }

  if (cmd == CMD_GET_DIAG)
  {
    if (len != 0)
    {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    sendDiagAck(cmd);
    return;
  }

  if (cmd == CMD_SET_PW_KEY)
  {
    if (len != 1)
    {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }

    if (rxPayload[0] > 1)
    {
      sendNack(cmd, ERR_BAD_VALUE);
      return;
    }

    if (pwLocked)
    {
      sendNack(cmd, ERR_LOCKED);
      return;
    }

    // Cancel any running timer before manual override
    pwTimerActive = false;
    digitalWrite(PIN_PW_KEY, rxPayload[0] ? PW_KEY_ON : PW_KEY_OFF);
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  if (cmd == CMD_SET_PW_TIMER)
  {
    if (len != 2)
    {
      sendNack(cmd, ERR_BAD_LEN);
      return;
    }
    if (pwLocked)
    {
      sendNack(cmd, ERR_LOCKED);
      return;
    }
    uint16_t minutes = ((uint16_t)rxPayload[0] << 8) | rxPayload[1];
    if (minutes == 0)
    {
      pwTimerActive = false;
      digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
    }
    else
    {
      pwTimerStartMs = millis();
      pwTimerDurationMs = (unsigned long)minutes * 60000UL;
      pwTimerActive = true;
      digitalWrite(PIN_PW_KEY, PW_KEY_ON);
    }
    sendStatusAck(cmd, pwLocked, batMv);
    return;
  }

  sendNack(cmd, ERR_BAD_CMD);
  satInc(&diagBadCmdErr);
}

void signalProtoInit(void)
{
  // Cấu hình chân RX: input để nhận data từ STM32
  pinMode(PD6, INPUT);
  // Cấu hình chân TX: high-impedance when idle (released)
  pinMode(PD5, INPUT);
  pwTimerActive = false;
  rxMuteUntilUs = 0;
  diagRxOk = 0;
  diagCrcErr = 0;
  diagTimeoutErr = 0;
  diagBadLenErr = 0;
  diagBadCmdErr = 0;
  diagFramingErr = 0;
  diagFalseStartErr = 0;
  diagLastCmd = 0;
}

void signalProtoUpdatePwTimer(void)
{
  if (!pwTimerActive)
    return;
  if ((millis() - pwTimerStartMs) >= pwTimerDurationMs)
  {
    pwTimerActive = false;
    digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
  }
}

void signalProtoPoll(bool pwLocked, uint16_t batMv)
{
  uint8_t byteIn;
  // 1. Đợi byte đầu tiên (SOF)
  if (!bitbangRxByte(&byteIn, 50000UL))
    return;
  if (byteIn != FRAME_SOF)
    return;

  // 2. Nhận các byte còn lại thật nhanh
  uint8_t tempCmd, tempLen, tempCrc;

  if (!bitbangRxByte(&tempCmd, INTER_BYTE_US))
  {
    satInc(&diagTimeoutErr);
    return;
  }
  if (!bitbangRxByte(&tempLen, INTER_BYTE_US))
  {
    satInc(&diagTimeoutErr);
    return;
  }

  if (tempLen > MAX_PAYLOAD_LEN)
  {
    delay(5); // Chờ PC gửi nốt CRC trước khi mình NACK
    sendNack(tempCmd, ERR_BAD_LEN);
    return;
  }

  for (uint8_t i = 0; i < tempLen; i++)
  {
    bitbangRxByte(&rxPayload[i], INTER_BYTE_US);
  }

  if (!bitbangRxByte(&tempCrc, INTER_BYTE_US))
  {
    satInc(&diagTimeoutErr);
    return;
  }

  // 3. Kiểm tra CRC
  if (tempCrc != frameCrc(tempCmd, tempLen, rxPayload))
  {
    delay(5);
    sendNack(tempCmd, ERR_BAD_CRC);
    return;
  }

  // 4. OK -> Xử lý
  delay(20); // Quan trọng để CP2102 kịp đổi chiều
  handleRequest(tempCmd, tempLen, pwLocked, batMv);
}

// Heartbeat: send frame CMD=0x06, LEN=1, PAYLOAD=seq
void sendHeartbeat(void)
{
  static uint8_t seq = 0;
  uint8_t payload = seq++;
  uint8_t crc = FRAME_SOF ^ 0x06 ^ 1 ^ payload;
  bitbangTxByte(FRAME_SOF);
  bitbangTxByte(0x06);
  bitbangTxByte(1);
  bitbangTxByte(payload);
  bitbangTxByte(crc);
}
