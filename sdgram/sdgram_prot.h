//
// Protocol definitions.
//
// author: aleksandar
//

#pragma once

#include "sdgram_stdint.h"

namespace SerialDatagram {

#pragma pack(push, 1)
struct DatagramHdr {
    uint16_t magic;
    uint8_t size;
    uint8_t port;
    uint16_t crc;
};

struct DatagramTrl {
    uint16_t magic;
};
#pragma pack(pop)

constexpr size_t DatagramHdrSize = 6;
constexpr size_t DatagramTrlSize = 2;

static_assert(sizeof(DatagramHdr) == DatagramHdrSize);
static_assert(sizeof(DatagramTrl) == DatagramTrlSize);

constexpr uint16_t DatagramHdrMagic = 0xa357;
constexpr uint16_t DatagramTrlMagic = 0xc69b;

}
