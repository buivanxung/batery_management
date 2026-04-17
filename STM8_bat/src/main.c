#include <Arduino.h>
#include "config.h"
#include "signal_proto.h"

// Prototype for heartbeat function
extern void sendHeartbeat(void);
//platformio\packages\tool-stm8flash\stm8flash.exe" -c stlinkv2 -p stm8s003f3 -u
// Debug logging control
#define DEBUG 0

#if DEBUG
  #define LOG_STR(s) logWriteStr(s)
  #define LOG_BOOL(v) logWriteBool(v)
  #define LOG_U16(v) logWriteU16(v)
#else
  #define LOG_STR(s) do {} while(0)
  #define LOG_BOOL(v) do {} while(0)
  #define LOG_U16(v) do {} while(0)
#endif

// State tracking
static unsigned long lastLogTime = 0;
static unsigned long holdStartTime = 0;
static bool holdQualified = false;
static bool tapWindowActive = false;
static int tapCount = 0;
static unsigned long tapWindowStartTime = 0;
static bool isLocked = false;
static bool prevBtnRaw = false;
static bool prevBtnDebounced = false;
static bool ledGreenState = false;
static unsigned long ledOffSince = 0;
static unsigned long redLedOnUntil = 0;

// Debouncing
static unsigned long lastDebounceTime = 0;
static bool btnDebouncedState = false;

// Power toggle lock (when locked and power is on)
static unsigned long powerToggleStartTime = 0;
static int powerTogglePhase = 0; // 0=idle, 1=off, 2=on, 3=off
static bool lockPowerToggling = false;

static void logWriteStr(const char *s) {
  while (*s) {
    Serial_write((uint8_t)(*s));
    s++;
  }
}

static void logWriteBool(bool v) {
  Serial_write((uint8_t)(v ? '1' : '0'));
}

static void logWriteU16(uint16_t v) {
  char buf[6];
  uint8_t i = 0;

  if (v == 0) {
    Serial_write((uint8_t)'0');
    return;
  }

  while (v > 0 && i < sizeof(buf)) {
    buf[i++] = (char)('0' + (v % 10));
    v /= 10;
  }

  while (i > 0) {
    Serial_write((uint8_t)buf[--i]);
  }
}

static bool isIp5189OnByLed(void) {
  // LED battery status: HIGH when powered, LOW when unplugged
  return digitalRead(PIN_IP_LED_BAT_STATUS) == HIGH;
}

uint16_t getBatteryMV() {
  uint16_t raw = analogRead(PIN_RET_BAT);
  return (uint32_t)raw * VREF_MV * DIV_RATIO / ADC_MAX;
}

void setup() {
  pinMode(PIN_BT_KEY, INPUT_PULLUP);
  pinMode(PIN_PW_KEY, OUTPUT);
  pinMode(PIN_IP_GREEN, OUTPUT);
  pinMode(PIN_IP_BLUE, OUTPUT);
  pinMode(PIN_IP_RED, OUTPUT);
  pinMode(PIN_IP_LED_BAT_STATUS, INPUT);

#if DEBUG
  Serial_begin(DEBUG_BAUD);
#endif

  // Initialize outputs
  digitalWrite(PIN_IP_GREEN, LOW);
  digitalWrite(PIN_IP_BLUE, LOW);
  digitalWrite(PIN_IP_RED, LOW);
  digitalWrite(PIN_PW_KEY, PW_KEY_ON);

  ledGreenState = isIp5189OnByLed();
  ledOffSince = 0;
  redLedOnUntil = 0;

  // Enable STM32 communication protocol
  signalProtoInit();

  LOG_STR("[STM8] Lock/Unlock mode enabled\r\n");
  LOG_STR("[INFO] Hold 10s + 5 taps = Lock, Hold 10s + 3 taps = Unlock\r\n");
}

void loop() {
  unsigned long now = millis();
  
  // Read and debounce button
  bool btnRaw = (digitalRead(PIN_BT_KEY) == LOW);
  if (btnRaw != prevBtnRaw) {
    lastDebounceTime = now;
  }
  prevBtnRaw = btnRaw;
  
  if (now - lastDebounceTime >= DEBOUNCE_MS) {
    btnDebouncedState = btnRaw;
  }

  bool btnPressedEdge = (btnDebouncedState && !prevBtnDebounced);
  bool btnReleasedEdge = (!btnDebouncedState && prevBtnDebounced);

  bool ledPowerRaw = isIp5189OnByLed();

  // OFF-delay filter for green LED:
  // - Raw ON  => green ON immediately
  // - Raw OFF => green OFF only after continuous OFF for 1.5s
  if (ledPowerRaw) {
    ledGreenState = true;
    ledOffSince = 0;
  } else if (ledGreenState) {
    if (ledOffSince == 0) {
      ledOffSince = now;
    } else if (now - ledOffSince >= PW_ON_VERIFY_TIMEOUT_MS) {
      ledGreenState = false;
    }
  }
  
  // ========== GREEN LED: Disabled for UART debug visibility ==========
  digitalWrite(PIN_IP_GREEN, LOW);

  // ========== BUTTON STATE MACHINE ==========
  
  // Track hold start from a clean pressed edge
  if (btnPressedEdge && !tapWindowActive) {
    holdStartTime = now;
    holdQualified = false;
  }
  
  // Check if button held long enough to enter sequence mode
  if (btnDebouncedState && holdStartTime > 0 && !tapWindowActive && !holdQualified) {
    unsigned long holdTime = now - holdStartTime;
    if (holdTime >= LOCK_HOLD_MS) {
      holdQualified = true;
      LOG_STR("[HOLD] Qualified, release then tap\r\n");
    }
  }

  // On release after qualified hold: open tap window
  if (btnReleasedEdge && holdQualified && !tapWindowActive) {
    tapWindowActive = true;
    tapWindowStartTime = now;
    tapCount = 0;
    LOG_STR("[SEQ] Waiting for taps...\r\n");
  }

  // Clean hold reset when released and not in tap window
  if (btnReleasedEdge && !tapWindowActive) {
    holdStartTime = 0;
    holdQualified = false;
  }

  // Count taps during tap window and apply lock/unlock
  if (tapWindowActive) {
    if (now - tapWindowStartTime > TAP_TIMEOUT_MS) {
      tapWindowActive = false;
      tapCount = 0;
      holdQualified = false;
      LOG_STR("[SEQ] Timeout\r\n");
    } else if (btnPressedEdge) {
      tapCount++;
      LOG_STR("[TAP] Count=");
      LOG_U16(tapCount);
      LOG_STR("\r\n");

      if (!isLocked && tapCount >= LOCK_TAPS) {
        isLocked = true;
        redLedOnUntil = now + LOCK_RED_ON_MS;
        tapWindowActive = false;
        tapCount = 0;
        holdQualified = false;
        LOG_STR("[LOCK] LOCKED - Red LED ON\r\n");
      } else if (isLocked && tapCount >= UNLOCK_TAPS) {
        isLocked = false;
        redLedOnUntil = 0;
        tapWindowActive = false;
        tapCount = 0;
        holdQualified = false;

        // Exit lock mode immediately: stop any pending lock toggle sequence.
        lockPowerToggling = false;
        powerTogglePhase = 0;
        digitalWrite(PIN_PW_KEY, btnDebouncedState ? PW_KEY_ON : PW_KEY_OFF);

        LOG_STR("[UNLOCK] UNLOCKED - Red LED OFF\r\n");
      }
    }
  }

  // In lock mode, each button press retriggers red LED for 5 seconds.
  if (isLocked && btnPressedEdge) {
    redLedOnUntil = now + LOCK_RED_ON_MS;
  }

  if (isLocked && redLedOnUntil != 0 && now < redLedOnUntil) {
    digitalWrite(PIN_IP_RED, HIGH);
  } else {
    digitalWrite(PIN_IP_RED, LOW);
  }


  prevBtnDebounced = btnDebouncedState;

  // ========== STM32 COMMUNICATION PROTOCOL ==========
  // Poll for and handle commands from STM32
  signalProtoPoll(isLocked, getBatteryMV());
  
  // Update remote timer (if set by CMD_SET_PW_TIMER from STM32)
  signalProtoUpdatePwTimer();

  // ========== POWER KEY CONTROL ==========
  
  // When locked and power is on, force PW_KEY toggle pattern
  if (isLocked && ledPowerRaw) {
    if (!lockPowerToggling) {
      // Start toggle sequence
      lockPowerToggling = true;
      powerTogglePhase = 1;
      powerToggleStartTime = now;
      digitalWrite(PIN_PW_KEY, PW_KEY_ON);  // Phase 1: OFF
      LOG_STR("[LOCK_PWR] Starting toggle pattern\r\n");
    }
    
    // Execute toggle pattern: OFF -> ON -> OFF
    if (lockPowerToggling) {
      unsigned long elapsed = now - powerToggleStartTime;
      
      if (powerTogglePhase == 1 && elapsed >= 2000) {
        // Switch to ON
        digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
        powerTogglePhase = 2;
      } else if (powerTogglePhase == 2 && elapsed >= 2200) {
        // Switch back to OFF
        digitalWrite(PIN_PW_KEY, PW_KEY_ON);
        powerTogglePhase = 3;
      } else if (powerTogglePhase == 3 && elapsed >= 2700) {
        // Switch back to OFF
        digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
        powerTogglePhase = 4;
      }
      else if (powerTogglePhase == 4 && elapsed >= 2900) {
        // Switch back to OFF
        digitalWrite(PIN_PW_KEY, PW_KEY_ON);
        powerTogglePhase = 5;
      }
        else if (powerTogglePhase == 5 && elapsed >= 3200) {
        // Pattern complete
        lockPowerToggling = false;
        powerTogglePhase = 0;
        LOG_STR("[LOCK_PWR] Pattern complete\r\n");
      }
    }
  } else if (!isLocked) {
    // Normal mode: direct button control
    if (btnDebouncedState) {
      digitalWrite(PIN_PW_KEY, PW_KEY_ON);
    } else {
      digitalWrite(PIN_PW_KEY, PW_KEY_OFF);
    }
    lockPowerToggling = false;
    powerTogglePhase = 0;
  }

  // ========== LOGGING ==========
  if (now - lastLogTime >= 500) {
    lastLogTime = now;
    LOG_STR("btn=");
    LOG_BOOL(btnDebouncedState);
    LOG_STR(" seq=");
    LOG_BOOL(tapWindowActive);
    LOG_STR(" tap=");
    LOG_U16(tapCount);
    LOG_STR(" locked=");
    LOG_BOOL(isLocked);
    LOG_STR(" ledRaw=");
    LOG_BOOL(ledPowerRaw);
    LOG_STR(" ledGreen=");
    LOG_BOOL(ledGreenState);
    LOG_STR(" pw=");
    LOG_BOOL(digitalRead(PIN_PW_KEY) == PW_KEY_ON);
    LOG_STR(" bat=");
    LOG_U16(getBatteryMV());
    LOG_STR("\r\n");
  }

  // ========== HEARTBEAT TEST ==========
  // static unsigned long lastHeartbeat = 0;
  // if (now - lastHeartbeat >= 1000) {
  //   lastHeartbeat = now;
  //   sendHeartbeat();
  // }
}