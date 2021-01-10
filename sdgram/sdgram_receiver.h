//
// Message receiver over serial.
//
// author: aleksandar
//

#pragma once

#include <crc16.h>

#include "sdgram_defs.h"
#include "sdgram_prot.h"
#include "sdgram_rcv_stats.h"
#include "sdgram_log.h"

namespace SerialDatagram {

template<
    typename Stream,
    typename RcvTable,
    uint16_t TotalBufLen>
class Receiver {
public:
    Receiver(
        Stream &stream,
        RcvTable &rcv_table)
            : stream(stream),
            rcv_table(rcv_table),
            state(State::SearchStart),
            next(0) {
        stats.Clear();
    }

    void Process() {
        while(ReadMoreData()) {
            LogBuffer();
            
            if(state == State::SearchStart) {
                ProcessSearchStart();
            } else {
                ProcessSearchEnd();
            }
        }
    }

    const RcvStats &GetRcvStats() const {
        return stats;
    }

    void ClearRcvStats() {
        stats.Clear();
    }


private:
    //
    // Constants.
    //
    static constexpr uint8_t MinMsgSize =
        sizeof(PacketHdr) + sizeof(PacketTrl);

    //
    // Types.
    //
    enum class State {
        SearchStart,
        SearchEnd
    };

    //
    // Functions.
    //
    PacketHdr &Hdr() {
        return *reinterpret_cast<PacketHdr *>(data);
    }

    uint16_t TotalMsgSize() const {
        return Hdr().size + sizeof(PacketHdr) + sizeof(PacketTrl);
    }

    uint16_t TrlOffset() const {
        return Hdr().size + sizeof(PacketHdr);
    }

    bool ReadMoreData() {
        auto available = stream.available();
        auto bytes_to_read = MaxBytesToRead();

        if(!available && bytes_to_read) {
            return false;
        }

        if(bytes_to_read) {
            auto bytes_read = ReadBytes(
                reinterpret_cast<void *>(data + next),
                min(available, MaxBytesToRead()));

            if(!bytes_read) {
                return false;
            }

            LogBytesRead(bytes_read);

            next += bytes_read;
        }

        return true;
    }

    uint8_t MaxBytesToRead() const {
        if(state == State::SearchStart) {
            return next < MinMsgSize
                ? MinMsgSize - next :
                0;
        }

        if(next < sizeof(PacketHdr)) {
            return MinMsgSize - next;
        }

        auto bytes_to_read = TotalMsgSize() - next;

        auto ret = bytes_to_read + next < TotalBufLen
            ? bytes_to_read
            : 0;

        return ret;
    }

    void ProcessSearchStart(uint8_t curr = 0) {
        if(next < sizeof(uint16_t)) {
            LogIncompleteHdr();
            return;
        }

        while(curr < next - 1) {
            uint16_t val = *reinterpret_cast<uint16_t *>(data + curr);

            if(val == HdrMagic) {
                LogFoundHdrMagic();

                // We rarely need to copy data. This is only needed
                // when we are recovering the lost sync.
                if(curr) {
                    LogBufMemmove();

                    memmove(
                        reinterpret_cast<void *>(data),
                        reinterpret_cast<void *>(data + curr),
                        next - curr);
                    next -= curr;

                    stats.dropped_bytes += curr;
                }

                state = State::SearchEnd;

                if(next >= sizeof(PacketHdr)) {
                    ProcessSearchEnd();
                }

                return;
            }

            ++curr;
        }

        // curr should be next - 1
        if(curr != 0) {
            data[0] = data[curr];
            stats.dropped_bytes += curr;
            next = sizeof(uint8_t);
        }
    }

    const PacketTrl &Trl() const {
        return *reinterpret_cast<const PacketTrl *>(
            data + TrlOffset());
    }
    
    void ProcessSearchEnd() {
        auto total_msg_size = TotalMsgSize();

        if(total_msg_size > TotalBufLen) {
            stats.size_error++;
            Recover();
            return;
        }

        if(next < total_msg_size) {
            LogIncompleteMsg(total_msg_size);
            return;
        }

        if(Trl().magic != TrlMagic) {
            LogTrailerMismatch();
            stats.trl_error++;

            Recover();
            return;
        }

        if(!CheckCrc()) {
            LogCrcMismatch();
            stats.crc_error++;

            Recover();
            return;
        }

        // invoke the callback
        rcv_table.Received(
            Hdr().endp,
            Buffer {
                reinterpret_cast<void *>(data + sizeof(PacketHdr)),
                Hdr().size });
        
        stats.msgs++;
        stats.bytes += TotalMsgSize();

        // return to searching for start
        state = State::SearchStart;
        
        if(next != total_msg_size) {
            memmove(
                reinterpret_cast<void *>(data),
                reinterpret_cast<void *>(data + total_msg_size),
                next - total_msg_size);
            next -= total_msg_size;
        } else {
            next = 0;
        }
    }

    void Recover() {
        state = State::SearchStart;
        ProcessSearchStart(sizeof(uint16_t));
    }

    uint16_t ReadBytes(void *buf, uint16_t max_to_read) {
        uint16_t read_so_far = 0;
        auto out = static_cast<char *>(buf);

        while(read_so_far < max_to_read && stream.available() > 0) {
            *out++ = stream.read();
            read_so_far++;
        }

        return read_so_far;
    }

    bool CheckCrc() {
        auto rcv = Hdr().crc;

        Hdr().crc = 0;
        auto calc = Crc16Usb::Calc(data, TotalMsgSize());

        return calc == rcv;
    }

    // logging
    static void LogBytesToRead(int ret) {
        LogVerbose("[NET-SER] Reading max ");
        LogVerbose(ret);
        LogVerboseLn(" bytes");
    }

    void LogBuffer() {
        char hex[3];

        LogVerbose("[NET-SER] Buf: ");

        for(uint8_t i = 0;i < next;i++) {
            sprintf(hex, "%02X", data[i]);
            LogVerbose(hex);
            LogVerbose(" ");
        }

        LogVerboseLn(" EOB");
    }

    static void LogCrcMismatch() {
        LogVerboseLn("[NET-SER] CRC mismatch");
    }

    void LogTrailerMismatch() const {
        LogVerbose("[NET-SER] trailer mismatch ");
        LogVerboseLn(Trl().magic);
    }

    void LogIncompleteMsg(uint16_t total_msg_size) const {
        LogVerbose("[NET-SER] not enough bytes in the message ");
        LogVerbose(total_msg_size);
        LogVerbose(" ");
        LogVerboseLn(next);
    }

    static void LogBufMemmove() {
        LogVerboseLn("[NET-SER] moving buffer data");
    }

    static void LogFoundHdrMagic() {
        LogVerboseLn("[NET-SER] found header magic");
    }

    static void LogIncompleteHdr() {
        LogVerboseLn("[NET-SER] not enough bytes for header");
    }

    static void LogBytesRead(uint16_t bytes_read) {
        LogVerbose("[NET-SER] read ");
        LogVerbose(bytes_read);
        LogVerboseLn(" bytes");
    }

    //
    // Data.
    //
    Stream &stream;
    RcvTable &rcv_table;

    State state;

    uint8_t data[TotalBufLen];
    uint8_t next;

    RcvStats stats;
};

}
