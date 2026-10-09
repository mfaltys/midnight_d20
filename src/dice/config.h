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
// ROLL_CHASE_STEP_MS. The chase loops for as long as the roll button is held and
// then always finishes the revolution it is in after release, so a roll lasts the
// hold time plus at most one lap of the perimeter.
constexpr uint16_t ROLL_CHASE_STEP_MS = 60;

// Spiral flourish: a rotating arc whose trail grows (or shrinks) a LED at a time
// while D13 strobes. Shared by the crit celebration and the power-on / power-off
// animations. One spiral step lasts SPIRAL_STEP_MS.
constexpr uint16_t SPIRAL_STEP_MS = 30;
constexpr uint16_t SPIRAL_STROBE_HALF_MS = 50;  // D13 strobe half-period (~10 Hz)

// Crit flourish for a perfect 20 on the d20: the spiral winds outward, adding a
// trail LED every CRIT_TAIL_GROW_STEPS steps, then reveals the result.
constexpr uint16_t CRIT_ANIMATION_MS = 900;
constexpr uint8_t CRIT_TAIL_GROW_STEPS = 2;

// Power-on / power-off flourishes: the same spiral, outward on wake and inward on
// the way to sleep, kept short so the device feels snappy.
constexpr uint16_t POWER_ON_MS = 450;
constexpr uint16_t POWER_OFF_MS = 450;
constexpr uint8_t POWER_TAIL_GROW_STEPS = 1;

// Short D13 confirmation pulse once a result is finalised.
constexpr uint16_t RESULT_PULSE_MS = 120;

// Inactivity timeout: LEDs off and power-down sleep (spec.md asks for 30-60 s).
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 45000;

#endif  // CONFIG_H
