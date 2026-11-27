"""CAN byte-stream transport for the MC02 PC tool.

The board replaced its USART1 "USB 转串口" status link with a Damiao USB2CANFD
adapter on CAN1. Classic CAN carries 8 bytes, so the application protocol frame
is segmented exactly like the firmware does:

    data[0]      segment index (0, 1, 2, ...)
    data[1..7]   up to seven payload bytes

The receiver learns the total length from the protocol header (byte 4) once the
first five bytes have arrived, so no length field is added on the wire.

    MCU -> PC : 11-bit id 0x601
    PC  -> MCU: 11-bit id 0x600

`FakeBus` implements the same interface without hardware so the segmentation
and the protocol can be tested on any machine.
"""
from __future__ import annotations

import can

SEG_DATA = 7
RX_ID = 0x601
TX_ID = 0x600
FRAME_MAX = 64 + 9


class SegmentError(RuntimeError):
    pass


class Reassembler:
    """Turns received CAN frames back into the protocol byte stream."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self._expected_seq = 0
        self._total = 0
        self.overflow = 0
        self.resyncs = 0

    def feed(self, frame: can.Message) -> bytes:
        """Feed one frame; returns the complete protocol frame when it closes."""
        data = bytes(frame.data)
        if len(data) < 2:
            return b""
        sequence, payload = data[0], data[1:]
        if sequence != self._expected_seq:
            # A lost or reordered segment leaves a half frame. Drop it and wait
            # for a segment 0 before trusting the stream again, otherwise the
            # middle of the old frame would be parsed as a new header.
            self.resyncs += 1
            self._buffer.clear()
            self._total = 0
            self._expected_seq = 0
            if sequence != 0:
                return b""
        self._expected_seq = (sequence + 1) & 0xFF
        self._buffer.extend(payload)

        if self._total == 0 and len(self._buffer) >= 5:
            self._total = self._buffer[4] + 9
            if self._total > FRAME_MAX:
                self.overflow += 1
                self._buffer.clear()
                self._total = 0
                self._expected_seq = 0
                return b""
        if self._total and len(self._buffer) >= self._total:
            out = bytes(self._buffer[: self._total])
            del self._buffer[: self._total]
            self._total = 0
            self._expected_seq = 0
            return out
        return b""


def segment(payload: bytes) -> list[can.Message]:
    """Split one protocol frame into CAN messages for TX_ID."""
    if not payload:
        raise SegmentError("empty payload")
    frames = []
    for index, offset in enumerate(range(0, len(payload), SEG_DATA)):
        chunk = payload[offset : offset + SEG_DATA]
        data = bytes([index & 0xFF]) + chunk
        frames.append(can.Message(arbitration_id=TX_ID, is_extended_id=False, data=data))
    return frames


class CanLink:
    """Minimal read/write byte transport matching the firmware's Transport."""

    def __init__(self, bus: can.BusABC, timeout: float = 0.05) -> None:
        self._bus = bus
        self._timeout = timeout
        self._pending = bytearray()
        self._reassembler = Reassembler()

    def write(self, payload: bytes) -> int:
        for message in segment(payload):
            self._bus.send(message)
        return len(payload)

    def read(self, capacity: int = 64) -> bytes:
        while len(self._pending) < capacity:
            message = self._bus.recv(timeout=self._timeout)
            if message is None:
                break
            if message.is_extended_id or message.arbitration_id != RX_ID:
                continue
            complete = self._reassembler.feed(message)
            if complete:
                self._pending.extend(complete)
                break
        out = bytes(self._pending[:capacity])
        del self._pending[:capacity]
        return out

    def close(self) -> None:
        self._bus.shutdown()


def open_bus(interface: str, channel: str, bitrate: int) -> can.BusABC:
    """Open a Damiao USB2CANFD adapter.

    interface="slcan"  -> module flashed with the SLCAN firmware (COM port)
    interface="gs_usb" -> module flashed with the gs_usb firmware (WinUSB driver)
    """
    return can.Bus(interface=interface, channel=channel, bitrate=bitrate)


class FakeBus(can.BusABC):
    """In-memory loopback used by the unit tests."""

    def __init__(self) -> None:  # noqa: D107 - test helper
        self.sent: list[can.Message] = []
        self._inbox: list[can.Message] = []

    def send(self, msg, timeout=None):  # noqa: ANN001, D102
        self.sent.append(can.Message(arbitration_id=msg.arbitration_id,
                                     is_extended_id=msg.is_extended_id,
                                     data=bytes(msg.data)))

    def recv(self, timeout=None):  # noqa: ANN001, D102
        return self._inbox.pop(0) if self._inbox else None

    def push(self, arbitration_id: int, data: bytes) -> None:
        self._inbox.append(can.Message(arbitration_id=arbitration_id,
                                       is_extended_id=False, data=data))

    def shutdown(self) -> None:  # noqa: D102
        pass
