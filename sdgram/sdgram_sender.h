//
// Sending messages over a stream.
//
// author: aleksandar
//

#pragma once

#include "crc16.h"

#include "sdgram_defs.h"
#include "sdgram_log.h"
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

    Status Send(Port port, Buffer buf) {
        CreateHdrAndTrl(port, buf);

        SendDatagram(buf);

        return Status::Success;
    }

    // Adds the header and the trailer to the buffer.
    void PrepareDatagram(Port port, Buffer &buf) {
        CreateHdrAndTrl(port, buf);
    }

    // Send an already prepared datagram.
    Status SendDatagram(Buffer &buf) {
        if(!queued.IsEmpty()) {
            if(queued.IsFull()) {
                LogQueueFull();
                return Status::NoMoreSpace;
            }

            LogAddToQueue();

            queued.Push(buf);
            return Status::Success;
        }

        auto just_written = WriteData(buf, 0);

        if(just_written == buf.len) {
            buf_alloc.Free(buf.ptr);

            LogMsgSend();
        } else {
            written = just_written;
            queued.Push(buf);

            LogMsgPartialSend();
        }

        return Status::Success;
    }

    void Process() {
        while(!queued.IsEmpty()) {
            auto &buf = queued.Peek();

            auto just_written = WriteData(buf, written);

            if(just_written + written == buf.len) {
                buf_alloc.Free(buf.ptr);
                queued.Pop();
                written = 0;

                LogQueuedMsgSend(just_written);
            } else {
                written += just_written;

                LogQueuedMsgPartialSend(just_written);

                break;
            }
        }
    }

protected:
    //
    // Functions.
    //
    static void CreateHdrAndTrl(Port port, Buffer &buf) {
        auto buf_ptr = static_cast<uint8_t *>(buf.ptr);
        auto len = static_cast<BufferLen>(
            buf.len + sizeof(DatagramHdr) + sizeof(DatagramTrl));

        auto hdr = reinterpret_cast<DatagramHdr *>(
            buf_ptr - sizeof(DatagramHdr));

        hdr->magic = DatagramHdrMagic;
        hdr->port = port;
        hdr->crc = 0;
        hdr->size = buf.len;

        auto trl = reinterpret_cast<DatagramTrl *>(
            buf_ptr + buf.len);
        trl->magic = DatagramTrlMagic;

        hdr->crc = Crc16Usb::Calc(hdr, len);

        buf.ptr = hdr;
        buf.len = len;
    }

    uint16_t WriteData(const Buffer &buf, uint16_t offset) {
        auto available = stream.availableForWrite();

        if(!available) {
            return false;
        }

        auto left_to_write = buf.len - offset;
        auto buf_ptr = static_cast<uint8_t *>(buf.ptr) + offset;

        if(available >= left_to_write) {
            stream.write(buf_ptr, left_to_write);
            return left_to_write;
        }

        stream.write(buf_ptr, available);

        return available;
    }

    // logging
#define LOGGER_PREFIX "[SDGRAM-SND] "

    static void LogQueueFull() {
        LogVerboseLn(LOGGER_PREFIX "Deffered queue full");
    }

    static void LogMsgSend() {
        LogVerboseLn(LOGGER_PREFIX "Datagram sent");
    }

    static void LogAddToQueue() {
        LogVerboseLn(LOGGER_PREFIX "Datagram added to queue");
    }

    void LogMsgPartialSend() const {
        LogVerbose(LOGGER_PREFIX "Datagram partially sent ");
        LogVerboseLn(written);
    }

    static void LogQueuedMsgSend(uint16_t just_written) {
        LogVerbose(LOGGER_PREFIX "Finished sending queued message ");
        LogVerboseLn(just_written);
    }

    static void LogQueuedMsgPartialSend(uint16_t just_written) {
        LogVerbose(LOGGER_PREFIX "Sent part of a queued message ");
        LogVerboseLn(just_written);
    }

    //
    // Data.
    //
    Stream &stream;
    BufAlloc &buf_alloc;

    StaticQueue<Buffer, TotalBufCount> queued;
    uint16_t written;
};

}
