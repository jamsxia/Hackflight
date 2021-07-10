/*
   Hackflight core algorithm for real or simulated vehicles

   Copyright (c) 2021 Simon D. Levy

   MIT License
 */

#pragma once

#include <RoboFirmwareToolkit.hpp>
#include <RFT_board.hpp>
#include <RFT_actuator.hpp>

#include "state.hpp"
#include "receiver.hpp"

namespace hf {

    class HackflightPure : public rft::RFT {

        protected:

            // Vehicle state
            State _state;

        public:

            HackflightPure(Receiver * receiver, rft::Actuator * actuator)
                : rft::RFT(receiver, actuator)
            {  
            }

            void begin(rft::Board * board, bool armed=false)
            {
                rft::RFT::begin(board, &_state, armed);
            }

            void update(rft::Board * board)
            {
                rft::RFT::update(board, &_state);
            }

    }; // class HackflightPure

} // namespace hf
