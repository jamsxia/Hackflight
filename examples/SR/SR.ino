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


//  Adapted from https://randomnerdtutorials.com/esp-now-two-way-communication-esp32/

#include <Wire.h>
#include <SPI.h>

#include <VL53L5cx.h>
#include <PAA3905_MotionCapture.h>

#include <hackflight.h>
#include <debugger.h>
#include "msp/parser.h"
#include "msp/serializer/usb.h"

// MCU choice --------------------------------------------------------

static const uint8_t SR_INT_PIN = 15;
static const uint8_t SR_LPN_PIN = 2;
static const uint8_t SR_CS_PIN  = 5;
static const uint8_t SR_MOT_PIN = 4;

static const uint8_t TP_INT_PIN = 4;
static const uint8_t TP_LPN_PIN = 14;
static const uint8_t TP_CS_PIN  = 5;
static const uint8_t TP_MOT_PIN = 32;

// VL53L5 -------------------------------------------------------------

static const uint8_t VL53L5_INT_PIN = TP_INT_PIN; // Set to 0 for polling
static const uint8_t VL53L5_LPN_PIN = TP_LPN_PIN;

// Set to 0 for continuous mode
static const uint8_t VL53L5_INTEGRAL_TIME_MS = 10;

static VL53L5cx _ranger(
        Wire, 
        VL53L5_LPN_PIN, 
        VL53L5_INTEGRAL_TIME_MS,
        VL53L5cx::RES_4X4_HZ_1);

static volatile bool _gotRangerInterrupt;

static void rangerInterruptHandler() 
{
    _gotRangerInterrupt = true;
}

static void startRanger(void)
{
    Wire.begin();                
    Wire.setClock(400000);      
    delay(1000);

    pinMode(VL53L5_INT_PIN, INPUT);     

    if (VL53L5_INT_PIN > 0) {
        attachInterrupt(VL53L5_INT_PIN, rangerInterruptHandler, FALLING);
    }

    _ranger.begin();
}

static void checkRanger(UsbMspSerializer & serializer, const uint8_t messageType)
{
    if (VL53L5_INT_PIN == 0 || _gotRangerInterrupt) {

        _gotRangerInterrupt = false;

        while (!_ranger.dataIsReady()) {
            delay(10);
        }

        _ranger.readData();

        for (auto i=0; i<_ranger.getPixelCount(); i++) {

            _ranger.getDistanceMm(i);
        }

        // serializer.serializeShorts(messageType, vmsg, 16);
    } 
}

// PAA3905 -----------------------------------------------------------

static const uint8_t PAA3905_CS_PIN  = TP_CS_PIN; 
static const uint8_t PAA3905_MOT_PIN = TP_MOT_PIN; 

PAA3905_MotionCapture _mocap(
        SPI,
        PAA3905_CS_PIN,
        PAA3905::DETECTION_STANDARD,
        PAA3905::AUTO_MODE_01,
        PAA3905::ORIENTATION_NORMAL,
        0x2A); // resolution 0x00 to 0xFF

static volatile bool _gotMotionInterrupt;

void motionInterruptHandler()
{
    _gotMotionInterrupt = true;
}


static void startMocap(void)
{
    // Start SPI
    SPI.begin();

    delay(100);

    // Check device ID as a test of SPI communications
    if (!_mocap.begin()) {
        Debugger::reportForever("PAA3905 initialization failed");
    }

    Debugger::printf("Resolution is %0.1f CPI per meter height\n", _mocap.getResolution());

    pinMode(PAA3905_MOT_PIN, INPUT); 
    attachInterrupt(PAA3905_MOT_PIN, motionInterruptHandler, FALLING);
}

static void checkMocap(UsbMspSerializer & serializer, const uint8_t messageType)
{
    static int16_t data[2];

    if (_gotMotionInterrupt) {

        _gotMotionInterrupt = false;

        _mocap.readBurstMode(); // use burst mode to read all of the data

        if (_mocap.motionDataAvailable()) { 

            uint8_t surfaceQuality = _mocap.getSurfaceQuality();

            uint32_t shutter = _mocap.getShutter();

            PAA3905_MotionCapture::lightMode_t lightMode = _mocap.getLightMode();

            // Send X,Y if surface quality and shutter are above thresholds
            if (_mocap.dataAboveThresholds(lightMode, surfaceQuality, shutter)) {
                data[0] = _mocap.getDeltaX();
                data[1] = _mocap.getDeltaY();
            }
        }
    }

    serializer.serializeShorts(messageType, data, 2);
}

// ------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    startRanger();
    startMocap();
}

void loop()
{
    static MspParser _parser;
    static UsbMspSerializer _serializer;

    while (Serial.available()) {

        auto messageType = _parser.parse(Serial.read());

        switch (messageType) {

            case 121:   // VL53L5
                checkRanger(_serializer, messageType);
                break;

            case 122: // PAA3905
                checkMocap(_serializer, messageType);
                break;
        }
    }
}
