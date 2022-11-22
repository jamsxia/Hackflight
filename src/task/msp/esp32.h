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

#pragma once

#include <stdint.h>
#include <string.h>

#include <WiFi.h>
#include <esp_now.h>

#include "task/msp.h"

class Esp32Msp : public Msp {

    private:

        uint8_t m_peerAddress[6];

        static void error(const char * msg)
        {
            while (true) {
                Serial.println(msg);
                delay(100);
            }
        }

        void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
        {
            Serial.print("\r\nLast Packet Send Status:\t");

            Serial.println(
                    status == ESP_NOW_SEND_SUCCESS ?
                    "Delivery Success" :
                    "Delivery Fail");
        }

    protected:

        virtual void serialBegin(const uint32_t baud) override
        {
        }

        virtual uint32_t serialAvailable(void) override
        {
            return 0;
        }

        virtual uint8_t serialRead(void) override
        {
            return 0;
        }

        virtual void serialWrite(const uint8_t buf[], const uint8_t count) override
        {
            esp_now_send(m_peerAddress, buf, count);
        }

    public:

        Esp32Msp(const uint8_t receiverAddress[6])
        {
            memcpy(m_peerAddress, receiverAddress, 6);
        }

        void begin(void)
        {
            // Set device as a Wi-Fi Station
            WiFi.mode(WIFI_STA);

            // Init ESP-NOW
            if (esp_now_init() != ESP_OK) {
                error("Error initializing ESP-NOW");
            }

            // Once ESPNow is successfully Init, we will register for Send CB to
            // get the status of Trasnmitted packet
            //esp_now_register_send_cb(onDataSent);

            // Register peer
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, m_peerAddress, 6);
            peerInfo.channel = 0;  
            peerInfo.encrypt = false;

            // Add peer        
            if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                error("Failed to add peer");
            }
        }

}; // class Esp32Msp
