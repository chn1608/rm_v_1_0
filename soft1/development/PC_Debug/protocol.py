"""V1.0 stream framing and the 36-byte status payload.

Shared by the CAN monitor, the C simulator link and binary log replay. The
layout below must stay identical to `snapshot_encode_status()` in
`Firmware/Service/snapshot.c` and to docs/PROTOCOL.md.
"""
from dataclasses import dataclass
import binascii
import struct

HEADER = b"\xa5\x5a"
VERSION = 0x10
MAX_PAYLOAD = 64
STATUS = 0x02
STATUS_REPLY = 0x82

# layout_version, state, owner, flags, left_cmd, right_cmd, left_out, right_out,
# servo_us, servo_decideg, battery_mv, current_ma, faults, uptime_s,
# good_frames, bad_frames, loop_max_us, battery_src, current_src
STATUS_LAYOUT = struct.Struct("<BBBBhhhhHhHHIIHHHBB")
STATUS_BYTES = STATUS_LAYOUT.size
SOURCES = ("invalid", "measured", "command", "estimated", "simulated",
           "absent", "uncalibrated", "stale")
STATES = ("boot", "self-test", "standby", "local", "remote", "fault", "estop")
OWNERS = ("none", "local", "ble", "pc")


@dataclass(frozen=True)
class Frame:
    kind: int
    sequence: int
    payload: bytes = b""

    def encode(self):
        if len(self.payload) > MAX_PAYLOAD:
            raise ValueError("payload exceeds V1.0 limit")
        body = struct.pack("<BBBH", VERSION, self.kind, len(self.payload), self.sequence)
        body += self.payload
        return HEADER + body + struct.pack("<H", binascii.crc_hqx(body, 0xFFFF))


class Decoder:
    def __init__(self):
        self.pending = bytearray()
        self.last_ms = None
        self.bad = 0
        self.expired = 0

    def feed(self, data, now_ms):
        for byte in data:
            if self.pending and now_ms - self.last_ms >= 100:
                self.expired += 1
                self.pending.clear()
            self.last_ms = now_ms
            self.pending.append(byte)
            while self.pending:
                buf = self.pending
                if buf[0] != 0xA5 or (len(buf) >= 2 and buf[1] != 0x5A):
                    del buf[0]
                    continue
                if len(buf) < 5:
                    break
                size = buf[4] + 9
                if buf[2] != VERSION or size > MAX_PAYLOAD + 9:
                    self.bad += 1
                    del buf[0]
                    continue
                if len(buf) < size:
                    break
                (expected,) = struct.unpack_from("<H", buf, size - 2)
                if binascii.crc_hqx(buf[2:size-2], 0xFFFF) != expected:
                    self.bad += 1
                    del buf[0]
                    continue
                result = Frame(buf[3], int.from_bytes(buf[5:7], "little"), bytes(buf[7:size-2]))
                del buf[:size]
                yield result


def decode_status(frame):
    if frame.kind != STATUS_REPLY or len(frame.payload) != STATUS_BYTES:
        raise ValueError("invalid status response")
    (layout, state, owner, flags, left_cmd, right_cmd, left_out, right_out,
     servo_us, servo_decideg, battery_mv, current_ma, faults, uptime_s,
     good, bad, loop_us, battery_src, current_src) = STATUS_LAYOUT.unpack(frame.payload)
    if layout != 1 or state >= len(STATES) or owner >= len(OWNERS):
        raise ValueError("invalid status metadata")
    if battery_src >= len(SOURCES) or current_src >= len(SOURCES):
        raise ValueError("invalid data source")
    return dict(
        uptime_s=uptime_s,
        state=STATES[state],
        owner=OWNERS[owner],
        motion_permitted=bool(flags & 0x01),
        power_enabled=bool(flags & 0x08),
        ble_link=bool(flags & 0x10),
        pc_link=bool(flags & 0x20),
        left_cmd=left_cmd,
        right_cmd=right_cmd,
        left_out=left_out,
        right_out=right_out,
        servo_us=servo_us,
        servo_mdeg=servo_decideg * 10,
        bus_mv=battery_mv if (flags & 0x02) else None,
        bus_source=SOURCES[battery_src],
        current_ma=current_ma if (flags & 0x04) else None,
        current_source=SOURCES[current_src],
        faults=faults,
        good_frames=good,
        bad_frames=bad,
        loop_max_us=loop_us,
    )
