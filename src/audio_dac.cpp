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
      // Convert signed 8-bit PCM (-128 to 127) to unsigned 8-bit (0 to 255)
      // Center at 128: silence = 0 signed -> 128 unsigned
      uint8_t pwmValue = (uint8_t)((int8_t)buffer[i] + 128);
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

// Placeholder interrupt handler (not used in blocking approach)
extern "C" void TIM2_IRQHandler(void)
{
  // This is not used in the simplified blocking implementation
}
