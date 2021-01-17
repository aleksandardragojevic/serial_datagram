//
// Receiver statistics.
//
// author: alekd
//

#pragma once

#include "sdgram_stdint.h"

namespace SerialDatagram {

struct RcvStats {
    void Clear() {
        msgs = 0;
        bytes = 0;
        dropped_bytes = 0;
        crc_error = 0;
        trl_error = 0;
        size_error = 0;
        rcv_error = 0;
    }

    uint16_t msgs;
    uint16_t bytes;
    uint16_t dropped_bytes;
    uint16_t crc_error;
    uint16_t trl_error;
    uint16_t size_error;
    uint16_t rcv_error;
};

}
