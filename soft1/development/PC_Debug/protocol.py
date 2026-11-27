"""V1.0 stream framing, shared by status polling and log replay."""
from dataclasses import dataclass
import binascii
import struct

HEADER = b"\xa5\x5a"
VERSION = 0x10
MAX_PAYLOAD = 64
STATUS = 0x02
STATUS_REPLY = 0x82
STATUS_LAYOUT = struct.Struct("<IBBBBI BBiIhhBII")
SOURCES = ("invalid", "measured", "command", "estimated", "simulated",
           "absent", "uncalibrated", "stale")


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
    if frame.kind != STATUS_REPLY or len(frame.payload) != STATUS_LAYOUT.size:
        raise ValueError("invalid status response")
    now, profile, state, owner, permit, faults, source, valid, mv, sample_ms, left, right, version, good, bad = (
        STATUS_LAYOUT.unpack(frame.payload)
    )
    if profile not in (1, 2, 3) or source >= len(SOURCES) or valid not in (0, 1):
        raise ValueError("invalid status metadata")
    return dict(time_ms=now, profile=("SIM", "BENCH", "RUN")[profile - 1],
                state=state, owner=owner, motion_permitted=permit, faults=faults,
                bus_mv=mv if valid else None, bus_source=SOURCES[source],
                bus_sample_ms=sample_ms, left_permille=left, right_permille=right,
                config=version, good_frames=good, bad_frames=bad)
