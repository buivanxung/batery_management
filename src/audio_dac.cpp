#include "audio_dac.h"
#include "pinout.h"
#include "logger.h"
#include <string.h>

// ===== Audio Playback State =====
typedef struct
{
  SPIFlash *flash;
  uint32_t dataAddr;
  uint32_t dataLength;
  uint32_t playbackPos;
  bool isPlaying;
} AudioState_t;

// Use volatile to prevent compiler from optimizing away state changes
// (important if implementing interrupt-based playback in future)
static volatile AudioState_t audioState = {0};

// ===== External function =====
extern bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength);

// ===== Public API =====

void audioDacInit(void)
{
  pinMode(AUDIO_PWM_PIN, OUTPUT);
  analogWrite(AUDIO_PWM_PIN, 128); // Center: 0-255, center is 128
  memset((void *)&audioState, 0, sizeof(audioState));
  logPrintln("audioDacInit initialized");
}

bool audioDacPlayFile(SPIFlash *flash, const char *filename)
{
  if (!flash || !filename)
    return false;

  // Stop any current playback
  audioDacStop();

  // Get file info
  uint32_t addr, length;
  if (!flashFsGetFileInfo(flash, filename, addr, length))
    return false;

  // Setup state
  audioState.flash = flash;
  audioState.dataAddr = addr;
  audioState.dataLength = length;
  audioState.playbackPos = 0;
  audioState.isPlaying = true;

  // Playback loop (blocking)
  uint8_t buffer[256];
  const uint32_t periodUs = 1000000UL / AUDIO_SAMPLE_RATE;
  uint32_t remaining = length;
  uint32_t offset = 0;

  while (remaining > 0 && audioState.isPlaying)
  {
    uint32_t chunkSize = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);

    if (!flash->readByteArray(addr + offset, buffer, chunkSize, true))
      return false;

    for (uint32_t i = 0; i < chunkSize; ++i)
    {
      // Convert signed 8-bit PCM to PWM with 1.3x GAIN (less loud)
      int16_t sample = (int8_t)buffer[i];
      uint8_t pwmValue = (uint8_t)constrain(sample * 1.3 + 128, 0, 255);
      analogWrite(AUDIO_PWM_PIN, pwmValue);
      delayMicroseconds(periodUs);
    }

    offset += chunkSize;
    remaining -= chunkSize;
    audioState.playbackPos = offset;
  }

  // Cleanup
  analogWrite(AUDIO_PWM_PIN, 128); // Center: 128 is the neutral point
  audioState.isPlaying = false;
  return true;
}

bool audioDacIsPlaying(void)
{
  return audioState.isPlaying;
}

void audioDacStop(void)
{
  audioState.isPlaying = false;
  analogWrite(AUDIO_PWM_PIN, 128); // Center: 128 is the neutral point
  memset((void *)&audioState, 0, sizeof(audioState));
}

uint8_t audioDacGetProgress(void)
{
  if (audioState.dataLength == 0)
    return 0;

  uint32_t pos = audioState.playbackPos;
  if (pos > audioState.dataLength)
    pos = audioState.dataLength;

  return (uint8_t)((pos * 100UL) / audioState.dataLength);
}

void test_beep(int duration_ms) {
  logPrintln("BEEP TEST: 1kHz sine wave");
  
  const uint32_t samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
  const float freq_hz = 1000.0f;
  
  for(uint32_t t = 0; t < samples; t++) {
    float phase = 2 * 3.14159f * freq_hz * t / AUDIO_SAMPLE_RATE;
    int16_t sample = 60 * sin(phase);  // 60 peak - less ear-piercing
    uint8_t pwm = (uint8_t)constrain(sample + 128, 0, 255);
    analogWrite(AUDIO_PWM_PIN, pwm);
    delayMicroseconds(1000000UL / AUDIO_SAMPLE_RATE);
  }
  analogWrite(AUDIO_PWM_PIN, 128);
  logPrintln("BEEP DONE");
}

// Placeholder interrupt handler (not used)
extern "C" void TIM2_IRQHandler(void)
{
}
