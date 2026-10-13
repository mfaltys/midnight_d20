#include "app.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "button.h"
#include "charlieplex.h"
#include "config.h"
#include "dice.h"
#include "prng.h"

namespace app {
namespace {

enum class State : uint8_t {
  SLEEP,        // LEDs off, MCU in power-down until the button is pressed
  POWER_ON,     // wake flourish (the spiral winds up) before SELECT_DIE
  SELECT_DIE,   // D13 blinks, the chosen die is shown on D1..D12
  ROLLING,      // clockwise chase animation
  CRIT,         // d20 landed on 20: fast spiral + D13 strobe before the result
  SHOW_RESULT,  // the rolled value on D1..D12 until the next interaction
  POWER_OFF,    // wind-down flourish (the spiral collapses) before sleeping
};

State s_state = State::SLEEP;
uint8_t s_dieIndex = 0;      // selection slot, 0 = d2; kept in RAM across sleep
uint8_t s_result = 0;        // last rolled value (1..20)
// Die index captured just before the most recent short press, so that a
// double-click can roll back the die change its first click already made.
uint8_t s_dieBeforeShortPress = 0;
uint32_t s_lastActivityMs = 0;
uint32_t s_stateEnteredMs = 0;
uint32_t s_lastChaseStepMs = 0;
uint8_t s_chaseIndex = 0;
bool s_rollReleasing = false;  // button released: finish the lap, then roll
uint8_t s_spiralStep = 0;    // step counter for any spiral flourish
uint32_t s_statusPulseUntilMs = 0;

void enterPowerOn(uint32_t now);
void enterSelectDie();
void enterRolling(uint32_t now);
void enterCrit(uint32_t now);
void enterShowResult(uint32_t now);
void enterPowerOff(uint32_t now);
void sleepUntilWake();
void showPattern(uint16_t mask, bool statusOn);

// True when the die just rolled is a d20 showing its maximum (a "perfect 20").
bool isCrit();

// ---------------------------------------------------------------------------
// Watchdog (see WATCHDOG_ENABLED / WATCHDOG_TIMEOUT_MS in config.h)
// ---------------------------------------------------------------------------
// Map the configured timeout to the first WDT_PERIOD_*_gc period that is at least
// that long. The WDT counts at 32.768 kHz, so the period codes are powers of two
// from 8 ms (8 cycles) to 8.2 s (8K cycles).
constexpr uint8_t wdtPeriodFor(uint16_t timeoutMs) {
  return timeoutMs <= 8      ? WDT_PERIOD_8CLK_gc
         : timeoutMs <= 16   ? WDT_PERIOD_16CLK_gc
         : timeoutMs <= 32   ? WDT_PERIOD_32CLK_gc
         : timeoutMs <= 64   ? WDT_PERIOD_64CLK_gc
         : timeoutMs <= 128  ? WDT_PERIOD_128CLK_gc
         : timeoutMs <= 256  ? WDT_PERIOD_256CLK_gc
         : timeoutMs <= 512  ? WDT_PERIOD_512CLK_gc
         : timeoutMs <= 1000 ? WDT_PERIOD_1KCLK_gc
         : timeoutMs <= 2000 ? WDT_PERIOD_2KCLK_gc
         : timeoutMs <= 4000 ? WDT_PERIOD_4KCLK_gc
                             : WDT_PERIOD_8KCLK_gc;
}
static_assert(WATCHDOG_TIMEOUT_MS >= 8 && WATCHDOG_TIMEOUT_MS <= 8200,
              "WATCHDOG_TIMEOUT_MS is outside the WDT's 8 ms..8.2 s range");
constexpr uint8_t WDT_TIMEOUT_CODE = wdtPeriodFor(WATCHDOG_TIMEOUT_MS);

// WDT.CTRLA is a protected register: the CCP write sequence must not be
// interrupted, so keep it inside a critical section (the same pattern the LED
// driver uses for its pattern update). A write is also dropped while the previous
// one is still syncing, hence the SYNCBUSY wait.
inline void wdtWrite(uint8_t period) {
  while (WDT.STATUS & WDT_SYNCBUSY_bm) {
  }
  const uint8_t sreg = SREG;
  cli();
  _PROTECTED_WRITE(WDT.CTRLA, period);
  SREG = sreg;
}

// Start the watchdog (compiles to nothing when WATCHDOG_ENABLED is false).
// WINDOW_OFF means the WDT only has to be pet before the period expires, never
// too early.
void watchdogEnable() {
  if (WATCHDOG_ENABLED) {
    wdtWrite(static_cast<uint8_t>(WDT_TIMEOUT_CODE | WDT_WINDOW_OFF_gc));
  }
}

// Stop the watchdog, so it cannot reset the device across a power-down sleep.
void watchdogDisable() {
  if (WATCHDOG_ENABLED) {
    wdtWrite(WDT_PERIOD_OFF_gc);
  }
}

// Feed the watchdog; called once per main-loop pass, so only a stalled update()
// ever lets it reach its timeout.
void watchdogPet() {
  if (WATCHDOG_ENABLED) {
    wdt_reset();  // the WDR instruction
  }
}

// ---------------------------------------------------------------------------
// Spiral flourish (shared by POWER_ON, POWER_OFF and CRIT)
// ---------------------------------------------------------------------------
uint16_t arcMask(uint8_t start, uint8_t length) {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < length; i++) {
    mask |= static_cast<uint16_t>(1u << ((start + i) % charlieplex::PERIMETER_COUNT));
  }
  return mask;
}

// One frame of a spiral flourish. `step` counts spiral steps and the trail grows
// one LED every `growDivisor` steps; `grow` winds the arc outward to fill the
// ring, otherwise it collapses to nothing. D13 strobes throughout.
void spiralFrame(uint32_t now, uint8_t step, uint8_t growDivisor, bool grow) {
  const uint8_t count = charlieplex::PERIMETER_COUNT;
  const uint8_t span = static_cast<uint8_t>(1u + step / growDivisor);
  uint8_t trail;
  if (grow) {
    trail = (span > count) ? count : span;
  } else {
    trail = (span > count) ? 0 : static_cast<uint8_t>(count - span + 1u);
  }
  const uint8_t head = static_cast<uint8_t>(step % count);
  const bool strobeOn = ((now / SPIRAL_STROBE_HALF_MS) & 1u) == 0u;
  showPattern(arcMask(head, trail), strobeOn);
}

// ---------------------------------------------------------------------------
// SLEEP
// ---------------------------------------------------------------------------
void sleepUntilWake() {
  charlieplex::off();   // LEDs dark
  charlieplex::end();   // refresh timer off
  ADC0.CTRLA = 0;       // the ADC is unused while asleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // Stop the millis timer interrupt so only the button can wake the MCU. TCD0 is
  // clocked from OSC20M and would stop in power-down anyway, but stopping it
  // explicitly means a timer overflow can never cost us a wake-up.
  stop_millis();

  // Arm the wake-up and check the button with interrupts disabled so an edge
  // cannot slip in between the check and the sleep.
  cli();
  button::armWake();
  if (button::isDown()) {
    // The button is held (for example the inactivity timeout expired mid-press).
    // Sleeping now would miss the falling edge, so stay awake and try again.
    sei();
    restart_millis();
    return;
  }
  sei();

  // The WDT keeps running in power-down and would reset the device (losing the
  // remembered die) long before the inactivity sleep ends, so stop it just before
  // parking the MCU and restart it the moment we wake.
  watchdogDisable();

  // Re-arm the sleep bit before every sleep_cpu(): it is cleared when the MCU
  // wakes, so without this a stray interrupt would leave the CPU spinning at full
  // current instead of going back to sleep.
  while (!button::wakeFlag()) {
    sleep_enable();
    sleep_cpu();
    sleep_disable();
  }

  // millis() is frozen across the sleep, which is fine: no timeout spans a sleep
  // (the inactivity timer is restarted on wake).
  restart_millis();
  watchdogEnable();
  charlieplex::begin();  // refresh timer back on (all LEDs still dark)
  // SRAM is retained through power-down and waking is not a reset, so the die the
  // user was last using is still in s_dieIndex: wake up on it rather than on d2.
  enterPowerOn(millis());
}

// ---------------------------------------------------------------------------
// POWER_ON (the wake flourish)
// ---------------------------------------------------------------------------
void enterPowerOn(uint32_t now) {
  s_state = State::POWER_ON;
  s_stateEnteredMs = now;
  s_lastChaseStepMs = now;
  s_spiralStep = 0;
  showPattern(1u << 0, false);  // the spiral starts at D1
}

void updatePowerOn(uint32_t now) {
  if ((uint32_t)(now - s_stateEnteredMs) >= POWER_ON_MS) {
    enterSelectDie();  // the device is on: let the user pick a die
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= SPIRAL_STEP_MS) {
    s_lastChaseStepMs = now;
    s_spiralStep++;
  }
  spiralFrame(now, s_spiralStep, POWER_TAIL_GROW_STEPS, true);
}

// ---------------------------------------------------------------------------
// SELECT_DIE
// ---------------------------------------------------------------------------
void enterSelectDie() {
  s_lastActivityMs = millis();
  s_state = State::SELECT_DIE;
}

void updateSelectDie(uint32_t now, button::Event ev) {
  if (ev == button::Event::SHORT_PRESS) {
    s_dieIndex = static_cast<uint8_t>((s_dieIndex + 1) % dice::COUNT);
    s_lastActivityMs = now;
  } else if (ev == button::Event::LONG_PRESS) {
    enterRolling(now);
    return;
  }

  // D13 blinks as the "choosing a die" cue, and the die is shown by its
  // selection indicator. Dice up to d12 light their maximum count; d20 has no
  // room for 20 LEDs, so it lights the whole ring, and because D13 is part of
  // that pattern the whole thing blinks to make "d20 selected" unmistakable.
  // Every other die keeps a steady perimeter with only D13 blinking.
  const uint8_t sides = dice::sidesAt(s_dieIndex);
  const dice::Display d = dice::selectionFor(sides);
  const bool blinkOn = ((now / STATUS_BLINK_HALF_MS) & 1u) == 0u;
  const uint16_t mask = (d.statusOn && !blinkOn) ? 0u : d.perimeterMask;
  showPattern(mask, blinkOn);

  if ((uint32_t)(now - s_lastActivityMs) >= INACTIVITY_TIMEOUT_MS) {
    enterPowerOff(now);  // wind down, then sleep
  }
}

// ---------------------------------------------------------------------------
// ROLLING
// ---------------------------------------------------------------------------
void enterRolling(uint32_t now) {
  charlieplex::off();  // clear the previous result / selection display
  prng::stir(micros());  // human timing is real entropy
  s_state = State::ROLLING;
  s_stateEnteredMs = now;
  s_lastChaseStepMs = now;
  s_chaseIndex = 0;
  s_rollReleasing = false;
  showPattern(1u << 0, false);  // the chase starts at D1
}

void updateRolling(uint32_t now) {
  // The chase loops for as long as the button is held. Releasing does not stop it
  // immediately: the chase always finishes the revolution it is in (all the way
  // back round to D1) before the die is rolled, so the result never appears
  // mid-circle however long the user wound the roll up for.
  if (!s_rollReleasing && !button::isDown()) {
    s_rollReleasing = true;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= ROLL_CHASE_STEP_MS) {
    s_lastChaseStepMs = now;
    s_chaseIndex = static_cast<uint8_t>((s_chaseIndex + 1) % charlieplex::PERIMETER_COUNT);
    if (s_rollReleasing && s_chaseIndex == 0) {
      // Back at D1: the revolution is complete, so roll the die now.
      s_result = prng::roll(dice::sidesAt(s_dieIndex));
      if (isCrit()) {
        enterCrit(now);  // a perfect 20 celebrates first
      } else {
        enterShowResult(now);
      }
      return;
    }
    showPattern(1u << s_chaseIndex, false);
  }
}

// ---------------------------------------------------------------------------
// CRIT (a d20 landed on 20)
// ---------------------------------------------------------------------------
bool isCrit() {
  const uint8_t sides = dice::sidesAt(s_dieIndex);
  return sides == dice::MAX_VALUE && s_result == sides;  // a perfect 20
}

void enterCrit(uint32_t now) {
  s_state = State::CRIT;
  s_stateEnteredMs = now;
  s_lastChaseStepMs = now;
  s_spiralStep = 0;
  showPattern(1u << 0, false);  // the spiral starts at D1
}

void updateCrit(uint32_t now) {
  if ((uint32_t)(now - s_stateEnteredMs) >= CRIT_ANIMATION_MS) {
    enterShowResult(now);
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= SPIRAL_STEP_MS) {
    s_lastChaseStepMs = now;
    s_spiralStep++;
  }
  spiralFrame(now, s_spiralStep, CRIT_TAIL_GROW_STEPS, true);
}

// ---------------------------------------------------------------------------
// SHOW_RESULT
// ---------------------------------------------------------------------------
void enterShowResult(uint32_t now) {
  const uint8_t sides = dice::sidesAt(s_dieIndex);
  s_state = State::SHOW_RESULT;
  s_lastActivityMs = now;
  s_statusPulseUntilMs = now + RESULT_PULSE_MS;
  // First frame: the result plus the D13 confirmation pulse. The d20 "+10"
  // marker is folded in by updateShowResult on the following passes.
  showPattern(dice::displayFor(sides, s_result).perimeterMask, true);
}

void updateShowResult(uint32_t now, button::Event ev) {
  if (ev == button::Event::SHORT_PRESS) {
    enterSelectDie();  // keep the die that is selected, back to choosing
    return;
  }
  if (ev == button::Event::LONG_PRESS) {
    enterRolling(now);  // roll the same die again
    return;
  }

  // D13 carries two meanings here: the brief confirmation pulse (every die) and
  // the d20 "+10" marker for results above 10. OR them so an 11..20 result keeps
  // the marker lit after the pulse has ended.
  const bool pulseActive = static_cast<int32_t>(s_statusPulseUntilMs - now) > 0;
  const dice::Display d = dice::displayFor(dice::sidesAt(s_dieIndex), s_result);
  showPattern(d.perimeterMask, d.statusOn || pulseActive);

  if ((uint32_t)(now - s_lastActivityMs) >= INACTIVITY_TIMEOUT_MS) {
    enterPowerOff(now);  // wind down, then sleep
  }
}

// ---------------------------------------------------------------------------
// POWER_OFF (the wind-down flourish, then sleep)
// ---------------------------------------------------------------------------
void enterPowerOff(uint32_t now) {
  s_state = State::POWER_OFF;
  s_stateEnteredMs = now;
  s_lastChaseStepMs = now;
  s_spiralStep = 0;
  showPattern(charlieplex::ALL_PERIMETER, false);  // the spiral starts full
}

void updatePowerOff(uint32_t now) {
  if ((uint32_t)(now - s_stateEnteredMs) >= POWER_OFF_MS) {
    s_state = State::SLEEP;  // next update() darkens and parks the MCU
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= SPIRAL_STEP_MS) {
    s_lastChaseStepMs = now;
    s_spiralStep++;
  }
  spiralFrame(now, s_spiralStep, POWER_TAIL_GROW_STEPS, false);
}

// ---------------------------------------------------------------------------
// Shared
// ---------------------------------------------------------------------------
void showPattern(uint16_t mask, bool statusOn) {
  charlieplex::setPattern(mask, statusOn);  // the driver drops redundant updates
}

}  // namespace

void begin() {
  charlieplex::begin();  // refresh timer running, all LEDs dark
  button::begin();
  prng::begin();  // ADC-noise + timing-jitter seed, ADC left disabled
  s_lastActivityMs = millis();
  watchdogEnable();  // on from the first awake moment, before the first sleep
  sleepUntilWake();  // boot dark and asleep; the first press wakes the device
}

void update() {
  if (s_state == State::SLEEP) {
    sleepUntilWake();
    return;
  }

  watchdogPet();  // the main loop is running, so keep the watchdog fed

  const uint32_t now = millis();
  const button::Event ev = button::poll();
  if (ev != button::Event::NONE) {
    s_lastActivityMs = now;  // any interaction resets the inactivity timer
  }

  if (ev == button::Event::SHORT_PRESS) {
    // Remember the die as it was before this click. If a second click completes a
    // double-click the pair powers the device off, and this click's effect (the
    // die advance) is rolled back so the device wakes on the die the user chose.
    s_dieBeforeShortPress = s_dieIndex;
  }

  if (ev == button::Event::DOUBLE_CLICK) {
    // Double-click powers the device off from any active state, without waiting
    // for the inactivity timeout. The first click of the pair already advanced the
    // die (or bounced SHOW_RESULT back to SELECT_DIE), so undo the die change
    // before powering down and the remembered die stays correct.
    s_dieIndex = s_dieBeforeShortPress;
    enterPowerOff(now);
    return;
  }

  switch (s_state) {
    case State::POWER_ON:
      updatePowerOn(now);
      break;
    case State::SELECT_DIE:
      updateSelectDie(now, ev);
      break;
    case State::ROLLING:
      updateRolling(now);
      break;
    case State::CRIT:
      updateCrit(now);
      break;
    case State::SHOW_RESULT:
      updateShowResult(now, ev);
      break;
    case State::POWER_OFF:
      updatePowerOff(now);
      break;
    case State::SLEEP:
      break;  // handled above
  }
}

}  // namespace app
