/*
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

#include <SPI.h>

#include "board/stm32.h"
#include "task/accelerometer.h"
#include "imu/softquat/invensense.h"
#include "imu/softquat/invensense/icm42688.h"

class Stm32FBoard : public Stm32Board {

    private:

        static const uint8_t IMU_MOSI_PIN = PA7;
        static const uint8_t IMU_MISO_PIN = PA6;
        static const uint8_t IMU_SCLK_PIN = PA5;

        SPIClass m_spi = SPIClass(IMU_MOSI_PIN, IMU_MISO_PIN, IMU_SCLK_PIN);

        InvenSenseImu * m_invenSenseImu;

        uint32_t m_initialSpiFreq; 
        uint32_t m_maxSpiFreq;

        // Enough room for seven two-byte integers (gyro XYZ, temperature,
        // accel XYZ) plus one byte for SPI transfer
        uint8_t m_buffer[15];

        void writeRegister(const uint8_t reg, const uint8_t val)
        {
            digitalWrite(m_invenSenseImu->csPin, LOW);
            m_spi.transfer(reg);
            m_spi.transfer(val);
            digitalWrite(m_invenSenseImu->csPin, HIGH);
        }

        void readRegisters(
                const uint8_t addr, uint8_t * buffer, const uint8_t count)
        {
            digitalWrite(m_invenSenseImu->csPin, LOW);

            buffer[0] = addr | 0x80;
            m_spi.transfer(buffer, count+1);

            digitalWrite(m_invenSenseImu->csPin, HIGH);
        }

        void readRegisters(const uint8_t addr)
        {
            readRegisters(addr, m_buffer, 14);
        }

        uint8_t readRegister(const uint8_t addr)
        {
            uint8_t buffer[2] = {};
            readRegisters(addr, buffer, 1);
            return buffer[1];
        }

        void setClockDivider(uint32_t divider)
        {
            m_spi.setClockDivider(divider);
        }

    protected:

        virtual void prioritizeExtraTasks(
                Task::prioritizer_t & prioritizer,
                const uint32_t usec) override
        {
            m_accelerometerTask.prioritize(usec, prioritizer);
            m_skyrangerTask.prioritize(usec, prioritizer);
        }

        virtual bool gyroIsReady(void) 
        {
            return true;
        }

        virtual void getRawGyro(int16_t rawGyro[3])
        {
            readRegisters(
                    m_invenSenseImu->dataRegister,
                    m_invenSenseImu->buffer,
                    InvenSenseImu::BUFFER_SIZE);

            m_invenSenseImu->bufferToRawGyro(rawGyro);
        }

        Stm32FBoard(
                Receiver & receiver,
                InvenSenseImu & imu,
                std::vector<PidController *> & pids,
                Mixer & mixer,
                Esc & esc,
                const uint8_t ledPin)
            : Stm32Board(receiver, &imu, pids, mixer, esc, ledPin)
        {
            m_invenSenseImu = &imu;

            m_initialSpiFreq = imu.initialSpiFreq;
            m_maxSpiFreq = imu.maxSpiFreq;
        }

        void beginIcm20689(void)
        {
            const uint8_t PWR_MGMT_1 = 0x6B;
            const uint8_t CLOCK_SEL_PLL = 0x01;
            const uint8_t PWR_RESET = 0x80;
            const uint8_t PWR_MGMT_2 = 0x6C;
            const uint8_t SEN_ENABLE = 0x00;
            const uint8_t ACCEL_CONFIG = 0x1C;
            const uint8_t ACCEL_FS_SEL_16G = 0x18;
            const uint8_t GYRO_CONFIG = 0x1B;
            const uint8_t GYRO_FS_SEL_2000DPS = 0x18;
            const uint8_t ACCEL_CONFIG2 = 0x1D;
            const uint8_t ACCEL_DLPF_1046HZ = 0x08;
            const uint8_t CONFIG = 0x1A;
            const uint8_t GYRO_DLPF_250HZ = 0x00;
            const uint8_t SMPLRT_DIV = 0x19;
            const uint8_t INT_PIN_CFG = 0x37;
            const uint8_t INT_ENABLE = 0x38;
            const uint8_t INT_PULSE_50US = 0x00;
            const uint8_t INT_RAW_RDY_EN = 0x01;

            // select clock source to gyro
            writeRegister(PWR_MGMT_1, CLOCK_SEL_PLL);

            // reset the ICM20689
            writeRegister(PWR_MGMT_1, PWR_RESET);

            // wait for ICM20689 to come back up
            delay(1);

            // select clock source to gyro
            writeRegister(PWR_MGMT_1, CLOCK_SEL_PLL);

            // enable accelerometer and gyro
            writeRegister(PWR_MGMT_2, SEN_ENABLE);

            // set accel range to 16G as default
            writeRegister(ACCEL_CONFIG, ACCEL_FS_SEL_16G);

            // set the gyro range to 2000DPS as default
            writeRegister(GYRO_CONFIG, GYRO_FS_SEL_2000DPS);

            // set bandwidth to 1046Hz as default
            writeRegister(ACCEL_CONFIG2, ACCEL_DLPF_1046HZ);

            writeRegister(CONFIG, GYRO_DLPF_250HZ);

            writeRegister(SMPLRT_DIV, 19);

            // setDlpfBandwidth(ICM20689::DLPF_BANDWIDTH_21HZ);
            //writeRegister(SMPLRT_DIV, 19);
            //writeRegister(SMPLRT_DIV, 19);

            writeRegister(INT_PIN_CFG, INT_PULSE_50US); // setup interrupt, 50 us pulse
            writeRegister(INT_ENABLE, INT_RAW_RDY_EN);  // set to data ready
        }

    public:

        void begin(void)
        {
            Board::begin();

            // Support MockImu
            if (m_invenSenseImu->csPin != 0) {

                m_spi.begin();

                pinMode(m_invenSenseImu->csPin, OUTPUT);

                const uint32_t clockSpeed = getClockSpeed();

                m_spi.setClockDivider(
                        InvenSenseImu::calculateSpiDivisor(clockSpeed, m_initialSpiFreq));

                if (m_invenSenseImu->isIcm20689()) {
                    beginIcm20689();
                }

                else {

                    std::vector<InvenSenseImu::registerSetting_t> registerSettings;

                    m_invenSenseImu->getRegisterSettings(registerSettings);

                    for (auto r : registerSettings) {
                        writeRegister(r.address, r.value);
                        delay(100); // arbitrary; should be long enough for any delays
                    }

                    // Enable data-ready interrupt on ICM42688
                    // https://github.com/finani/ICM42688/blob/master/src/ICM42688.cpp
                    if (m_invenSenseImu->isIcm42688()) {
                        writeRegister(Icm42688::REG_INT_CONFIG, 0x18 | 0x03);
                        uint8_t reg = readRegister(Icm42688::REG_INT_CONFIG1);
                        reg &= ~0x10;
                        writeRegister(Icm42688::REG_INT_CONFIG1, reg);
                        delay(100);
                    }
                }

                m_spi.setClockDivider(
                        InvenSenseImu::calculateSpiDivisor(clockSpeed, m_maxSpiFreq));
            }

            m_accelerometerTask.begin(m_imu);
        }

        void handleSkyrangerEvent(HardwareSerial & serial)
        {
            while (serial.available()) {
                m_skyrangerTask.parse(serial.read());
            }
        }

}; // class Stm32FBoard
