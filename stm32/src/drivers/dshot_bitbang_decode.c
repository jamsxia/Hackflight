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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "platform.h"

#include <maths.h>
#include "utils.h"
#include "dshot.h"
#include "dshot_bitbang_decode.h"

#define MIN(a,b) \
  __extension__ ({ __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a < _b ? _a : _b; })

#define MAX(a,b) \
  __extension__ ({ __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; })

#define MIN_VALID_BBSAMPLES ((21 - 2) * 3)
#define MAX_VALID_BBSAMPLES ((21 + 2) * 3)

// setting this define in dshot.h allows the cli command dshot_telemetry_info to
// display the received telemetry data in raw form which helps identify
// the root cause of packet decoding issues.

/* Bit band SRAM definitions */
#define BITBAND_SRAM_REF   0x20000000
#define BITBAND_SRAM_BASE  0x22000000
#define BITBAND_SRAM(a,b) ((BITBAND_SRAM_BASE + (((a)-BITBAND_SRAM_REF)<<5) + ((b)<<2)))  // Convert SRAM address

typedef struct bitBandWord_s {
    uint32_t value;
    uint32_t junk[15];
} bitBandWord_t;


static uint32_t decode_bb_value(uint32_t value, uint16_t buffer[], uint32_t count, uint32_t bit)
{
    UNUSED(buffer);
    UNUSED(count);
    UNUSED(bit);

#define iv 0xffffffff
    // First bit is start bit so discard it.
    value &= 0xfffff;
    static const uint32_t decode[32] = {
        iv, iv, iv, iv, iv, iv, iv, iv, iv, 9, 10, 11, iv, 13, 14, 15,
        iv, iv, 2, 3, iv, 5, 6, 7, iv, 0, 8, 1, iv, 4, 12, iv };

    uint32_t decodedValue = decode[value & 0x1f];
    decodedValue |= decode[(value >> 5) & 0x1f] << 4;
    decodedValue |= decode[(value >> 10) & 0x1f] << 8;
    decodedValue |= decode[(value >> 15) & 0x1f] << 12;

    uint32_t csum = decodedValue;
    csum = csum ^ (csum >> 8); // xor bytes
    csum = csum ^ (csum >> 4); // xor nibbles

    if ((csum & 0xf) != 0xf || decodedValue > 0xffff) {
        value = BB_INVALID;
    } else {
        value = decodedValue >> 4;

        if (value == 0x0fff) {
            return 0;
        }
        // Convert value to 16 bit from the GCR telemetry format (eeem mmmm mmmm)
        value = (value & 0x000001ff) << ((value & 0xfffffe00) >> 9);
        if (!value) {
            return BB_INVALID;
        }
        // Convert period to erpm * 100
        value = (1000000 * 60 / 100 + value / 2) / value;
    }
    return value;
}


uint32_t decode_bb_bitband( uint16_t buffer[], uint32_t count, uint32_t bit)
{
    uint32_t value = 0;

    bitBandWord_t* p = (bitBandWord_t*)BITBAND_SRAM((uint32_t)buffer, bit);
    bitBandWord_t* b = p;
    bitBandWord_t* endP = p + (count - MIN_VALID_BBSAMPLES);

    // Eliminate leading high signal level by looking for first zero bit in data stream.
    // Manual loop unrolling and branch hinting to produce faster code.
    while (p < endP) {
        if (__builtin_expect((!(p++)->value), 0) ||
            __builtin_expect((!(p++)->value), 0) ||
            __builtin_expect((!(p++)->value), 0) ||
            __builtin_expect((!(p++)->value), 0)) {
            break;
        }
    }

    if (p >= endP) {
        // not returning telemetry is ok if the esc cpu is
        // overburdened.  in that case no edge will be found and
        // BB_NOEDGE indicates the condition to caller
        return BB_NOEDGE;
    }

    int remaining = MIN(count - (p - b), (unsigned int)MAX_VALID_BBSAMPLES);

    bitBandWord_t* oldP = p;
    uint32_t bits = 0;
    endP = p + remaining;

    while (endP > p) {
        do {
            // Look for next positive edge. Manual loop unrolling and branch hinting to produce faster code.
            if(__builtin_expect((p++)->value, 0) ||
               __builtin_expect((p++)->value, 0) ||
               __builtin_expect((p++)->value, 0) ||
               __builtin_expect((p++)->value, 0)) {
                break;
            }
        } while (endP > p);

        if (endP > p) {

            // A level of length n gets decoded to a sequence of bits of
            // the form 1000 with a length of (n+1) / 3 to account for 3x
            // oversampling.
            const int len = MAX((p - oldP + 1) / 3, 1);
            bits += len;
            value <<= len;
            value |= 1 << (len - 1);
            oldP = p;

            // Look for next zero edge. Manual loop unrolling and branch hinting to produce faster code.
            do {
                if (__builtin_expect(!(p++)->value, 0) ||
                    __builtin_expect(!(p++)->value, 0) ||
                    __builtin_expect(!(p++)->value, 0) ||
                    __builtin_expect(!(p++)->value, 0)) {
                    break;
                }
            } while (endP > p);

            if (endP > p) {

                // A level of length n gets decoded to a sequence of bits of
                // the form 1000 with a length of (n+1) / 3 to account for 3x
                // oversampling.
                const int len = MAX((p - oldP + 1) / 3, 1);
                bits += len;
                value <<= len;
                value |= 1 << (len - 1);
                oldP = p;
            }
        }
    }

    if (bits < 18) {
        return BB_NOEDGE;
    }

    // length of last sequence has to be inferred since the last bit with inverted dshot is high
    const int nlen = 21 - bits;
    if (nlen < 0) {
        value = BB_INVALID;
    }

    if (nlen > 0) {
        value <<= nlen;
        value |= 1 << (nlen - 1);
    }
    return decode_bb_value(value, buffer, count, bit);
}

 uint32_t decode_bb( uint16_t buffer[], uint32_t count, uint32_t bit)
{
    uint32_t mask = 1 << bit;
    uint16_t lastValue = 0;
    uint32_t value = 0;

    uint16_t* p = buffer;
    uint16_t* endP = p + count - MIN_VALID_BBSAMPLES;
    // Eliminate leading high signal level by looking for first zero bit in data stream.
    // Manual loop unrolling and branch hinting to produce faster code.
    while (p < endP) {
        if (__builtin_expect(!(*p++ & mask), 0) ||
            __builtin_expect(!(*p++ & mask), 0) ||
            __builtin_expect(!(*p++ & mask), 0) ||
            __builtin_expect(!(*p++ & mask), 0)) {
            break;
        }
    }

    if(*p & mask) {
        // not returning telemetry is ok if the esc cpu is
        // overburdened.  in that case no edge will be found and
        // BB_NOEDGE indicates the condition to caller
        return BB_NOEDGE;
    }

    int remaining = MIN(count - (p - buffer), (unsigned int)MAX_VALID_BBSAMPLES);

    uint16_t* oldP = p;
    uint32_t bits = 0;
    endP = p + remaining;

    while (endP > p ) {
        // Look for next edge. Manual loop unrolling and branch hinting to produce faster code.
        if (__builtin_expect((*p++ & mask) != lastValue, 0) ||
            __builtin_expect((*p++ & mask) != lastValue, 0) ||
            __builtin_expect((*p++ & mask) != lastValue, 0) ||
            __builtin_expect((*p++ & mask) != lastValue, 0)) {
            if (endP > p) {
                // A level of length n gets decoded to a sequence of bits of
                // the form 1000 with a length of (n+1) / 3 to account for 3x
                // oversampling.
                const int len = MAX((p - oldP + 1) / 3,1);
                bits += len;
                value <<= len;
                value |= 1 << (len - 1);
                oldP = p;
                lastValue = *(p-1) & mask;
            }
        }
    }

    // length of last sequence has to be inferred since the last bit with inverted dshot is high
    if (bits < 18) {
        return BB_NOEDGE;
    }

    const int nlen = 21 - bits;

    if (nlen < 0) {
        value = BB_INVALID;
    }
    if (nlen > 0) {
        value <<= nlen;
        value |= 1 << (nlen - 1);
    }
    return decode_bb_value(value, buffer, count, bit);
}