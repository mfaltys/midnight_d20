/*
 * Dice definitions and the mapping from a value to the LEDs that show it.
 *
 * Supported dice and the order the user cycles through them: d2, d4, d6, d8,
 * d10, d12, d20, then back to d2. d100 is deliberately not implemented.
 */
#ifndef DICE_H
#define DICE_H

#include <stdint.h>

namespace dice {

constexpr uint8_t COUNT = 7;       // number of selectable dice
constexpr uint8_t MAX_VALUE = 20;  // largest supported die

// The perimeter ring has only 12 LEDs, so a d20 face cannot be shown as a plain
// count. A value above TENS_THRESHOLD reuses the perimeter for its ones digit
// and switches D13 on as a "+10" marker instead:
//
//   1..10  -> D1..Dvalue,         status LED off
//   11..20 -> D1..D(value - 10),  status LED on
//
// Dice up to d12 are unaffected: their faces never exceed TENS_THRESHOLD.
constexpr uint8_t TENS_THRESHOLD = 10;

// The LEDs that display one face: a perimeter bitmask (bit 0 = D1 .. bit 11 =
// D12) plus the status LED flag. Keeping the two separate mirrors the LED
// driver's setPattern(mask, statusOn) interface.
struct Display {
  uint16_t perimeterMask;
  bool statusOn;
};

// Sides of the selection slot `index` (0-based, wrapping is the caller's job).
uint8_t sidesAt(uint8_t index);

// How to show `value` on a die with `sides` sides. For the dice up to d12 this
// is simply the first `value` perimeter LEDs (d2 shows D1-D2, d12 shows
// D1-D12); for d20 values above TENS_THRESHOLD add the status LED as "+10".
Display displayFor(uint8_t sides, uint8_t value);

// The indicator for a die while the user is choosing it. Dice up to d12 show
// their maximum count; d20 cannot reach 20 on the ring, so it lights the whole
// perimeter. `statusOn` is true when D13 is part of the indicator, so the caller
// flashes the whole pattern to keep "d20 selected" distinct from d12.
Display selectionFor(uint8_t sides);

}  // namespace dice

#endif  // DICE_H
