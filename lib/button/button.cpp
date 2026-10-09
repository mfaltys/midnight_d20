#include "button.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

namespace button {
namespace {

// Debounce state (raw = last sample, stable = debounced state).
bool s_stable = false;
bool s_lastRaw = false;
uint32_t s_rawChangedAt = 0;

// Press bookkeeping.
uint32_t s_pressedAt = 0;
bool s_longSent = false;

// Absorb-the-wake-press bookkeeping.
bool s_absorbPending = false;  // the next press edge is not a UI event
bool s_absorbActive = false;   // the press currently in progress is absorbed

// Double-click bookkeeping: when the last short press was released, and whether
// it is still eligible to pair with the next release.
bool s_shortPressSeen = false;
uint32_t s_lastShortPressMs = 0;

volatile bool s_wakeFlag = false;

inline bool readRaw() { return (PORTA.IN & PIN6_bm) == 0; }  // active low

}  // namespace

// The only enabled pin interrupt in this firmware is PA6 (the button), so the
// handler just acknowledges the flag and records the wake request.
ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = PIN6_bm;
  s_wakeFlag = true;
}

void begin() {
  PORTA.PIN6CTRL = PORT_PULLUPEN_bm;  // pull-up, no interrupt until armWake()
  s_stable = readRaw();
  s_lastRaw = s_stable;
  s_rawChangedAt = millis();
  s_shortPressSeen = false;
}

Event poll() {
  const uint32_t now = millis();
  const bool raw = readRaw();

  if (raw != s_lastRaw) {
    s_lastRaw = raw;
    s_rawChangedAt = now;
  }

  if (raw != s_stable && (uint32_t)(now - s_rawChangedAt) >= DEBOUNCE_MS) {
    s_stable = raw;
    if (s_stable) {  // press edge
      s_longSent = false;
      if (s_absorbPending) {
        s_absorbPending = false;
        s_absorbActive = true;
      } else {
        s_absorbActive = false;
        s_pressedAt = now;
      }
    } else {  // release edge
      if (s_absorbActive) {
        s_absorbActive = false;
        return Event::NONE;
      }
      if (s_longSent) {
        s_longSent = false;
        s_shortPressSeen = false;  // a long press can never complete a double-click
        return Event::NONE;
      }
      // A genuine short press. If the previous one is still inside the
      // double-click window this release completes a double-click, otherwise it
      // starts a new pair.
      if (s_shortPressSeen &&
          (uint32_t)(now - s_lastShortPressMs) <= DOUBLE_CLICK_MS) {
        s_shortPressSeen = false;
        return Event::DOUBLE_CLICK;
      }
      s_shortPressSeen = true;
      s_lastShortPressMs = now;
      return Event::SHORT_PRESS;
    }
  } else if (!raw && !s_stable && s_absorbPending) {
    // The press that woke the device ended before we could sample it, so there
    // is nothing left to absorb.
    s_absorbPending = false;
  }

  if (s_stable && !s_absorbActive && !s_longSent &&
      (uint32_t)(now - s_pressedAt) >= LONG_PRESS_MS) {
    s_longSent = true;
    return Event::LONG_PRESS;
  }
  return Event::NONE;
}

bool isDown() { return readRaw(); }

void armWake() {
  s_wakeFlag = false;
  s_stable = false;
  s_lastRaw = false;
  s_longSent = false;
  s_absorbActive = false;
  s_shortPressSeen = false;  // a click from before the sleep cannot pair with the wake press
  s_absorbPending = true;  // the press that wakes the device is not an event
  PORTA.INTFLAGS = PIN6_bm;
  PORTA.PIN6CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
}

bool wakeFlag() { return s_wakeFlag; }

}  // namespace button
