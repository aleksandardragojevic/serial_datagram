#
# Mock of the serial to use for testing serial datagram.
#
# author: aleksandar
#

class SerialChannelMock:
    def __init__(self):
        self._rcv_buf = bytearray()

    @property
    def in_waiting(self):
        return len(self._rcv_buf)

    def read(self, count):
        ret_count = min(count, len(self._rcv_buf))
        ret = self._rcv_buf[:ret_count]
        self._rcv_buf = self._rcv_buf[ret_count:]
        return ret

    def write(self, data):
        self._rcv_buf = self._rcv_buf + data

class SerialChannelMockPair:
    def __init__(self):
        self.a = SerialChannelMock()
        self.b = SerialChannelMock()
    
    def create_a(self):
        return SerialMockEndp(self.a, self.b)

    def create_b(self):
        return SerialMockEndp(self.b, self.a)

class SerialMockEndp:
    def __init__(self, r, w):
        self._r = r
        self._w = w

    @property
    def in_waiting(self):
        return self._r.in_waiting

    def read(self, count):
        return self._r.read(count)

    def write(self, data):
        self._w.write(data)
