#include "app.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

#include "button.h"
#include "charlieplex.h"
#include "config.h"
#include "dice.h"
#include "prng.h"

namespace app {
namespace {

enum class State : uint8_t {
  SLEEP,        // LEDs off, MCU in power-down until the button is pressed
  SELECT_DIE,   // D13 blinks, the chosen die is shown on D1..D12
  ROLLING,      // clockwise chase animation
  CRIT,         // d20 landed on 20: fast spiral + D13 strobe before the result
  SHOW_RESULT,  // the rolled value on D1..D12 until the next interaction
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
uint8_t s_critStep = 0;      // spiral step during the CRIT flourish
uint32_t s_statusPulseUntilMs = 0;

void enterSelectDie();
void enterRolling(uint32_t now);
void enterCrit(uint32_t now);
void enterShowResult(uint32_t now);
void enterSleep();
void sleepUntilWake();
void showPattern(uint16_t mask, bool statusOn);

// True when the die just rolled is a d20 showing its maximum (a "perfect 20").
bool isCrit();
// Contiguous arc of `length` perimeter LEDs starting at `start`, wrapping.
uint16_t arcMask(uint8_t start, uint8_t length);

// ---------------------------------------------------------------------------
// SLEEP
// ---------------------------------------------------------------------------
void sleepUntilWake() {
  charlieplex::off();   // LEDs dark
  charlieplex::end();   // refresh timer off
  ADC0.CTRLA = 0;       // the ADC is unused while asleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Arm the wake-up and check the button with interrupts disabled so an edge
  // cannot slip in between the check and the sleep.
  cli();
  button::armWake();
  if (button::isDown()) {
    // The button is held (for example the inactivity timeout expired mid-press).
    // Sleeping now would miss the falling edge, so stay awake and try again.
    sei();
    return;
  }
  sleep_enable();
  sei();
  while (!button::wakeFlag()) {
    sleep_cpu();  // any other interrupt returns here and we sleep again
  }
  sleep_disable();
  sei();

  charlieplex::begin();  // refresh timer back on (all LEDs still dark)
  // SRAM is retained through power-down and waking is not a reset, so the die the
  // user was last using is still in s_dieIndex: wake up on it rather than on d2.
  enterSelectDie();
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
    s_state = State::SLEEP;
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
  showPattern(1u << 0, false);  // the chase starts at D1
}

void updateRolling(uint32_t now) {
  if ((uint32_t)(now - s_stateEnteredMs) >= ROLL_ANIMATION_MS) {
    // The chase is over: roll, then either celebrate a perfect 20 or reveal.
    s_result = prng::roll(dice::sidesAt(s_dieIndex));
    if (isCrit()) {
      enterCrit(now);
    } else {
      enterShowResult(now);
    }
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= ROLL_CHASE_STEP_MS) {
    s_lastChaseStepMs = now;
    s_chaseIndex = static_cast<uint8_t>((s_chaseIndex + 1) % charlieplex::PERIMETER_COUNT);
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

uint16_t arcMask(uint8_t start, uint8_t length) {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < length; i++) {
    mask |= static_cast<uint16_t>(1u << ((start + i) % charlieplex::PERIMETER_COUNT));
  }
  return mask;
}

void enterCrit(uint32_t now) {
  s_state = State::CRIT;
  s_stateEnteredMs = now;
  s_lastChaseStepMs = now;
  s_critStep = 0;
  showPattern(1u << 0, false);  // the spiral starts at D1
}

void updateCrit(uint32_t now) {
  if ((uint32_t)(now - s_stateEnteredMs) >= CRIT_ANIMATION_MS) {
    enterShowResult(now);
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= CRIT_SPIRAL_STEP_MS) {
    s_lastChaseStepMs = now;
    s_critStep++;
  }
  // A fast spiral: a rotating arc whose trail grows one LED at a time until the
  // whole ring is lit, while D13 strobes on its own, faster period.
  const uint8_t head = static_cast<uint8_t>(s_critStep % charlieplex::PERIMETER_COUNT);
  uint8_t trail = static_cast<uint8_t>(1u + s_critStep / CRIT_TAIL_GROW_STEPS);
  if (trail > charlieplex::PERIMETER_COUNT) {
    trail = charlieplex::PERIMETER_COUNT;
  }
  const bool strobeOn = ((now / CRIT_STROBE_HALF_MS) & 1u) == 0u;
  showPattern(arcMask(head, trail), strobeOn);
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
    s_state = State::SLEEP;
  }
}

// ---------------------------------------------------------------------------
// Shared
// ---------------------------------------------------------------------------
void enterSleep() {
  // Power off now: dark immediately, then the next update() stops the refresh
  // timer and parks the MCU in the same power-down sleep as the timeout.
  charlieplex::off();
  s_state = State::SLEEP;
}

void showPattern(uint16_t mask, bool statusOn) {
  charlieplex::setPattern(mask, statusOn);  // the driver drops redundant updates
}

}  // namespace

void begin() {
  charlieplex::begin();  // refresh timer running, all LEDs dark
  button::begin();
  prng::begin();  // ADC-noise + timing-jitter seed, ADC left disabled
  s_lastActivityMs = millis();
  sleepUntilWake();  // boot dark and asleep; the first press wakes the device
}

void update() {
  if (s_state == State::SLEEP) {
    sleepUntilWake();
    return;
  }

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
    enterSleep();
    return;
  }

  switch (s_state) {
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
    case State::SLEEP:
      break;  // handled above
  }
}

}  // namespace app
