/*
Copyright (c) 2022 Simon D. Levy

This file is part of Hackflight.

Hackflight is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Hackflight is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Hackflight. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdint.h>

#include "time.h"
#include "led.h"

static uint32_t warningLedTimer = 0;

typedef enum {
    WARNING_LED_OFF = 0,
    WARNING_LED_ON,
    WARNING_LED_FLASH
} warningLedState_e;

static warningLedState_e warningLedState = WARNING_LED_OFF;

static void warningLedRefresh(void)
{
    switch (warningLedState) {
        case WARNING_LED_OFF:
            ledSet(false);
            break;
        case WARNING_LED_ON:
            ledSet(true);
            break;
        case WARNING_LED_FLASH:
            ledToggle();
            break;
    }

    uint32_t now = timeMicros();
    warningLedTimer = now + 500000;
}

// ----------------------------------------------------------------------------

void ledWarningDisable(void)
{
    warningLedState = WARNING_LED_OFF;
}

void ledWarningFlash(void)
{
    warningLedState = WARNING_LED_FLASH;
}

void ledWarningUpdate(void)
{
    uint32_t now = timeMicros();

    if ((int32_t)(now - warningLedTimer) < 0) {
        return;
    }

    warningLedRefresh();
}

void ledFlash(uint8_t reps, uint16_t delayMs)
{
    ledSet(false);
    for (uint8_t i=0; i<reps; i++) {
        ledToggle();
        delayMillis(delayMs);
    }
    ledSet(false);
}