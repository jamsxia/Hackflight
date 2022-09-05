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

#include <string.h>
#include <math.h>

#include "core/clock.h"
#include "core/constrain.h"
#include "core/filters/pt1.h"
#include "core/filters/pt2.h"
#include "core/pid.h"
#include "time.h"

class AnglePidController : public PidController {

    private:

        // minimum of 5ms between updates
        static const uint16_t DYN_LPF_THROTTLE_UPDATE_DELAY_US = 5000; 

        static const uint16_t DYN_LPF_THROTTLE_STEPS = 100;

        // The constant scale factor to replace the Kd component of the
        // feedforward calculation.  This value gives the same "feel" as the
        // previous Kd default of 26 (26 * DTERM_SCALE)
        static constexpr float FEEDFORWARD_SCALE = 0.013754;

        // Full iterm suppression in setpoint mode at high-passed setpoint rate
        // > 40deg/sec
        static constexpr float   ITERM_RELAX_SETPOINT_THRESHOLD = 40;
        static const uint8_t ITERM_RELAX_CUTOFF     = 15;

        static const uint16_t DTERM_LPF1_DYN_MIN_HZ = 75;
        static const uint16_t DTERM_LPF1_DYN_MAX_HZ = 150;
        static const uint16_t DTERM_LPF2_HZ         = 150;
        static const uint16_t YAW_LOWPASS_HZ     = 100;
        static const uint8_t  ITERM_WINDUP_POINT_PERCENT = 85;        

        // How much integrated yaw should be reduced to offset the drag based
        // yaw component
        static const uint8_t INTEGRATED_YAW_RELAX = 200;  

        static const uint8_t D_MIN = 30;
        static const uint8_t D_MIN_GAIN = 37;
        static const uint8_t D_MIN_ADVANCE = 20;

        // Amount of lowpass type smoothing for feedforward steps
        static constexpr float FEEDFORWARD_SMOOTH_FACTOR = 0.75;      

        static const uint8_t FEEDFORWARD_JITTER_FACTOR = 7;
        static const uint8_t FEEDFORWARD_BOOST_FACTOR  = 15;
        static const uint8_t FEEDFORWARD_MAX_RATE_LIMIT = 90;
        static const uint8_t DYN_LPF_CURVE_EXPO = 5;

        // PT2 lowpass input cutoff to peak D around propwash frequencies
        static constexpr float D_MIN_RANGE_HZ   = 85;  

        // PT2 lowpass cutoff to smooth the boost effect
        static constexpr float D_MIN_LOWPASS_HZ = 35;  

        static constexpr float D_MIN_GAIN_FACTOR          = 0.00008;
        static constexpr float D_MIN_SETPOINT_GAIN_FACTOR = 0.00008f;

        static const uint16_t RATE_ACCEL_LIMIT = 0;
        static const uint16_t YAW_RATE_ACCEL_LIMIT = 0;
        static const uint16_t ITERM_LIMIT = 400;

        static constexpr float LEVEL_ANGLE_LIMIT = 45;

        static const uint8_t RC_EXPO = 0;
        static const uint8_t RC_RATE = 7;
        static const uint8_t RATE    = 67;

        static float MAX_VELOCITY_CYCLIC() 
        {
            return RATE_ACCEL_LIMIT * 100 * Clock::DT();
        }

        static float MAX_VELOCITY_YAW() 
        {
            return YAW_RATE_ACCEL_LIMIT * 100 * Clock::DT(); 
        }

        static float FREQUENCY() 
        {
            return 1.0f / Clock::DT(); 
        }

        typedef struct {

            float previousSetpointCorrection;
            float previousSetpoint;
            float I;

        } axis_t;

        typedef struct {

            axis_t    axis;
            Pt1Filter dtermLpf1 = Pt1Filter(DTERM_LPF1_DYN_MIN_HZ);
            Pt1Filter dtermLpf2 = Pt1Filter(DTERM_LPF2_HZ);
            Pt2Filter dMinLpf = Pt2Filter(D_MIN_LOWPASS_HZ);
            Pt2Filter dMinRange = Pt2Filter(D_MIN_RANGE_HZ);
            Pt1Filter windupLpf = Pt1Filter(ITERM_RELAX_CUTOFF); 
            float     previousDterm;

        } cyclicAxis_t;

        cyclicAxis_t m_roll;
        cyclicAxis_t m_pitch;
        axis_t       m_yaw;

        int32_t       m_dynLpfPreviousQuantizedThrottle;  
        bool          m_feedforwardLpfInitialized;
        float         m_k_rate_p;
        float         m_k_rate_i;
        float         m_k_rate_d;
        float         m_k_rate_f;
        float         m_k_level_p;
        float         m_sum;
        uint32_t      m_lastDynLpfUpdateUs;

        Pt1Filter m_ptermYawLpf = Pt1Filter(YAW_LOWPASS_HZ);

        float applyFeedforwardLimit(
                const float value,
                const float currentPidSetpoint,
                const float maxRateLimit) {

            return value * currentPidSetpoint > 0.0f ?
                fabsf(currentPidSetpoint) <= maxRateLimit ?
                constrain_f(value, (-maxRateLimit -
                            currentPidSetpoint) * m_k_rate_p,
                        (maxRateLimit - currentPidSetpoint) *
                        m_k_rate_p) :
                0 :
                0;
        }

        float accelerationLimit(
                axis_t * axis,
                float currentPidSetpoint,
                const float maxVelocity)
        {
            const float currentVelocity =
                currentPidSetpoint - axis->previousSetpoint;

            if (fabsf(currentVelocity) > maxVelocity) {
                currentPidSetpoint = (currentVelocity > 0) ?
                    axis->previousSetpoint + maxVelocity :
                    axis->previousSetpoint - maxVelocity;
            }

            axis->previousSetpoint = currentPidSetpoint;

            return currentPidSetpoint;
        }

        float applyItermRelax(
                cyclicAxis_t * cyclicAxis,
                const float iterm,
                const float currentPidSetpoint,
                const float itermErrorRate)
        {
            const float
                setpointLpf = cyclicAxis->windupLpf.apply(currentPidSetpoint);

            const float setpointHpf = fabsf(currentPidSetpoint - setpointLpf);

            const auto itermRelaxFactor =
                fmaxf(0, 1 - setpointHpf / ITERM_RELAX_SETPOINT_THRESHOLD);

            const auto isDecreasingI =
                ((iterm > 0) && (itermErrorRate < 0)) ||
                ((iterm < 0) && (itermErrorRate > 0));

            return itermErrorRate * (!isDecreasingI ? itermRelaxFactor : 1);
        }

        static float dynLpfCutoffFreq(
                const float throttle,
                const uint16_t dynLpfMin,
                const uint16_t dynLpfMax,
                const uint8_t expo) {
            const float expof = expo / 10.0f;
            const auto curve = throttle * (1 - throttle) * expof + throttle;
            return (dynLpfMax - dynLpfMin) * curve + dynLpfMin;
        }

        static void initLpf1(cyclicAxis_t * cyclicAxis, const float cutoffFreq)
        {
            cyclicAxis->dtermLpf1.computeGain(cutoffFreq);
        }

        void pidDynLpfDTermUpdate(const float throttle)
        {
            const auto dyn_lpf_min = DTERM_LPF1_DYN_MIN_HZ;
            const auto dyn_lpf_max = DTERM_LPF1_DYN_MAX_HZ;
            const auto cutoffFreq =
                dynLpfCutoffFreq(throttle, dyn_lpf_min, dyn_lpf_max,
                        DYN_LPF_CURVE_EXPO);

            initLpf1(&m_roll, cutoffFreq);
            initLpf1(&m_pitch, cutoffFreq);
        }

        void updateDynLpfCutoffs(const uint32_t currentTimeUs, const float throttle)
        {
            if (cmpTimeUs(currentTimeUs, m_lastDynLpfUpdateUs) >=
                    DYN_LPF_THROTTLE_UPDATE_DELAY_US) {

                // quantize the throttle reduce the number of filter updates
                const int32_t quantizedThrottle =
                    lrintf(throttle * DYN_LPF_THROTTLE_STEPS); 

                if (quantizedThrottle != m_dynLpfPreviousQuantizedThrottle) {

                    // scale the quantized value back to the throttle range so the
                    // filter cutoff steps are repeatable
                    auto dynLpfThrottle =
                        (float)quantizedThrottle / DYN_LPF_THROTTLE_STEPS;
                    pidDynLpfDTermUpdate(dynLpfThrottle);
                    m_dynLpfPreviousQuantizedThrottle = quantizedThrottle;
                    m_lastDynLpfUpdateUs = currentTimeUs;
                }
            }
        }

        float levelPid(const float currentSetpoint, const float currentAngle)
        {
            // calculate error angle and limit the angle to the max inclination
            // rcDeflection in [-1.0, 1.0]
            float angle = LEVEL_ANGLE_LIMIT * currentSetpoint;
            angle = constrain_f(angle, -LEVEL_ANGLE_LIMIT, LEVEL_ANGLE_LIMIT);
            const float errorAngle = angle - (currentAngle / 10);
            return m_k_level_p > 0 ?
                errorAngle * m_k_level_p :
                currentSetpoint;
        }

        float computeFeedforward(
                const float currentPidSetpoint,
                const float feedforwardMaxRate,
                const float demandDelta)
        {
            // halve feedforward in Level mode since stick sensitivity is
            // weaker by about half transition now calculated in
            // feedforward.c when new RC data arrives 
            const auto feedForward = m_k_rate_f * demandDelta * FREQUENCY();

            const auto feedforwardMaxRateLimit =
                feedforwardMaxRate * FEEDFORWARD_MAX_RATE_LIMIT * 0.01f;

            const bool shouldApplyFeedforwardLimits = feedforwardMaxRateLimit != 0;

            return shouldApplyFeedforwardLimits ?
                applyFeedforwardLimit(
                        feedForward,
                        currentPidSetpoint,
                        feedforwardMaxRateLimit) :
                feedForward;
        }

        float computeDMinFactor(
                cyclicAxis_t * cyclicAxis,
                const float dMinPercent,
                const float demandDelta,
                const float delta)
        {
            const auto d_min_gyro_gain =
                D_MIN_GAIN * D_MIN_GAIN_FACTOR / D_MIN_LOWPASS_HZ;

            const auto dMinGyroFactor = 
                fabsf(cyclicAxis->dMinRange.apply(delta)) * d_min_gyro_gain;

            const auto d_min_setpoint_gain =
                D_MIN_GAIN * D_MIN_SETPOINT_GAIN_FACTOR *
                D_MIN_ADVANCE * FREQUENCY() /
                (100 * D_MIN_LOWPASS_HZ);

            const auto dMinSetpointFactor =
                (fabsf(demandDelta)) * d_min_setpoint_gain;

            const auto dMinFactor = dMinPercent + (1.0f - dMinPercent) *
                fmaxf(dMinGyroFactor, dMinSetpointFactor);

            const auto dMinFactorFiltered = cyclicAxis->dMinLpf.apply(dMinFactor);

            return fminf(dMinFactorFiltered, 1.0f);
        }

        float computeDerivative(
                cyclicAxis_t * cyclicAxis,
                const float demandDelta,
                const float dterm)
        {
            // Divide rate change by dT to get differential (ie dr/dt).
            // dT is fixed and calculated from the target PID loop time
            // This is done to avoid DTerm spikes that occur with
            // dynamically calculated deltaT whenever another task causes
            // the PID loop execution to be delayed.
            const float delta = -(dterm - cyclicAxis->previousDterm) * FREQUENCY();

            const auto preTpaD = m_k_rate_d * delta;

            const auto dMinPercent = 
                D_MIN > 0 && D_MIN < m_k_rate_d ?
                D_MIN / m_k_rate_d :
                0.0f;

            const auto dMinFactor =
                dMinPercent > 0 ?
                computeDMinFactor(cyclicAxis, dMinPercent, demandDelta, delta) :
                1.0f;

            // Apply the dMinFactor
            return preTpaD * dMinFactor;
        }

        float updateCyclic(
                const float demand,
                const float angle,
                const float angvel,
                cyclicAxis_t * cyclicAxis)
        {
            auto dterm =
                cyclicAxis->dtermLpf2.apply(cyclicAxis->dtermLpf1.apply(angvel));

            auto currentPidSetpoint = demand;

            auto maxVelocity = MAX_VELOCITY_CYCLIC();

            axis_t * axis = &cyclicAxis->axis;

            if (maxVelocity) {
                currentPidSetpoint =
                    accelerationLimit(axis, currentPidSetpoint, maxVelocity);
            }

            currentPidSetpoint = levelPid(currentPidSetpoint, angle);

            // -----calculate error rate
            auto errorRate = currentPidSetpoint - angvel;
            const auto previousIterm = axis->I;
            auto itermErrorRate = errorRate;
            auto uncorrectedSetpoint = currentPidSetpoint;

            itermErrorRate = applyItermRelax(
                    cyclicAxis,
                    previousIterm,
                    currentPidSetpoint,
                    itermErrorRate);

            errorRate = currentPidSetpoint - angvel;
            float setpointCorrection =
                currentPidSetpoint - uncorrectedSetpoint;

            // -----calculate P component
            const auto P = m_k_rate_p * errorRate;

            // -----calculate I component
            // if launch control is active override the iterm gains and apply
            // iterm windup protection to all axes
            auto Ki = m_k_rate_i; 

            auto axisDynCi = Clock::DT(); // check windup for yaw only

            axis->I = constrain_f(previousIterm + (Ki * axisDynCi) * itermErrorRate,
                    -ITERM_LIMIT, ITERM_LIMIT);

            // -----calculate demandDelta
            auto demandDelta = 0.0f;
            auto feedforwardMaxRate = applyRates(1, 1);

            // -----calculate D component
            auto D = m_k_rate_d > 0 ?
                computeDerivative(cyclicAxis, demandDelta, dterm) :
                0;

            cyclicAxis->previousDterm = dterm;

            // -----calculate feedforward component
            // include abs control correction in feedforward
            demandDelta += setpointCorrection - axis->previousSetpointCorrection;
            axis->previousSetpointCorrection = setpointCorrection;

            const auto F =
                m_k_rate_f > 0 ?
                computeFeedforward(
                        currentPidSetpoint, feedforwardMaxRate, demandDelta) :
                0;

            return P + axis->I + D + F;
        }

        float updateYaw(const float demand, const float angvel)
        {
            // gradually scale back integration when above windup point
            auto dynCi = Clock::DT();
            const auto itermWindupPointInv =
                1 / (1 - (ITERM_WINDUP_POINT_PERCENT / 100));
            if (itermWindupPointInv > 1.0f) {
                dynCi *= constrain_f(itermWindupPointInv, 0.0f, 1.0f);
            }

            auto currentPidSetpoint = demand;

            auto maxVelocity = MAX_VELOCITY_YAW();

            if (maxVelocity) {
                currentPidSetpoint =
                    accelerationLimit(&m_yaw, currentPidSetpoint, maxVelocity);
            }

            // Handle yaw spin recovery - zero the setpoint on yaw to aid in
            // recovery It's not necessary to zero the set points for R/P
            // because the PIDs will be zeroed below

            // -----calculate error rate
            auto errorRate = currentPidSetpoint - angvel; // r - y
            const auto previousIterm = m_yaw.I;
            auto itermErrorRate = errorRate;
            auto uncorrectedSetpoint = currentPidSetpoint;
            errorRate = currentPidSetpoint - angvel;
            const auto setpointCorrection = currentPidSetpoint - uncorrectedSetpoint;

            // --------low-level gyro-based PID based on 2DOF PID controller.
            // ---------- 2-DOF PID controller with optional filter on
            // derivative term.  b = 1 and only c (feedforward weight) can be
            // tuned (amount derivative on measurement or error).

            // -----calculate P component
            const auto P = m_ptermYawLpf.apply(m_k_rate_p * errorRate);

            // -----calculate I component, constraining windup
            m_yaw.I = constrain_f(
                    previousIterm + (m_k_rate_i * dynCi) * itermErrorRate,
                    -ITERM_LIMIT, ITERM_LIMIT);

            // -----calculate feedforward component
            // include abs control correction in feedforward
            const auto demandDelta =
                setpointCorrection - m_yaw.previousSetpointCorrection;

            m_yaw.previousSetpointCorrection = setpointCorrection;

            return P + m_yaw.I + m_k_rate_f * demandDelta * FREQUENCY();
        }

    public:

        AnglePidController(
                const float k_rate_p,
                const float k_rate_i,
                const float k_rate_d,
                const float k_rate_f,
                const float k_level_p)
        {
            m_k_rate_p = k_rate_p;
            m_k_rate_i = k_rate_i;
            m_k_rate_d = k_rate_d;
            m_k_rate_f = k_rate_f;
            m_k_level_p = k_level_p;

            // to allow an initial zero throttle to set the filter cutoff
            m_dynLpfPreviousQuantizedThrottle = -1;  
        }

        static float applyRates(const float commandf, const float commandfAbs)
        {
            const auto expo = RC_EXPO / 100.0f;

            const auto expof =
                commandfAbs * (powf(commandf, 5) * expo + commandf * (1 - expo));

            const auto centerSensitivity = RC_RATE * 10.0f;
            const auto stickMovement = fmaxf(0, RATE * 10.0f - centerSensitivity);
            const auto angleRate = commandf * centerSensitivity + 
                stickMovement * expof;

            return angleRate;
        }

        virtual auto update(
                const uint32_t currentTimeUs,
                const Demands & demands,
                const VehicleState & vstate,
                const bool reset) -> Demands override
        {
            const auto roll=
                updateCyclic(demands.roll, vstate.phi, vstate.dphi, &m_roll);

            const auto pitch =
                updateCyclic(demands.pitch, vstate.theta, vstate.dtheta, &m_pitch);

            const auto yaw = updateYaw(demands.yaw, vstate.dpsi);

            if (reset) {
                m_roll.axis.I = 0;
                m_pitch.axis.I = 0;
                m_yaw.I = 0;
            }

            updateDynLpfCutoffs(currentTimeUs, demands.throttle);

            return Demands(demands.throttle, roll, pitch, yaw);

        } // update

}; // class AnglePidController

