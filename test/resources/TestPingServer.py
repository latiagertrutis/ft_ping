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

def _generate_message(icmp_type: int, icmp_id: int, icmp_seq: int, data: bytes) -> bytes:

    resp_type = icmp_type
    resp_id = icmp_id
    resp_seq = icmp_seq
    resp_data = data

    # Calc checksum with 0 value
    packet = struct.pack("!BBHHH", resp_type, 0, 0, resp_id, resp_seq) + resp_data
    checksum = _calc_checksum(packet)

    # Construct the final package
    return struct.pack("!BBHHH", resp_type, 0, checksum, resp_id, resp_seq) + resp_data



class TestPingServer:
    def __init__(self) -> None:
        self.socket: fileno

    def start_test_server(self) -> None:
        self.socket = socket.socket(socket.AF_INET,socket.SOCK_RAW,socket.IPPROTO_ICMP)
        self.socket.bind(("127.0.0.1", 0))

    def wait_for_messages(self, count: int) -> None:

        while count:
            data, addr = self.socket.recvfrom(MAX_ICMP_PACKET_SIZE)

            req_type, req_code, req_checksum, req_id, req_seq, req_data = _decode_icmp_data(data)

            if req_type != ICMP_ECHO_REQUEST:
                continue

            # Generate the response packet
            packet = _generate_message(ICMP_ECHO_REPLY, req_id, req_seq, req_data)
            
            # Send back the response
            self.socket.sendto(packet, addr)

            count -= 1
        
    def stop_test_server(self) -> None:
        self.socket.close()
