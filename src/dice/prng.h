/*
 * Lightweight PRNG for a casual dice roller (not a cryptographic device).
 *
 * xorshift32 supplies the raw bits; roll() maps them to 1..sides with rejection
 * sampling so no result is favoured. The generator is seeded from ADC noise plus
 * timing jitter and is stirred again whenever the user starts a roll, and it
 * knows nothing about the UI or the LED driver.
 */
#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

namespace prng {

// Gather startup entropy (ADC noise + timing jitter), seed the generator and
// leave the ADC disabled.
void begin();

// Mix extra entropy (e.g. micros() captured at the moment of a button press).
void stir(uint32_t noise);

// Raw 32-bit pseudo-random value.
uint32_t next();

// Uniformly distributed integer in 1..sides (rejection sampling, no modulo bias).
uint8_t roll(uint8_t sides);

}  // namespace prng

#endif  // PRNG_H
