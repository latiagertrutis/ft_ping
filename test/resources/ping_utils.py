import struct
import textwrap

ICMP_HEADER_FORMAT = "!BBHHH"  # type, code, checksum, identifier, sequence
ICMP_HEADER_SIZE = 8
IP_HEADER_SIZE = 20

ICMP_TYPE_DESCRIPTIONS = {
    0: "Echo Reply",
    3: "Destination Unreachable",
    5: "Redirect",
    8: "Echo Request",
    11: "Time Exceeded",
    13: "Timestamp Request",
    14: "Timestamp Reply",
}

def pretty_print_icmp(packet: bytes):
    if len(packet) < ICMP_HEADER_SIZE:
        print("Packet too short to be ICMP.")
        return

    # Parse header
    icmp_type, code, checksum, identifier, sequence = struct.unpack(ICMP_HEADER_FORMAT, packet[:ICMP_HEADER_SIZE])
    payload = packet[ICMP_HEADER_SIZE:]

    type_desc = ICMP_TYPE_DESCRIPTIONS.get(icmp_type, "Unknown")

    print(f"\n📦 ICMP Packet (Total size: {len(packet)} bytes)\n")
    print("==[ ICMP Header ]==")
    print(f"Type:       {icmp_type} ({type_desc})")
    print(f"Code:       {code}")
    print(f"Checksum:   0x{checksum:04X}")
    print(f"Identifier: {identifier}")
    print(f"Sequence #: {sequence}")

    if payload:
        print("\n==[ Payload ({} bytes) ]==".format(len(payload)))
        print(textwrap.indent(hexdump(payload), "  "))

def hexdump(data: bytes, width: int = 16) -> str:
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i:i+width]
        hex_part = ' '.join(f'{b:02X}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        lines.append(f"{i:04X}  {hex_part:<{width*3}}  {ascii_part}")
    return '\n'.join(lines)

def calc_checksum(data: bytes) -> int:
    if len(data) % 2 == 1:
        data += b'\x00'
    checksum = 0
    for i in range(0, len(data), 2):
        word = data[i] << 8 | data[i + 1]
        checksum += word
        checksum = (checksum & 0xffff) + (checksum >> 16)
    return ~checksum & 0xffff

def decode_icmp_data(data: bytes) -> tuple[int, int, int, int, int, bytes]:
    if (len(data) < IP_HEADER_SIZE+ICMP_HEADER_SIZE):
        raise ValueError("Data received lenght is shorter than the minimum")
    icmp_hdr = data[IP_HEADER_SIZE:IP_HEADER_SIZE+ICMP_HEADER_SIZE]
    return struct.unpack("!BBHHH", icmp_hdr) + (data[IP_HEADER_SIZE+ICMP_HEADER_SIZE:],)

def generate_message(icmp_type: int, icmp_id: int, icmp_seq: int, data: bytes) -> bytes:

    resp_type = icmp_type
    resp_id = icmp_id
    resp_seq = icmp_seq
    resp_data = data

    # Calc checksum with 0 value
    packet = struct.pack("!BBHHH", resp_type, 0, 0, resp_id, resp_seq) + resp_data
    checksum = calc_checksum(packet)

    # Construct the final package
    return struct.pack("!BBHHH", resp_type, 0, checksum, resp_id, resp_seq) + resp_data
