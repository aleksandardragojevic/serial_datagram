//
// A pair of memory buffers.
//
// author: aleksandar
//

#pragma once

#include "memory_buffer.h"
#include "serial_mock.h"

class MemoryBufferPair {
public:
    MemoryBufferPair(size_t capacity)
            : a(capacity),
            b(capacity) {
        // empty
    }

    SerialMock CreateA() {
        return SerialMock(a, b);
    }

    SerialMock CreateB() {
        return SerialMock(b, a);
    }

private:
    //
    // Data.
    //
    MemoryBuffer a;
    MemoryBuffer b;
};

