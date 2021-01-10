//
// Sending messages over a stream.
//
// author: aleksandar
//

#pragma once

#include "crc16.h"

#include "sdgram_defs.h"
#include "static_queue.h"

namespace SerialDatagram {

template<
    typename Stream,
    typename BufAlloc,
    uint16_t TotalBufCount>
class Sender {
public:
    Sender(
        Stream &stream,
        BufAlloc &buf_alloc)
            : stream(stream),
            buf_alloc(buf_alloc),
            written(0) {
        // empty
    }

    Status Send(Endpoint endp, Buffer buf) {
        CreateHdrAndTrl(endp, buf);

        if(!queued.IsEmpty()) {
            if(queued.IsFull()) {
                return Status::NoMoreSpace;
            }

            queued.Push(buf);
            return Status::Success;
        }

        auto available = stream.availableForWrite();

        if(available > buf.len) {
            stream.write(static_cast<char *>(buf.ptr), buf.len);
        } else {
            stream.write(static_cast<char *>(buf.ptr), available);
            written = available;
            queued.Push(buf);
        }

        return Status::Success;
    }

    void Process() {
        while(!queued.IsEmpty()) {
            if(!WriteData()) {
                break;
            }
        }
    }

private:
    //
    // Functions.
    //
    void CreateHdrAndTrl(Endpoint endp, Buffer &buf) {
        auto buf_ptr = static_cast<uint8_t *>(buf.ptr);
        auto len = buf.len + sizeof(PacketHdr) + sizeof(PacketTrl);

        auto hdr = reinterpret_cast<PacketHdr *>(
            buf_ptr - sizeof(PacketHdr));

        hdr->magic = HdrMagic;
        hdr->endp = endp;
        hdr->crc = 0;
        hdr->size = buf.len;

        hdr->crc = Crc16Usb(hdr, len);

        auto trl = reinterpret_cast<PacketTrl *>(
            buf_ptr + buf.len);

        trl->magic = TrlMagic;

        buf.ptr = hdr;
        buf.len = len;
    }

    bool WriteData() {
        auto available = stream.availableForWrite();

        if(!available) {
            return false;
        }

        auto &buf = queued.Peek();
        auto left_to_write = buf.len - written;
        auto buf_ptr = static_cast<uint8_t *>(buf.ptr) + written;

        if(available >= left_to_write) {
            stream.write(buf_ptr, left_to_write);
            written = 0;
            queued.Pop();
        } else {
            stream.write(buf_ptr, available);
            written += available;
        }

        return true;
    }

    //
    // Data.
    //
    Stream &stream;
    BufAlloc &buf_alloc;

    StaticQueue<Buffer, TotalBufCount> queued;
    uint8_t written;
};

}