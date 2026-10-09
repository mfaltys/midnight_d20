#include "dice.h"

#include "charlieplex.h"

namespace dice {
namespace {

const uint8_t DIE_SIDES[COUNT] = {2, 4, 6, 8, 10, 12};

}  // namespace

uint8_t sidesAt(uint8_t index) { return DIE_SIDES[index % COUNT]; }

uint16_t ledMaskForValue(uint8_t value) {
  if (value >= charlieplex::PERIMETER_COUNT) {
    return charlieplex::ALL_PERIMETER;
  }
  return static_cast<uint16_t>((1u << value) - 1u);
}

}  // namespace dice
