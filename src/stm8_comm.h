#pragma once

#include <Arduino.h>

// Number of STM8 slave boards connected via 74HC4051 mux
#define STM8_SLOT_COUNT  8

/**
 * Result of a single-wire UART transaction with an STM8 board.
 *   ok       – true = ACK received; false = NACK, CRC error, or timeout
 *   pwLocked – power-key locked state reported by STM8
 *   pwKeyOn  – current power-key output state on the STM8 (charges the battery)
 *   batMv    – battery voltage in mV as measured by STM8 internal ADC
 *   errCode  – NACK error code when !ok; 0 = timeout / framing error
 */
typedef struct {
    bool     ok;
    bool     pwLocked;
    bool     pwKeyOn;
    uint16_t batMv;
    uint8_t  errCode;
} Stm8Status_t;

/**
 * @brief Initialize MUX select pins (OUTPUT) and POINT_CTR signal line.
 *        Must be called once before any stm8GetStatus() / stm8SetPwKey() call.
 */
void stm8CommInit(void);

/**
 * @brief Send CMD_GET_STATUS to the STM8 on the given slot (0-7) via 74HC4051 mux.
 *        Returns voltage in mV, power-key state, and lock state from the STM8.
 */
Stm8Status_t stm8GetStatus(uint8_t slot);

/**
 * @brief Send CMD_SET_PW_KEY to the STM8 on the given slot (0-7).
 * @param on  true = turn power key ON (enable charging), false = OFF
 */
Stm8Status_t stm8SetPwKey(uint8_t slot, bool on);

/**
 * @brief Send CMD_SET_PW_TIMER to the STM8 on the given slot.
 *        The STM8 will keep PIN_PW_KEY HIGH for the given duration,
 *        then automatically turn it OFF when the timer expires.
 * @param slot     Slot number (0-7)
 * @param minutes  Duration in minutes. 0 = cancel timer and turn OFF immediately.
 */
Stm8Status_t stm8SetPwTimer(uint8_t slot, uint16_t minutes);

/**
 * @brief Send CMD_GET_DATA to the STM8 on the given slot.
 *        Lightweight version that only returns millivolts and locked status (3-byte response).
 * @param slot  Slot number (0-7)
 * @return status with batMv and pwLocked populated; pwKeyOn not provided
 */
Stm8Status_t stm8GetData(uint8_t slot);

/**
 * @brief Lock STM8 communication bus for long audio playback section.
 *        While held, stm8GetStatus()/stm8SetPwTimer()/... will block.
 */
void stm8AudioLock(void);

/**
 * @brief Release lock acquired by stm8AudioLock().
 */
void stm8AudioUnlock(void);
