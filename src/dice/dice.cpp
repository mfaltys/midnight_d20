#include "dice.h"

#include "charlieplex.h"

namespace dice {
namespace {

constexpr uint8_t DIE_SIDES[COUNT] = {2, 4, 6, 8, 10, 12, 20};

// Bitmask of the first `count` perimeter LEDs (D1..D_count).
constexpr uint16_t perimeterMask(uint8_t count) {
  return count >= charlieplex::PERIMETER_COUNT
             ? charlieplex::ALL_PERIMETER
             : static_cast<uint16_t>((1u << count) - 1u);
}

// The LEDs that show one face, used by both SELECT_DIE (the die's maximum) and
// SHOW_RESULT (the rolled value).
constexpr Display displayOf(uint8_t sides, uint8_t value) {
  return (sides > charlieplex::PERIMETER_COUNT && value > TENS_THRESHOLD)
             ? Display{perimeterMask(static_cast<uint8_t>(value - TENS_THRESHOLD)), true}
             : Display{perimeterMask(value), false};
}

// The indicator shown while the user chooses a die. Dice up to d12 show their
// maximum count; d20 cannot reach 20 on the ring, so it lights the whole thing
// and the caller flashes it (statusOn) so it stays distinct from d12.
constexpr Display selectionOf(uint8_t sides) {
  return sides > charlieplex::PERIMETER_COUNT
             ? Display{charlieplex::ALL_PERIMETER, true}
             : Display{perimeterMask(sides), false};
}

constexpr uint8_t sidesOf(uint8_t index) { return DIE_SIDES[index % COUNT]; }

// The face mapping is the crux of the d20 feature and the AVR build has no host
// test harness, so pin the invariants down at compile time.
static_assert(COUNT == 7 && TENS_THRESHOLD == 10,
              "dice table or +10 threshold changed: update the mapping asserts");
static_assert(sidesOf(0) == 2 && sidesOf(5) == 12 && sidesOf(6) == 20 && sidesOf(7) == 2,
              "dice must cycle d2,d4,d6,d8,d10,d12,d20 then wrap to d2");
static_assert(displayOf(2, 2).perimeterMask == 0x003 && !displayOf(2, 2).statusOn,
              "d2 must show D1-D2 with D13 off");
static_assert(displayOf(12, 12).perimeterMask == charlieplex::ALL_PERIMETER &&
                  !displayOf(12, 12).statusOn,
              "d12 must show all 12 perimeter LEDs with D13 off");
static_assert(displayOf(20, 10).perimeterMask == 0x3FF && !displayOf(20, 10).statusOn,
              "d20 face 10 must show D1-D10 with D13 off");
static_assert(displayOf(20, 11).perimeterMask == 0x001 && displayOf(20, 11).statusOn,
              "d20 face 11 must show D1 with D13 on");
static_assert(displayOf(20, 20).perimeterMask == 0x3FF && displayOf(20, 20).statusOn,
              "d20 face 20 must show D1-D10 with D13 on");
static_assert(selectionOf(12).perimeterMask == charlieplex::ALL_PERIMETER &&
                  !selectionOf(12).statusOn,
              "d12 selection must be the 12 steady perimeter LEDs with D13 off");
static_assert(selectionOf(20).perimeterMask == charlieplex::ALL_PERIMETER &&
                  selectionOf(20).statusOn,
              "d20 selection must light the whole ring with D13 part of the flash");

}  // namespace

uint8_t sidesAt(uint8_t index) { return sidesOf(index); }

Display displayFor(uint8_t sides, uint8_t value) { return displayOf(sides, value); }

Display selectionFor(uint8_t sides) { return selectionOf(sides); }

}  // namespace dice
