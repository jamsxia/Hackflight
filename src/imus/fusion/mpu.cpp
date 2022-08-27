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

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <constrain.h>
#include <imu.h>
#include <system.h>
#include <time.h>

#include "mpu.h"
#include "atomic.h"
#include "bus.h"
#include "bus_spi.h"
#include "exti.h"
#include "io.h"
#include "macros.h"
#include "mpudev.h"
#include "nvic.h"
#include "platform.h"
#include "systemdev.h"

static void mpuIntExtiHandler(extiCallbackRec_t *cb)
{
    gyroDev_t *gyroDev = gyroContainerOf(cb);

    // Ideally we'd use a time to capture such information, but unfortunately
    // the port used for EXTI interrupt does not have an associated timer
    uint32_t nowCycles = systemGetCycleCounter();
    int32_t gyroLastPeriod = cmpTimeCycles(nowCycles, gyroDev->gyroLastEXTI);
    // This detects the short (~79us) EXTI interval of an MPU6xxx gyro
    if ((gyroDev->gyroShortPeriod == 0) ||
            (gyroLastPeriod < gyroDev->gyroShortPeriod)) {
        gyroDev->gyroSyncEXTI =
            gyroDev->gyroLastEXTI + gyroDev->gyroDmaMaxDuration;
    }
    gyroDev->gyroLastEXTI = nowCycles;

    gyroDev->detectedEXTI++;
}

bool mpuGyroRead(gyroDev_t *gyro)
{
    uint8_t data[6];

    const bool ack = busReadRegisterBuffer(&gyro->dev, MpuImu::RA_GYRO_XOUT_H, data, 6);
    if (!ack) {
        return false;
    }

    gyro->adcRaw[0] = (int16_t)((data[0] << 8) | data[1]);
    gyro->adcRaw[1] = (int16_t)((data[2] << 8) | data[3]);
    gyro->adcRaw[2] = (int16_t)((data[4] << 8) | data[5]);

    return true;
}

bool mpuGyroReadSPI(gyroDev_t *gyro)
{
    uint16_t *gyroData = (uint16_t *)gyro->dev.rxBuf;

    // Ensure any prior DMA has completed before continuing
    spiWaitClaim(&gyro->dev);

    gyro->dev.txBuf[0] = MpuImu::RA_GYRO_XOUT_H | 0x80;

    busSegment_t segments[] = {
        {NULL, NULL, 7, true, NULL},
        {NULL, NULL, 0, true, NULL},
    };
    segments[0].txData = gyro->dev.txBuf;
    segments[0].rxData = &gyro->dev.rxBuf[1];

    spiSequence(&gyro->dev, &segments[0]);

    // Wait for completion
    spiWait(&gyro->dev);

    gyro->adcRaw[0] = __builtin_bswap16(gyroData[1]);
    gyro->adcRaw[1] = __builtin_bswap16(gyroData[2]);
    gyro->adcRaw[2] = __builtin_bswap16(gyroData[3]);

    return true;
}

typedef uint8_t (*gyroSpiDetectFn_t)(const extDevice_t *dev);

static bool detectSPISensorsAndUpdateDetectionResult(gyroDev_t *gyro,
        const MpuImu::gyroDeviceConfig_t *config)
{
    if (!config->csnTag || !spiSetBusInstance(&gyro->dev, config->spiBus)) {
        return false;
    }

    gyro->dev.busType_u.spi.csnPin = IOGetByTag(config->csnTag);

    IOInit(gyro->dev.busType_u.spi.csnPin, OWNER_GYRO_CS, RESOURCE_INDEX(config->index));
    IOConfigGPIO(gyro->dev.busType_u.spi.csnPin, SPI_IO_CS_CFG);

    // Ensure device is disabled, important when two devices are on the same bus.
    IOHi(gyro->dev.busType_u.spi.csnPin); 

    // It is hard to use hardware to optimize the detection loop here,
    // as hardware type and detection function name doesn't match.
    // May need a bitmap of hardware to detection function to do it right?
    auto sensor = mpuBusDetect(&gyro->dev);
    if (sensor != MPU_NONE) {
        gyro->mpuDetectionResult.sensor = sensor;
        busDeviceRegister(&gyro->dev);
        return true;
    }

    // Detection failed, disable CS pin again
    spiPreinitByTag(config->csnTag);

    return false;
}

static bool mpuDetect(gyroDev_t *gyro, const MpuImu::gyroDeviceConfig_t *config)
{
    static busDevice_t bus;
    gyro->dev.bus = &bus;

    // MPU datasheet specifies 30ms.
    delay(35);

    if (config->busType == BUS_TYPE_NONE) {
        return false;
    }

    if (config->busType == BUS_TYPE_GYRO_AUTO) {
        gyro->dev.bus->busType = BUS_TYPE_I2C;
    } else {
        gyro->dev.bus->busType = config->busType;
    }

    gyro->dev.bus->busType = BUS_TYPE_SPI;

    return detectSPISensorsAndUpdateDetectionResult(gyro, config);
}

void MpuImu::gyroInit(gyroDev_t *gyro)
{
    if (gyro->mpuIntExtiTag == IO_TAG_NONE) {
        return;
    }

    const IO_t mpuIntIO = IOGetByTag(gyro->mpuIntExtiTag);

    IOInit(mpuIntIO, OWNER_GYRO_EXTI, 0);
    EXTIHandlerInit(&gyro->exti, mpuIntExtiHandler);
    EXTIConfig(mpuIntIO, &gyro->exti, NVIC_PRIO_MPU_INT_EXTI, IOCFG_IN_FLOATING,
            BETAFLIGHT_EXTI_TRIGGER_RISING);
    EXTIEnable(mpuIntIO, true);
}

// ----------------------------------------------------------------------------

static gyroDev_t gyroDev;

uint32_t imuDevGyroInterruptCount(void)
{
    return gyroDev.detectedEXTI;
}

bool  imuDevGyroIsReady(void)
{
    bool ready = gyroDev.readFn(&gyroDev);

    if (ready) {
        gyroDev.dataReady = false;
    }

    return ready;
}

int16_t imuDevReadRawGyro(uint8_t k)
{
    return gyroDev.adcRaw[k];
}

uint32_t imuDevGyroSyncTime(void)
{
    return gyroDev.gyroSyncEXTI;
}

void imuDevInit(uint8_t interruptPin)
{
    (void)interruptPin;

    static MpuImu::gyroDeviceConfig_t gyroDeviceConfig; 

    gyroDeviceConfig.busType = BUS_TYPE_SPI; // XXX pass from subclass
    gyroDeviceConfig.spiBus = 1;
    gyroDeviceConfig.csnTag = 20;
    gyroDeviceConfig.extiTag = 52;

    spiPreinitRegister(gyroDeviceConfig.csnTag, IOCFG_IPU, 1);

    mpuDetect(&gyroDev, &gyroDeviceConfig);
    mpuBusGyroDetect(&gyroDev);

    // SPI DMA buffer required per device
    static uint8_t gyroBuf1[MpuImu::GYRO_BUF_SIZE];
    gyroDev.dev.txBuf = gyroBuf1;
    gyroDev.dev.rxBuf = &gyroBuf1[MpuImu::GYRO_BUF_SIZE / 2];

    gyroDev.mpuIntExtiTag = gyroDeviceConfig.extiTag;

    gyroDev.initFn(&gyroDev);
}