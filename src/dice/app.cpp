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
  SHOW_RESULT,  // the rolled value on D1..D12 until the next interaction
};

State s_state = State::SLEEP;
uint8_t s_dieIndex = 0;      // selection slot, 0 = d2; kept in RAM across sleep
uint8_t s_result = 0;        // last rolled value (1..12)
// Die index captured just before the most recent short press, so that a
// double-click can roll back the die change its first click already made.
uint8_t s_dieBeforeShortPress = 0;
uint32_t s_lastActivityMs = 0;
uint32_t s_stateEnteredMs = 0;
uint32_t s_lastChaseStepMs = 0;
uint8_t s_chaseIndex = 0;
uint32_t s_statusPulseUntilMs = 0;

void enterSelectDie();
void enterRolling(uint32_t now);
void enterShowResult(uint32_t now);
void enterSleep();
void sleepUntilWake();
void showPattern(uint16_t mask, bool statusOn);

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

  const bool statusOn = ((now / STATUS_BLINK_HALF_MS) & 1u) == 0u;
  showPattern(dice::ledMaskForValue(dice::sidesAt(s_dieIndex)), statusOn);

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
    enterShowResult(now);
    return;
  }
  if ((uint32_t)(now - s_lastChaseStepMs) >= ROLL_CHASE_STEP_MS) {
    s_lastChaseStepMs = now;
    s_chaseIndex = static_cast<uint8_t>((s_chaseIndex + 1) % charlieplex::PERIMETER_COUNT);
    showPattern(1u << s_chaseIndex, false);
  }
}

// ---------------------------------------------------------------------------
// SHOW_RESULT
// ---------------------------------------------------------------------------
void enterShowResult(uint32_t now) {
  s_result = prng::roll(dice::sidesAt(s_dieIndex));
  s_state = State::SHOW_RESULT;
  s_lastActivityMs = now;
  s_statusPulseUntilMs = now + RESULT_PULSE_MS;
  showPattern(dice::ledMaskForValue(s_result), true);
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

  const bool pulseActive = static_cast<int32_t>(s_statusPulseUntilMs - now) > 0;
  showPattern(dice::ledMaskForValue(s_result), pulseActive);

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
    case State::SHOW_RESULT:
      updateShowResult(now, ev);
      break;
    case State::SLEEP:
      break;  // handled above
  }
}

}  // namespace app
