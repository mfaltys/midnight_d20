#include "charlieplex.h"

#include <avr/interrupt.h>
#include <avr/io.h>

namespace charlieplex {
namespace {

// ---------------------------------------------------------------------------
// Charlieplex bus: PA1..PA5, one bit per pin in PORTA.
// ---------------------------------------------------------------------------
constexpr uint8_t PA1 = 1 << 1;
constexpr uint8_t PA2 = 1 << 2;
constexpr uint8_t PA3 = 1 << 3;
constexpr uint8_t PA4 = 1 << 4;
constexpr uint8_t PA5 = 1 << 5;
constexpr uint8_t BUS_MASK = PA1 | PA2 | PA3 | PA4 | PA5;

// Exact LED mapping (anode = source/high, cathode = sink/low).
struct LedDef {
  uint8_t anode;
  uint8_t cathode;
};

const LedDef LEDS[LED_COUNT] = {
    {PA1, PA2},  // D1
    {PA2, PA1},  // D2
    {PA1, PA3},  // D3
    {PA3, PA1},  // D4
    {PA1, PA4},  // D5
    {PA4, PA1},  // D6
    {PA1, PA5},  // D7
    {PA5, PA1},  // D8
    {PA2, PA3},  // D9
    {PA3, PA2},  // D10
    {PA2, PA4},  // D11
    {PA4, PA2},  // D12
    {PA2, PA5},  // D13 (status)
};

// ---------------------------------------------------------------------------
// Refresh timing
// ---------------------------------------------------------------------------
// One LED slot per interrupt and SLOT_COUNT slots per full scan, so every LED is
// re-illuminated LED_REFRESH_HZ times per second. 100 Hz per LED is well above
// flicker fusion and keeps the ISR load near 1% of the CPU.
constexpr uint8_t SLOT_COUNT = LED_COUNT;
constexpr uint16_t LED_REFRESH_HZ = 100;
constexpr uint32_t SLOT_RATE_HZ = static_cast<uint32_t>(LED_REFRESH_HZ) * SLOT_COUNT;

// TCB1 counts at CLK_PER/2 in periodic interrupt mode, so this compare value
// produces exactly SLOT_RATE_HZ interrupts per second.
constexpr uint32_t TCB1_TOP_RAW = (F_CPU / 2UL) / SLOT_RATE_HZ - 1UL;
static_assert(TCB1_TOP_RAW <= 0xFFFFUL,
              "LED refresh rate too low for the 16-bit TCB1 compare register");
constexpr uint16_t TCB1_TOP = static_cast<uint16_t>(TCB1_TOP_RAW);

// ---------------------------------------------------------------------------
// State shared with the refresh interrupt
// ---------------------------------------------------------------------------
volatile uint16_t s_pattern = 0;  // bits 0..11 = D1..D12
volatile uint8_t s_status = 0;    // D13
uint8_t s_slot = 0;               // next slot to service

// Drive `anode` high and `cathode` low, leaving the other bus pins high
// impedance. Everything is released before the new LED is enabled so the two
// anti-parallel LEDs that share a pin pair can never both conduct (no ghosting).
inline void drive(uint8_t anode, uint8_t cathode) {
  PORTA.DIRCLR = BUS_MASK;   // 1. release the previous LED
  PORTA.OUTCLR = BUS_MASK;   // 2. output latches low, pull-ups stay off
  PORTA.OUTSET = anode;      // 3. latch the new levels before enabling drivers
  PORTA.DIRSET = static_cast<uint8_t>(anode | cathode);
}

inline void releaseAll() {
  PORTA.DIRCLR = BUS_MASK;
  PORTA.OUTCLR = BUS_MASK;
}

}  // namespace

// ---------------------------------------------------------------------------
// Refresh interrupt: light one slot's LED and advance to the next slot.
// ---------------------------------------------------------------------------
ISR(TCB1_INT_vect) {
  TCB1.INTFLAGS = TCB_CAPT_bm;

  const uint8_t slot = s_slot;
  s_slot = static_cast<uint8_t>((slot + 1) % SLOT_COUNT);

  if (slot < PERIMETER_COUNT) {
    if (s_pattern & (1u << slot)) {
      drive(LEDS[slot].anode, LEDS[slot].cathode);
    } else {
      releaseAll();
    }
  } else if (s_status) {
    drive(LEDS[STATUS_LED].anode, LEDS[STATUS_LED].cathode);
  } else {
    releaseAll();
  }
}

void begin() {
  releaseAll();

  // Bus pins: no pull-ups, no pin interrupts (they are outputs while driving).
  PORTA.PIN1CTRL = 0;
  PORTA.PIN2CTRL = 0;
  PORTA.PIN3CTRL = 0;
  PORTA.PIN4CTRL = 0;
  PORTA.PIN5CTRL = 0;

  s_pattern = 0;
  s_status = 0;
  s_slot = 0;

  // TCB1: periodic interrupt mode, CLK_PER/2.
  TCB1.CTRLB = TCB_CNTMODE_INT_gc;
  TCB1.CCMP = TCB1_TOP;
  TCB1.INTFLAGS = TCB_CAPT_bm;
  TCB1.INTCTRL = TCB_CAPT_bm;
  TCB1.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
}

void end() {
  TCB1.INTCTRL = 0;
  TCB1.CTRLA = 0;  // stop and reset the counter
  TCB1.INTFLAGS = TCB_CAPT_bm;
  releaseAll();
}

void setPattern(uint16_t perimeterMask, bool statusOn) {
  const uint8_t status = statusOn ? 1 : 0;
  if (perimeterMask == s_pattern && status == s_status) {
    return;  // nothing to do
  }
  const uint8_t sreg = SREG;
  cli();
  s_pattern = perimeterMask;
  s_status = status;
  SREG = sreg;
}

void off() {
  setPattern(0, false);
  releaseAll();
}

void testDriveOne(uint8_t ledIndex) {
  if (ledIndex >= LED_COUNT) {
    testDriveNone();
    return;
  }
  drive(LEDS[ledIndex].anode, LEDS[ledIndex].cathode);
}

void testDriveNone() { releaseAll(); }

}  // namespace charlieplex
