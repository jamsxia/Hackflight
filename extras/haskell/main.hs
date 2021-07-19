{--
  Copyright(C) 2021 Simon D.Levy

  MIT License
--}

import Hackflight(hackflightFun)
import Mixer(quadXAPMixer)
import Server(runServer)
import PidControl(newAltHoldController)

main :: IO ()

main = let altHoldController = newAltHoldController 1 0 1 0.2
       in runServer hackflightFun altHoldController quadXAPMixer
