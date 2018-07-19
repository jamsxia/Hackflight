/* 
   filters.hpp: Filter classes and static methods

   Copyright (c) 2018 Simon D. Levy

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with EM7180.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cmath>
#include <math.h>

#include "debug.hpp"

namespace hf {

    class Filter {

        public:


            static float max(float a, float b)
            {
                return a > b ? a : b;
            }

            static float complementary(float a, float b, float c)
            {
                return a * c + b * (1 - c);
            }

            static float constrainMinMax(float val, float min, float max)
            {
                return (val<min) ? min : ((val>max) ? max : val);
            }

            static float constrainAbs(float val, float max)
            {
                return constrainMinMax(val, -max, +max);
            }

    }; // class Filter

    class LowPassFilter {

        private:

            float _history[256];
            uint8_t _historySize;
            uint8_t _historyIdx;
            float _sum;

        public:

            LowPassFilter(uint16_t historySize)
            {
                _historySize = historySize;
            }

            void init(void)
            {
                for (uint8_t k=0; k<_historySize; ++k) {
                    _history[k] = 0;
                }
                _historyIdx = 0;
                _sum = 0;
            }

            float update(float value)
            {
                uint8_t indexplus1 = (_historyIdx + 1) % _historySize;
                _history[_historyIdx] = value;
                _sum += _history[_historyIdx];
                _sum -= _history[indexplus1];
                _historyIdx = indexplus1;
                return _sum / _historySize;
            }

    }; // class LowPassFilter

    // Adapted from https://github.com/kriswiner/MPU9250/blob/master/quaternionFilters.ino
    class MadgwickQuaternionFilter {

        private:

            float _beta;

        public:

            MadgwickQuaternionFilter(float beta)
            {
                this->_beta = beta;
            }

            void update(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz, float deltat, float q[4])
            {
                float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];   // short name local variable for readability
                float norm;
                float hx, hy, _2bx, _2bz;
                float s1, s2, s3, s4;
                float qDot1, qDot2, qDot3, qDot4;

                // Auxiliary variables to avoid repeated arithmetic
                float _2q1mx;
                float _2q1my;
                float _2q1mz;
                float _2q2mx;
                float _4bx;
                float _4bz;
                float _2q1 = 2.0f * q1;
                float _2q2 = 2.0f * q2;
                float _2q3 = 2.0f * q3;
                float _2q4 = 2.0f * q4;
                float _2q1q3 = 2.0f * q1 * q3;
                float _2q3q4 = 2.0f * q3 * q4;
                float q1q1 = q1 * q1;
                float q1q2 = q1 * q2;
                float q1q3 = q1 * q3;
                float q1q4 = q1 * q4;
                float q2q2 = q2 * q2;
                float q2q3 = q2 * q3;
                float q2q4 = q2 * q4;
                float q3q3 = q3 * q3;
                float q3q4 = q3 * q4;
                float q4q4 = q4 * q4;

                // Normalise accelerometer measurement
                norm = sqrtf(ax * ax + ay * ay + az * az);
                if (norm == 0.0f) return; // handle NaN
                norm = 1.0f/norm;
                ax *= norm;
                ay *= norm;
                az *= norm;

                // Normalise magnetometer measurement
                norm = sqrtf(mx * mx + my * my + mz * mz);
                if (norm == 0.0f) return; // handle NaN
                norm = 1.0f/norm;
                mx *= norm;
                my *= norm;
                mz *= norm;

                // Reference direction of Earth's magnetic field
                _2q1mx = 2.0f * q1 * mx;
                _2q1my = 2.0f * q1 * my;
                _2q1mz = 2.0f * q1 * mz;
                _2q2mx = 2.0f * q2 * mx;
                hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
                hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
                _2bx = sqrtf(hx * hx + hy * hy);
                _2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
                _4bx = 2.0f * _2bx;
                _4bz = 2.0f * _2bz;

                // Gradient decent algorithm corrective step
                s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
                s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
                s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
                s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
                norm = sqrtf(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);    // normalise step magnitude
                norm = 1.0f/norm;
                s1 *= norm;
                s2 *= norm;
                s3 *= norm;
                s4 *= norm;

                // Compute rate of change of quaternion
                qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz) - _beta * s1;
                qDot2 = 0.5f * (q1 * gx + q3 * gz - q4 * gy) - _beta * s2;
                qDot3 = 0.5f * (q1 * gy - q2 * gz + q4 * gx) - _beta * s3;
                qDot4 = 0.5f * (q1 * gz + q2 * gy - q3 * gx) - _beta * s4;

                // Integrate to yield quaternion
                q1 += qDot1 * deltat;
                q2 += qDot2 * deltat;
                q3 += qDot3 * deltat;
                q4 += qDot4 * deltat;
                norm = sqrtf(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);    // normalise quaternion
                norm = 1.0f/norm;
                q[0] = q1 * norm;
                q[1] = q2 * norm;
                q[2] = q3 * norm;
                q[3] = q4 * norm;
            }

    }; // class MadgwickQuaternionFilter

    // Adapted from https://github.com/kriswiner/MPU9250/blob/master/quaternionFilters.ino
    class MahonyQuaternionFilter {

        private:

            // Free parameters in the Mahony filter and fusion scheme, Kp for proportional feedback, Ki for integral
            const float Kp  = 2.0f * 5.0f; 
            const float Ki = 0.0f;

            float _eInt[3];

        public:

            MahonyQuaternionFilter(void)
            {
                _eInt[0] = 0;
                _eInt[1] = 0;
                _eInt[2] = 0;
            }

            void update(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz, float deltat, float q[4])
            {
                float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];   // short name local variable for readability
                float norm;
                float hx, hy, bx, bz;
                float vx, vy, vz, wx, wy, wz;
                float ex, ey, ez;
                float pa, pb, pc;

                // Auxiliary variables to avoid repeated arithmetic
                float q1q1 = q1 * q1;
                float q1q2 = q1 * q2;
                float q1q3 = q1 * q3;
                float q1q4 = q1 * q4;
                float q2q2 = q2 * q2;
                float q2q3 = q2 * q3;
                float q2q4 = q2 * q4;
                float q3q3 = q3 * q3;
                float q3q4 = q3 * q4;
                float q4q4 = q4 * q4;   

                // Normalise accelerometer measurement
                norm = sqrtf(ax * ax + ay * ay + az * az);
                if (norm == 0.0f) return; // handle NaN
                norm = 1.0f / norm;        // use reciprocal for division
                ax *= norm;
                ay *= norm;
                az *= norm;

                // Normalise magnetometer measurement
                norm = sqrtf(mx * mx + my * my + mz * mz);
                if (norm == 0.0f) return; // handle NaN
                norm = 1.0f / norm;        // use reciprocal for division
                mx *= norm;
                my *= norm;
                mz *= norm;

                // Reference direction of Earth's magnetic field
                hx = 2.0f * mx * (0.5f - q3q3 - q4q4) + 2.0f * my * (q2q3 - q1q4) + 2.0f * mz * (q2q4 + q1q3);
                hy = 2.0f * mx * (q2q3 + q1q4) + 2.0f * my * (0.5f - q2q2 - q4q4) + 2.0f * mz * (q3q4 - q1q2);
                bx = sqrtf((hx * hx) + (hy * hy));
                bz = 2.0f * mx * (q2q4 - q1q3) + 2.0f * my * (q3q4 + q1q2) + 2.0f * mz * (0.5f - q2q2 - q3q3);

                // Estimated direction of gravity and magnetic field
                vx = 2.0f * (q2q4 - q1q3);
                vy = 2.0f * (q1q2 + q3q4);
                vz = q1q1 - q2q2 - q3q3 + q4q4;
                wx = 2.0f * bx * (0.5f - q3q3 - q4q4) + 2.0f * bz * (q2q4 - q1q3);
                wy = 2.0f * bx * (q2q3 - q1q4) + 2.0f * bz * (q1q2 + q3q4);
                wz = 2.0f * bx * (q1q3 + q2q4) + 2.0f * bz * (0.5f - q2q2 - q3q3);  

                // Error is cross product between estimated direction and measured direction of gravity
                ex = (ay * vz - az * vy) + (my * wz - mz * wy);
                ey = (az * vx - ax * vz) + (mz * wx - mx * wz);
                ez = (ax * vy - ay * vx) + (mx * wy - my * wx);
                if (Ki > 0.0f)
                {
                    _eInt[0] += ex;      // accumulate integral error
                    _eInt[1] += ey;
                    _eInt[2] += ez;
                }
                else
                {
                    _eInt[0] = 0.0f;     // prevent integral wind up
                    _eInt[1] = 0.0f;
                    _eInt[2] = 0.0f;
                }

                // Apply feedback terms
                gx = gx + Kp * ex + Ki * _eInt[0];
                gy = gy + Kp * ey + Ki * _eInt[1];
                gz = gz + Kp * ez + Ki * _eInt[2];

                // Integrate rate of change of quaternion
                pa = q2;
                pb = q3;
                pc = q4;
                q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * deltat);
                q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * deltat);
                q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * deltat);
                q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * deltat);

                // Normalise quaternion
                norm = sqrtf(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
                norm = 1.0f / norm;
                q[0] = q1 * norm;
                q[1] = q2 * norm;
                q[2] = q3 * norm;
                q[3] = q4 * norm;
            }

    }; // class MahonyQuaternionFilter

} // namespace hf
