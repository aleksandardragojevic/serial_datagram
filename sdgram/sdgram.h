//
// Simple networking over a serial connection.
//
// author: aleksandar
//

#pragma once

#include <stdint.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(ARDUINO)
#include "WProgram.h"
#endif /* ARDUINO */

#include "sdgram_defs.h"
#include "sdgram_buf_alloc.h"
#include "sdgram_prot.h"
#include "sdgram_rcv_table.h"
#include "sdgram_receiver.h"
#include "sdgram_sender.h"
#include "sdgram_rcv_stats.h"

namespace SerialDatagram {

template<typename Stream>
class Net {
public:
    // this corresponds to the user payload
    static constexpr BufferLen MaxBufferLen = 56;

    Net(
        Stream &stream)
            : stream(stream),
            receiver(
                stream,
                rcv_table),
            sender(
                stream,
                buf_alloc) {
        // empty
    }

    void Process() {
        receiver.Process();

        sender.Process();
    }

    // To send a message, the client first allocates a buffer.
    // The returned buffer length is the maximum payload size.
    // After filling in the buffer and setting the buffer len,
    // the client can either invoke send or first prepare
    // the datagram and then send the prepared datagram.
    // The prepared path is mostly used for tests.
    Buffer AllocBuffer() {
        auto ptr = buf_alloc.Alloc();

        if(ptr) {
            ptr = static_cast<PacketHdr *>(ptr) + 1;
        }

        return Buffer { ptr, MaxBufferLen }; 
    }

    // The buffer ownership is passed to the network object.
    Status Send(Port port, Buffer buf) {
        return sender.Send(port, buf);
    }

    void PrepareDatagram(Port port, Buffer &buf) {
        sender.PrepareDatagram(port, buf);
    }

    // Send an already prepared datagram.
    Status SendPreparedDatagram(Buffer &buf) {
        return sender.SendPreparedDatagram(buf);
    }


    Status RegisterReceiver(
            Port port,
            Rcv &rcv) {
        return rcv_table.Register(port, rcv);
    }

    const RcvStats &GetRcvStats() const {
        return receiver.GetStats();
    }

    void ClearRcvStats() {
        receiver.ClearStats();
    }

private:
    //
    // Constants.
    //
    static constexpr uint16_t TotalBufLen =
        MaxBufferLen +
        sizeof(PacketHdr) +
        sizeof(PacketTrl);
    static constexpr uint16_t TotalBufs = 4;

    static constexpr uint8_t MaxReceivers = 4;

    //
    // Types.
    //
    using BufAlloc_ = BufAlloc<TotalBufLen, TotalBufs>;
    using RcvTable_ = RcvTable<MaxReceivers>;
    using Receiver_ = Receiver<Stream, RcvTable_, TotalBufLen>;
    using Sender_ = Sender<Stream, BufAlloc_, TotalBufs>;

    //
    // Data.
    //
    Stream &stream;

    BufAlloc_ buf_alloc;

    RcvTable_ rcv_table;

    Receiver_ receiver;
    Sender_ sender;
};

}
