#include "prng.h"

#include <Arduino.h>
#include <avr/io.h>

namespace prng {
namespace {

uint32_t s_state = 0x2545F491UL;

// xorshift32: cheap, plenty for a dice roller.
uint32_t xorshift32(uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

// Collect entropy from the internal temperature sensor. The low bits of those
// conversions are dominated by noise, and each sample is also folded with the
// free-running LED refresh counter so conversion-timing jitter is included.
uint32_t entropyFromAdc() {
  uint32_t acc = 0x811C9DC5UL;

  ADC0.MUXPOS = ADC_MUXPOS_TEMPSENSE_gc;
  ADC0.CTRLC = ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV16_gc;
  ADC0.CTRLA = ADC_ENABLE_bm;  // 10-bit, single conversion

  for (uint8_t i = 0; i < 16; i++) {
    ADC0.COMMAND = ADC_STCONV_bm;
    while ((ADC0.INTFLAGS & ADC_RESRDY_bm) == 0) {
      // Sample the LED refresh counter while the conversion runs: its value
      // depends on the exact instruction timing, so it contributes jitter.
      acc = xorshift32(acc ^ TCB1.CNT);
    }
    const uint16_t sample = ADC0.RES;
    ADC0.INTFLAGS = ADC_RESRDY_bm;
    acc = xorshift32(acc ^ (sample & 0x03));  // keep the noisy low bits
  }

  ADC0.CTRLA = 0;  // ADC off again: it is not used anywhere else
  return acc;
}

uint32_t mix(uint32_t state, uint32_t value) {
  return xorshift32(state ^ value) * 2654435761UL + 0x9E3779B9UL;
}

}  // namespace

void begin() {
  uint32_t seed = entropyFromAdc();
  seed = mix(seed, millis());
  seed = mix(seed, micros());
  s_state = seed ? seed : 0x2545F491UL;  // xorshift32 must never be all zero
  for (uint8_t i = 0; i < 8; i++) {
    next();  // decorrelate the seed
  }
}

void stir(uint32_t noise) { s_state = mix(s_state, noise); }

uint32_t next() {
  s_state = xorshift32(s_state);
  return s_state;
}

uint8_t roll(uint8_t sides) {
  if (sides < 2) {
    return 1;
  }
  // Reject the tail of the 0..255 range that would make some faces more likely
  // than others, then reduce the remaining (unbiased) range with a modulo.
  //
  // `limit` must be 16-bit, not 8-bit: for the power-of-two dice (d2, d4, d8)
  // sides divides 256, so the limit is exactly 256, which does not fit in a
  // uint8_t. Storing it in one wraps it to 0, the rejection test `value >= limit`
  // becomes `value >= 0` (always true) and the roll hangs forever - which looks
  // like the device freezing on the last chase LED.
  const uint16_t limit = static_cast<uint16_t>(256u - (256u % sides));
  uint8_t value;
  do {
    value = static_cast<uint8_t>(next() >> 24);
  } while (value >= limit);
  return static_cast<uint8_t>((value % sides) + 1);
}

}  // namespace prng
