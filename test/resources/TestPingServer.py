import socket
import struct
import threading
import time
import random
import binascii
from ping_utils import *

# === ICMP constants ===
ICMP_ECHO_REQUEST = 8
ICMP_ECHO_REPLY = 0
ICMP_UNREACHABLE = 3
MAX_ICMP_PACKET_SIZE = 1508  # includes IP + ICMP + payload

class TestPingServer:
    def __init__(self) -> None:
        self.socket: fileno

    def start_test_server(self) -> None:
        self.socket = socket.socket(socket.AF_INET,socket.SOCK_RAW,socket.IPPROTO_ICMP)
        self.socket.bind(("127.0.0.1", 0))

    def wait_for_messages(self, count: int, payload: bytes = b'') -> None:
        while count:
            data, addr = self.socket.recvfrom(MAX_ICMP_PACKET_SIZE)

            req_type, req_code, req_checksum, req_id, req_seq, req_payload = decode_icmp_data(data)

            if req_type != ICMP_ECHO_REQUEST:
                continue

            print("=========================== RECV[%d] ===========================\n" % count)
            pretty_print_icmp(data)
            print("\n")

            if payload == bytes():
                payload = req_payload

            # Generate the response packet
            packet = generate_message(ICMP_ECHO_REPLY, req_id, req_seq, payload)
            
            # Send back the response
            self.socket.sendto(packet, addr)

            print("=========================== SENT[%d] ===========================\n" % count)
            pretty_print_icmp(packet)
            print("\n")
            
            count -= 1
        
    def stop_test_server(self) -> None:
        self.socket.close()
