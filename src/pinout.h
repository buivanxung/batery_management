#pragma once

// ===== Pin mapping (cấu hình chân) =====
//
// Update the pin numbers below to match your schematic.
// This file is intended to centralize the board-specific pin mapping
// so the application code in main.cpp can remain readable.

// --- SPI Flash (W25Q32) ---
// The chip select (CS) pin is required by the SPIMemory library.
// SCK / MISO / MOSI are the default SPI pins on Nucleo G070RB.
#define FLASH_SPI_CS_PIN   PA8
#define FLASH_SPI_SCK_PIN  PA5
#define FLASH_SPI_MISO_PIN PA6
#define FLASH_SPI_MOSI_PIN PA7

// --- UART (Serial1) ---
#define SERIAL1_RX_PIN     PA10
#define SERIAL1_TX_PIN     PA9

// --- LEDs ---
// These are example LED pins; adjust to match your schematic.
#define LED_BLINK_PIN      PD1
#define LED_STATUS_PIN     PF0

// --- Motor control (example placeholders) ---
// Update these to match your motor driver pinout in the schematic.
//
// Each motor is controlled by a pair of pins:
//   MOTORx_IN_PIN  - the "IN" input that turns the motor on/off
//   MOTORx_CTRL_PIN - the corresponding MCU control line (enable, gate, etc.)
//
// When motorSet() is called, both pins are driven HIGH for ON and LOW for OFF.
#define MOTOR1_IN_PIN     PB0
#define MOTOR1_CTRL_PIN   PB15

#define MOTOR2_IN_PIN     PB1
#define MOTOR2_CTRL_PIN   PC1

#define MOTOR3_IN_PIN     PB2
#define MOTOR3_CTRL_PIN   PC3

#define MOTOR4_IN_PIN     PB3
#define MOTOR4_CTRL_PIN   PC5

#define MOTOR5_IN_PIN     PB4
#define MOTOR5_CTRL_PIN   PC7

#define MOTOR6_IN_PIN     PB5
#define MOTOR6_CTRL_PIN   PC9

#define MOTOR7_IN_PIN     PB6
#define MOTOR7_CTRL_PIN   PC10

#define MOTOR8_IN_PIN     PB7
#define MOTOR8_CTRL_PIN   PC11

// --- Enable charge battery---
// Enable pin control
#define POGO1_CTR_PIN     PC8
#define POGO2_CTR_PIN     PC6
#define POGO3_CTR_PIN     PD9
#define POGO4_CTR_PIN     PC2
#define POGO5_CTR_PIN     PC0
#define POGO6_CTR_PIN     PB14
#define POGO7_CTR_PIN     PC12
#define POGO8_CTR_PIN     PC13

// --- Audio output (PWM) ---
// Used by the audio playback helper.
#define AUDIO_PWM_PIN      PA4
#define AUDIO_SAMPLE_RATE  8000  // Hz (adjust if needed)

// --- Button input ---
// For multi-function button control.
#define BUTTON_PIN         PB4  // Adjust to your schematic

#define MUX_S0 A11
#define MUX_S1 A12
#define MUX_S2 A15
#define MUX_SIG PA0
