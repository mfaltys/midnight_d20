/*
 * Active-low navigation button on PA6 (internal pull-up), nonblocking.
 *
 * poll() debounces the pin and reports at most one event per physical press:
 * a SHORT_PRESS on release before LONG_PRESS_MS, or a LONG_PRESS as soon as the
 * button has been held that long. A long press never also reports a short press,
 * and one physical press never produces more than one event.
 *
 * Two short presses released within DOUBLE_CLICK_MS of each other are reported as
 * a DOUBLE_CLICK in place of the second SHORT_PRESS. Detection is deliberately
 * NOT deferred: a lone short press is still reported the instant it is released,
 * so the first click of a double-click has already performed its normal action by
 * the time the DOUBLE_CLICK arrives. A click followed by a long hold still
 * reports LONG_PRESS, never a double-click.
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

namespace button {

// Debounce window, long-press threshold and double-click window, each a named
// constant so the timing behaviour can be tuned in one place.
// DOUBLE_CLICK_MS is measured from the release of the first
// short press to the release of the second, so the whole second click must fit
// inside it. Lowering it makes accidental double-clicks (fast die cycling) less
// likely at the cost of requiring a snappier double-click.
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint16_t LONG_PRESS_MS = 600;
constexpr uint16_t DOUBLE_CLICK_MS = 300;

enum class Event : uint8_t {
  NONE,
  SHORT_PRESS,
  LONG_PRESS,
  DOUBLE_CLICK,
};

// Configure PA6: pull-up enabled, no interrupt (armWake() enables it).
void begin();

// Sample the button and return at most one event. Call once per main-loop pass.
Event poll();

// Raw (undebounced) pin state: true while the button is held down.
bool isDown();

// --- Wake-up support -------------------------------------------------------
// armWake() clears any pending edge, enables the falling-edge sense interrupt
// that wakes the MCU from power-down, and marks the press that follows as
// absorbed, so the press that wakes the device does not also act as a UI event.
// Must be called with interrupts disabled to avoid missing an edge.
void armWake();

// True once the wake interrupt has fired since the last armWake().
bool wakeFlag();

}  // namespace button

#endif  // BUTTON_H
