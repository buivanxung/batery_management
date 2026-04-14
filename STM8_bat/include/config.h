#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Pin definitions (STM8S003F3P6 - sduino framework)
// ============================================================
#define PIN_BT_KEY       PA3  // PA3  - Button input, pulled up via R4 (21.8k), active LOW
// PD5(TX)/PD6(RX) tied to 74HC4051 mux via 22Ω resistors.
// Bit-bang UART on PD5 (PIN_SIGNAL_OUT). Hardware UART1 must NOT run.
#define PIN_SIGNAL_OUT   PD5  // PD5  - bit-bang UART to STM32 via 74HC4051 mux
#define PIN_PW_KEY       PD3  // PD3  - Power key to IP5189T charger IC (HIGH = OFF, LOW = ON)
#define PIN_RET_BAT      PD2  // AIN4 / PD2 - Battery voltage ADC (R10/R7 divider 510k+510k)

// IP5189 LED feedback pins
#define PIN_IP_GREEN      PC7
#define PIN_IP_BLUE      PC3
#define PIN_IP_RED      PC6

#define PIN_IP_LED_BAT_STATUS    PA2
#define PIN_CHG_STATUS    PB5

// 4900, 5361
// Many IP5189 LED lines are active-low (LED lights when line is LOW).
#define IP_LED_ON_LEVEL  LOW

#define PW_KEY_ON        LOW
#define PW_KEY_OFF       HIGH

// If PW_KEY is ON but no LED is HIGH after this timeout, force PW_KEY_OFF.
#define PW_ON_VERIFY_TIMEOUT_MS 1500

// UART1 baud rate for STM32 communication (PD5 TX / PD6 RX via 74HC4051 mux)
#define SIGNAL_UART_BAUD 9600

// Debug Serial baud — NOTE: Serial is now used for SIGNAL_UART_BAUD comms.
// Set DEBUG=1 in main.c only if a separate debug channel is available.
#define DEBUG_BAUD       115200

// Battery voltage divider: BAT+ --[R10 510k]-- node --[R7 510k]-- GND
// VBAT(mV) = ADC_raw * 5000 / 1024 * 2
#define VREF_MV          5000
#define ADC_MAX          1024
#define DIV_RATIO        2     // (R10+R7)/R7 with R10=R7=510k

// Battery protection thresholds (mV)
#define BAT_LOW_MV       3500  // LED warning threshold
#define BAT_CUTOFF_MV    3000  // Force output off

// Button timing (ms)
#define DEBOUNCE_MS      50
#define LONG_PRESS_MS    2000
#define LOCK_HOLD_MS     10000  // hold 10s to enter lock/unlock sequence mode
#define TAP_TIMEOUT_MS   5000   // 5s window to complete tap sequence after hold
#define DOUBLE_TAP_MS    350    // max gap to classify short press pattern as 1x/2x
#define LOCK_TAPS        3      // 3 short presses to lock PIN_PW_KEY
#define UNLOCK_TAPS      5      // 5 short presses to unlock PIN_PW_KEY
#define LOCK_RED_ON_MS   5000   // red LED indication time while locked

#endif // CONFIG_H
