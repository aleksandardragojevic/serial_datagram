//
// Simple testing of sdgram protocol. For this to work,
// sdgram needs to be in the Arduino library directory.
//
// author: aleksandar
//

#include <SoftwareSerial.h>

#include <sdgram.h>

SoftwareSerial mySerial(2, 3); // RX, TX
SerialDatagram::Net<SoftwareSerial> net(mySerial);

class SimpleRcv : public SerialDatagram::Rcv {
public:
    virtual ~SimpleRcv() = default;

    void ProcessMsg(SerialDatagram::Buffer buf) override {
        Serial.print("[RCV] received bytes ");
        Serial.print(buf.len);
        Serial.print(" ");

        for(int i = 0;i < buf.len;i++) {
            int v = ((uint8_t *)buf.ptr)[i];
            Serial.print(v);
            Serial.print(" ");
        }

        Serial.println(" EOM");
    }
};

SimpleRcv rcv;

void setup() {
  Serial.begin(115200);
  Serial.println("Goodnight moon!");

  mySerial.begin(115200);
  mySerial.println("Hello, world?");

  net.RegisterReceiver(1, rcv);
}

void loop() {
  net.Process();
//   if(mySerial.available()) {
//     Serial.write(mySerial.read());
//   }

  mySerial.write("Testing");

  delay(100);
}
