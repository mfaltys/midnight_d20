/*
 * Application timing constants for the dice roller.
 *
 * The button debounce window, the long-press threshold and the double-click
 * window live with the button driver (lib/button/button.h) because they define
 * its behaviour; everything the state machine times is here.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// D13 blink while choosing a die: on for STATUS_BLINK_HALF_MS, off for the same.
constexpr uint16_t STATUS_BLINK_HALF_MS = 250;

// Rolling animation: a clockwise chase around D1..D12 that advances every
// ROLL_CHASE_STEP_MS and stops after ROLL_ANIMATION_MS (about one and a bit
// laps of the perimeter).
constexpr uint16_t ROLL_ANIMATION_MS = 800;
constexpr uint16_t ROLL_CHASE_STEP_MS = 60;

// Crit flourish for a perfect 20 on the d20: a fast spiral around the perimeter
// (a rotating arc whose trail grows a LED at a time until the ring is full)
// while D13 strobes, played before the result is revealed.
constexpr uint16_t CRIT_ANIMATION_MS = 900;
constexpr uint16_t CRIT_SPIRAL_STEP_MS = 30;
constexpr uint8_t CRIT_TAIL_GROW_STEPS = 2;   // spiral steps per extra trail LED
constexpr uint16_t CRIT_STROBE_HALF_MS = 50;  // D13 strobe half-period (~10 Hz)

// Short D13 confirmation pulse once a result is finalised.
constexpr uint16_t RESULT_PULSE_MS = 120;

// Inactivity timeout: LEDs off and power-down sleep (readme.md asks for 30-60 s).
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 45000;

#endif  // CONFIG_H
