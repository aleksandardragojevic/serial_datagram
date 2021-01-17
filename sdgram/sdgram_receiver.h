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

    const RcvStats &GetStats() const {
        return stats;
    }

    void ClearStats() {
        stats.Clear();
    }


private:
    //
    // Constants.
    //
    static constexpr uint16_t MinMsgSize =
        sizeof(DatagramHdr) + sizeof(DatagramTrl);

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
    DatagramHdr &Hdr() {
        return *reinterpret_cast<DatagramHdr *>(data);
    }

    const DatagramHdr &Hdr() const {
        return *reinterpret_cast<const DatagramHdr *>(data);
    }

    bool IsHdrReceived() const {
        return next >= sizeof(DatagramHdr);
    }

    uint16_t TotalMsgSize() const {
        return Hdr().size + sizeof(DatagramHdr) + sizeof(DatagramTrl);
    }

    uint16_t TrlOffset() const {
        return Hdr().size + sizeof(DatagramHdr);
    }

    bool ReadMoreData() {
        auto available = stream.available();
        auto bytes_to_read = MaxBytesToRead();

        if(!available && bytes_to_read) {
            return false;
        }

        if(bytes_to_read > available) {
            bytes_to_read = static_cast<uint16_t>(available);
        }

        if(bytes_to_read) {
            auto bytes_read = ReadBytes(
                reinterpret_cast<void *>(data + next),
                bytes_to_read);

            if(!bytes_read) {
                return false;
            }

            LogBytesRead(bytes_read);

            next += bytes_read;
        }

        return true;
    }

    uint16_t MaxBytesToRead() const {
        if(state == State::SearchStart) {
            return next < MinMsgSize
                ? MinMsgSize - next :
                0;
        }

        if(next < sizeof(DatagramHdr)) {
            return MinMsgSize - next;
        }

        auto bytes_to_read = TotalMsgSize() - next;

        auto ret = bytes_to_read + next < TotalBufLen
            ? bytes_to_read
            : 0;

        return ret;
    }

    void ProcessSearchStart(uint16_t curr = 0) {
        if(next < sizeof(DatagramHdrMagic)) {
            LogIncompleteHdrMagic();
            return;
        }

        while(curr < next - 1) {
            uint16_t val = *reinterpret_cast<uint16_t *>(data + curr);

            if(val == DatagramHdrMagic) {
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

                if(next >= sizeof(DatagramHdr)) {
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

    const DatagramTrl &Trl() const {
        return *reinterpret_cast<const DatagramTrl *>(
            data + TrlOffset());
    }
    
    void ProcessSearchEnd() {
        if(!IsHdrReceived()) {
            return;
        }

        auto total_msg_size = TotalMsgSize();

        if(total_msg_size > TotalBufLen) {
            LogMsgTooLarge(total_msg_size);
            stats.size_error++;
            Recover();
            return;
        }

        if(next < total_msg_size) {
            LogIncompleteMsg(total_msg_size);
            return;
        }

        if(Trl().magic != DatagramTrlMagic) {
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

        InvokeCb();

        StartNextMsg(total_msg_size);
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
        auto calc = Crc16Usb::Calc(data, static_cast<size_t>(TotalMsgSize()));

        return calc == rcv;
    }

    void InvokeCb() {
        auto status = rcv_table.Received(
            Hdr().port,
            Buffer {
                reinterpret_cast<void *>(data + sizeof(DatagramHdr)),
                Hdr().size });

        if(status == Status::Success) {
            stats.msgs++;
            stats.bytes += TotalMsgSize();
        } else if(status == Status::NoReceiver) {
            stats.rcv_error++;
            stats.dropped_bytes += TotalMsgSize();
        } else {
            LogUnexpectedInvokeCb(status);
        }
    }

    void StartNextMsg(uint16_t total_msg_size) {
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

    // logging
#define LOGGER_PREFIX_RCV "[SDGRAM-RCV] "

    static void LogBytesToRead(int ret) {
        LogVerbose(LOGGER_PREFIX_RCV "Reading max ");
        LogVerbose(ret);
        LogVerboseLn(" bytes");
    }

    void LogBuffer() {
        char hex[3];

        LogVerbose(LOGGER_PREFIX_RCV "Buf: ");

        for(uint8_t i = 0;i < next;i++) {
            sprintf(hex, "%02X", data[i]);
            LogVerbose(hex);
            LogVerbose(" ");
        }

        LogVerboseLn(" EOB");
    }

    static void LogCrcMismatch() {
        LogVerboseLn(LOGGER_PREFIX_RCV "CRC mismatch");
    }

    void LogTrailerMismatch() const {
        LogVerbose(LOGGER_PREFIX_RCV "trailer mismatch ");
        LogVerboseLn(Trl().magic);
    }

    static void LogMsgTooLarge(uint16_t total_msg_size) {
        LogVerbose(LOGGER_PREFIX_RCV "message too large (could be because of dropped bytes) ");
        LogVerbose(total_msg_size);
        LogVerbose(" > ");
        LogVerboseLn(TotalBufLen);
    }

    void LogIncompleteMsg(uint16_t total_msg_size) const {
        LogVerbose(LOGGER_PREFIX_RCV "not enough bytes in the message ");
        LogVerbose(total_msg_size);
        LogVerbose(" > ");
        LogVerboseLn(next);
    }

    static void LogBufMemmove() {
        LogVerboseLn(LOGGER_PREFIX_RCV "moving buffer data");
    }

    static void LogFoundHdrMagic() {
        LogVerboseLn(LOGGER_PREFIX_RCV "found header magic");
    }

    static void LogIncompleteHdrMagic() {
        LogVerboseLn(LOGGER_PREFIX_RCV "not enough bytes for header magic");
    }

    static void LogBytesRead(uint16_t bytes_read) {
        LogVerbose(LOGGER_PREFIX_RCV "read ");
        LogVerbose(bytes_read);
        LogVerboseLn(" bytes");
    }

    static void LogUnexpectedInvokeCb(Status status) {
        LogVerbose(LOGGER_PREFIX_RCV "unexpected invoke status ");
        LogVerboseLn(static_cast<int>(status));
    }

    //
    // Data.
    //
    Stream &stream;
    RcvTable &rcv_table;

    State state;

    uint8_t data[TotalBufLen];
    uint16_t next;

    RcvStats stats;
};

}
