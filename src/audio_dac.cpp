#include "audio_dac.h"
#include "pinout.h"
#include "logger.h"
#include <string.h>
#include <math.h>
#include <HardwareTimer.h>

// ===== Audio Playback State =====
typedef struct
{
  SPIFlash *flash;
  uint32_t dataAddr;
  uint32_t dataLength;
  uint32_t playbackPos;
  uint32_t bytesRead;
  uint8_t buffer[256];  // Non-volatile for flash access
  uint32_t bufferIdx;
  uint32_t bufferLen;
  bool isPlaying;
  bool streamComplete;
} AudioState_t;

static volatile AudioState_t audioState = {0};
static HardwareTimer *audioTimer = NULL;

// ===== External function =====
extern bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength);

// ===== Timer Interrupt Handler =====
// Called at 8kHz (every 125µs) to output next audio sample
void audioTimerCallback(void)
{
  if (!audioState.isPlaying)
    return;

  // Check if we need to read more data
  if (audioState.bufferIdx >= audioState.bufferLen)
  {
    // Current buffer exhausted, read next chunk
    if (audioState.bytesRead < audioState.dataLength)
    {
      uint32_t toRead = audioState.dataLength - audioState.bytesRead;
      if (toRead > sizeof(audioState.buffer))
        toRead = sizeof(audioState.buffer);

      if (audioState.flash && audioState.flash->readByteArray(
            audioState.dataAddr + audioState.bytesRead,
            (uint8_t *)audioState.buffer, toRead, true))
      {
        audioState.bufferLen = toRead;
        audioState.bytesRead += toRead;
        audioState.bufferIdx = 0;
      }
      else
      {
        // Read error
        audioState.isPlaying = false;
        return;
      }
    }
    else
    {
      // All data read, mark complete
      audioState.streamComplete = true;
      audioState.isPlaying = false;
      analogWrite(AUDIO_PWM_PIN, 0);  // Rest at 0V
      return;
    }
  }

  // Output current sample
  // PWM range: 0-255 represents 0V to Vcc
  // Audio samples: signal centered at 127 (0V AC reference)
  if (audioState.bufferIdx < audioState.bufferLen)
  {
    int16_t sample = (int8_t)audioState.buffer[audioState.bufferIdx++];
    // Scale: -128 to +127 → 0 to 255 via: (sample * 0.8) + 127
    uint8_t pwmValue = (uint8_t)constrain(sample * 0.8 + 127, 0, 255);
    analogWrite(AUDIO_PWM_PIN, pwmValue);
    audioState.playbackPos++;
  }
}

// ===== Public API =====

void audioDacInit(void)
{
  pinMode(AUDIO_PWM_PIN, OUTPUT);
  analogWrite(AUDIO_PWM_PIN, 0);  // Rest at 0V (not 128)
  memset((void *)&audioState, 0, sizeof(audioState));

  // Setup TIM15 for 8kHz interrupt (125µs period)
  // STM32G070 has: TIM1, TIM3, TIM14, TIM15, TIM16, TIM17 (no TIM2)
  audioTimer = new HardwareTimer(TIM15);
  audioTimer->setOverflow(AUDIO_SAMPLE_RATE, HERTZ_FORMAT);  // 8000 Hz
  audioTimer->attachInterrupt(audioTimerCallback);

  // Setup PWM on PA4 (TIM14) for 100kHz carrier
  HardwareTimer *pwmTimer = new HardwareTimer(TIM14);
  pwmTimer->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, AUDIO_PWM_PIN);
  pwmTimer->setOverflow(100000, HERTZ_FORMAT);  // 100 kHz
  pwmTimer->resume();

  logPrintln("audioDacInit: Timer + PWM ready (8kHz audio, 100kHz PWM)");
}

bool audioDacPlayFile(SPIFlash *flash, const char *filename)
{
  if (!flash || !filename)
    return false;

  // Stop current playback
  audioDacStop();

  // Get file info
  uint32_t addr, length;
  if (!flashFsGetFileInfo(flash, filename, addr, length))
  {
    logPrintln("ERROR: File not found");
    return false;
  }

  logPrintf("Playing: %s (addr=0x%X, len=%u bytes, %.1fs)\n",
            filename, addr, length, (float)length / AUDIO_SAMPLE_RATE);

  // Initialize playback state
  audioState.flash = flash;
  audioState.dataAddr = addr;
  audioState.dataLength = length;
  audioState.playbackPos = 0;
  audioState.bytesRead = 0;
  audioState.bufferIdx = 0;
  audioState.bufferLen = 0;
  audioState.streamComplete = false;
  audioState.isPlaying = false;

  // Read first chunk
  uint32_t toRead = (length < sizeof(audioState.buffer)) ? length : sizeof(audioState.buffer);
  if (!flash->readByteArray(addr, (uint8_t *)audioState.buffer, toRead, true))
  {
    logPrintln("ERROR: Flash read failed");
    return false;
  }

  audioState.bufferLen = toRead;
  audioState.bytesRead = toRead;
  audioState.bufferIdx = 0;

  // Debug output
  logPrintf("First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            audioState.buffer[0], audioState.buffer[1],
            audioState.buffer[2], audioState.buffer[3],
            audioState.buffer[4], audioState.buffer[5],
            audioState.buffer[6], audioState.buffer[7]);

  // Start playback (non-blocking)
  audioState.isPlaying = true;
  audioTimer->resume();

  // Wait for completion
  while (audioState.isPlaying)
  {
    vTaskDelay(10);  // Yield to FreeRTOS
  }

  audioTimer->pause();
  logPrintf("Playback done: %u samples played\n", audioState.playbackPos);
  return audioState.streamComplete;
}

bool audioDacIsPlaying(void)
{
  return audioState.isPlaying;
}

void audioDacStop(void)
{
  audioState.isPlaying = false;
  analogWrite(AUDIO_PWM_PIN, 0);  // Rest at 0V (not 128)
  memset((void *)&audioState, 0, sizeof(audioState));
  if (audioTimer)
    audioTimer->pause();
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

void test_beep(int duration_ms)
{
  logPrintln("BEEP TEST: 1kHz sine wave (2s)");
  
  // Temporarily setup audio buffer with sine wave
  AudioState_t savedState;
  memcpy(&savedState, (void *)&audioState, sizeof(audioState));
  
  // Generate sine wave samples in buffer
  uint32_t samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
  audioState.dataLength = (samples > sizeof(audioState.buffer)) ? sizeof(audioState.buffer) : samples;
  
  for (uint32_t t = 0; t < audioState.dataLength; t++)
  {
    float phase = 2.0f * 3.14159f * 1000.0f * t / AUDIO_SAMPLE_RATE;
    int16_t sample = (int16_t)(60.0f * sinf(phase));
    audioState.buffer[t] = (uint8_t)sample;
  }
  
  audioState.bufferLen = audioState.dataLength;
  audioState.bufferIdx = 0;
  audioState.bytesRead = audioState.dataLength;
  audioState.playbackPos = 0;
  audioState.isPlaying = true;
  audioState.streamComplete = false;
  audioState.flash = NULL;  // Don't read from flash
  
  // Start playback
  audioTimer->resume();
  
  // Wait for completion
  while (audioState.isPlaying)
  {
    vTaskDelay(10);
  }
  
  audioTimer->pause();
  
  // Restore state
  memcpy((void *)&audioState, &savedState, sizeof(audioState));
  analogWrite(AUDIO_PWM_PIN, 0);  // Rest at 0V
  logPrintln("BEEP DONE");
}
