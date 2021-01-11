//
// Protocol definitions.
//
// author: aleksandar
//

#pragma once

#include "sdgram_stdint.h"

namespace SerialDatagram {

#pragma pack(push, 1)
struct PacketHdr {
    uint16_t magic;
    uint8_t size;
    uint8_t port;
    uint16_t crc;
};

struct PacketTrl {
    uint16_t magic;
};
#pragma pack(pop)

static_assert(sizeof(PacketHdr) == 6);
static_assert(sizeof(PacketTrl) == 2);

constexpr uint16_t HdrMagic = 0xa357;
constexpr uint16_t TrlMagic = 0xc69b;

}
