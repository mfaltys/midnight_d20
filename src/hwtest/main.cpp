/*
 * Noctis Arcanum d20 - hardware bring-up test.
 *
 * Build/flash:  pio run -e hwtest -t upload
 *
 * 1. Walk phase: lights D1..D13 one at a time, in table order, 400 ms each, so
 *    every LED and the exact Charlieplex mapping can be verified. Only one LED
 *    is ever driven at a time here, with the refresh timer stopped.
 * 2. Interactive phase: exercises the multiplexed driver and the button.
 *      - hold the button  : all 12 perimeter LEDs lit (multiplexed refresh)
 *      - release           : one short D13 pulse per short press
 *    Pressing and holding therefore proves the long-press path, and each
 *    release proves the debounced short-press path.
 */
#include <Arduino.h>

#include "button.h"
#include "charlieplex.h"

namespace {

constexpr uint16_t WALK_STEP_MS = 400;   // time each LED stays lit in the walk
constexpr uint16_t CONFIRM_PULSE_MS = 250;  // D13 pulse per short press

enum class Phase : uint8_t { WALK, INTERACTIVE };

Phase s_phase = Phase::WALK;
uint8_t s_walkIndex = 0;
uint32_t s_phaseStepMs = 0;
bool s_holding = false;
uint32_t s_pulseUntilMs = 0;

void startInteractive(uint32_t now) {
  charlieplex::testDriveNone();
  charlieplex::begin();  // multiplexed refresh for the interactive phase
  charlieplex::setPattern(0, false);
  s_phase = Phase::INTERACTIVE;
  s_phaseStepMs = now;
}

void updateWalk(uint32_t now) {
  if ((uint32_t)(now - s_phaseStepMs) < WALK_STEP_MS) {
    return;
  }
  s_phaseStepMs = now;

  if (s_walkIndex < charlieplex::LED_COUNT) {
    charlieplex::testDriveOne(s_walkIndex);  // 0 = D1 ... 12 = D13
    s_walkIndex++;
    return;
  }
  startInteractive(now);
}

void updateInteractive(uint32_t now, button::Event ev) {
  if (ev == button::Event::SHORT_PRESS) {
    s_pulseUntilMs = now + CONFIRM_PULSE_MS;
  }
  if (button::isDown()) {
    s_holding = true;
  } else if (s_holding) {
    s_holding = false;
  }

  if (s_holding) {
    charlieplex::setPattern(charlieplex::ALL_PERIMETER, false);
  } else if (static_cast<int32_t>(s_pulseUntilMs - now) > 0) {
    charlieplex::setPattern(0, true);
  } else {
    charlieplex::setPattern(0, false);
  }
}

}  // namespace

void setup() {
  button::begin();
  // The walk phase drives single LEDs directly, so the refresh timer stays off.
  charlieplex::testDriveNone();
  s_phaseStepMs = millis();
}

void loop() {
  const uint32_t now = millis();

  if (s_phase == Phase::WALK) {
    updateWalk(now);
    return;
  }
  updateInteractive(now, button::poll());
}
