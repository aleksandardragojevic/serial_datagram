#
# Datagram communication over serial interface.
#
# author: aleksandar
#

import crcmod
import struct
import binascii
from enum import Enum

class _ReceiverState(Enum):
    START_SEARCH = 1
    HDR_SEARCH = 2
    END_SEARCH = 3

class SerialDatagramStats:
    def __init__(self):
        self.clear()
    
    def clear(self):
        self.snd_msgs = 0
        self.snd_bytes = 0
        self.snd_incomplete = 0

        self.rcv_msgs = 0
        self.rcv_bytes = 0
        self.rcv_dropped_bytes = 0
        self.rcv_error_crc = 0
        self.rcv_error_size = 0
        self.rcv_error_trl = 0
        self.rcv_error_no_rcv = 0

class SerialDatagram:
    """Simple datagram communication over a serial interface.
    Each datagram is up to a specific size (56 bytes of payload by default).
    Each datagram is protected by a crc. The protocol uses well-known message
    headers and trailers to allow it to resync the stream after data loss.
    The protocol supports a notion of 8-bit ports to allow multiple clients of
    the protocl in the application."""

    # well-known trailer and header 2-byte words
    _HdrMagic = 0xa357
    _TrlMagic = 0xc69b

    _HdrFmt = '=HBBH'
    _HdrMagicFmt = '=H'
    _TrlFmt = '=H'
    _TrlMagicFmt = '=H'
    _CrcFmt = '=H'
    _MaxPayloadSize = 56

    def __init__(self, serial):
        self._serial = serial

        self._stats = SerialDatagramStats()

        self._crc16_usb = crcmod.mkCrcFun(0x18005, 0, True, 0xffff)

        self._hdr_len = struct.calcsize(SerialDatagram._HdrFmt)
        self._trl_len = struct.calcsize(SerialDatagram._TrlFmt)
        self._crc_len = struct.calcsize(SerialDatagram._CrcFmt)
        self._hdr_magic = struct.pack(SerialDatagram._HdrMagicFmt, SerialDatagram._HdrMagic)
        self._trl_magic = struct.pack(SerialDatagram._TrlMagicFmt, SerialDatagram._TrlMagic)

        self._rcv_table = { }
        self._rcv_buf = bytearray()
        self._rcv_state = _ReceiverState.START_SEARCH
        self._rcv_total_msg_size = 0
        self._rcv_hdr = []

    @property
    def stats(self):
        return self._stats

    #
    # Sending.
    #
    def send_bytes(self, port, payload):
        """Send a payload to the other end. Blocks until
        there is enough buffer space."""
        datagram = self._create_datagram_from_bytes(port, payload)
        self._send_datagram(datagram)

    def send_struct(self, port, *payload_args):
        """Send a payload formated from the arguments to the other end.
        Blocks until there is enough buffer space."""
        self.send_bytes(port, struct.pack(*payload_args))

    def _create_datagram_from_bytes(self, port, payload):
        size = len(payload)
        hdr = struct.pack(SerialDatagram._HdrFmt, SerialDatagram._HdrMagic, size, port, 0)
        trl = struct.pack(SerialDatagram._TrlFmt, SerialDatagram._TrlMagic)
        return self._add_crc(hdr + payload + trl)

    def _add_crc(self, datagram):
        crc = self._crc16_usb(datagram)
        crcbuf = struct.pack(SerialDatagram._CrcFmt, crc)
        return datagram[0:self._hdr_len - len(crcbuf)] + crcbuf + datagram[self._hdr_len:]

    def _send_datagram(self, datagram):
        self._send_track_stats(datagram)
        sent = self._serial.write(datagram)

        while sent < len(datagram):
            sent += self._serial.write(datagram[sent:])
            self._stats.snd_incomplete += 1

    def _send_track_stats(self, datagram):
        self._stats.snd_msgs += 1
        self._stats.snd_bytes += len(datagram)

    #
    # Receiving
    #
    def register_rcv(self, endp, cb):
        self._rcv_table[endp] = cb
        
    def process(self):
        """Process all received data. This function needs to be invoked
        periodically to process the incoming data."""
        while self._process_rcv_data():
            pass
            
    def _process_rcv_data(self):
        self._read_data()
        if self._rcv_state == _ReceiverState.START_SEARCH:
            if not self._start_search():
                return False
        if self._rcv_state == _ReceiverState.HDR_SEARCH:
            if not self._hdr_search():
                return False
        if self._rcv_state == _ReceiverState.END_SEARCH:
            return self._end_search()
        return True

    def _read_data(self):
        in_waiting = self._serial.in_waiting
        if in_waiting > 0:
            self._rcv_buf = self._rcv_buf + self._serial.read(in_waiting)

    def _start_search(self):
        idx = 0
        while True:
            if len(self._rcv_buf) < idx + len(self._hdr_magic):
                break
            if self._hdr_trl_buf(idx) == self._hdr_magic:
                self._rcv_state = _ReceiverState.HDR_SEARCH
                break
            else:
                idx = idx + 1
        if idx > 0:
            self._rcv_buf = self._rcv_buf[idx:]
            self._stats.rcv_dropped_bytes += idx
        return self._rcv_state == _ReceiverState.HDR_SEARCH
                
    def _hdr_search(self):
        if len(self._rcv_buf) < self._hdr_len:
            return False
        self._parse_hdr()
        if self._rcv_hdr_size() > SerialDatagram._MaxPayloadSize:
            self.stats.rcv_error_size += 1
            self._rcv_recover()
        else:
            self._calc_rcv_total_msg_size()
            self._rcv_state = _ReceiverState.END_SEARCH
        return True
    
    def _end_search(self):
        if len(self._rcv_buf) < self._rcv_total_msg_size:
            return False
        if self._trl_buf() != self._trl_magic:
            self._stats.rcv_error_trl += 1
            self._rcv_recover()
            return True
        if self._msg_crc() != self._rcv_hdr_crc():
            self._stats.rcv_error_crc += 1
            self._rcv_recover()
            return True
        self._invoke_rcv()
        self._start_next_msg()
        return True

    def _hdr_trl_buf(self, idx):
        return self._rcv_buf[idx:idx + len(self._hdr_magic)]

    def _parse_hdr(self):
        self._rcv_hdr = struct.unpack(SerialDatagram._HdrFmt, self._rcv_buf[:self._hdr_len])

    def _calc_rcv_total_msg_size(self):
        self._rcv_total_msg_size = self._rcv_hdr_size() + self._hdr_len + self._trl_len

    def _rcv_hdr_size(self):
        return self._rcv_hdr[1]

    def _rcv_hdr_crc(self):
        return self._rcv_hdr[3]

    def _rcv_hdr_port(self):
        return self._rcv_hdr[2]

    def _trl_buf(self):
        return self._rcv_buf[self._rcv_total_msg_size - self._trl_len : self._rcv_total_msg_size]

    def _msg_crc(self):
        return self._crc16_usb(self._crc_buf())

    def _crc_buf(self):
        return \
            self._rcv_buf[0:self._hdr_len - self._crc_len] + \
            struct.pack(SerialDatagram._CrcFmt, 0) + \
            self._rcv_buf[self._hdr_len : self._rcv_total_msg_size]

    def _invoke_rcv(self):
        self._stats.rcv_msgs += 1
        self._stats.rcv_bytes += self._rcv_total_msg_size

        port = self._rcv_hdr_port()
        if port in self._rcv_table:
            payload = self._rcv_buf[self._hdr_len : self._hdr_len + self._rcv_hdr_size()]
            self._rcv_table[port](payload)
        else:
            self._stats.rcv_error_no_rcv += 1

    def _start_next_msg(self):
        self._rcv_buf = self._rcv_buf[self._rcv_total_msg_size:]
        self._rcv_state = _ReceiverState.START_SEARCH
    
    def _rcv_recover(self):
        self.stats.rcv_dropped_bytes += len(self._hdr_magic)
        self._rcv_buf = self._rcv_buf[len(self._hdr_magic):]
        self._rcv_state = _ReceiverState.START_SEARCH
