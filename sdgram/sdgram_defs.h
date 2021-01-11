//
// Definitions for the network serial.
//
// author: aleksandar
//

#pragma once

#include "sdgram_stdint.h"

namespace SerialDatagram {

enum class Status {
    Success,
    Failure,
    Duplicate,
    NoMoreSpace,
    NoReceiver,
};

using BufferLen = uint8_t;
using Port = uint8_t;

constexpr Port InvalidPort = 0xff; 

struct Buffer {
    void *ptr;
    BufferLen len;
};

class Rcv {
public:
    Rcv() = default;
    virtual ~Rcv() = default;

    // Buffer is valid until the callback returns.
    // If the user code needs the data after returning,
    // it needs to make a copy of it.
    virtual void ProcessMsg(Buffer buf) = 0;
};

}
