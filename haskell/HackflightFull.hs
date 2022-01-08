{--
  Hackflight full algorithm

  Copyright(C) 2021 on D.Levy

  MIT License
--}

{-# LANGUAGE RebindableSyntax #-}

module HackflightFull where

import Language.Copilot

import Receiver
import State
import Sensor
import Demands
import PidController
import Safety
import Time
import Mixers
import Motors
import Parser
import Messages
import Serial
import Utils


hackflight :: Receiver -> [Sensor] -> [PidFun] -> Mixer -> (MessageBuffer, Motors, SBool)

hackflight receiver sensors pidfuns mixer = (msgbuff, motors, led)

  where

    -- Get receiver demands from external C functions
    rdemands = getDemands receiver

    -- Get the vehicle state by composing the sensor functions over the previous state
    state = compose sensors (state' state)

    -- Check safety (arming / failsafe)
    (armed, failsafe, cut) = safety rdemands state

    -- Periodically get demands by composing PID controllers over receiver demands
    (_, _, pdemands) = compose pidfuns (state, timerReady 300, rdemands)

    -- Run mixer on demands to get motor values
    motors' = mix (\m -> if cut then 0 else m) pdemands mixer

    -- Blink LED during first couple of seconds; keep it solid when armed
    led = if usec < 2000000 then (mod (div usec 50000) 2 == 0) else armed

    -- Run the serial comms parser checking for data requests
    (msgtype, sending, payindex, _checked) = parse serialAvailable serialByte

    -- Reply with a message to GCS if indicated
    msgbuff = reply sending msgtype state

    -- Check for incoming SET_MOTOR messages from GCS
    (motor_index, motor_percent) = getMotors msgtype payindex serialByte

    -- Set motors based on arming state and whether we have GCS input
    motors = (motorfun mixer) motors' armed motor_index motor_percent
