# AGENTS.md — Programming guide for the GEVINO Motor

> This file is written for **LLMs and AI coding agents** (Claude Code, Cursor, Copilot, etc.) that
> write firmware for the **GEVINO Motor** PLC. It is also useful to human developers. If you are
> generating Arduino/C++ code for this board, follow these rules.

## What the board is

GEVINO Motor is an **Arduino Zero compatible motor-control PLC** based on the Microchip **SAMD21**
(32-bit ARM Cortex-M0+, 48 MHz, 256 KB flash, 32 KB RAM) and a **Toshiba TB67S128FTG** driver
(one stepper or two brushed-DC motors, constant current 50 V / 5 A, 10 A peak, microstepping to
1/128). It has 5 PNP inputs, 4 protected PNP outputs, 3 analog inputs (+ 2 motor-current channels),
an RS485 bus, a micro-SD slot and optional Ethernet / Wi-Fi modules. Program it with the Arduino IDE
using the **Arduino SAMD core** (board **Arduino Zero, Native USB Port**); it also works with Cursor
and OpenPLC.

The complete pin map and specifications are in [README.md](README.md) and in the single library
header [`src/gevino_motor.h`](src/gevino_motor.h).

## Golden rules

1. **Write non-blocking, state-based code.** Prefer `switch`/`case` state machines. Never block the
   `loop()` with `delay()` in production logic (short `delayMicroseconds()` between microsteps is OK).
2. **Include a sine table first, then the library:**
   ```cpp
   #include "Sinewave128.h"   // 1,2,4,8,16,32,64,128,256,512,1024 microsteps
   #include "gevino_motor.h"
   ```
   The library references `dataword_rom` / `SinwaveTableSize`, which the `Sinewave*.h` table defines.
3. **Include `gevino_motor.h` in exactly ONE translation unit** (your main `.ino`): it defines global
   objects (`ISEq`, `Param`, `analogResult`, …) and functions.
4. **Do not initialise the I/O yourself.** `gevino_motor_setup()` configures every pin, drive
   strength, LEDs, RS485, the driver pins and resets the Ethernet module. Call it once in `setup()`.
5. **Use the macros** (`In1`…`In5`, `setOut1`…`setOut4`, `resOut1`…`resOut4`, `setLed`,
   `set_Rs485_TxEn`, `setLATCH`/`resLATCH`, `setBANK_EN`/`resBANK_EN`, `setSTANDBY`/`resSTANDBY`).

## Minimal sketch skeleton

```cpp
#include "Sinewave128.h"
#include "gevino_motor.h"

void setup() {
  SerialUSB.begin(115200);   // USB-C debug console
  Serial1.begin(115200);     // RS485 bus
  gevino_motor_setup();      // configures ALL the board I/O

  resSTANDBY;                // hold the motor in position
  ISEq.AGC = 1;              // configure AGC / current limit / etc.
  MotorInit();               // send the init sequence to the TB67S128FTG
  Param.Torque = 6;          // 0=100% ... 6=10%
  Param.MDTA = 3;            // 3 = ADMD
  Param.MDTB = 3;
  Microstep();               // open the SPI transaction
}

void loop() {
  MicrostepUp();             // advance one microstep
  delayMicroseconds(500);    // step rate
}
```

## Motor control (TB67S128FTG)

The driver is controlled over SPI (24 MHz, `LSBFIRST`, `SPI_MODE1`) using two register structs:

- **`ISEq`** — initialization sequence: `AGC`, `CLIM` (current limiter 45..80%), `FLIM` (frequency
  limit), `BST` (current boost), `RS_SEL`, `GAIN_SEL`. Set these, then call **`MotorInit()`**.
- **`Param`** — per-step parameters: `Torque` (0=100% … 6=10%), `MDTA`/`MDTB` (decay mode; 3 = ADMD),
  `Mot1dir`/`Mot2dir`, `Mot1`/`Mot2` (0..1023 current for brushed-DC mode).

Stepper functions:
- `Microstep()` — open the SPI transaction (call once before stepping).
- `MicrostepUp()` / `MicrostepDw()` — advance / retreat one microstep (walk the sine table).
- `MicrostepRefresh()` — resend the current `Param`.
- `Microsteps(steps, dir, count)` — blocking move of `steps` microsteps, `count` µs between steps.

Brushed-DC: set `Param.Mot1` / `Param.Mot1dir` (and `Mot2`) and call `SendBrushed()`.

Pins: `setSTANDBY`/`resSTANDBY` (resSTANDBY = motor energised/held), `setBANK_EN`/`resBANK_EN`,
`setLATCH`/`resLATCH`.

> The `AccelStepper` library can drive the motor by passing `MicrostepUp` / `MicrostepDw` as the
> step callbacks: `AccelStepper stepper(MicrostepUp, MicrostepDw);` — see the AccelStepper example.

## Analog inputs and motor current (non-blocking)

The standard `analogRead()` blocks for several ms. Use the non-blocking polling instead.

In `setup()` (after `gevino_motor_setup()`):

```cpp
analogPrescaler(ADC_PRESCALER_DIV32);
analogReadExtended(16);              // 16-bit (oversampled)
analogGain(ADC_GAIN_1);
analogReference2(ADC_REF_INT1V);     // 1.0 V reference
analogCalibrate();
analogEnable();
```

In `loop()`, call `analogPolling()` often. Results:

```
analogResult[1] = An1 (A1)
analogResult[2] = An2 (A2)
analogResult[3] = An3 (A3)
analogResult[4] = motor current A
analogResult[5] = motor current B
```

## Serial ports

| Port | Use |
|------|-----|
| `SerialUSB` | USB-C native port — **debug console**. |
| `Serial1`   | **RS485** half-duplex bus. |
| `Serial`    | Bluetooth (SPP-C) or Wi-Fi (ESP) module, depending on the fitted module. |

## RS485 (half duplex)

Enable the transmitter with `set_Rs485_TxEn`, send on `Serial1`, then disable it after the last
character has left the UART (the SERCOM0 TXC flag):

```cpp
if (SERCOM0->USART.INTFLAG.bit.TXC) {
  res_Rs485_TxEn;
  SERCOM0->USART.INTFLAG.bit.TXC = 1;
}
```

### Modbus RTU / TCP
Modbus RTU: **ArduinoModbus** + the **GEVA-modified `ArduinoRS485`** (warn the user it must be the
GEVA version configured for this board). Modbus TCP: **ArduinoModbus** over Ethernet.

## Ethernet (optional W5500 module)

```cpp
#include <Ethernet.h>
Ethernet.init(ETH_CS);   // ETH_CS = 10, defined in gevino_motor.h
```

## SD card

Use **SD** or **SdFat** (chip-select `SD_CS` = 12). Firmware can be updated from the SD card with the
optional bootloader.

## EEPROM emulation (flash)

To read/write the flash as if it were EEPROM, use the **FlashStorage** library by cmaglie:
https://github.com/cmaglie/FlashStorage

## Timer class

`gevino_motor.h` includes a small non-blocking `Timer` based on `millis()`:

- `reset()` restarts the count; while it keeps being reset it never fires.
- `stato()` returns `true` once the configured time has elapsed since `reset()`.
- `fronte()` returns `true` only once (rising edge) when the time elapses.
- `tempo(ms)` sets the period.

See [`examples/Timer/Timer.ino`](examples/Timer/Timer.ino).

## Firmware update and debug

- Update the program **over the Internet or from the SD card** using the dedicated bootloader.
- **Debug** over Ethernet via **Telnet**.
- **Programming** over Ethernet via **FTP** (requires the SD bootloader).

---

## Conventions for building an HTML web interface on the board

When the firmware serves a web UI (e.g. via the W5500 Ethernet module), follow these conventions —
they are tuned for a resource-constrained SAMD21:

### File structure (modular)
1. Separate the content by concern:
   - HTML in `html_content.h`
   - CSS in `css_content.h`
   - JavaScript in `js_content.h`
   - API / JSON in `json_handler.h`
   - main request handling in `html_server.h`
2. Keep sessions/authentication in a dedicated `session_manager.h`, and translations in a dedicated
   `translation.h`.

### Authentication
3. Prefer **GET** for login on Arduino (avoid POST). Use a `pwd` URL parameter: `/login?pwd=xxx`.
4. Read the password from the configuration **immediately before comparing**; never reuse a
   hard-coded password in the code.

### HTML and templates
5. Use complete HTML templates with `R"rawliteral(...)"` and substitutable `{{placeholder}}` tokens.
   Avoid building HTML with many `client.println()` calls.
6. Use a clear HTTP router with separate handling per path; handle `/login` with and without
   parameters separately.

### Multilingual
7. Implement translations as an array of structs. Translate every user-visible string. Auto-detect
   the browser language.

### Debug and security
8. Add `SerialUSB.print()` logging at strategic points. Log both failed and successful auth
   attempts. Log extracted password values **only in debug builds**.
9. Implement timeouts: **sessions 30 minutes**, **HTTP requests 15 seconds max**.

### Resource-constrained compatibility
10. Minimise JavaScript — prefer pure HTML/CSS. Avoid external CSS/JS frameworks that inflate the
    firmware size. Prefer direct links over JavaScript event listeners where possible.
