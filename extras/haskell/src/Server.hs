{--
  Socket-based multicopter control

  Copyright(C) 2021 Simon D.Levy

  MIT License
--}

module Server (runServer) where

import Network.Socket
import Network.Socket.ByteString -- from network

import Utils(bytesToDoubles, doublesToBytes)
import Sockets(makeUdpSocket)
import SimReceiver(simReceiver)
import Mixer
import VehicleState
import Hackflight(HackflightFun)
import ClosedLoopControl(PidController)

runServer :: HackflightFun -> [PidController] -> Mixer -> IO ()

-- runServer hackflight pidControllers mixer = withSocketsDo $
runServer hackflightFun pidControllers mixer = 

    do 

       (telemetryServerSocket, telemetryServerSocketAddress) <-
           makeUdpSocket "5001"

       (demandsServerSocket, demandsServerSocketAddress) <-
           makeUdpSocket "5002"

       (motorClientSocket, motorClientSocketAddress) <- makeUdpSocket "5000"

       bind telemetryServerSocket telemetryServerSocketAddress

       bind demandsServerSocket demandsServerSocketAddress

       putStrLn "Hit the Play button ..."

       loop telemetryServerSocket
            demandsServerSocket
            motorClientSocket
            motorClientSocketAddress
            hackflightFun
            mixer
            pidControllers

loop :: Socket ->
        Socket ->
        Socket ->
        SockAddr ->
        HackflightFun ->
        Mixer ->
        [PidController] ->
        IO ()

loop telemetryServerSocket
     demandsServerSocket
     motorClientSocket
     motorClientSockAddr
     hackflightFun
     mixer
     pidControllers =

  do 

      -- Get raw bytes for time and 12D state vector from client
      (telemBytes, _) <- 
          Network.Socket.ByteString.recvFrom telemetryServerSocket 104

      -- Convert bytes to a list of doubles
      let telem = bytesToDoubles telemBytes

      if telem!!0 >= 0 

      then do
 
          let vehicleState = VehicleState (telem!!1) (telem!!2) (telem!!3)
                                          (telem!!4) (telem!!5) (telem!!6)
                                          (telem!!7) (telem!!8) (telem!!9)
                                          (telem!!10) (telem!!11) (telem!!12)

          -- Get raw bytes for stick demands from client
          (demandsBytes, _) <- 
              Network.Socket.ByteString.recvFrom demandsServerSocket 32

          let receiver = simReceiver $ bytesToDoubles demandsBytes

          -- Run the Hackflight algorithm to get the motor values
          let (motors, newPidControllers) = hackflightFun receiver
                                                          []
                                                          vehicleState
                                                          mixer
                                                          pidControllers
          -- Send the motor values to the client
          _ <- Network.Socket.ByteString.sendTo
                motorClientSocket
                (doublesToBytes (motorValues motors))
                motorClientSockAddr

          loop telemetryServerSocket
               demandsServerSocket
               motorClientSocket
               motorClientSockAddr
               hackflightFun
               mixer
               newPidControllers

        else putStrLn "Done"
