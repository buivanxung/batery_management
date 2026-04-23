#include "stm8_comm.h"
#include "pinout.h"
#include "logger.h"
#include <STM32FreeRTOS.h>

// ── Protocol constants (must match STM8 signal_proto.c) ──────────────────────
#define STM8_BAUD         2400U
#define FRAME_SOF         0xA5u
#define CMD_GET_STATUS    0x01u
#define CMD_SET_PW_KEY    0x02u
#define CMD_SET_PW_TIMER  0x03u  // payload: 2 bytes = minutes big-endian; 0 = cancel
#define CMD_GET_DATA      0x04u  // payload: 3 bytes = {batMv_hi, batMv_lo, pwLocked}
#define CMD_ACK_MASK      0x80u
#define CMD_NACK          0x7Fu
#define MAX_PAYLOAD       8u
#define STM8_TXRX_RETRIES 3u

#define STM8_DIAG_ENABLE  1
#define STM8_DIAG_SLOT    4u

// ── Timeouts ──────────────────────────────────────────────────────────────────
// COMPATIBILITY: STM8 uses hardware UART, STM32 uses bit-bang UART on same line.
// Timeouts are generous to accommodate hardware UART latency variability.
// STM8 turnaround time ~2 ms + hardware buffering; allow 100 ms for SOF.
#define RX_SOF_TIMEOUT_US    100000UL
// Inter-byte timeout increased from 3ms to 10ms to match STM8 signalProtoInit.
// Hardware UART RX buffering may accumulate bytes before CPU reads them.
#define INTER_BYTE_TO_US     10000UL

// ── Bit-bang helpers ──────────────────────────────────────────────────────────

static SemaphoreHandle_t stm8Mutex = nullptr;
static SemaphoreHandle_t stm8BusArbMutex = nullptr;
static volatile bool stm8AudioPending = false;
static uint8_t stm8LastRxDiag = 0u;
static bool stm8DiagScanActive = false;
static uint32_t stm8LastDiagScanMs = 0;

enum {
    STM8_DIAG_OK = 0,
    STM8_DIAG_TIMEOUT_SOF,
    STM8_DIAG_BAD_SOF,
    STM8_DIAG_TIMEOUT_CMD,
    STM8_DIAG_TIMEOUT_LEN,
    STM8_DIAG_BAD_LEN,
    STM8_DIAG_TIMEOUT_PAYLOAD,
    STM8_DIAG_TIMEOUT_CRC,
    STM8_DIAG_BAD_CRC
};

static void stm8DiagScanAllChannels(void)
{
#if STM8_DIAG_ENABLE
    if (stm8DiagScanActive) return;
    stm8DiagScanActive = true;

    logPrintln("[STM8_DIAG] ---- Begin full MUX scan ----");
    // for (uint8_t i = 0; i < STM8_SLOT_COUNT; i++) {
    //     Stm8Status_t s = stm8GetStatus(i);
    //     logPrintf("[STM8_DIAG] scan slot=%u ok=%d batMv=%u lock=%d err=%u\n",
    //               i, s.ok, s.batMv, s.pwLocked, s.errCode);
    // }
    logPrintln("[STM8_DIAG] ---- End full MUX scan ----");

    stm8DiagScanActive = false;
#endif
}

static uint8_t stm8CalcCrc(uint8_t cmd, uint8_t len, const uint8_t *pl)
{
    uint8_t c = FRAME_SOF ^ cmd ^ len;
    for (uint8_t i = 0; i < len; i++) c ^= pl[i];
    return c;
}

// Transmit one byte LSB-first. Open-drain mode:
//   HIGH bit → INPUT_PULLUP  (10 kΩ pull-up drives line to 5 V; STM8 VIH = 3.5 V ✓)
//   LOW  bit → OUTPUT LOW    (STM32 actively pulls to 0 V)
// This avoids driving 3.3 V HIGH into a 5 V bus where STM8 VIH min = 3.5 V.
static void stm8TxByte(uint8_t val)
{
    const uint32_t b = 1000000UL / STM8_BAUD;

    // Protect bit timing from ISR jitter during one UART byte.
    taskENTER_CRITICAL();
    // Start bit: pull LOW
    pinMode(MUX_SIG, OUTPUT);
    digitalWrite(MUX_SIG, LOW);
    delayMicroseconds(b);
    for (uint8_t i = 0; i < 8; i++) {
        if (val & 1u) {
            pinMode(MUX_SIG, INPUT_PULLDOWN); // release → 10 kΩ pulls to 5 V
        } else {
            pinMode(MUX_SIG, OUTPUT);
            digitalWrite(MUX_SIG, LOW);
        }
        delayMicroseconds(b);
        val >>= 1;
    }
    // Stop bit: release (HIGH)
    pinMode(MUX_SIG, INPUT_PULLDOWN);
    delayMicroseconds(b);
    taskEXIT_CRITICAL();
}

// Receive one byte. Pin must already be INPUT_PULLUP.
// Returns false on timeout.
static bool stm8RxByte(uint8_t *out, uint32_t timeoutUs)
{
    const uint32_t b = 1000000UL / STM8_BAUD;

    // Wait for start bit (LOW)
    uint32_t t0 = micros();
    while (digitalRead(MUX_SIG) != LOW) {
        if ((micros() - t0) >= timeoutUs) return false;
    }

    // Critical section only while sampling bits at fixed timing.
    taskENTER_CRITICAL();

    // Skip to centre of bit 0
    delayMicroseconds(b + b / 2u);

    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (digitalRead(MUX_SIG) == HIGH) v |= (1u << i);
        delayMicroseconds(b);
    }
    delayMicroseconds(b);   // consume stop bit
    taskEXIT_CRITICAL();
    *out = v;
    return true;
}

// Send a complete request frame (open-drain TX, then release for RX).
// 
// COMPATIBILITY: STM8 receiver uses hardware UART with RX buffering.
// This adds non-deterministic latency to frame reception, so:
// 1) We allow longer RX_SOF_TIMEOUT_US (100ms vs 50ms)
// 2) We allow longer INTER_BYTE_TO_US (10ms vs 3ms)
// 3) STM8 transmits with hardware UART, so RX mute period accounts for TX buffering
//
static void stm8TxFrame(uint8_t cmd, uint8_t len, const uint8_t *pl)
{
    uint8_t crc = stm8CalcCrc(cmd, len, pl);

    // Debug: log TX frame
    logPrint("[STM8_UART][TX] ");
    logPrintf("%02X %02X %02X ", (unsigned)FRAME_SOF, (unsigned)cmd, (unsigned)len);
    for (uint8_t i = 0; i < len; i++) logPrintf("%02X ", pl ? pl[i] : 0);
    logPrintf("%02X\n", crc);

    // Ensure line is released (idle HIGH via 10 kΩ) before we start
    pinMode(MUX_SIG, INPUT_PULLDOWN);
    delayMicroseconds(500);

    stm8TxByte(FRAME_SOF);
    stm8TxByte(cmd);
    stm8TxByte(len);
    for (uint8_t i = 0; i < len; i++) stm8TxByte(pl ? pl[i] : 0);
    stm8TxByte(crc);
    // After last stop bit stm8TxByte already leaves pin as INPUT_PULLUP
}

// Switch to INPUT_PULLUP and receive one response frame.
// Returns false on timeout, bad SOF, oversized payload, or CRC mismatch.
static bool stm8RxFrame(uint8_t *cmd, uint8_t *len, uint8_t *pl)
{
    pinMode(MUX_SIG, INPUT_PULLDOWN); // Ensure line is released for STM8 to drive

    uint8_t b;
    uint8_t rxbuf[16];
    int rxidx = 0;
    stm8LastRxDiag = STM8_DIAG_OK;
    if (!stm8RxByte(&b, RX_SOF_TIMEOUT_US)) {
        stm8LastRxDiag = STM8_DIAG_TIMEOUT_SOF;
        return false;
    }
    rxbuf[rxidx++] = b;
    if (b != FRAME_SOF) {
        stm8LastRxDiag = STM8_DIAG_BAD_SOF;
        goto log_and_fail;
    }
    if (!stm8RxByte(cmd, INTER_BYTE_TO_US)) {
        stm8LastRxDiag = STM8_DIAG_TIMEOUT_CMD;
        goto log_and_fail;
    }
    rxbuf[rxidx++] = *cmd;
    if (!stm8RxByte(len, INTER_BYTE_TO_US)) {
        stm8LastRxDiag = STM8_DIAG_TIMEOUT_LEN;
        goto log_and_fail;
    }
    rxbuf[rxidx++] = *len;
    if (*len > MAX_PAYLOAD) {
        stm8LastRxDiag = STM8_DIAG_BAD_LEN;
        goto log_and_fail;
    }
    for (uint8_t i = 0; i < *len; i++) {
        if (!stm8RxByte(&pl[i], INTER_BYTE_TO_US)) {
            stm8LastRxDiag = STM8_DIAG_TIMEOUT_PAYLOAD;
            goto log_and_fail;
        }
        rxbuf[rxidx++] = pl[i];
    }
    uint8_t crc;
    if (!stm8RxByte(&crc, INTER_BYTE_TO_US)) {
        stm8LastRxDiag = STM8_DIAG_TIMEOUT_CRC;
        goto log_and_fail;
    }
    rxbuf[rxidx++] = crc;

    if (crc != stm8CalcCrc(*cmd, *len, pl)) {
        stm8LastRxDiag = STM8_DIAG_BAD_CRC;
        goto log_and_fail;
    }

    // Debug: log RX frame
    logPrint("[STM8_UART][RX] ");
    for (int i = 0; i < rxidx; i++) logPrintf("%02X ", rxbuf[i]);
    logPrint("\n");

    stm8LastRxDiag = STM8_DIAG_OK;
    return true;

log_and_fail:
    // Log partial RX frame if failed
    logPrint("[STM8_UART][RX] (fail) ");
    for (int i = 0; i < rxidx; i++) logPrintf("%02X ", rxbuf[i]);
    logPrint("\n");
    return false;
}

// ── Core transaction ──────────────────────────────────────────────────────────

static Stm8Status_t stm8DoTransaction(uint8_t slot,
                                       uint8_t cmd,
                                       uint8_t plen,
                                       const uint8_t *pl)
{
    Stm8Status_t r = {false, false, false, 0u, 0u};
    if (slot >= STM8_SLOT_COUNT) return r;

    // Audio has highest priority: do not start new STM8 transactions while pending.
    if (stm8AudioPending) return r;

    // Audio and STM8 share timing-sensitive resources. Serialize access.
    if (stm8BusArbMutex)
        xSemaphoreTake(stm8BusArbMutex, portMAX_DELAY);

    // Re-check after lock acquisition in case audio requested while we were waiting.
    if (stm8AudioPending) {
        if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);
        return r;
    }

    if (stm8Mutex && xSemaphoreTake(stm8Mutex, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);
        return r;  // could not acquire bus within 200 ms
    }

    uint8_t rxCmd = 0, rxLen = 0, rxPl[MAX_PAYLOAD];
    bool ok = false;
    uint8_t attemptsUsed = 0;

    for (uint8_t attempt = 0; attempt < STM8_TXRX_RETRIES; attempt++) {
        if (stm8AudioPending) break;
        attemptsUsed = (uint8_t)(attempt + 1u);
        // Select the target STM8 via 74HC4051
        digitalWrite(MUX_S0,  slot & 0x01u);
        digitalWrite(MUX_S1, (slot >> 1) & 0x01u);
        digitalWrite(MUX_S2, (slot >> 2) & 0x01u);
        delayMicroseconds(100);   // allow mux + line to settle before frame

        // Send request then wait response
        stm8TxFrame(cmd, plen, pl);
        ok = stm8RxFrame(&rxCmd, &rxLen, rxPl);
        if (ok) {
#if STM8_DIAG_ENABLE
            if (attemptsUsed > 1u) {
                logPrintf("[STM8_DIAG] slot=%u cmd=0x%02X recovered on attempt=%u/%u\n",
                          slot, cmd, attemptsUsed, (uint8_t)STM8_TXRX_RETRIES);
            }
#endif
            break;
        }

        // Fast retry for cases where STM8 missed SOF due to polling window.
        delayMicroseconds(1500);
    }

    // Restore MUX_SIG to INPUT_PULLUP (idle)
    pinMode(MUX_SIG, INPUT_PULLDOWN);

    if (stm8Mutex) xSemaphoreGive(stm8Mutex);
    if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);

    if (!ok) {
#if STM8_DIAG_ENABLE
        if (slot == STM8_DIAG_SLOT) {
            uint8_t sigLevel = (uint8_t)digitalRead(MUX_SIG);
            logPrintf("[STM8_DIAG] slot=%u cmd=0x%02X fail=%u attempt=%u/%u mux(S2,S1,S0)=(%u,%u,%u) sig=%u\n",
                      slot,
                      cmd,
                      stm8LastRxDiag,
                      attemptsUsed,
                      (uint8_t)STM8_TXRX_RETRIES,
                      (slot >> 2) & 0x01u,
                      (slot >> 1) & 0x01u,
                      slot & 0x01u,
                      sigLevel);

            // If SOF timeout persists, scan all channels to identify mapping/bus issues.
            if (!stm8AudioPending && stm8LastRxDiag == STM8_DIAG_TIMEOUT_SOF) {
                uint32_t now = millis();
                if ((now - stm8LastDiagScanMs) > 3000UL) {
                    stm8LastDiagScanMs = now;
                    stm8DiagScanAllChannels();
                }
            }
        }
#endif
        return r;
    }

    if (rxCmd == CMD_NACK) {
        r.errCode = (rxLen >= 2u) ? rxPl[1] : 0xFFu;
        return r;
    }

    // Handle different response formats based on command type
    if (cmd == CMD_GET_STATUS && rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 4u) {
        // CMD_GET_STATUS: payload[4] = {pwLocked, pwKeyOn, batMv_hi, batMv_lo}
        r.ok       = true;
        r.pwLocked = (rxPl[0] != 0u);
        r.pwKeyOn  = (rxPl[1] != 0u);
        r.batMv    = ((uint16_t)rxPl[2] << 8) | rxPl[3];
    } else if (cmd == CMD_GET_DATA && rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 3u) {
        // CMD_GET_DATA: payload[3] = {batMv_hi, batMv_lo, pwLocked}
        r.ok       = true;
        r.batMv    = ((uint16_t)rxPl[0] << 8) | rxPl[1];
        r.pwLocked = (rxPl[2] != 0u);
        r.pwKeyOn  = false;  // Not provided in GET_DATA
    } else if (rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 4u) {
        // Default handler for other commands expecting 4-byte payload
        r.ok       = true;
        r.pwLocked = (rxPl[0] != 0u);
        r.pwKeyOn  = (rxPl[1] != 0u);
        r.batMv    = ((uint16_t)rxPl[2] << 8) | rxPl[3];
    }
    return r;
}

// ── Public API ────────────────────────────────────────────────────────────────

void stm8CommInit(void)
{
    // Mutex (safe to call before scheduler starts)
    if (!stm8Mutex)
        stm8Mutex = xSemaphoreCreateMutex();
    if (!stm8BusArbMutex)
        stm8BusArbMutex = xSemaphoreCreateMutex();

    // MUX select lines
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    digitalWrite(MUX_S0, LOW);
    digitalWrite(MUX_S1, LOW);
    digitalWrite(MUX_S2, LOW);

    // Signal line: idle as INPUT_PULLUP
    pinMode(MUX_SIG, INPUT_PULLDOWN);
}

Stm8Status_t stm8GetStatus(uint8_t slot)
{
    return stm8DoTransaction(slot, CMD_GET_STATUS, 0u, nullptr);
}

Stm8Status_t stm8SetPwKey(uint8_t slot, bool on)
{
    uint8_t pl[1] = {on ? 1u : 0u};
    return stm8DoTransaction(slot, CMD_SET_PW_KEY, 1u, pl);
}

Stm8Status_t stm8SetPwTimer(uint8_t slot, uint16_t minutes)
{
    uint8_t pl[2] = {
        (uint8_t)(minutes >> 8),
        (uint8_t)(minutes & 0xFFu)
    };
    return stm8DoTransaction(slot, CMD_SET_PW_TIMER, 2u, pl);
}

Stm8Status_t stm8GetData(uint8_t slot)
{
    return stm8DoTransaction(slot, CMD_GET_DATA, 0u, nullptr);
}

void stm8AudioLock(void)
{
    stm8AudioPending = true;
    if (stm8BusArbMutex)
        xSemaphoreTake(stm8BusArbMutex, portMAX_DELAY);
}

void stm8AudioUnlock(void)
{
    if (stm8BusArbMutex)
        xSemaphoreGive(stm8BusArbMutex);
    stm8AudioPending = false;
}
