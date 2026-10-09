/*
 * Charlieplexed LED driver for the Noctis Arcanum d20.
 *
 * Thirteen LEDs (D1..D12 around the perimeter, D13 status) share PA1..PA5. Only
 * one LED can conduct at any instant, so a TCB1 compare interrupt round-robins
 * the LEDs and this driver presents the caller with a simple "which LEDs should
 * be lit" interface:
 *
 *   charlieplex::setPattern(0x000F, false);   // D1..D4 lit, status LED dark
 *
 * The exact LED table and the driver requirements are in readme.md.
 */
#ifndef CHARLIEPLEX_H
#define CHARLIEPLEX_H

#include <stdint.h>

namespace charlieplex {

// D1..D12 (perimeter) are addressed as a bitmask: bit 0 = D1 ... bit 11 = D12.
constexpr uint8_t PERIMETER_COUNT = 12;
constexpr uint16_t ALL_PERIMETER = 0x0FFF;

// All 13 LEDs in the same order as the table in readme.md (0 = D1 ... 12 = D13).
constexpr uint8_t LED_COUNT = 13;
constexpr uint8_t STATUS_LED = 12;  // D13

// Release every LED pin to high impedance and start the refresh timer.
// The pattern starts empty (all LEDs dark).
void begin();

// Stop the refresh timer and release every LED pin. Call before power-down
// sleep so no LED can stay lit and the timer stops drawing current.
void end();

// Display `perimeterMask` (bits 0..11 = D1..D12) plus the status LED.
// May be called from the main loop; the update is atomic with respect to the
// refresh interrupt. Redundant calls are cheap no-ops.
void setPattern(uint16_t perimeterMask, bool statusOn);

// All LEDs off.
void off();

// Hardware bring-up helpers: drive exactly one LED (0-based index into the
// D1..D13 table) or release the bus. Only valid while the refresh timer is
// stopped, i.e. before begin() or after end().
void testDriveOne(uint8_t ledIndex);
void testDriveNone();

}  // namespace charlieplex

#endif  // CHARLIEPLEX_H
