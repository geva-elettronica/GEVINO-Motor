---
name: gevino-motor
description: Use when writing Arduino/C++ firmware for the GEVA GEVINO Motor PLC (SAMD21, Arduino Zero compatible) with a Toshiba TB67S128FTG stepper/brushed-DC driver. Covers the single-header library, the microstep sine tables, motor control (ISEq/Param, MotorInit, MicrostepUp/Dw, Microsteps, brushed mode), pin/I-O macros, non-blocking ADC + motor-current sensing, RS485 + Modbus, Ethernet (W5500), SD, the Timer class, and HTML web-UI conventions.
---

# GEVINO Motor firmware

You are writing firmware for the **GEVINO Motor** — an Arduino Zero compatible motor-control PLC
based on the Microchip SAMD21 (ARM Cortex-M0+, 48 MHz, 256 KB flash, 32 KB RAM) and a Toshiba
**TB67S128FTG** driver (one stepper or two brushed-DC motors, 50 V / 5 A, microstepping to 1/128),
made by GEVA Elettronica. Build in the Arduino IDE / arduino-cli with the **Arduino SAMD core**,
board **Arduino Zero (Native USB Port)**. It also works with Cursor and OpenPLC.

## Read these first
- [`AGENTS.md`](../../../AGENTS.md) — the full programming guide (this skill is its short form).
- [`src/gevino_motor.h`](../../../src/gevino_motor.h) — the single library header with every pin
  macro, the motor-control functions and the `ISEq`/`Param` registers. When unsure, read this file.

## Non-negotiable rules
1. **Include a sine table first, then the library:**
   ```cpp
   #include "Sinewave128.h"   // 1,2,4,8,16,32,64,128,256,512,1024 microsteps
   #include "gevino_motor.h"
   ```
   The library references `dataword_rom` / `SinwaveTableSize` from the `Sinewave*.h` table.
2. **Include `gevino_motor.h` in exactly ONE translation unit** (the main `.ino`): it defines global
   objects (`ISEq`, `Param`, `analogResult`, …) and functions.
3. **Call `gevino_motor_setup()` once in `setup()`** and do **not** initialise pins yourself.
4. **Use the macros**, never raw pin numbers: `In1`…`In5`, `setOut1`…`setOut4` / `resOut1`…`resOut4`,
   `setLed`/`resLed`, `set_Rs485_TxEn`/`res_Rs485_TxEn`, and driver pins `setLATCH`/`resLATCH`,
   `setBANK_EN`/`resBANK_EN`, `setSTANDBY`/`resSTANDBY`.
5. **Write non-blocking, state-based code.** Prefer `switch`/`case` and `millis()` / the bundled
   `Timer`. Avoid `delay()` in `loop()` (short `delayMicroseconds()` between microsteps is fine).

## Skeleton

```cpp
#include "Sinewave128.h"
#include "gevino_motor.h"

void setup() {
  SerialUSB.begin(115200);
  gevino_motor_setup();
  resSTANDBY;            // hold the motor in position
  ISEq.AGC = 1;
  MotorInit();           // send init sequence to the driver
  Param.Torque = 6;  Param.MDTA = 3;  Param.MDTB = 3;   // 3 = ADMD
  Microstep();           // open the SPI transaction
}

void loop() {
  MicrostepUp();
  delayMicroseconds(500);
}
```

## Motor control (TB67S128FTG, SPI 24 MHz, LSBFIRST, MODE1)
- **`ISEq`** init register: `AGC`, `CLIM` (45..80%), `FLIM`, `BST`, `RS_SEL`, `GAIN_SEL` → set, then `MotorInit()`.
- **`Param`** per-step: `Torque` (0=100%…6=10%), `MDTA`/`MDTB` (3 = ADMD), `Mot1dir`/`Mot2dir`, `Mot1`/`Mot2` (0..1023 brushed current).
- Stepper: `Microstep()` once, then `MicrostepUp()` / `MicrostepDw()` per step, or `Microsteps(steps, dir, count)` for a blocking move. `MicrostepRefresh()` resends `Param`.
- Brushed-DC: set `Param.Mot1`/`Param.Mot1dir` and call `SendBrushed()`.
- AccelStepper integration: `AccelStepper stepper(MicrostepUp, MicrostepDw);`.

## Analog + current sensing (non-blocking)
Configure in setup (`analogPrescaler/analogReadExtended/analogGain/analogReference2/analogCalibrate/
analogEnable`), then call `analogPolling()` often. `analogResult[1..3]` = An1..An3, `analogResult[4]`
= motor current A, `analogResult[5]` = motor current B. Avoid the blocking `analogRead()`.

## Serial / RS485 / recipes (see AGENTS.md for full code)
- `SerialUSB` = debug; `Serial1` = RS485 (SERCOM0); `Serial` = Bluetooth/Wi-Fi module.
- RS485: `set_Rs485_TxEn` before sending; `res_Rs485_TxEn` only after `SERCOM0->USART.INTFLAG.bit.TXC`.
- **Modbus RTU:** ArduinoModbus + GEVA-modified ArduinoRS485. **Modbus TCP:** ArduinoModbus over Ethernet.
- **Ethernet (W5500):** `Ethernet.init(ETH_CS)` (`ETH_CS` = 10). **SD:** SD/SdFat (`SD_CS` = 12).
- **EEPROM emulation:** FlashStorage by cmaglie.

## HTML web UI
If you build a web interface, follow the resource-constrained conventions in **AGENTS.md**
(modular `*_content.h` files, GET-based login with a `pwd` param, `R"rawliteral(...)"` templates
with `{{placeholders}}`, struct-array translations, 30-min session / 15-s request timeouts, minimal
JavaScript).
