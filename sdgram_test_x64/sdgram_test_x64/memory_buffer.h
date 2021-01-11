//
// A simple memory buffer to simulate serial channels.
//
// This is used for testing, so not very efficient.
//
// author: aleksandar
//

#pragma once

#include <deque>
#include <cstdint>
#include <functional>

class MemoryBuffer {
public:
    MemoryBuffer(
        size_t capacity)
            : capacity(capacity) {
        // empty
    }

    uint16_t available() const {
        return static_cast<uint16_t>(data.size());
    }

    uint8_t read() {
        auto ret = data.front();
        data.pop_front();
        return ret;
    }

    uint16_t availableForWrite() const {
        return static_cast<uint16_t>(capacity - data.size());
    }

    uint16_t write(void *buf, uint16_t buf_len) {
        uint16_t written = 0;
        auto in = static_cast<uint8_t *>(buf);

        auto to_write = std::min(
            buf_len,
            static_cast<uint16_t>(capacity - data.size()));

        for(auto i = 0;i < to_write;i++) {
            data.push_back(in[i]);
        }

        return to_write;
    }

private:
    //
    // Data.
    //
    const size_t capacity;

    std::deque<uint8_t> data;
};
