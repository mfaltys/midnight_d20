/*
 * Dice definitions and the mapping from a value to the LEDs that show it.
 *
 * Supported dice and the order the user cycles through them: d2, d4, d6, d8,
 * d10, d12, then back to d2. d20/d100 are deliberately not implemented in this
 * revision (readme.md).
 */
#ifndef DICE_H
#define DICE_H

#include <stdint.h>

namespace dice {

constexpr uint8_t COUNT = 6;      // number of selectable dice
constexpr uint8_t MAX_VALUE = 12; // largest supported die

// Sides of the selection slot `index` (0-based, wraps is the caller's job).
uint8_t sidesAt(uint8_t index);

// Bitmask of the first `value` perimeter LEDs (D1..D_value). For every supported
// die the LED count equals the rolled value: d2 shows D1-D2, d12 shows D1-D12.
uint16_t ledMaskForValue(uint8_t value);

}  // namespace dice

#endif  // DICE_H
