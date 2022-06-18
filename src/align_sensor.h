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

#include <math.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

    static void alignSensorViaRotation(float *dest)
    {
        const float x = dest[0];
        const float y = dest[1];
        const float z = dest[2];

        // 270 degrees
        dest[0] = -y;
        dest[1] = x;
        dest[2] = z;
    }

#if defined(__cplusplus)
}
#endif