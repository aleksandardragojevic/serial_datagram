//
// A mock of a serial endpoint.
//
// author: aleksandar
//

#pragma once

#include "memory_buffer.h"

class SerialMock {
public:
    SerialMock(
        MemoryBuffer &read_buf,
        MemoryBuffer &write_buf)
            : read_buf(read_buf),
            write_buf(write_buf) {
        // empty
    }

    uint16_t available() const {
        return read_buf.available();
    }

    uint8_t read() {
        return read_buf.read();
    }

    uint16_t availableForWrite() const {
        return write_buf.availableForWrite();
    }

    uint16_t write(void *buf, uint16_t buf_len) {
        return write_buf.write(buf, buf_len);
    }

private:
    //
    // Data.
    //
    MemoryBuffer &read_buf;
    MemoryBuffer &write_buf;
};
