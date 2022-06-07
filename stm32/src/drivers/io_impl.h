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

#include "platform.h"

#include "io.h"

typedef struct ioDef_s {
    ioTag_t tag;
} ioDef_t;

typedef struct ioRec_s {
    GPIO_TypeDef *gpio;
    uint16_t pin;
    resourceOwner_e owner;
    uint8_t index;
} ioRec_t;

extern ioRec_t ioRecs[];

int IO_GPIOPortIdx(IO_t io);
int IO_GPIOPinIdx(IO_t io);

int IO_GPIO_PinSource(IO_t io);
int IO_GPIO_PortSource(IO_t io);

int IO_EXTI_PortSourceGPIO(IO_t io);
int IO_EXTI_PinSource(IO_t io);

GPIO_TypeDef* IO_GPIO(IO_t io);
uint16_t IO_Pin(IO_t io);

#define IO_GPIOBYTAG(tag) IO_GPIO(IOGetByTag(tag))
#define IO_PINBYTAG(tag) IO_Pin(IOGetByTag(tag))
#define IO_GPIOPortIdxByTag(tag) DEFIO_TAG_GPIOID(tag)
#define IO_GPIOPinIdxByTag(tag) DEFIO_TAG_PIN(tag)

uint32_t IO_EXTI_Line(IO_t io);
ioRec_t *IO_Rec(IO_t io);