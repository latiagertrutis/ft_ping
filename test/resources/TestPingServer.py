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
        print ("[Main] Packet from %r: %r" % self.addr,self.data)
        

    def stop_test_server(self) -> None:
        self.socket.close()
