//
// Handle registration of receivers and calls.
//
// author: aleksandar
//

#pragma once

#include "sdgram_defs.h"

namespace SerialDatagram {

template<uint8_t MaxReceiverCount>
class RcvTable {
public:
    Status Register(Port port, Rcv &rcv) {
        auto existing = FindRegistered(port);

        if(existing != nullptr) {
            return Status::Duplicate;
        }

        auto empty = FindEmptySlot();

        if(empty == nullptr) {
            return Status::NoMoreSpace;
        }

        empty->port = port;
        empty->rcv = &rcv;

        return Status::Success;
    }

    Status Received(Port port, Buffer buf) {
        auto existing = FindRegistered(port);

        if(existing == nullptr) {
            return Status::NoReceiver;
        }

        existing->rcv->ProcessMsg(buf);

        return Status::Success;
    }

private:
    //
    // Types.
    //
    struct Registered {
        Registered()
                : port(InvalidPort),
                rcv(nullptr) {
            // empty
        }

        Port port;
        Rcv *rcv;
    };

    //
    // Functions.
    //
    Registered *FindRegistered(Port port) {
        for(uint16_t i = 0;i < MaxReceiverCount;i++) {
            if(registered[i].port == port) {
                return registered + i;
            }
        }

        return nullptr;
    }

    Registered *FindEmptySlot() {
        return FindRegistered(InvalidPort);
    }

    //
    // Data.
    //
    Registered registered[MaxReceiverCount];
};

}
