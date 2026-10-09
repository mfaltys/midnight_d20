/*
 * Dice roller application: a nonblocking state machine over
 * SLEEP -> SELECT_DIE -> ROLLING -> SHOW_RESULT.
 *
 * Call begin() once from setup() and update() from loop() as fast as possible;
 * update() never blocks for more than a debounce sample, and the only place the
 * CPU is parked is power-down sleep, which the button wakes.
 */
#ifndef APP_H
#define APP_H

namespace app {

void begin();
void update();

}  // namespace app

#endif  // APP_H
