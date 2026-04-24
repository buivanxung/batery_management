#include "stm8_comm.h"
#include "pinout.h"
#include "logger.h"
#include <STM32FreeRTOS.h>

// Thiết lập tỷ lệ 1 để khớp chính xác Baudrate 1200 (1 bit ~ 833us)
#define STM8_BITBANG_DELAY_SCALE 1

// ── Protocol constants ──────────────────────────────────────────────────────
#define STM8_BAUD 1200U
#define FRAME_SOF 0xA5u
#define CMD_GET_STATUS 0x01u
#define CMD_SET_PW_KEY 0x02u
#define CMD_SET_PW_TIMER 0x03u 
#define CMD_GET_DATA 0x04u     
#define CMD_ACK_MASK 0x80u
#define CMD_NACK 0x7Fu
#define MAX_PAYLOAD 8u
#define STM8_TXRX_RETRIES 3u

#define STM8_DIAG_ENABLE 1
#define STM8_DIAG_SLOT 4u

// ── Timeouts ────────────────────────────────────────────────────────────────
#define RX_SOF_TIMEOUT_US 200000UL
#define INTER_BYTE_TO_US 15000UL

// ── Variables ───────────────────────────────────────────────────────────────
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

// ── Helpers ──────────────────────────────────────────────────────────────────

static void stm8DiagScanAllChannels(void) {
#if STM8_DIAG_ENABLE
    if (stm8DiagScanActive) return;
    stm8DiagScanActive = true;
    logPrintln("[STM8_DIAG] ---- Begin full MUX scan ----");
    // Có thể gọi stm8GetStatus lặp qua các slot ở đây nếu cần
    logPrintln("[STM8_DIAG] ---- End full MUX scan ----");
    stm8DiagScanActive = false;
#endif
}

static uint8_t stm8CalcCrc(uint8_t cmd, uint8_t len, const uint8_t *pl) {
    uint8_t c = FRAME_SOF ^ cmd ^ len;
    for (uint8_t i = 0; i < len; i++) c ^= pl[i];
    return c;
}

// Gửi một byte LSB-first dùng chế độ Open-Drain
static void stm8TxByte(uint8_t val) {
    const uint32_t b = (1000000UL / STM8_BAUD) * STM8_BITBANG_DELAY_SCALE;

    taskENTER_CRITICAL();
    // Start bit: Pull LOW
    digitalWrite(MUX_SIG, LOW);
    delayMicroseconds(b);

    // 8 Data bits
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(MUX_SIG, (val & 0x01u) ? HIGH : LOW);
        delayMicroseconds(b);
        val >>= 1;
    }

    // Stop bit: Release HIGH
    digitalWrite(MUX_SIG, HIGH);
    delayMicroseconds(b);
    taskEXIT_CRITICAL();
}

// Nhận một byte LSB-first
static bool stm8RxByte(uint8_t *out, uint32_t timeoutUs) {
    const uint32_t b = (1000000UL / STM8_BAUD) * STM8_BITBANG_DELAY_SCALE;
    uint32_t t0 = micros();

    while (digitalRead(MUX_SIG) != LOW) {
        if ((micros() - t0) >= timeoutUs) return false;
    }

    taskENTER_CRITICAL();
    delayMicroseconds(b / 2u); // Nhảy vào giữa Start-bit

    if (digitalRead(MUX_SIG) != LOW) {
        taskEXIT_CRITICAL();
        return false;
    }

    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; i++) {
        delayMicroseconds(b); // Nhảy vào giữa bit dữ liệu
        if (digitalRead(MUX_SIG) == HIGH) v |= (1u << i);
    }
    delayMicroseconds(b); // Đợi Stop-bit
    taskEXIT_CRITICAL();
    *out = v;
    return true;
}

static void stm8TxFrame(uint8_t cmd, uint8_t len, const uint8_t *pl) {
    uint8_t crc = stm8CalcCrc(cmd, len, pl);

    // logPrint("[STM8_UART][TX] ");
    // logPrintf("%02X %02X %02X ", (unsigned)FRAME_SOF, (unsigned)cmd, (unsigned)len);
    // for (uint8_t i = 0; i < len; i++) logPrintf("%02X ", pl ? pl[i] : 0);
    // logPrintf("%02X\n", crc);

    // Bus Idle rảnh (High-Z)
    digitalWrite(MUX_SIG, HIGH);
    delayMicroseconds(500);

    stm8TxByte(FRAME_SOF);
    stm8TxByte(cmd);
    stm8TxByte(len);
    for (uint8_t i = 0; i < len; i++) stm8TxByte(pl ? pl[i] : 0);
    stm8TxByte(crc);

    // Chờ 20ms để khớp với delay(20) của STM8 trước khi nó phản hồi
    delay(20);
}

static bool stm8RxFrame(uint8_t *cmd, uint8_t *len, uint8_t *pl) {
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

    // logPrint("[STM8_UART][RX] ");
    // for (int i = 0; i < rxidx; i++) logPrintf("%02X ", rxbuf[i]);
    // logPrint("\n");
    return true;

log_and_fail:
    // logPrint("[STM8_UART][RX] (fail) ");
    // for (int i = 0; i < rxidx; i++) logPrintf("%02X ", rxbuf[i]);
    // logPrint("\n");
    return false;
}

// ── Core transaction ──────────────────────────────────────────────────────────

static Stm8Status_t stm8DoTransaction(uint8_t slot, uint8_t cmd, uint8_t plen, const uint8_t *pl) {
    Stm8Status_t r = {false, false, false, 0u, 0u};
    if (slot >= STM8_SLOT_COUNT || stm8AudioPending) return r;

    if (stm8BusArbMutex) xSemaphoreTake(stm8BusArbMutex, portMAX_DELAY);
    if (stm8AudioPending) {
        if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);
        return r;
    }

    if (stm8Mutex && xSemaphoreTake(stm8Mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);
        return r;
    }

    uint8_t rxCmd = 0, rxLen = 0, rxPl[MAX_PAYLOAD];
    bool ok = false;
    uint8_t attemptsUsed = 0;

    for (uint8_t attempt = 0; attempt < STM8_TXRX_RETRIES; attempt++) {
        if (stm8AudioPending) break;
        attemptsUsed = (uint8_t)(attempt + 1u);
        
        // Điều khiển MUX
        digitalWrite(MUX_S0, slot & 0x01u);
        digitalWrite(MUX_S1, (slot >> 1) & 0x01u);
        digitalWrite(MUX_S2, (slot >> 2) & 0x01u);
        delayMicroseconds(200); 

        stm8TxFrame(cmd, plen, pl);
        ok = stm8RxFrame(&rxCmd, &rxLen, rxPl);
        if (ok) break;

        delay(10); 
    }

    // Bus Idle
    digitalWrite(MUX_SIG, HIGH);

    if (stm8Mutex) xSemaphoreGive(stm8Mutex);
    if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);

    if (!ok) {
#if STM8_DIAG_ENABLE
        if (slot == STM8_DIAG_SLOT) {
            logPrintf("[STM8_DIAG] slot=%u cmd=0x%02X fail=%u attempt=%u\n", slot, cmd, stm8LastRxDiag, attemptsUsed);
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

    // Parse Data
    if (cmd == CMD_GET_STATUS && rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 4u) {
        r.ok = true;
        r.pwLocked = (rxPl[0] != 0u);
        r.pwKeyOn = (rxPl[1] != 0u);
        r.batMv = ((uint16_t)rxPl[2] << 8) | rxPl[3];
    } else if (cmd == CMD_GET_DATA && rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 3u) {
        r.ok = true;
        r.batMv = ((uint16_t)rxPl[0] << 8) | rxPl[1];
        r.pwLocked = (rxPl[2] != 0u);
    } else if (rxCmd == (cmd | CMD_ACK_MASK) && rxLen >= 4u) {
        r.ok = true;
        r.pwLocked = (rxPl[0] != 0u);
        r.pwKeyOn = (rxPl[1] != 0u);
        r.batMv = ((uint16_t)rxPl[2] << 8) | rxPl[3];
    }
    return r;
}

// ── Public API ────────────────────────────────────────────────────────────────

void stm8CommInit(void) {
    if (!stm8Mutex) stm8Mutex = xSemaphoreCreateMutex();
    if (!stm8BusArbMutex) stm8BusArbMutex = xSemaphoreCreateMutex();

    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    
    // Khởi tạo chân Signal là Open Drain
    pinMode(MUX_SIG, OUTPUT_OPEN_DRAIN);
    digitalWrite(MUX_SIG, HIGH); // Idle
}

Stm8Status_t stm8GetStatus(uint8_t slot) {
    return stm8DoTransaction(slot, CMD_GET_STATUS, 0u, nullptr);
}

Stm8Status_t stm8SetPwKey(uint8_t slot, bool on) {
    uint8_t pl[1] = {on ? 1u : 0u};
    return stm8DoTransaction(slot, CMD_SET_PW_KEY, 1u, pl);
}

Stm8Status_t stm8SetPwTimer(uint8_t slot, uint16_t minutes) {
    uint8_t pl[2] = {(uint8_t)(minutes >> 8), (uint8_t)(minutes & 0xFFu)};
    return stm8DoTransaction(slot, CMD_SET_PW_TIMER, 2u, pl);
}

Stm8Status_t stm8GetData(uint8_t slot) {
    return stm8DoTransaction(slot, CMD_GET_DATA, 0u, nullptr);
}

void stm8AudioLock(void) {
    stm8AudioPending = true;
    if (stm8BusArbMutex) xSemaphoreTake(stm8BusArbMutex, portMAX_DELAY);
}

void stm8AudioUnlock(void) {
    if (stm8BusArbMutex) xSemaphoreGive(stm8BusArbMutex);
    stm8AudioPending = false;
}