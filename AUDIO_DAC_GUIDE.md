# Audio DAC Hardware Guide

**Current:** PWM PA4 → 3.3V square wave (distorted buzz, now louder)

**To Get Clear Audio:**
1. **RC Low-Pass Filter:** PA4 -- 1kΩ --[RC node]-- speaker+
                                    |
                                    0.1µF -- GND
   
   Cutoff ~1.6kHz smooths PWM to sine.

2. **Amplifier:** LM386 or PAM8403 module (speaker 4-8Ω).

3. **Power:** 5V supply for amp (not 3.3V MCU).

**Test:**
- `beep` → clear 1kHz tone (not buzz)
- `play xinchao` → clear speech

**Wiring Diagram:**
```
MCU PA4 ── 1kΩ ──┬── 10uF ── LM386 IN+
                 │
                 0.1uF
                 │
                GND
```

