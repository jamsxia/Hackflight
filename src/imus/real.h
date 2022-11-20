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

#include "arming.h"
#include "core/axes.h"
#include "core/clock.h"
#include "core/constrain.h"
#include "core/filters/pt1.h"
#include "core/vstate.h"
#include "imu.h"
#include "stats.h"
#include "time.h"

class RealImu : public Imu {

    friend class Task;
    friend class AttitudeTask;
    friend class Receiver;

    public:

        typedef Axes (*rotateFun_t)(Axes & axes);

    private:

        static const uint32_t GYRO_CALIBRATION_DURATION      = 1250000;
        static const uint16_t GYRO_LPF1_DYN_MIN_HZ           = 250;
        static const uint16_t GYRO_LPF2_STATIC_HZ            = 500;
        static const uint8_t  MOVEMENT_CALIBRATION_THRESHOLD = 48;

        typedef struct {
            float sum[3];
            Stats stats[3];
        } calibration_t;

        typedef struct {

            float dps;           // aligned, calibrated, scaled, unfiltered
            float dpsFiltered;   // filtered 
            float sampleSum;     // summed samples used for downsampling
            float zero;

            Pt1Filter lowpassFilter1 = Pt1Filter(GYRO_LPF1_DYN_MIN_HZ);
            Pt1Filter lowpassFilter2 = Pt1Filter(GYRO_LPF2_STATIC_HZ);

        } axis_t;

        axis_t m_x;
        axis_t m_y;
        axis_t m_z;

        calibration_t m_calibration;

        rotateFun_t m_rotateFun;

        int32_t  m_calibrationCyclesRemaining;
        uint32_t m_gyroInterruptCount;
        float    m_gyroScale;
        uint8_t  m_interruptPin;
        bool     m_isCalibrating;

        static uint32_t calculateCalibratingCycles(void)
        {
            return GYRO_CALIBRATION_DURATION / Clock::PERIOD();
        }

        void setCalibrationCycles(void)
        {
            m_calibrationCyclesRemaining = (int32_t)calculateCalibratingCycles();
        }

        void calibrateAxis(axis_t & axis, const uint8_t index)
        {
            // Reset at start of calibration
            if (m_calibrationCyclesRemaining == (int32_t)calculateCalibratingCycles()) {
                m_calibration.sum[index] = 0.0f;
                m_calibration.stats[index].clear();
                // zero is set to zero until calibration complete
                axis.zero = 0.0f;
            }

            // Sum up CALIBRATING_GYRO_TIME_US readings
            m_calibration.sum[index] += readRawGyro(index);
            m_calibration.stats[index].push(readRawGyro(index));

            if (m_calibrationCyclesRemaining == 1) {
                const float stddev = m_calibration.stats[index].stdev();

                // check deviation and startover in case the model was moved
                if (MOVEMENT_CALIBRATION_THRESHOLD && stddev >
                        MOVEMENT_CALIBRATION_THRESHOLD) {
                    setCalibrationCycles();
                    return;
                }

                axis.zero = m_calibration.sum[index] / calculateCalibratingCycles();
            }
        }

        void calibrate(void)
        {
            calibrateAxis(m_x, 0);
            calibrateAxis(m_y, 1);
            calibrateAxis(m_z, 2);

            --m_calibrationCyclesRemaining;
        }

        void applyLpf1(axis_t & axis)
        {
            axis.dpsFiltered = axis.lowpassFilter1.apply(axis.sampleSum);
        }

        void applyLpf2(axis_t & axis)
        {
            axis.sampleSum = axis.lowpassFilter2.apply(axis.dps);
        }

        void scaleGyro(axis_t & axis, const float adc)
        {
            axis.dps = adc * m_gyroScale; 
        }

        float readCalibratedGyro(axis_t & axis, uint8_t index)
        {
            return readRawGyro(index) - axis.zero;
        }

    protected:

        uint32_t m_gyroSyncTime;

        RealImu(const rotateFun_t rotateFun, const float gyroScale) 
        {
            m_gyroScale = gyroScale;

            setCalibrationCycles(); // start calibrating
        }

        virtual int16_t readRawGyro(uint8_t k) = 0;

        typedef void (*align_fun)(Axes * axes);

        static auto quat2euler(
                const float qw, const float qx, const float qy, const float qz) -> Axes 
        {
            const auto phi = atan2(2.0f*(qw*qx+qy*qz), qw*qw-qx*qx-qy*qy+qz*qz);
            const auto theta = asin(2.0f*(qx*qz-qw*qy));
            const auto psi = atan2(2.0f*(qx*qy+qw*qz), qw*qw+qx*qx-qy*qy-qz*qz);

            // Convert heading from [-pi,+pi] to [0,2*pi]
            return Axes(phi, theta, psi + (psi < 0 ? 2*M_PI : 0)); 
        }

        virtual void accumulateGyro(const float gx, const float gy, const float gz)
        {
            (void)gx;
            (void)gy;
            (void)gz;
        }

        auto readGyroDps(const align_fun align) -> Axes
        {
            const auto calibrationComplete = m_calibrationCyclesRemaining <= 0;

            static Axes _adc;

            if (calibrationComplete) {

                // move 16-bit gyro data into floats to avoid overflows in
                // calculations

                _adc.x = readCalibratedGyro(m_x, 0);
                _adc.y = readCalibratedGyro(m_y, 1);
                _adc.z = readCalibratedGyro(m_z, 2);

                align(&_adc);

            } else {
                calibrate();
            }

            if (calibrationComplete) {
                scaleGyro(m_x, _adc.x);
                scaleGyro(m_y, _adc.y);
                scaleGyro(m_z, _adc.z);
            }

            // Use gyro lowpass 2 filter for downsampling
            applyLpf2(m_x);
            applyLpf2(m_y);
            applyLpf2(m_z);

            // Then apply lowpass 1
            applyLpf1(m_x);
            applyLpf1(m_y);
            applyLpf1(m_z);

            // Used for fusion with accelerometer
            accumulateGyro(m_x.dpsFiltered, m_y.dpsFiltered, m_z.dpsFiltered);

            m_isCalibrating = !calibrationComplete;

            return Axes(m_x.dpsFiltered, m_y.dpsFiltered, m_z.dpsFiltered);
        }

        bool gyroIsCalibrating(void)
        {
            return m_isCalibrating;
        }

        virtual int32_t getGyroSkew(
                const uint32_t nextTargetCycles,
                const int32_t desiredPeriodCycles) override
        {
            const auto skew =
                cmpTimeCycles(nextTargetCycles, m_gyroSyncTime) % desiredPeriodCycles;

            return skew > (desiredPeriodCycles / 2) ? skew - desiredPeriodCycles : skew;
        }

        virtual uint32_t getGyroInterruptCount(void) override
        {
            return m_gyroInterruptCount;
        }

        void handleInterrupt(void)
        {
            m_gyroInterruptCount++;
        }

    public:

        static auto rotate0(Axes & axes) -> Axes
        {
            return Axes(axes.x, axes.y, axes.z);
        }

        static auto rotate90(Axes & axes) -> Axes
        {
            return Axes(axes.y, -axes.x, axes.z);
        }

        static auto rotate180(Axes & axes) -> Axes
        {
            return Axes(-axes.x, -axes.y, axes.z);
        }

        static auto rotate270(Axes & axes) -> Axes
        {
            return Axes(-axes.y, axes.x, axes.z);
        }

        static auto rotate0Flip(Axes & axes) -> Axes
        {
            return Axes(-axes.x, axes.y, -axes.z);
        }

        static auto rotate90Flip(Axes & axes) -> Axes
        {
            return Axes(axes.y, axes.x, -axes.z);
        }

        static auto rotate180Flip(Axes & axes) -> Axes
        {
            return Axes(axes.x, -axes.y, -axes.z);
        }

        static auto rotate270Flip(Axes & axes) -> Axes
        {
            return Axes(-axes.y, -axes.x, -axes.z);
        }
        
}; // class Imu
