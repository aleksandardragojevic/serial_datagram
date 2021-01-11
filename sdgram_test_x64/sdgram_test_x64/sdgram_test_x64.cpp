//
// Testing SerialDatagram library on windows.
//
// author: aleksandar
//

#include <iostream>
#include <cstdio>

#include "logger.h"
#include "sdgram.h"
#include "memory_buffer_pair.h"

constexpr size_t DefaultCapacity = 256;
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

class TestRcv : public SerialDatagram::Rcv {
public:
    virtual ~TestRcv() = default;

    void ProcessMsg(SerialDatagram::Buffer buf) override {
        PrintBuf(buf, "Got data: ");
    }
};

int main(int, char **) {
    std::cout << "Hello World!\n";

    MemoryBufferPair serial(DefaultCapacity);
    auto sdgram_serial = serial.CreateA();
    auto my_serial = serial.CreateB();

    SDgram sdgram(sdgram_serial);

    TestRcv rcv;
    sdgram.RegisterReceiver(DefaultPort, rcv);

    auto buf = sdgram.AllocBuffer();
    buf.len = 0;

    for(size_t i = 0;i < 10;i++) {
        static_cast<uint8_t *>(buf.ptr)[i] = static_cast<uint8_t>(i + 10);
        buf.len++;
    }

    sdgram.PrepareDatagram(DefaultPort, buf);
    PrintBuf(buf, "Prepared datagram: ");

    my_serial.write(buf.ptr, buf.len);

    sdgram.Process();

    return 0;
}
