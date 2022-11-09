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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "datatypes.h"
#include "time.h"

#define MAX_SUPPORTED_MOTORS 8

#if defined(__cplusplus)
extern "C" {
#endif

    // void    motorCheckDshotBitbangStatus(Arming::data_t * arming);
    float   motorConvertFromExternal(void * motorDevice, uint16_t externalValue);
    void    motorInitBrushed(uint8_t * pins);
    void  * motorInitDshot(uint8_t count);
    bool    motorIsProtocolDshot(void);
    bool    motorIsReady(uint32_t currentTime);
    float   motorValueDisarmed(void);
    float   motorValueHigh(void);
    float   motorValueLow(void);
    void    motorStop(void * motorDevice);
    void    motorWrite(void * motorDevice, float *values);

#if defined(__cplusplus)
}
#endif