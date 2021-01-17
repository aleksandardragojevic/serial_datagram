//
// Testing the sdgram on x64.
//
// author: aleksandar
//

#include "gtest/gtest.h"

#include <iostream>
#include <cstdio>

#include "logger.h"
#include "sdgram.h"
#include "memory_buffer_pair.h"

constexpr size_t DefaultCapacity = 2048;
constexpr SerialDatagram::Port DefaultPort = 1;

using SDgram = SerialDatagram::Net<SerialMock>;

static void PrintBuf(SerialDatagram::Buffer buf) {
    auto data = static_cast<uint8_t *>(buf.ptr);

    for(auto i = 0;i < buf.len;i++) {
        auto d = static_cast<unsigned>(data[i]);
        printf("%02X", d);
    }
}

static void PrintBuf(SerialDatagram::Buffer buf, const char *msg) {
    printf("%s", msg);
    PrintBuf(buf);
    printf("\n");
}

//
// Receive tests.
//
class TestRcv : public SerialDatagram::Rcv {
public:
    TestRcv()
            : msgs_received(0),
            bytes_received(0) {
        // empty
    }

    virtual ~TestRcv() = default;

    void ProcessMsg(SerialDatagram::Buffer buf) override {
        ++msgs_received;
        bytes_received += buf.len;

        PrintBuf(buf, "Got data: ");
    }

    size_t msgs_received;
    size_t bytes_received;
};

//
// Tests.
//
struct ReceiveTest {
    static constexpr size_t BytesToSend = 10;
    static constexpr size_t ByteOffset = 10;

    static constexpr size_t DatagramSize =
        BytesToSend +
        SerialDatagram::DatagramHdrSize +
        SerialDatagram::DatagramTrlSize;

    ReceiveTest(SerialDatagram::Port port = DefaultPort)
            : serial(DefaultCapacity),
            sdgram_serial(serial.CreateA()),
            my_serial(serial.CreateB()),
            sdgram(sdgram_serial) {
        sdgram.RegisterReceiver(DefaultPort, rcv);

        InitBuf(port);
    }

    void InitBuf(SerialDatagram::Port port) {
        buf = sdgram.AllocBuffer();

        buf.len = 0;

        for(size_t i = 0;i < BytesToSend;i++) {
            static_cast<uint8_t *>(buf.ptr)[i] = static_cast<uint8_t>(i + ByteOffset);
            buf.len++;
        }

        sdgram.PrepareDatagram(port, buf);
        PrintBuf(buf, "Prepared datagram: ");
    }

    MemoryBufferPair serial;
    SerialMock sdgram_serial;
    SerialMock my_serial;
    SDgram sdgram;

    TestRcv rcv;

    SerialDatagram::Buffer buf;
};

TEST(SdgramTests, ReceiveOne) {
    ReceiveTest test;

    test.my_serial.write(test.buf.ptr, test.buf.len);

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    EXPECT_EQ(ReceiveTest::DatagramSize, test.sdgram.GetRcvStats().bytes);
}

TEST(SdgramTests, ReceiveThree) {
    constexpr size_t MsgsToSend = 3;

    ReceiveTest test;

    for(size_t i = 0;i < MsgsToSend;i++) {
        test.my_serial.write(test.buf.ptr, test.buf.len);
        test.sdgram.Process();
    }

    EXPECT_EQ(ReceiveTest::BytesToSend * MsgsToSend, test.rcv.bytes_received);
    EXPECT_EQ(MsgsToSend, test.rcv.msgs_received);

    EXPECT_EQ(MsgsToSend * ReceiveTest::DatagramSize, test.sdgram.GetRcvStats().bytes);
}

TEST(SdgramTests, ReceiveThreeBatch) {
    constexpr size_t MsgsToSend = 3;

    ReceiveTest test;

    for(size_t i = 0;i < MsgsToSend;i++) {
        test.my_serial.write(test.buf.ptr, test.buf.len);
    }

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend * MsgsToSend, test.rcv.bytes_received);
    EXPECT_EQ(MsgsToSend, test.rcv.msgs_received);

    EXPECT_EQ(MsgsToSend * ReceiveTest::DatagramSize, test.sdgram.GetRcvStats().bytes);
}

TEST(SdgramTests, ReceivePartial) {
    ReceiveTest test;

    uintptr_t ptr = reinterpret_cast<uintptr_t>(test.buf.ptr);

    for(size_t i = 0;i < test.buf.len;i++) {
        test.my_serial.write(reinterpret_cast<void *>(ptr + i), 1);
        test.sdgram.Process();
    }

    EXPECT_EQ(ReceiveTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    EXPECT_EQ(ReceiveTest::DatagramSize, test.sdgram.GetRcvStats().bytes);
}

TEST(SdgramTests, ReceiveOverlap) {
    ReceiveTest test;

    uintptr_t ptr = reinterpret_cast<uintptr_t>(test.buf.ptr);

    for(uint16_t i = 1;i < test.buf.len;i++) {
        test.my_serial.write(reinterpret_cast<void *>(ptr), i);
        test.sdgram.Process();

        test.my_serial.write(reinterpret_cast<void *>(ptr + i), test.buf.len - i);
        test.my_serial.write(reinterpret_cast<void *>(ptr), test.buf.len);
        test.sdgram.Process();

        EXPECT_EQ(ReceiveTest::BytesToSend * 2 * i, test.rcv.bytes_received);
        EXPECT_EQ(i * 2, test.rcv.msgs_received);

        EXPECT_EQ(ReceiveTest::DatagramSize * 2 * i, test.sdgram.GetRcvStats().bytes);
    }
}

TEST(SdgramTests, ReceiveIncomplete) {
    ReceiveTest test;

    uintptr_t ptr = reinterpret_cast<uintptr_t>(test.buf.ptr);

    uint16_t expected_dropped = 0;

    for(uint16_t i = 1;i < test.buf.len;i++) {
        test.my_serial.write(reinterpret_cast<void *>(ptr), i);
        test.my_serial.write(reinterpret_cast<void *>(ptr), test.buf.len);

        expected_dropped += i;
    }

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend * (test.buf.len - 1), test.rcv.bytes_received);
    EXPECT_EQ(test.buf.len - 1, test.rcv.msgs_received);

    auto stats = test.sdgram.GetRcvStats();

    EXPECT_EQ(ReceiveTest::DatagramSize * (test.buf.len - 1), stats.bytes);
    EXPECT_EQ(expected_dropped, stats.dropped_bytes);
}

TEST(SdgramTests, ReceiveIncompletePartial) {
    ReceiveTest test;

    uintptr_t ptr = reinterpret_cast<uintptr_t>(test.buf.ptr);

    uint16_t expected_dropped = 0;

    for(uint16_t i = 1;i < test.buf.len;i++) {
        test.my_serial.write(reinterpret_cast<void *>(ptr), i);
        test.sdgram.Process();

        test.my_serial.write(reinterpret_cast<void *>(ptr), test.buf.len);
        test.sdgram.Process();

        EXPECT_EQ(ReceiveTest::BytesToSend * i, test.rcv.bytes_received);
        EXPECT_EQ(i, test.rcv.msgs_received);

        expected_dropped += i;

        auto stats = test.sdgram.GetRcvStats();

        EXPECT_EQ(ReceiveTest::DatagramSize * i, stats.bytes);
        EXPECT_EQ(expected_dropped, stats.dropped_bytes);
    }
}

TEST(SdgramTests, ReceiveErrorTrl) {
    ReceiveTest test;

    auto last_byte = static_cast<uint8_t *>(test.buf.ptr) + test.buf.len - 1;
    auto old_last = *last_byte;

    *last_byte = 0;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    *last_byte = old_last;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    auto stats = test.sdgram.GetRcvStats();

    EXPECT_EQ(ReceiveTest::DatagramSize, stats.bytes);
    EXPECT_EQ(1, stats.trl_error);
    EXPECT_EQ(ReceiveTest::DatagramSize, stats.dropped_bytes);
}

TEST(SdgramTests, ReceiveErrorCrc) {
    ReceiveTest test;

    auto hdr = static_cast<SerialDatagram::DatagramHdr *>(test.buf.ptr);
    auto old_crc = hdr->crc;

    hdr->crc = 0;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    hdr->crc = old_crc;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    auto stats = test.sdgram.GetRcvStats();

    EXPECT_EQ(ReceiveTest::DatagramSize, stats.bytes);
    EXPECT_EQ(1, stats.crc_error);
    EXPECT_EQ(ReceiveTest::DatagramSize, stats.dropped_bytes);
}

TEST(SdgramTests, ReceiveErrorSize) {
    ReceiveTest test;

    auto hdr = static_cast<SerialDatagram::DatagramHdr *>(test.buf.ptr);
    auto old_size = hdr->size;

    hdr->size = SDgram::MaxBufferLen + 1;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    hdr->size = old_size;
    test.my_serial.write(test.buf.ptr, test.buf.len);

    test.sdgram.Process();

    EXPECT_EQ(ReceiveTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    auto stats = test.sdgram.GetRcvStats();

    EXPECT_EQ(ReceiveTest::DatagramSize, stats.bytes);
    EXPECT_EQ(1, stats.size_error);
    EXPECT_EQ(ReceiveTest::DatagramSize, stats.dropped_bytes);
}

TEST(SdgramTests, ReceiveNoRcv) {
    ReceiveTest test(DefaultPort + 1);

    test.my_serial.write(test.buf.ptr, test.buf.len);
    test.sdgram.Process();

    EXPECT_EQ(0, test.rcv.bytes_received);
    EXPECT_EQ(0, test.rcv.msgs_received);

    auto stats = test.sdgram.GetRcvStats();

    EXPECT_EQ(0, stats.bytes);
    EXPECT_EQ(1, stats.rcv_error);
    EXPECT_EQ(ReceiveTest::DatagramSize, stats.dropped_bytes);
}

//
// Send tests.
//

struct SendTest {
    static constexpr size_t BytesToSend = 10;
    static constexpr size_t ByteOffset = 10;

    static constexpr size_t DatagramSize =
        BytesToSend +
        SerialDatagram::DatagramHdrSize +
        SerialDatagram::DatagramTrlSize;

    SendTest(
        size_t channel_capacity = DefaultCapacity)
            : serial(channel_capacity),
            serial_rcv(serial.CreateA()),
            serial_snd(serial.CreateB()),
            sdgram_rcv(serial_rcv),
            sdgram_snd(serial_snd) {
        sdgram_rcv.RegisterReceiver(DefaultPort, rcv);
    }

    SerialDatagram::Buffer AllocBuf(SerialDatagram::Port port = DefaultPort) {
        auto buf = sdgram_snd.AllocBuffer();

        if(!buf.ptr) {
            return buf;
        }

        buf.len = 0;

        for(size_t i = 0;i < BytesToSend;i++) {
            static_cast<uint8_t *>(buf.ptr)[i] = static_cast<uint8_t>(i + ByteOffset);
            buf.len++;
        }

        sdgram_snd.PrepareDatagram(port, buf);
        PrintBuf(buf, "Prepared datagram: ");

        return buf;
    }

    MemoryBufferPair serial;
    SerialMock serial_rcv;
    SerialMock serial_snd;
    SDgram sdgram_rcv;
    SDgram sdgram_snd;

    TestRcv rcv;
};

TEST(SdgramTests, SendAndReceive) {
    SendTest test;

    auto buf = test.AllocBuf();
    test.sdgram_snd.SendDatagram(buf);

    test.sdgram_rcv.Process();

    EXPECT_EQ(SendTest::BytesToSend, test.rcv.bytes_received);
    EXPECT_EQ(1, test.rcv.msgs_received);

    auto stats = test.sdgram_rcv.GetRcvStats();

    EXPECT_EQ(1, stats.msgs);
    EXPECT_EQ(SendTest::DatagramSize, stats.bytes);
}

TEST(SdgramTests, SendAndReceiveTwenty) {
    constexpr size_t SendCount = 20;

    SendTest test;

    for(size_t i = 0;i < SendCount;i++) {
        auto buf = test.AllocBuf();
        test.sdgram_snd.SendDatagram(buf);
    }

    test.sdgram_rcv.Process();

    EXPECT_EQ(SendTest::BytesToSend * SendCount, test.rcv.bytes_received);
    EXPECT_EQ(SendCount, test.rcv.msgs_received);

    auto stats = test.sdgram_rcv.GetRcvStats();

    EXPECT_EQ(SendCount, stats.msgs);
    EXPECT_EQ(SendTest::DatagramSize * SendCount, stats.bytes);
}

TEST(SdgramTests, SendAllocTooManyBuffers) {
    SendTest test;

    for(uint16_t i = 0;i < SDgram::TotalBufs;i++) {
        auto buf = test.AllocBuf();
        EXPECT_NE(nullptr, buf.ptr);
    }

    auto buf = test.AllocBuf();
    EXPECT_EQ(nullptr, buf.ptr);
}

TEST(SdgramTests, SendFillChannel) {
    constexpr size_t MsgsToSend = 4;
    constexpr size_t TotalBytes = MsgsToSend * SendTest::DatagramSize;

    for(size_t i = 1;i < SendTest::DatagramSize * 2;i++) {
        printf("===\n");
        printf("===   Running with channel size %zd\n", i);
        printf("===\n\n");

        SendTest test(i);

        for(size_t m = 0;m < MsgsToSend;m++) {
            auto buf = test.AllocBuf();
            test.sdgram_snd.SendDatagram(buf);
        }

        for(size_t r = 0;r < (TotalBytes + i - 1) / i;r++) {
            test.sdgram_rcv.Process();
            test.sdgram_snd.Process();
        }

        EXPECT_EQ(MsgsToSend * SendTest::BytesToSend, test.rcv.bytes_received);
        EXPECT_EQ(MsgsToSend, test.rcv.msgs_received);

        auto stats = test.sdgram_rcv.GetRcvStats();

        EXPECT_EQ(MsgsToSend, stats.msgs);
        EXPECT_EQ(TotalBytes, stats.bytes);

        printf("\n");
    }
}
