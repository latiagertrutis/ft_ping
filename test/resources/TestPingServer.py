import socket
import struct
import threading
import time
import random

# === ICMP constants ===
ICMP_ECHO_REQUEST = 8
ICMP_ECHO_REPLY = 0
ICMP_UNREACHABLE = 3
ICMP_HEADER_SIZE = 8
IP_HEADER_SIZE = 20
MAX_ICMP_PACKET_SIZE = 1508  # includes IP + ICMP + payload


def _calc_checksum(data: bytes) -> int:
    if len(data) % 2 == 1:
        data += b'\x00'
    checksum = 0
    for i in range(0, len(data), 2):
        word = data[i] << 8 | data[i + 1]
        checksum += word
        checksum = (checksum & 0xffff) + (checksum >> 16)
    return ~checksum & 0xffff

def _decode_icmp_data(data: bytes) -> tuple[int, int, int, int, int, bytes]:
    if (len(data) < IP_HEADER_SIZE+ICMP_HEADER_SIZE):
        raise ValueError("Data received lenght is shorter than the minimum")
    icmp_hdr = data[IP_HEADER_SIZE:IP_HEADER_SIZE+ICMP_HEADER_SIZE]
    return struct.unpack("!BBHHH", icmp_hdr) + (data[IP_HEADER_SIZE+ICMP_HEADER_SIZE:],)

class TestPingServer:
    def __init__(self) -> None:
        self.finish: bool = False
        self.data: bytes
        self.addr: _RetAddress
        self.socket: fileno
        self.thread: threading.Thread

    def _receive_message(self) -> None:
        self.data, self.addr = self.socket.recvfrom(MAX_ICMP_PACKET_SIZE)
        print ("[Thread] Packet from %r: %r" % self.addr,self.data)

    def start_test_server(self) -> None:
        self.socket = socket.socket(socket.AF_INET,socket.SOCK_RAW,socket.IPPROTO_ICMP)
        self.socket.bind(("127.0.0.1", 0))
        self.thread = threading.Thread(target=self._receive_message)
        self.thread.start()

    def wait_for_message(self) -> None:
        self.thread.join()
        req_type = _decode_icmp_data(self.data)[0]
        if req_type != ICMP_ECHO_REQUEST:
            raise ValueError("Received type [%d] when [%d] was expected" %
                             (req_type, ICMP_ECHO_REQUEST))

    def reply_message(self) -> None:
        req_type, _, req_checksum, req_id, req_seq, req_data = _decode_icmp_data(self.data)

        resp_type = ICMP_ECHO_REPLY
        resp_id = req_id
        resp_seq = req_seq
        resp_data = req_data

        # Calc checksum with 0 value
        resp_packet = struct.pack("!BBHHH", resp_type, 0, 0, resp_id, resp_seq) + resp_data
        resp_checksum = _calc_checksum(resp_packet)

        # Construct the final package
        resp_packet = struct.pack("!BBHHH", resp_type, 0, resp_checksum,
                                  resp_id, resp_seq) + resp_data

        # Send back the response
        self.socket.sendto(resp_packet, self.addr)
        
    def stop_test_server(self) -> None:
        self.socket.close()
