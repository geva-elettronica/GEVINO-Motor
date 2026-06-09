# GEVINO Motor

**GEVINO Motor** is an affordable, Arduino-compatible **PLC for motor control**, built around the
Microchip **SAMD21** microcontroller (32-bit ARM Cortex-M0+) and a **Toshiba TB67S128FTG** motor
driver. It is **Arduino Zero compatible**, so you program it with the standard Arduino IDE / Arduino
SAMD core (it also works with Cursor and OpenPLC), while getting industrial I/O and a powerful
stepper / brushed-DC driver with constant-current control and microstepping down to 1/128 step.

Designed and manufactured by **[GEVA Elettronica](https://www.gevaelettronica.it)** (Italy).

> Looking to program the board? Jump to **[Quick start](#quick-start)**.
> Writing firmware with an AI coding assistant? Read **[AGENTS.md](AGENTS.md)** — the LLM/agent
> programming guide for this board.

---

## Highlights

- **Arduino Zero compatible** — program it with the Arduino IDE / Arduino SAMD core (also Cursor, OpenPLC).
- **Toshiba TB67S128FTG** driver: one **stepper** motor or two **brushed-DC** motors.
- **Constant-current** output: **50 V 5 A (10 A peak) per phase**.
- **Microstepping**: full, half, 1/4, 1/8, 1/16, 1/32, 1/64, 1/128 step.
- Software control: torque setting, frequency limits, step selection, **current reading**,
  **ADMD** (Advanced Dynamic Mixed Decay), **anti-stall**.
- **5 PNP digital inputs**, **4 short-circuit-protected PNP outputs** (40 V 2.5 A), **3 analog inputs** (12-bit).
- **RS485** bus for parallel connection of up to 255 devices, M-Bus or other.
- **micro-SD** slot, **USB-C** port, status LEDs, reset button.
- Optional **Ethernet** and **Wi-Fi** modules.

---

## Technical specifications

### Motor driver

| | |
|---|---|
| Driver | Toshiba **TB67S128FTG** |
| Motors | one stepper **or** two brushed-DC motors |
| Output | constant current, **50 V / 5 A (10 A peak)** per phase |
| Step modes | full, half, 1/4, 1/8, 1/16, 1/32, 1/64, 1/128 |
| Software functions | torque setting, frequency limits, step selection, current reading, ADMD, anti-stall |
| Control interface | SPI (24 MHz) |

### Inputs / Outputs

- **5 PNP digital inputs.**
- **4 short-circuit-protected PNP outputs**, 40 V 2.5 A, for resistive, inductive and capacitive loads.
- **3 analog inputs**, 20 mA / 1 V / 10 V, 12-bit resolution. Two extra ADC channels read the
  **motor phase currents**.

### Connectivity

- **1 RS485** port for parallel connection of up to **255** GEVINO devices, M-Bus or other.

### Front panel (behind the door)

- micro-SD card slot
- micro USB-C port
- LEDs for inputs, outputs, serial and power
- Reset button

### Processor

- 32-bit **ARM Cortex-M0+**, 48 MHz (Microchip SAMD21)
- Flash memory **256 KB**, RAM **32 KB**

### Optional modules

- Ethernet RJ45 module (Wiznet **W5500**)
- Wi-Fi **ESP** module

### Update and debug

- Update the program **over the Internet or from the SD card** using a dedicated bootloader.
- **Debug via Ethernet Telnet.**
- **Programming via Ethernet FTP** (requires the bootloader on SD).

---

## Microstepping and the sine tables

Microstepping uses a precomputed sine lookup table. Pick the `Sinewave*.h` table that matches the
microstep resolution you want and **include it before the library header**:

```cpp
#include "Sinewave128.h"   // 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
#include "gevino_motor.h"
```

Available tables: `Sinewave1.h`, `Sinewave2.h`, `Sinewave4.h`, `Sinewave8.h`, `Sinewave16.h`,
`Sinewave32.h`, `Sinewave64.h`, `Sinewave128.h`, `Sinewave256.h`, `Sinewave512.h`, `Sinewave1024.h`.
Each defines `SinwaveTableSize` and the `dataword_rom[]` table used by the microstep functions.

---

## Pinout / pin mapping

Functions are exposed through named macros defined in
[`src/gevino_motor.h`](src/gevino_motor.h). Pin numbers refer to the Arduino SAMD (Arduino Zero)
numbering.

### PNP digital inputs

| Macro | Arduino pin | | Macro | Arduino pin |
|-------|-------------|---|-------|-------------|
| `In1` | 27 | | `In4` | 14 (A0) |
| `In2` | 9  | | `In5` | 38 |
| `In3` | 8  | |       |    |

`In1` … `In5` return the pin level via `digitalRead`.

### PNP outputs

| Macro | Arduino pin | Set / Reset |
|-------|-------------|-------------|
| `Out1` | 2 | `setOut1` / `resOut1` |
| `Out2` | 3 | `setOut2` / `resOut2` |
| `Out3` | 4 | `setOut3` / `resOut3` |
| `Out4` | 5 | `setOut4` / `resOut4` |

### Analog inputs

| Macro | Arduino pin | `analogPolling()` result |
|-------|-------------|--------------------------|
| `An1_name` | A1 | `analogResult[1]` |
| `An2_name` | A2 | `analogResult[2]` |
| `An3_name` | A3 | `analogResult[3]` |
| motor current A | — | `analogResult[4]` |
| motor current B | — | `analogResult[5]` |

### Motor driver (TB67S128FTG)

| Macro | Arduino pin | Function |
|-------|-------------|----------|
| `LATCH` | 6 | latch the SPI parameters into the driver — `setLATCH` / `resLATCH` |
| `BANK_EN` | 45 | bank enable (init vs operating mode) — `setBANK_EN` / `resBANK_EN` |
| `STANDBY` | 42 | standby — `setSTANDBY` / `resSTANDBY` |
| SPI | MOSI / MISO / SCK | 24 MHz, `LSBFIRST`, `SPI_MODE1` |

### System / bus pins

| Macro | Arduino pin | Function |
|-------|-------------|----------|
| `Rs485_TxEn` | 7 | RS485 transmit enable (half duplex) — `set_Rs485_TxEn` / `res_Rs485_TxEn` |
| `SD_CS` | 12 | micro-SD chip-select |
| `ETH_CS` | 10 | W5500 Ethernet chip-select |
| `ETH_RES` | 11 | W5500 Ethernet reset |
| `LED_BUILTIN` | — | yellow front LED — `setLed` / `resLed` |
| RX / TX LEDs | 25 / 26 | serial activity LEDs |

### Serial ports

| Port | Use |
|------|-----|
| `SerialUSB` | USB-C native port — **debug / programming console** |
| `Serial1`   | **RS485** half-duplex bus (use `set_Rs485_TxEn` / `res_Rs485_TxEn`) |
| `Serial`    | Bluetooth (SPP-C) or Wi-Fi (ESP) module, depending on the fitted module |

---

## Repository contents

| Path | Description |
|------|-------------|
| [`src/`](src/) | The single-header support library (`gevino_motor.h`) and the microstep sine tables (`Sinewave*.h`). |
| [`examples/`](examples/) | Ready-to-flash Arduino sketches. |
| [`docs/`](docs/) | Hardware manual, CE declaration and the TB67S128FTG datasheet (PDF). |
| [`AGENTS.md`](AGENTS.md) | Programming guide for LLMs / AI coding agents. |
| [`llms.txt`](llms.txt) | LLM-friendly map of the repository (llmstxt.org convention). |
| [`keywords.txt`](keywords.txt) | Arduino IDE syntax highlighting. |
| [`library.properties`](library.properties) | Arduino library metadata. |

---

## Requirements

- **Arduino IDE** (or arduino-cli).
- **Arduino SAMD core** ("Arduino SAMD Boards" / a core compatible with the GEVINO board).
- Select the board as **Arduino Zero (Native USB Port)**.

The library is **self-contained** — no external dependencies for the core I/O, ADC helpers, motor
control and Timer. The `AccelStepper` example uses the AccelStepper library; the factory test uses
SD, Ethernet, RTC, CAN and LoRa libraries (noted in each sketch). Requires **C++17** (default on
recent Arduino SAMD cores).

## Installation

1. Download the ZIP from GitHub, or clone the repository.
2. In the Arduino IDE: **Sketch → Include Library → Add .ZIP Library…**
3. The examples then appear under **File → Examples → GEVINO Motor**.

## Quick start

```cpp
#include "Sinewave128.h"   // pick your microstep resolution
#include "gevino_motor.h"  // include AFTER the sine table

void setup() {
  SerialUSB.begin(115200);  // USB-C debug console
  gevino_motor_setup();     // configures all the board I/O (do NOT init pins yourself)

  resSTANDBY;               // hold the motor in position
  ISEq.AGC = 1;             // configure the driver...
  MotorInit();              // send the init sequence to the TB67S128FTG
  Param.Torque = 6;
  Microstep();              // prepare the SPI transaction
}

void loop() {
  MicrostepUp();            // advance one microstep
  delayMicroseconds(500);   // step rate
}
```

> **Important:** include `gevino_motor.h` in exactly **one** translation unit (your main `.ino`), and
> always include a `Sinewave*.h` table **before** it (the library references `dataword_rom`).

Write **non-blocking, state-based** code (prefer `switch`/`case`, avoid `delay()` in `loop()`).
See [AGENTS.md](AGENTS.md) for the full programming method, motor control, RS485, Ethernet, SD and
analog recipes.

## API reference

### Inputs / outputs
- Inputs: `In1` … `In5` (`digitalRead`).
- Outputs: `setOut1` … `setOut4`, `resOut1` … `resOut4`, state `Out1` … `Out4`.
- LEDs / RS485: `setLed` / `resLed`, `set_Rs485_TxEn` / `res_Rs485_TxEn`.

### Functions
| Function | Description |
|----------|-------------|
| `gevino_motor_setup()` | Configures pins, drive strength, LEDs, RS485, the driver pins and resets the Ethernet module. Call it in `setup()`. |
| `testLeds()` | LED / output power-up sequence for bench testing. |
| `MotorInit()` | Sends the 32-bit initialization sequence (`ISEq`) to the driver. |
| `Microstep()` | Opens the SPI transaction for microstepping. |
| `MicrostepUp()` / `MicrostepDw()` | Advance / retreat one microstep. |
| `MicrostepRefresh()` | Resend the current `Param`. |
| `Microsteps(steps, dir, count)` | Blocking move of `steps` microsteps with `count` µs delay. |
| `SendBrushed()` | Sends `Param` for brushed-DC mode. |
| `analogEnable()` | Starts the first non-blocking ADC conversion. |
| `analogPolling()` | Non-blocking analog read; updates `analogResult[]`. Call it often in `loop()`. |

### Driver registers
- `ISEq` — initialization sequence (AGC, current limit, frequency limit, boost, gain).
- `Param` — per-step parameters (torque, decay mode/ADMD, direction, current for brushed mode).

### Timer
A lightweight non-blocking software timer class (`Timer`) based on `millis()` is included — see the
[`Timer`](examples/Timer/Timer.ino) example.

## Examples

| Example | What it shows |
|---------|---------------|
| [`GEVINO_motor`](examples/GEVINO_motor/GEVINO_motor.ino) | Full factory test: stepper/brushed control, inputs, outputs, analog + current sensing, RS485, SD, RTC, Ethernet, CAN, LoRa. |
| [`AccelStepper`](examples/AccelStepper/AccelStepper.ino) | Driving the stepper with the AccelStepper library (constant speed / positioning). |
| [`Timer`](examples/Timer/Timer.ino) | Non-blocking blink using the `Timer` class. |

## Documentation

The hardware manual, the CE declaration and the TB67S128FTG datasheet (PDF) are in [`docs/`](docs/).

## Buy / contact

GEVINO PLCs are designed and produced by **GEVA Elettronica**.

- Website: https://www.gevaelettronica.it
- Email: info@gevaelettronica.it

## License

MIT — see [LICENSE](LICENSE). The bundled SAMD21 ADC helpers are derived from Blake Felt's
`ATSAMD21_ADC` and retain their original attribution in the source header.
