#pragma once

#include <Arduino.h>
#include "SPIMemory.h"

// ===== Audio DAC Configuration =====
// Plays 8-bit PCM audio files from flash via PWM on AUDIO_PWM_PIN
// BLOCKING approach: audioDacPlayFile() returns only after playback completes
// Other FreeRTOS tasks can run during playback (task scheduler continues)

/// Initialize audio DAC (PWM pin)
void audioDacInit(void);

/// Play audio file from flash (BLOCKING - returns when playback complete)
/// flash: SPIFlash device pointer
/// filename: Name of audio file stored in flash filesystem
/// Returns true on successful playback, false if file not found or read error
bool audioDacPlayFile(SPIFlash *flash, const char *filename);

/// Get current playback status (true = playing, false = stopped)
bool audioDacIsPlaying(void);

/// Stop current playback immediately
void audioDacStop(void);

/// Get playback progress (0 to 100)
uint8_t audioDacGetProgress(void);

/// Called by timer interrupt (internal use)
extern "C" void TIM2_IRQHandler(void);
