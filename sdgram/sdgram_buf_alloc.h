//
// Allocating buffers for network serial.
//
// author: aleksandar
//

#pragma once

#include "sdgram_defs.h"

namespace SerialDatagram {

template<
    BufferLen BufSize,
    uint16_t BufCount = 4>
class BufAlloc {
public:
    BufAlloc()
            : free(nullptr) {
        InitBufs();
    }

    void *Alloc() {
        auto ret = free;

        if(free) {
            free = free->next;
        }

        return ret;
    }

    void Free(void *ptr) {
        auto buf = static_cast<FreeBuf *>(ptr);

        buf->next = free;
        free = buf;
    }

private:
    //
    // Types.
    //
    struct FreeBuf {
        FreeBuf *next;
    };

    //
    // Functions.
    //
    void InitBufs() {
        auto buf = buffers + (BufCount - 1) * BufSize;

        for(uint16_t i = 0;i < BufCount;i++) {
            Free(reinterpret_cast<FreeBuf *>(buf));

            buf -= BufSize;
        }
    }

    //
    // Data.
    //
    uint8_t buffers[BufSize * BufCount];
    FreeBuf *free;
};

}
