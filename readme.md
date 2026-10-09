# Noctis Arcanum — ATtiny1614 Dice Firmware

## Hardware

- MCU: ATtiny1614
- Power: CR2032
- 12 perimeter LEDs, D1–D12, arranged clockwise starting at the top center.
- One status LED, D13.
- One active-low navigation button connected between PA6 and GND, using the internal pull-up.
- All 13 LEDs are Charlieplexed using PA1–PA5.

### Exact LED mapping

| LED | Source (HIGH) | Sink (LOW) |
|---|---|---|
| D1 | PA1 | PA2 |
| D2 | PA2 | PA1 |
| D3 | PA1 | PA3 |
| D4 | PA3 | PA1 |
| D5 | PA1 | PA4 |
| D6 | PA4 | PA1 |
| D7 | PA1 | PA5 |
| D8 | PA5 | PA1 |
| D9 | PA2 | PA3 |
| D10 | PA3 | PA2 |
| D11 | PA2 | PA4 |
| D12 | PA4 | PA2 |
| D13 (status) | PA2 | PA5 |

## Supported Dice

Support the following dice, cycled in this order:

1. d2
2. d4
3. d6
4. d8
5. d10
6. d12
7. d20
8. Return to d2

For each die, a roll produces a uniformly distributed integer from 1 through the selected die's maximum value, inclusive.

Display the final result by illuminating exactly N perimeter LEDs, starting at D1 and proceeding clockwise through D_N. For example, a result of 4 lights D1–D4; a result of 12 lights all perimeter LEDs.

Because the ring has only 12 LEDs, a d20 face cannot be shown as a plain count. For d20, a result of 1–10 lights D1–D_N as above, and a result of 11–20 lights D1–D_(N−10) plus D13 as a "+10" marker: 11 lights D1 + D13, and 20 lights D1–D10 + D13. Results on the dice up to d12 are unchanged.

Do not implement d100 in this revision.

## Button Interaction

The button is active-low on PA6.

- Debounce button input in software.
- A short press is an action released before the long-press threshold.
- A long press is recognized after approximately 600 ms of continuous press.
- A double-click (a second short press released within approximately 300 ms of
  the first) powers the device off. The first press still performs its normal
  action, but the die change it made is rolled back when the double-click
  completes; the second press powers off (after the power-off flourish) instead
  of acting again.
- Do not use blocking delays for button handling.
- Ensure one physical press generates no more than one action.
- A long press must not also trigger a short-press action when released.

## State Machine

Implement the application as a nonblocking state machine with these states:

### 1. SLEEP

- LEDs off.
- Enter a low-power ATtiny1614 sleep mode.
- PA6 must wake the device when the button is pressed.
- Entered on power-up, after the inactivity timeout, or after the power-off
  flourish that a double-click starts.
- On wake, play the POWER_ON flourish, then enter SELECT_DIE on the previously
  selected die.

### 2. POWER_ON

- A short spiral flourish as the device comes on: a rotating arc whose trail
  grows a LED at a time until the whole ring is lit, with D13 strobing.
- Nonblocking, like every other animation.
- When it finishes, enter SELECT_DIE on the previously selected die.

### 3. SELECT_DIE

- D13 blinks continuously to indicate that the user is selecting a die.
- Show the selected die by illuminating the corresponding number of perimeter LEDs:
  - d2: D1–D2
  - d4: D1–D4
  - d6: D1–D6
  - d8: D1–D8
  - d10: D1–D10
  - d12: D1–D12
  - d20: all 12 perimeter LEDs (D1–D12) with D13 lit
- For every die except d20 the perimeter pattern stays steady and only D13
  blinks. d20 has no room for 20 LEDs, so it lights the whole ring, and because
  D13 is part of that pattern the whole thing (D1–D12 together with D13) blinks
  to make "d20 selected" unmistakable.
- A short press advances to the next die.
- After d20, selection wraps to d2.
- A long press of approximately 600 ms confirms the selected die and starts a roll.
- Stop the status LED selection blink when rolling begins.

The selected die is kept in RAM, which survives the power-down sleep, so the
device wakes up on the die the user was last using. A true power cycle (cell
removed) starts the sequence at d2 for the initial firmware revision.

### 4. ROLLING

When a roll starts:

1. Clear the previous result.
2. Run a clockwise chase animation around D1–D12.
3. Keep the animation fast and brief, initially approximately 600–1000 ms.
4. Stop the animation cleanly and display the newly generated result.
5. Use nonblocking timing so the MCU is not held in long delays.

The chase should illuminate one perimeter LED at a time, advancing D1, D2, D3, and so on through D12 before repeating as needed for the animation duration.

### 5. CRIT

When a d20 lands on its maximum face (a "perfect 20"), celebrate before
revealing the result:

- Run a fast spiral around D1–D12: a rotating arc whose trail grows a LED at a
  time until the whole ring is lit.
- Strobe D13 (roughly 10 Hz) for the whole flourish.
- After the flourish, continue to SHOW_RESULT to display the 20.
- Use nonblocking timing, as with ROLLING.

Only a perfect 20 on the d20 takes this detour; every other result goes straight
from ROLLING to SHOW_RESULT.

### 6. SHOW_RESULT

- Turn off the selection blink.
- Display exactly N perimeter LEDs, where N is the roll result.
- The result remains visible until the next interaction or inactivity timeout.
- A short press returns to SELECT_DIE so another die can be selected.
- A long press immediately rolls the same selected die again.
- Reset the inactivity timer on user interaction.

If practical, provide a brief D13 confirmation pulse when the result is finalized.

### 7. POWER_OFF

- A short spiral flourish as the device winds down: the same rotating arc, but
  collapsing from the full ring to nothing, with D13 strobing.
- Entered on a double-click (from any active state) and when the inactivity
  timeout expires, so the device always winds down the same way.
- When it finishes, turn off all LEDs and enter SLEEP.

## LED Driver

Implement a reusable Charlieplex driver.

- All LED pins PA1–PA5 must be high impedance when not actively driving the selected LED.
- To illuminate an LED, configure its source pin as output-high and its sink pin as output-low; leave all other LED pins as inputs.
- Disable the previous LED's drive before switching to the next LED.
- Implement multi-LED patterns by rapidly scanning the requested LEDs using a timer-driven refresh routine.
- Use a bitmask or equivalent logical pattern representation for the 12 perimeter LEDs, keeping the status LED separately addressable.
- Validate the refresh frequency and brightness on the actual CR2032-powered hardware.
- Avoid unintended current paths or ghost illumination during Charlieplex transitions.

## Random Number Generation

Implement a lightweight PRNG suitable for the ATtiny1614.

- Seed it using a suitably evaluated startup entropy source, potentially combining ADC noise and timing jitter.
- Use unbiased range mapping, such as rejection sampling, to produce a value in the range 1 through the selected die's maximum value.
- Avoid simple modulo mapping where it introduces bias.
- Keep random generation independent from the UI and LED drivers.
- This is a casual dice roller, not a cryptographic device.

## Power Management

- Use an initial inactivity timeout of approximately 30–60 seconds.
- When the timeout expires, play the power-off flourish, then turn off all LEDs
  and enter low-power sleep.
- A button press wakes the MCU, plays the power-on flourish and enters SELECT_DIE.
- A double-click plays the power-off flourish and enters low-power sleep without
  waiting for the timeout.
- Disable unnecessary peripherals and timers during sleep.
- Ensure the button wake-up configuration is compatible with the selected sleep mode.
- Avoid EEPROM writes for selection state in this revision: the selection lives
  in RAM, which survives the power-down sleep and is only lost on a true power
  cycle (cell removed).

## Implementation Requirements

- Use the existing project toolchain and framework; do not assume Arduino, bare-metal AVR C, or another environment without checking the project files.
- Keep GPIO mappings, debounce intervals, long-press threshold, animation speed, blink period, and inactivity timeout in named constants.
- Keep the main loop nonblocking and event-driven.
- Separate hardware drivers, button handling, dice logic, animations, and application state where practical.
- Add a hardware test mode or temporary test program that illuminates each LED individually and verifies the button before implementing the complete UI.
- Do not invent pin mappings or change the supplied mappings.
- Clearly document the state transitions and timing behavior.

---

# Firmware implementation notes

## Toolchain and layout

PlatformIO with the `atmelmegaavr` platform (`framework = arduino`, which is
megaTinyCore for the ATtiny1614), following the same conventions as the ornament
project: one environment per program, selected by `build_src_filter`. Shared code
lives in `lib/` so both programs use identical driver code.

```
platformio.ini
lib/
  charlieplex/   Charlieplex LED driver (TCB1 refresh interrupt)
  button/        Debounced button, short/long press, wake-up
src/
  dice/          dice roller application (main.cpp, app, dice, prng, config)
  hwtest/        hardware bring-up test (LED walk + button check)
```

```bash
pio run -e dice              # build the dice roller
pio run -e dice -t upload    # flash it over UPDI (upload_protocol = serialupdi)
pio run -e hwtest -t upload  # build + flash the hardware test
pio run -e dice -t fuses     # one-time: write OSCCFG/BODCFG for the 16 MHz clock
```

`platformio.ini` sets `[platformio] default_envs = dice`, so a bare `pio run`,
`pio run -t upload` and the PlatformIO IDE toolbar buttons all target the dice
roller. This is deliberate: `pio run` processes *every* `[env:*]` when no
`-e/--environment` is given, in definition order, so without `default_envs` a
bare upload would flash `dice` and then flash `hwtest` over it, leaving the
device running the LED walk / button bring-up test instead of the roller. Always
pass `-e hwtest` when you actually want the hardware test.

`F_CPU` is 16 MHz (the PlatformIO board definition) and every timing constant is
derived from it, so run the `fuses` target once per blank chip: `upload` only
writes flash, and a fresh part powers up on a different oscillator setting, which
would scale every delay. `board_fuses.BODCFG = 0x00` disables the brown-out
detector to save CR2032 sleep current.

## State machine

```
        power-up
            |
            v
   +----------------+   press (wakes MCU)   +----------------+
   |     SLEEP      |---------------------->|    POWER_ON    |
   | LEDs off,      |                       | spiral grows,  |
   | PWR_DOWN       |<----------------------| D13 strobes    |
   +----------------+   flourish done:      +--------+-------+
            ^           turn off, sleep              | flourish done
            |                                        v
   +--------+-------+                       +----------------+
   |   POWER_OFF    |  double-click or      |   SELECT_DIE   |
   | spiral shrinks,|  inactivity timeout   | D13 blinks,    |
   | D13 strobes    |  (any active state)   | D1..Dn shown   |
   +----------------+                       +----+------+----+
                                                 |      ^
                                     long press  |      | short press
                                                 v      |
                                            +----+------+----+
                                            |    ROLLING     |
                                            | D1..D12 chase  |
                                            +--------+-------+
                                                     | animation done: roll()
                                                     v
                                            +----------------+
                                            |      CRIT      |  a perfect 20
                                            | spiral + D13   |  (d20 = 20) only;
                                            | strobe         |  skipped otherwise
                                            +--------+-------+
                                                     | flourish done
                                                     v
                                            +----------------+
                                            |  SHOW_RESULT   |
                                            | result + D13   |
                                            | confirmation   |
                                            +----------------+
                                              |            |
                                     long press (same die) |
                                     rolls again            | short press
                                                             v
                                                        SELECT_DIE
```

| Transition | Trigger | Timing |
|---|---|---|
| SLEEP -> POWER_ON | button press (PA6 falling edge) wakes the MCU | immediate |
| POWER_ON -> SELECT_DIE | flourish finished | 450 ms spiral, 30 ms per step, D13 strobing |
| SELECT_DIE: next die | short press | on release, < 600 ms |
| SELECT_DIE -> ROLLING | long press | 600 ms of continuous press |
| ROLLING -> SHOW_RESULT | animation finished (not a perfect 20) | 800 ms, 60 ms per chase step |
| ROLLING -> CRIT | a perfect 20 (d20 = 20) after the chase | immediately |
| CRIT -> SHOW_RESULT | flourish finished | 900 ms spiral, 30 ms per step, D13 strobing |
| SHOW_RESULT -> SELECT_DIE | short press | on release |
| SHOW_RESULT -> ROLLING | long press (same die) | 600 ms of continuous press |
| any -> POWER_OFF | double-click | second short press released within 300 ms |
| any -> POWER_OFF | inactivity timeout | 45 s after the last interaction |
| POWER_OFF -> SLEEP | flourish finished | 450 ms spiral, 30 ms per step, D13 strobing |

Details that are easy to get wrong and are handled explicitly:

- The press that wakes the device is **absorbed**: it puts the device in
  SELECT_DIE but does not also advance the die.
- A long press never also produces a short press, and one physical press
  produces at most one action (`lib/button`).
- The device never enters power-down while the button is held, so a press can
  never be lost to a missed edge.
- A double-click powers off from any active state (LEDs off, power-down) without
  waiting for the timeout. Detection is not deferred, so the first click of the
  pair is applied at once, but its die change is rolled back when the double-click
  completes - so the device powers off, and wakes, on the die the user had
  actually chosen. A click followed by a long hold is a long press, never a
  double-click. The POWER_OFF flourish plays before the device sleeps.
- Waking plays the POWER_ON flourish and then returns to SELECT_DIE on the die the
  user was last using: the selection lives in RAM, which survives the power-down
  sleep and is not reset on wake (a true power cycle restarts at d2). A short press
  from SHOW_RESULT returns to SELECT_DIE with the same die still selected.
- The inactivity timeout applies in SELECT_DIE as well, so a forgotten blinking
  device still goes to sleep instead of draining the cell.
- A perfect 20 on the d20 detours through CRIT before the 20 is revealed; no other
  result does. POWER_ON, POWER_OFF and CRIT all share one spiral animation
  (`spiralFrame`): a rotating arc that grows to fill the ring (power-on, crit) or
  shrinks to nothing (power-off) while D13 strobes. Every flourish is timed like
  the chase (nonblocking), so the button keeps working throughout.

## LED driver

All 13 LEDs share PA1-PA5, so only one can conduct at a time. `lib/charlieplex`
scans the LEDs with a TCB1 compare interrupt: one LED slot per interrupt, 13 slots
per scan, 100 Hz per LED (1300 interrupts/s, roughly 1% of the CPU). The
application only ever calls `setPattern(mask, status)`, where bit 0 of the mask is
D1, bit 11 is D12 and D13 is a separate flag.

- The anode pin is driven high and the cathode pin low; every other bus pin stays
  a high-impedance input with no pull-up.
- The previous LED is always released (pins back to inputs) *before* the next one
  is enabled, so the anti-parallel pairs (D1/D2, D3/D4, ...) can never both
  conduct and there is no ghosting.
- The pattern update is atomic with respect to the refresh interrupt, and
  redundant updates are dropped.
- TCB1 is used because the core's `millis()` runs on TCD0 and TCB0 is left free
  for `tone()`. `charlieplex::end()` stops the timer and releases the bus before
  sleep, so no LED can stay lit and the timer draws no current.
- Duty cycle is 1/13 per LED, which keeps the CR2032's peak and average current
  predictable. If the display is too dim on real hardware, raise `LED_REFRESH_HZ`
  in `charlieplex.cpp` (lowering it causes visible flicker) or scan only the lit
  LEDs, which raises the duty cycle at the cost of average current. Brightness
  must be validated on the board, as the readme asks.

## Button

`lib/button` samples PA6 (pull-up, active low) without blocking. A change must be
stable for `DEBOUNCE_MS` (20 ms) before it counts; a release before
`LONG_PRESS_MS` (600 ms) is a short press, and a long press is reported as soon as
the threshold is reached with the later release suppressed. A second short press
released within `DOUBLE_CLICK_MS` (300 ms) of the previous one is reported as
`DOUBLE_CLICK` instead of `SHORT_PRESS`; detection is not deferred, so a lone
short press is still reported on release and the first click of a double-click has
already acted. `armWake()` enables the falling-edge pin interrupt that wakes the
MCU from power-down and marks the next press as absorbed.

Because waking from power-down is not a reset, the application's state lives on in
SRAM: `app.cpp` simply does not reset `s_dieIndex` when it re-enters `SELECT_DIE`,
so the device wakes on the die it was last using. `update()` captures the die
index before every short press and restores it if that press turns out to be the
first half of a double-click, so the power-off gesture cannot leave the remembered
die one step off.

`button.cpp` owns `ISR(PORTA_PORT_vect)` directly. PlatformIO's atmelmegaavr
Arduino framework defines `CORE_ATTACH_ALL`, which suppresses the core's
`attachInterrupt()` dispatcher ISR, so there is no duplicate symbol (the define is
also stated explicitly in `platformio.ini`).

## Random numbers

`src/dice/prng.cpp` uses xorshift32. The seed combines the noisy low bits of 16
internal temperature-sensor conversions with the free-running LED refresh counter
sampled during each conversion (timing jitter); `millis()` and `micros()` are
mixed in as well, and `stir(micros())` at the start of every roll adds the human
press timing. `roll()` maps to 1..sides with rejection sampling, so no face is
favoured (the modulo is applied only to an unbiased sub-range). The ADC is
disabled again after seeding. This is a casual dice roller, not a cryptographic
device.

## Hardware bring-up test

`pio run -e hwtest -t upload` builds a self-contained test:

1. **Walk phase** - D1 through D13 light one at a time, in table order, 400 ms
   each, driven directly (refresh timer stopped) so each LED and the exact
   mapping can be checked individually.
2. **Interactive phase** - exercises the multiplexed driver and the button: hold
   the button and all 12 perimeter LEDs light; release and D13 pulses once per
   short press. This verifies the debounce, the short-press and long-press paths,
   and that multi-LED patterns show no ghosting.


