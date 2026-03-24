# Production Readiness Checklist

## 1. Overview
This document summarizes required production hardening for the battery management MCU project.
It includes hardware, firmware, testing, and safety checks for release.

## 2. Firmware features
- FreeRTOS tasks:
  - mainTask (heart-beat LED)
  - ledTask
  - motorTask
  - audioTask
  - commTask (UART CLI)
  - adcTask (ADC + MUX + battery voltage conversion)
  - loggerTask (serial logging)

- SPI flash FS for audio storage:
  - `flashFsInit`, `flashFsFormat`, `flashFsWriteFile`, `flashFsReadFile`.
  - `cmd_store` command via UART + `upload_audio.py`.

- ADC floor/ceiling detection:
  - `BAT_UNDERVOLTAGE` = 3.2V
  - `BAT_OVERVOLTAGE` = 4.3V

## 3. Hardware requirements
1. MCU board: STM32 Nucleo G070RB.
2. MUX: 74HC4051, control pins S0/S1/S2 to pins A11/A12/A15, output to ADC pin PA0.
3. Battery input:
   - Use resistor divider, recommended 100k/68k for 0-4.2V to 0-1.64V.
   - No 5V pullup directly to ADC input.
4. Audio PWM: PA4, external amplifier.
5. SPI flash: W25Q32, SPI pins configured in `pinout.h`.
6. UART Serial1 @115200 for CLI and logging.

## 4. Software robustness
- Serial debug should go through `logger` API.
- ADC MUX uses `adcMutex` to prevent simultaneous access.
- `cmd_mon` does multi-channel raw+voltage + warnings.
- `cmd_format` performs FS erase, status message.
- `cmd_store` handles serial binary upload with timeout preservation.

## 5. Release test plan
1. Build: `platformio run` (pass). 2. Smoke test CLI: `help`, `list`, `format`, `mon`, `play`, `motor`.
2. Flash upload test: run `upload_audio.py` and play back.
3. ADC range test: inject known voltage 3.0/3.7/4.2 and verify
   - output ~1.2/1.6/1.95 on ADC (review divider value)
   - computed battery voltage close measured
4. Stress test: run 24h, run `mon` each second, `cmd_store` and playback every minute.
5. Watchdog and recovery:
   - Added IWDG (Independent Watchdog) with 10-second timeout
   - Reloads every second in mainTask
   - Automatic MCU reset on deadlock/hang
6. Interference and noise:
   - confirm ADC stable with typical harness EMI.

## 6. Git hygiene
- Use meaningful commits: `fix: adc conversion + warnings`, `feat: logger queue timeout`.
- Branch: `release/v1.0`.

## 7. Documentation
- `AUDIO_DAC_GUIDE.md` explains file format and PWM limits.
- `UART_TROUBLESHOOTING.md` explains serial diagnostics.
- `CODE_REVIEW_REPORT.md` and `FIXES_APPLIED.md` track issues/resolutions.

## 8. Known issues and mitigations
1. `Serial1` actual output may be dropped if logger queue is full (set queue to 10). The logger should drop new messages, not block.
2. `cmd_store` large upload from Python must respect `Serial1` timeout; recover gracefully on failure.
3. Ensure MUX deselect state does not expose 5V to ADC.
4. Add pre-charge/hot-swap protection for LiPo in final product.

---

## 9. Configuration values
- `UART_BAUD`: 115200
- `AUDIO_SAMPLE_RATE`: 8000
- `FLASH_FS_DATA_START`: 0x1000
- `FLASH_FS_MAX_FILES`: 16
