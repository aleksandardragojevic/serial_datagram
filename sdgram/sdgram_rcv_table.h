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
    Status Register(Endpoint endp, Rcv &rcv) {
        auto existing = FindRegistered(endp);

        if(existing != nullptr) {
            return Status::Duplicate;
        }

        auto empty = FindEmptySlot();

        if(empty == nullptr) {
            return Status::NoMoreSpace;
        }

        empty->endp = endp;
        empty->rcv = &rcv;

        return Status::Success;
    }

    Status Received(Endpoint endp, Buffer buf) {
        auto existing = FindRegistered(endp);

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
                : endp(InvalidEndpoint),
                rcv(nullptr) {
            // empty
        }

        Endpoint endp;
        Rcv *rcv;
    };

    //
    // Functions.
    //
    Registered *FindRegistered(Endpoint endp) {
        for(uint16_t i = 0;i < MaxReceiverCount;i++) {
            if(registered[i].endp == endp) {
                return registered + i;
            }
        }

        return nullptr;
    }

    Registered *FindEmptySlot() {
        return FindRegistered(InvalidEndpoint);
    }

    //
    // Data.
    //
    Registered registered[MaxReceiverCount];
};

}
