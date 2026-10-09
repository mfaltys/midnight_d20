/*
 * Noctis Arcanum d20 - dice roller firmware entry point.
 *
 * Build/flash:  pio run -e dice -t upload
 * The application lives in app.cpp; loop() stays nonblocking.
 */
#include <Arduino.h>

#include "app.h"

void setup() { app::begin(); }

void loop() { app::update(); }
