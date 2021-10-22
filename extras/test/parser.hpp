/*
   Timer task for serial comms

   MIT License
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {

    P_IDLE,          // 0
    P_GOT_DOLLAR,    // 1
    P_GOT_M,         // 2
    P_GOT_DIRECTION, // 3
    P_GOT_SIZE,      // 4
    P_GOT_TYPE,      // 5
    P_GOT_CRC        // 6

} parser_state_t;

static uint8_t type2count(uint8_t type)
{
    switch (type) {
        case 121:
            return 24;
        case 122:
            return 12;
    }

    return 0;
}

void parse(uint8_t byte)
{
    static parser_state_t pstate_;
    static uint8_t size_;
    static uint8_t type_;
    static uint8_t crc_;
    static uint8_t count_;
  
    static float phi = 1.5, theta = -0.6, psi = 2.7;

    // Parser state transition function
    pstate_
        = pstate_ == P_IDLE && byte == '$' ? P_GOT_DOLLAR
        : pstate_ == P_GOT_DOLLAR && byte == 'M' ? P_GOT_M
        : pstate_ == P_GOT_M && (byte == '<' || byte == '>') ? P_GOT_DIRECTION 
        : pstate_ == P_GOT_DIRECTION ? P_GOT_SIZE
        : pstate_ == P_GOT_SIZE ? P_GOT_TYPE
        : pstate_ == P_GOT_TYPE && byte == crc_ ? P_GOT_CRC
        : P_IDLE;

    size_ = pstate_ == P_GOT_SIZE ? byte : pstate_ == P_IDLE ? 0 : size_;

    type_ = pstate_ == P_GOT_TYPE ? byte : pstate_ == P_IDLE ? 0 : type_;

    crc_ = pstate_ == P_GOT_SIZE || pstate_ == P_GOT_TYPE ?  crc_ ^ byte 
         : pstate_ == P_IDLE  ? 0
         : crc_;

    count_ = pstate_ == P_GOT_TYPE ? type2count(byte) 
           : pstate_ == P_IDLE ? 0
           : count_;

    //printf("%d %d\n", pstate_, byte);

    printf("%d\n", count_);
}
