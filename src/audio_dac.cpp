#include "audio_dac.h"
#include "pinout.h"
#include "logger.h"
#include <string.h>
#include <math.h>
#include <HardwareTimer.h>

// ===== Double-buffer config =====
#define AUDIO_BUF_SIZE   2048   // 2KB per buffer = 128ms at 16kHz
#define FADE_SAMPLES     256    // 16ms fade at 16kHz, power-of-2 → use >>8

typedef struct
{
  SPIFlash *flash;
  uint32_t  dataAddr;
  uint32_t  dataLength;
  uint32_t  totalRead;
  uint32_t  playbackPos;

  uint8_t   bufA[AUDIO_BUF_SIZE];
  uint8_t   bufB[AUDIO_BUF_SIZE];
  uint8_t  *playBuf;
  uint8_t  *fillBuf;

  volatile uint16_t playIdx;
  volatile uint16_t playLen;
  volatile uint16_t fillLen;
  volatile bool     fillReady;
  volatile bool     needFill;
  volatile bool     isPlaying;
  volatile bool     streamComplete;
  volatile uint8_t  lastSample;

  // Fade in/out
  volatile uint16_t fadeIdx;      // 0..FADE_SAMPLES: fade-in counter
  volatile uint16_t fadeOutIdx;   // 0..FADE_SAMPLES: fade-out counter
  volatile bool     doFadeOut;    // trigger fade-out when stream ends
} AudioState_t;

static AudioState_t audioState = {0};
static HardwareTimer *audioTimer = NULL;
static HardwareTimer *pwmTimer   = NULL;

// ===== External function =====
extern bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength);

// ===== Timer Interrupt Handler =====
void audioTimerCallback(void)
{
  if (!audioState.isPlaying)
    return;

  if (audioState.playIdx < audioState.playLen)
  {
    int16_t s = (int8_t)audioState.playBuf[audioState.playIdx++];

    // Fade-in: chỉ scale AC component, DC offset luôn = 128
    if (audioState.fadeIdx < FADE_SAMPLES)
    {
      s = (int16_t)((int32_t)s * audioState.fadeIdx >> 8);
      audioState.fadeIdx++;
    }

    uint8_t v = (uint8_t)(s + 128);
    TIM14->CCR1 = v;
    audioState.lastSample = v;
    audioState.playbackPos++;
  }
  else if (audioState.doFadeOut)
  {
    // Fade-out: scale AC component về 0, DC offset giữ = 128 → không DC shift
    if (audioState.fadeOutIdx < FADE_SAMPLES)
    {
      int16_t s = (int16_t)audioState.lastSample - 128;
      s = (int16_t)((int32_t)s * (FADE_SAMPLES - audioState.fadeOutIdx) >> 8);
      TIM14->CCR1 = (uint8_t)(s + 128);
      audioState.fadeOutIdx++;
    }
    else
    {
      TIM14->CCR1 = 128;  // Keep DC bias, no pop
      audioState.isPlaying = false;
    }
  }
  else
  {
    // playBuf hết - swap ngay nếu fillBuf sẵn sàng
    if (audioState.fillReady)
    {
      uint8_t *tmp       = audioState.playBuf;
      audioState.playBuf = audioState.fillBuf;
      audioState.fillBuf = tmp;
      audioState.playLen = audioState.fillLen;
      audioState.playIdx = 0;
      audioState.fillReady = false;
      audioState.needFill  = true;
    }
    else if (audioState.streamComplete)
    {
      // Hết data → bắt đầu fade-out
      audioState.doFadeOut  = true;
      audioState.fadeOutIdx = 0;
    }
    else
    {
      // fillBuf chưa ready: giữ sample cuối, không click
      TIM14->CCR1 = audioState.lastSample;
      audioState.needFill = true;
    }
  }
}

// ===== Public API =====

void audioDacInit(void)
{
  memset(&audioState, 0, sizeof(audioState));

  // Setup TIM14 PWM on PA4 - ARR=255, PSC=0 → 64MHz/256 = 250kHz carrier
  // CCR=0 at idle: 0% duty = always LOW = no switching = no heat
  pwmTimer = new HardwareTimer(TIM14);
  pwmTimer->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, AUDIO_PWM_PIN);
  pwmTimer->pause();
  TIM14->PSC  = 0;
  TIM14->ARR  = 255;
  TIM14->CCR1 = 0;    // Idle = 0% duty = pin LOW = no switching = no noise
  TIM14->EGR  = TIM_EGR_UG;
  pwmTimer->resume();

  // Setup TIM15 for 16kHz sample rate interrupt
  audioTimer = new HardwareTimer(TIM15);
  audioTimer->setOverflow(AUDIO_SAMPLE_RATE, HERTZ_FORMAT);
  audioTimer->attachInterrupt(audioTimerCallback);
  audioTimer->pause();  // Ensure NOT running until audioDacPlayFile() calls resume()

  logPrintln("audioDacInit: 250kHz PWM, CCR=128 idle, fade in/out ready");
}

bool audioDacPlayFile(SPIFlash *flash, const char *filename)
{
  if (!flash || !filename)
    return false;

  audioDacStop();

  uint32_t addr, length;
  if (!flashFsGetFileInfo(flash, filename, addr, length))
  {
    logPrintln("ERROR: File not found");
    return false;
  }

  logPrintf("Playing: %s (addr=0x%X, len=%u bytes, %.1fs)\n",
            filename, addr, length, (float)length / AUDIO_SAMPLE_RATE);

  // Init state
  memset(&audioState, 0, sizeof(audioState));
  audioState.flash      = flash;
  audioState.dataAddr   = addr;
  audioState.dataLength = length;
  audioState.playBuf    = audioState.bufA;
  audioState.fillBuf    = audioState.bufB;

  // Pre-load first chunk into playBuf
  uint32_t toRead = (length < AUDIO_BUF_SIZE) ? length : AUDIO_BUF_SIZE;
  if (!flash->readByteArray(addr, audioState.playBuf, toRead, true))
  {
    logPrintln("ERROR: Flash read failed");
    return false;
  }
  audioState.totalRead = toRead;
  audioState.playLen   = (uint16_t)toRead;
  audioState.playIdx   = 0;

  logPrintf("First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            audioState.playBuf[0], audioState.playBuf[1],
            audioState.playBuf[2], audioState.playBuf[3],
            audioState.playBuf[4], audioState.playBuf[5],
            audioState.playBuf[6], audioState.playBuf[7]);

  // Pre-load second chunk into fillBuf (nếu có data)
  if (audioState.totalRead < length)
  {
    uint32_t r2 = length - audioState.totalRead;
    if (r2 > AUDIO_BUF_SIZE) r2 = AUDIO_BUF_SIZE;
    flash->readByteArray(addr + audioState.totalRead, audioState.fillBuf, r2, true);
    audioState.totalRead += r2;
    audioState.fillLen   = (uint16_t)r2;
    audioState.fillReady = true;   // fillBuf sẵn sàng cho ISR swap
  }
  else
  {
    audioState.streamComplete = true;  // File nhỏ hơn 1 buffer
  }

  // DC ramp UP: 0 → 128 trước khi ISR chạy (loại bỏ pop đầu)
  // 128 bước × 200µs ≈ 25ms ramp time - slow enough speaker filter handles smoothly
  for (int16_t v = 0; v <= 128; v++)
  {
    TIM14->CCR1 = (uint8_t)v;
    delayMicroseconds(200);  // 128 × 200µs ≈ 25ms
  }

  audioState.fadeIdx    = 0;
  audioState.fadeOutIdx = 0;
  audioState.doFadeOut  = false;
  audioState.lastSample = 128;
  audioState.isPlaying  = true;
  audioTimer->resume();

  // Waiting loop: task đọc SPI và chuẩn bị fillBuf kế tiếp
  while (audioState.isPlaying)
  {
    if (audioState.needFill && !audioState.fillReady)
    {
      uint32_t remaining = length - audioState.totalRead;
      if (remaining > 0)
      {
        uint32_t r = (remaining > AUDIO_BUF_SIZE) ? AUDIO_BUF_SIZE : remaining;
        flash->readByteArray(addr + audioState.totalRead, audioState.fillBuf, r, true);
        audioState.totalRead += r;
        audioState.fillLen   = (uint16_t)r;

        if (audioState.totalRead >= length)
          audioState.streamComplete = true;

        audioState.fillReady = true;   // Báo ISR: fillBuf sẵn sàng swap
      }
      else
      {
        audioState.streamComplete = true;
      }
      audioState.needFill = false;
    }
    vTaskDelay(1);
  }

  audioTimer->pause();

  // Smooth DC ramp-down: 128→0 in task (slower = no audible pop)
  // 128 bước × 200µs ≈ 25ms total ramp - imperceptible to ear
  for (int16_t v = 128; v >= 0; v--)
  {
    TIM14->CCR1 = (uint8_t)v;
    delayMicroseconds(200);  // 128 × 200µs ≈ 25ms
  }
  TIM14->CCR1 = 0;

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
  if (audioTimer) audioTimer->pause();
  TIM14->CCR1 = 0;  // Idle: no switching, no noise
  memset(&audioState, 0, sizeof(audioState));
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
  logPrintln("BEEP TEST: 1kHz sine wave");

  audioDacStop();

  // Generate sine wave into bufA
  uint32_t samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
  if (samples > AUDIO_BUF_SIZE) samples = AUDIO_BUF_SIZE;

  for (uint32_t t = 0; t < samples; t++)
  {
    float phase = 2.0f * 3.14159f * 1000.0f * t / AUDIO_SAMPLE_RATE;
    audioState.bufA[t] = (uint8_t)((int16_t)(60.0f * sinf(phase)));
  }

  audioState.playBuf       = audioState.bufA;
  audioState.fillBuf       = audioState.bufB;
  audioState.playLen       = (uint16_t)samples;
  audioState.playIdx       = 0;
  audioState.dataLength    = samples;
  audioState.streamComplete = true;
  audioState.flash          = NULL;

  // Enable PWM for beep
  TIM14->CCR1 = 128;
  audioState.isPlaying = true;

  audioTimer->resume();
  while (audioState.isPlaying) vTaskDelay(10);
  audioTimer->pause();
  TIM14->CCR1 = 128;  // Idle = silence
  logPrintln("BEEP DONE");
}
