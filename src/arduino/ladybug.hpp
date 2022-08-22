/*
   Copyright (c) 2022 Simon D. Levy

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option)
   any later version.

   Hackflight is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   Hackflight. If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <Wire.h>

#include <hackflight_full.h>
#include <imu_alignment/rotate_0.h>
#include <mixers/fixedpitch/quadxbf.h>
#include <motor.h>
#include <stm32_clock.h>

#include "imu_usfs.h"

static ImuUsfs _imu;

static AnglePidController _anglePid(
        1.441305,     // Rate Kp
        19.55048,     // Rate Ki
        0.021160,     // Rate Kd
        0.0165048,    // Rate Kf
        0.0); // 3.0; // Level Kp


QuadXbfMixer _mixer; 

static uint8_t _motorPins[4] = {13, 16, 3, 11};

class LadybugFc : public Hackflight {

    public:

        LadybugFc(Receiver * receiver) 

            : Hackflight(
                    receiver,
                    &_imu,
                    imuRotate0,
                    &_anglePid,
                    &_mixer,
                    (void *)&_motorPins,
                    12,  // IMU interrupt pin
                    18)  // LED pin
        {
        }

        void begin(void)
        {
            Wire.begin();
            delay(100);

            motorInitBrushed(_motorPins);

            stm32_startCycleCounter();

            Hackflight::begin();
        }
};
