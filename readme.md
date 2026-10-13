# Noctis Arcanum

A pocket-sized electronic dice roller: a ring of twelve LEDs, one button, and a
coin cell. Pick a die, press and hold to roll, and watch the light chase its way
around the ring before settling on your number.

It runs on an ATtiny1614 with all thirteen LEDs Charlieplexed onto just five pins,
so the whole thing fits on a single CR2032 and sips power between rolls.

## Features

- **Seven dice** — d2, d4, d6, d8, d10, d12 and d20, cycled with a short press.
- **Hold to roll** — a long press winds the chase up around the ring; let go and it
  always finishes the lap before revealing the result, so a roll is never cut off
  mid-circle.
- **Natural 20 celebration** — roll a perfect 20 on the d20 and it spins up a fast
  spiral with the status LED strobing before the crit lands.
- **Power flourishes** — the ring spirals to life when you wake it, and collapses
  again on the way to sleep.
- **Sleeps deeply** — after about 45 seconds idle, or on a double-click, it powers
  down and waits for a button press. The die you were using is remembered.
- **Recovers itself** — a watchdog resets the device automatically if the firmware
  ever hangs, so a rare glitch clears itself without pulling the cell out.
- **Fair rolls** — a seeded xorshift32 generator with rejection sampling, so no
  face is ever favoured.

## Using it

1. **Wake it.** Press the button; the ring spirals on into die selection.
2. **Pick a die.** A short press cycles d2 → d4 → d6 → d8 → d10 → d12 → d20. The
   ring shows the die you've landed on.
3. **Roll.** Press and hold. The chase starts spinning — hold as long as you like,
   then release to let it finish the lap and reveal your number.
4. **Again or re-pick.** A short press goes back to choosing a die; a long press
   rolls the same die again.
5. **Power off.** Double-click. It also sleeps on its own when left alone.

## Reading the result

The ring lights one LED per point, clockwise from the top. The d20 needs a little
extra room, so it uses the status LED as a "+10" marker: 17 lights seven LEDs plus
the status LED, and a natural 20 lights ten LEDs plus the status LED.

## Hardware

| | |
|---|---|
| MCU | ATtiny1614 @ 16 MHz |
| Power | CR2032 coin cell |
| Display | 12 perimeter LEDs (D1–D12) + 1 status LED (D13) |
| Input | One active-low button on PA6 |
| Wiring | All 13 LEDs Charlieplexed across PA1–PA5 |

## Building and flashing

Built with [PlatformIO](https://platformio.org/) (the `atmelmegaavr` platform and
the Arduino framework via megaTinyCore).

```bash
pio run -e dice              # build the dice roller
pio run -e dice -t upload    # flash it over UPDI
pio run -e hwtest -t upload  # flash the LED + button hardware test
pio run -e dice -t fuses     # run once per blank chip (clock + brown-out fuses)
```

A bare `pio run` builds the roller; pass `-e hwtest` when you want the bring-up
test instead.

## Project layout

```
lib/
  charlieplex/   Charlieplex LED driver with a timer-driven refresh
  button/        Debounced button: short/long press, double-click, wake-up
src/
  dice/          the dice roller application
  hwtest/        LED walk + button bring-up test
```

## Going deeper

The full specification, LED mapping and firmware implementation notes live in
[`spec.md`](spec.md).

