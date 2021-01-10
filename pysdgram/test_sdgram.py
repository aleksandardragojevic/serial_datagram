#
# Tests for the serial datagram using mock serial.
#
# author: aleksandar
#

import binascii
import struct

from sdgram import SerialDatagram
from serial_mock import SerialChannelMockPair

class SerialDatagramForTest(SerialDatagram):
    def create_datagram_from_struct(self, port, *payload_args):
        return self._create_datagram_from_bytes(port, struct.pack(*payload_args))

class SdgramTestBase:
    def __init__(self):
        self.serial_pair = SerialChannelMockPair()
        self.serial_b = self.serial_pair.create_b()
        self.net = SerialDatagramForTest(self.serial_pair.create_a())

class CountMsg:
    def __init__(self):
        self.count = 0

    def __call__(self, packet):
        self.count += 1
        print('received: {0}'.format(binascii.hexlify(packet)))

def test_rcv_one():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
    test.serial_b.write(packet)

    test.net.process()

    assert cnt.count == 1
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)

def test_rcv_3():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    for _ in range(3):
        packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
        test.serial_b.write(packet)

        test.net.process()

    assert cnt.count == 3
    assert test.net.stats.rcv_msgs == 3
    assert test.net.stats.rcv_bytes == 3 * len(packet)

def test_rcv_3_batch():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    for _ in range(3):
        packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
        test.serial_b.write(packet)

    test.net.process()

    assert cnt.count == 3
    assert test.net.stats.rcv_msgs == 3
    assert test.net.stats.rcv_bytes == 3 * len(packet)

def test_rcv_partial():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)

    for i in range(len(packet)):
        print(i)
        test.serial_b.write(packet[i:i+1])
        test.net.process()

    assert cnt.count == 1
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)

def test_rcv_overlap():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
    expected = 0

    for i in range(1, len(packet) - 1):
        test.serial_b.write(packet[:i])
        test.net.process()
        assert expected == cnt.count

        test.serial_b.write(packet[i:])
        test.serial_b.write(packet)
        test.net.process()

        expected += 2

        assert expected == cnt.count

    assert test.net.stats.rcv_msgs == expected
    assert test.net.stats.rcv_bytes == expected * len(packet)

def test_error_incomplete():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
    expected = 0
    dropped = 0

    for i in range(1, len(packet) - 1):
        test.serial_b.write(packet[:i])
        test.serial_b.write(packet)
        expected += 1
        dropped += i

    test.net.process()

    assert expected == cnt.count

    assert test.net.stats.rcv_msgs == expected
    assert test.net.stats.rcv_bytes == expected * len(packet)
    assert test.net.stats.rcv_dropped_bytes == dropped

def test_error_incomplete_partial():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
    expected = 0
    dropped = 0

    for i in range(1, len(packet) - 1):
        test.serial_b.write(packet[:i])
        test.serial_b.write(packet)
        expected += 1
        dropped += i
        test.net.process()

    assert expected == cnt.count

    assert test.net.stats.rcv_msgs == expected
    assert test.net.stats.rcv_bytes == expected * len(packet)
    assert test.net.stats.rcv_dropped_bytes == dropped

def test_error_trl():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)

    test.serial_b.write(packet[:len(packet) - 1])
    test.serial_b.write(struct.pack('=B', 0))
    test.serial_b.write(packet)

    test.net.process()

    assert 1 == cnt.count
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)
    assert test.net.stats.rcv_error_trl == 1
    assert test.net.stats.rcv_dropped_bytes == len(packet)

def test_error_crc():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)

    boundary = 8

    test.serial_b.write(packet[:boundary])
    test.serial_b.write(struct.pack('=B', 0))
    test.serial_b.write(packet[boundary+1:])
    test.serial_b.write(packet)

    test.net.process()

    assert 1 == cnt.count
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)
    assert test.net.stats.rcv_error_crc == 1
    assert test.net.stats.rcv_dropped_bytes == len(packet)

def test_error_size():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(1, '=BBBBBB', 11, 12, 13, 14, 15, 16)
    broken = test.net.create_datagram_from_struct(1, '=QQQQQQQQ', 11, 12, 13, 14, 15, 16, 17, 18)

    test.serial_b.write(broken)
    test.serial_b.write(packet)

    test.net.process()

    assert 1 == cnt.count
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)
    assert test.net.stats.rcv_error_size == 1
    assert test.net.stats.rcv_dropped_bytes == len(broken)

def test_error_no_rcv():
    test = SdgramTestBase()
    cnt = CountMsg()
    test.net.register_rcv(1, cnt)

    packet = test.net.create_datagram_from_struct(2, '=BBBBBB', 11, 12, 13, 14, 15, 16)

    test.serial_b.write(packet)

    test.net.process()

    assert 0 == cnt.count
    assert test.net.stats.rcv_msgs == 1
    assert test.net.stats.rcv_bytes == len(packet)
    assert test.net.stats.rcv_error_no_rcv == 1
    assert test.net.stats.rcv_dropped_bytes == 0
