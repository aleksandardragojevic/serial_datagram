//
// Simple networking over a serial connection.
//
// author: aleksandar
//

#pragma once

#include <stdint.h>

#include <Arduino.h>

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

    Buffer AllocBuffer() {
        auto ptr = buf_alloc.Alloc();

        if(ptr) {
            ptr = static_cast<PacketHdr *>(ptr) + 1;
        }

        return Buffer { ptr, MaxBufferLen }; 
    }

    // The buffer ownership is passed to the network object.
    Status Send(Endpoint endp, Buffer buf) {
        return sender.Send(endp, buf);
    }

    Status RegisterReceiver(
            Endpoint endp,
            Rcv &rcv) {
        return rcv_table.Register(endp, rcv);
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
