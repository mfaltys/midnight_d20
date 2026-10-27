<div align="center">

# Noctis Arcanum

[![Firmware build](https://github.com/mfaltys/midnight_d20/actions/workflows/build.yml/badge.svg)](https://github.com/mfaltys/midnight_d20/actions/workflows/build.yml)
[![Firmware flasher](https://img.shields.io/badge/%F0%9F%94%A5%20Flasher-mfaltys.github.io%2Fmidnight__d20-green.svg)](https://mfaltys.github.io/midnight_d20/)
[![ATtiny1614](https://img.shields.io/badge/ATtiny1614-tinyAVR-0FA0CE?labelColor=333&logo=microchip&logoColor=white)]()

**Flash the firmware directly from your browser → mfaltys.github.io/midnight_d20**

</div>

## Overview

A pocket-sized electronic dice roller

It runs on an ATtiny1614 with all thirteen LEDs Charlieplexed onto just five pins,
so the whole thing fits on a single CR2032 and sips power between rolls. Need to
load new firmware? Use the hosted
[web flasher](https://mfaltys.github.io/midnight_d20/)


## Using it

1. **Wake it.** Press the button; the ring spirals on into die selection.
2. **Pick a die.** A short press cycles d2 → d4 → d6 → d8 → d10 → d12 → d20. The
   ring shows the die you've landed on.
3. **Roll.** Press and hold. The chase starts spinning - hold as long as you like,
   then release to let it finish the lap and reveal your number.
4. **Again or re-pick.** A short press goes back to choosing a die; a long press
   rolls the same die again.
5. **Power off.** Double-click. It also sleeps on its own when left alone.

## Reading the result

The ring lights one LED per point, clockwise from the top. The d20 needs a little
extra room, so it uses the status LED as a "+10" marker: 17 lights seven LEDs plus
the status LED, and a natural 20 lights ten LEDs plus the status LED.

## Web flasher

Load new firmware onto your dice roller straight from your browser - no toolchain
or PlatformIO install needed. Open the flasher at
**[mfaltys.github.io/midnight_d20](https://mfaltys.github.io/midnight_d20/)** in
**Chrome, Edge, or Opera** (Web Serial support required):

1. Connect a serialUPDI programmer - the
   [UPDI programmer adapter (USB-C)](https://www.etsy.com/listing/4591195235/updi-programmer-adapter-usbc)
   is the one made for this - and click **Connect**. The flasher auto-detects the
   ATtiny1614 over UPDI, reports the fuses it finds, and lists the available
   firmware.
2. Pick a rite and click **Inscribe Device**. The chip is erased, the firmware is
   written and read back page by page, and the fuses are set to match
   `platformio.ini` (16 MHz clock, brown-out detector off).

If the ring stays dark afterwards, unplug the UPDI adapter and plug it back in - or
lift and reseat the coin cell - and the ring will spiral to life.

## Firmware (`src/<name>/`)

| Program | What it does |
|---|---|
| `dice` | The roller: die selection, hold-to-roll chase, natural-20 celebration, result, deep sleep |
| `hwtest` | Bring-up test for a freshly soldered board: walks D1–D13 one LED at a time (400 ms each), then lights the whole ring while the button is held |

## Building from source

The firmware is a [PlatformIO](https://platformio.org/) project (the
`atmelmegaavr` platform and the Arduino framework via megaTinyCore) with one
environment per program - the env name **is** the program name:

```sh
pio run                      # build the dice roller
pio run -e dice              # build just the dice roller
pio run -e dice -t upload    # flash it over UPDI
pio run -e hwtest -t upload  # flash the LED + button bring-up test
pio run -e dice -t fuses     # run once per blank chip
```

`platformio.ini` sets `[platformio] default_envs = dice`, so a bare `pio run`
builds the roller rather than the bring-up test. Run the `fuses` target once per
blank chip: `upload` only writes flash, and a fresh part powers up on a different
oscillator setting, which scales every delay. (The [web flasher](#web-flasher)
writes those fuses for you.) 16 MHz is the setting the firmware is built for and
the animation timings in `src/dice/config.h` are tuned to; the part's maximum
frequency only reaches 20 MHz above 4.5 V, and a CR2032 sits at 3 V.

To add a program, create `src/<name>/main.cpp` and add a matching `[env:<name>]`
block with `build_src_filter = +<<name>/>` to `platformio.ini`. Pushing to `main`
builds every environment and republishes the flasher, so a new program shows up in
the picker on its own.

## Hardware

| | |
|---|---|
| MCU | ATtiny1614 @ 16 MHz |
| Power | CR2032 coin cell |
| Display | 12 perimeter LEDs (D1–D12) + 1 status LED (D13) |
| Input | One active-low button on PA6 |
| Wiring | All 13 LEDs Charlieplexed across PA1–PA5 |
| Programming | UPDI on PA0, over serialUPDI |

## Project layout

```
lib/
  charlieplex/   Charlieplex LED driver with a timer-driven refresh
  button/        Debounced button: short/long press, double-click, wake-up
src/
  dice/          the dice roller application
  hwtest/        LED walk + button bring-up test
docs/            the web flasher published to GitHub Pages
```

## Credits

The browser flasher's UPDI stack is a TypeScript port of Microchip's
[pymcuprog](https://github.com/microchip-pic-avr-tools/pymcuprog) by way of
[WebUPDI](https://github.com/manuelkasper/webupdi) by Manuel Kasper, used under the
MIT License (see [`docs/serialupdi/LICENSE`](docs/serialupdi/LICENSE)).
