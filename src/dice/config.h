/*
 * Application timing constants for the dice roller.
 *
 * These were tuned by feel on a chip still running its factory clock fuse
 * (OSCCFG = 0x02, 20 MHz) while the firmware was built for 16 MHz, so millis()
 * ran 25% fast and every constant produced a shorter real interval than it says.
 * The clock now matches the build (the flasher writes OSCCFG = 0x01, 16 MHz), so
 * the animation constants below are 4/5 of the values they were tuned at: same
 * look, honest numbers. Retune on the device, not by scaling from here.
 *
 * The human-scale thresholds are deliberately left alone - button.h's
 * LONG_PRESS_MS / DOUBLE_CLICK_MS and INACTIVITY_TIMEOUT_MS below are meant to be
 * read as real milliseconds. (On the 20 MHz chip they behaved as 480 ms / 240 ms
 * / 36 s; scale them by 4/5 too if that is the feel you want back.)
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Status LED/D13 blink while choosing a die: on for STATUS_BLINK_HALF_MS, off for the same.
constexpr uint16_t STATUS_BLINK_HALF_MS = 200;

// Rolling animation: a clockwise chase around D1..D12 that advances every
// ROLL_CHASE_STEP_MS. The chase loops for as long as the roll button is held and
// then always finishes the revolution it is in after release, so a roll lasts the
// hold time plus at most one lap of the perimeter.
constexpr uint16_t ROLL_CHASE_STEP_MS = 40;

// Spiral flourish: a rotating arc whose trail grows (or shrinks) a LED at a time
// while D13 strobes. Shared by the crit celebration and the power-on / power-off
// animations. One spiral step lasts SPIRAL_STEP_MS.
constexpr uint16_t SPIRAL_STEP_MS = 34;
constexpr uint16_t SPIRAL_STROBE_HALF_MS = 40;  // D13 strobe half-period (~12 Hz)

// Crit flourish for a perfect 20 on the d20: the spiral winds outward, adding a
// trail LED every CRIT_TAIL_GROW_STEPS steps, then reveals the result.
constexpr uint16_t CRIT_ANIMATION_MS = 960;
constexpr uint8_t CRIT_TAIL_GROW_STEPS = 2;

// Power-on / power-off flourishes: the same spiral, outward on wake and inward on
// the way to sleep, kept short so the device feels snappy.
constexpr uint16_t POWER_ON_MS = 520;
constexpr uint16_t POWER_OFF_MS = 520;
constexpr uint8_t POWER_TAIL_GROW_STEPS = 1;

// Short D13 confirmation pulse once a result is finalised.
constexpr uint16_t RESULT_PULSE_MS = 96;

// Inactivity timeout: LEDs off and power-down sleep. Long enough to leave the
// die selected between rolls, short enough to protect the coin cell.
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 45000;

// ---------------------------------------------------------------------------
// Watchdog
// ---------------------------------------------------------------------------
// The watchdog lets the device recover on its own from a hung or corrupted main
// loop, so a glitch that used to need the cell pulled out clears itself instead.
constexpr bool WATCHDOG_ENABLED = true;

// Timeout in milliseconds, rounded up to the next available WDT period
constexpr uint16_t WATCHDOG_TIMEOUT_MS = 2000;

#endif  // CONFIG_H
