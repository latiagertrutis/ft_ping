import struct
import textwrap

ICMP_HEADER_FORMAT = "!BBHHH"  # type, code, checksum, identifier, sequence
ICMP_HEADER_SIZE = 8
ICMP_COMPARABLE_OFF = 16
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

def pretty_icmp_as_string(packet: bytes, comparable: bool = False,
                          payload_comparable: bool = False) -> str:
    if len(packet) < ICMP_HEADER_SIZE:
        return "Packet too short to be ICMP."

    icmp_type, code, checksum, identifier, sequence = struct.unpack(ICMP_HEADER_FORMAT, packet[:ICMP_HEADER_SIZE])

    type_desc = ICMP_TYPE_DESCRIPTIONS.get(icmp_type, "Unknown")

    output = []
    output.append(f"📦 ICMP Packet (Total size: {len(packet)} bytes)\n")
    output.append("==[ ICMP Header ]==")
    output.append(f"Type:       {icmp_type} ({type_desc})")
    output.append(f"Code:       {code}")
    if comparable == False:
        output.append(f"Checksum:   0x{checksum:04X}")
        output.append(f"Identifier: {identifier}")
    output.append(f"Sequence #: {sequence}")

    if payload_comparable:
        payload = packet[ICMP_HEADER_SIZE + ICMP_COMPARABLE_OFF:]
        output.append(f"\n==[ Payload ({len(packet[ICMP_HEADER_SIZE:])} bytes (showing trimmed version for comparison)) ]==")
    else:
        payload = packet[ICMP_HEADER_SIZE:]
        output.append(f"\n==[ Payload ({len(payload)} bytes) ]==")
        
    if payload:
        output.append(textwrap.indent(hexdump(payload), "  "))

    return "\n".join(output)

def hexdump(data: bytes, width: int = 16) -> str:
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i:i+width]
        hex_part = ' '.join(f'{b:02X}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        lines.append(f"{i:04X}  {hex_part:<{width*3}}  {ascii_part}")
    return "\n".join(lines)

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

def generate_message(icmp_type: int, wrong_checksum: bool, icmp_id: int, icmp_seq: int, data: bytes) -> bytes:

    resp_type = icmp_type
    resp_id = icmp_id
    resp_seq = icmp_seq
    resp_data = data

    # Calc checksum with 0 value
    packet = struct.pack("!BBHHH", resp_type, 0, 0, resp_id, resp_seq) + resp_data
    checksum = calc_checksum(packet)
    if wrong_checksum:
        checksum += 1

    # Construct the final package
    return struct.pack("!BBHHH", resp_type, 0, checksum, resp_id, resp_seq) + resp_data
