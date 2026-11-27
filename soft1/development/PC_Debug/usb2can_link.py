"""Damiao USB2CAN serial protocol (CDC), implemented from the official sources.

Reference material (gitee.com/kit-miao/damiao -> kit-miao/usb-2-can):
  * 通讯协议/USB 转CAN 帧格式.xlsx
  * 开源上位机/开源例程.7z  (AcTool/can.cs, AcTool/Form1.cs)

PC -> device, 30 bytes:
    55 AA 1E CMD | sendTimes u32 | sendInterval u32 | idType u8 | CANID u32 |
    frameType u8 | dataLen u8 | idAcc u8 | dataAcc u8 | data[8] | CRC8
    CMD 0x01 forwards a frame and reports status, 0x03 forwards silently.

device -> PC, 16 bytes:
    AA CMD (len:6|ide:1|rtr:1) CANID u32 data[8] 55
    CMD 0x11 = received frame.

Four-byte commands: 55 02 AA 55 handshake, 55 03 AA 55 stop sending,
55 05 <index> AA 55 set CAN bitrate (index 0 = 1 Mbit/s).

CRC-8: init 0, LSB first, polynomial 0x8C, computed over all 30 bytes with the
CRC slot left at zero.
"""
from __future__ import annotations

import struct
import time

import serial

TX_LEN = 30
RX_LEN = 16
CMD_TX_FRAME = 0x01
CMD_STOP = 0x03
CMD_HANDSHAKE = 0x02
CMD_RX_FRAME = 0x11
HEADER = bytes([0x55, 0xAA, TX_LEN, 0x00])

SEG_DATA = 7
RX_ID = 0x601
TX_ID = 0x600
FRAME_MAX = 64 + 9


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x01:
                crc = (crc >> 1) ^ 0x8C
            else:
                crc >>= 1
            crc &= 0xFF
    return crc


def build_tx_frame(can_id: int, payload: bytes, command: int = CMD_TX_FRAME) -> bytes:
    if len(payload) > 8:
        raise ValueError("USB2CAN carries at most 8 bytes per frame")
    frame = bytearray(TX_LEN)
    frame[0:4] = bytes([0x55, 0xAA, TX_LEN, command])
    struct.pack_into("<IIBIBBBB", frame, 4,
                     1,              # sendTimes
                     0,              # sendInterval (100 us units)
                     0,              # idType: 0 = standard
                     can_id,
                     0,              # frameType: 0 = data
                     len(payload),
                     0,              # idAcc
                     0)              # dataAcc
    frame[21:21 + len(payload)] = payload
    frame[29] = crc8(bytes(frame))
    return bytes(frame)


class Reassembler:
    """Turns 0x601 CAN frames back into the V1.0 protocol byte stream."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self._total = 0
        self._expected = 0
        self.resyncs = 0

    def feed(self, data: bytes) -> bytes:
        if len(data) < 2:
            return b""
        sequence, payload = data[0], data[1:]
        if sequence != self._expected:
            self.resyncs += 1
            self._buffer.clear()
            self._total = 0
            self._expected = 0
            if sequence != 0:
                return b""
        self._expected = (sequence + 1) & 0xFF
        self._buffer.extend(payload)
        if self._total == 0 and len(self._buffer) >= 5:
            self._total = self._buffer[4] + 9
            if self._total > FRAME_MAX:
                self._buffer.clear()
                self._total = 0
                self._expected = 0
                return b""
        if self._total and len(self._buffer) >= self._total:
            out = bytes(self._buffer[:self._total])
            del self._buffer[:self._total]
            self._total = 0
            self._expected = 0
            return out
        return b""


class Usb2CanLink:
    """Byte-stream bridge over the USB2CAN adapter, same interface as CanLink."""

    def __init__(self, port: str, bitrate_index: int = 0,
                 serial_baud: int = 10_000_000) -> None:
        self._port = serial.Serial(port, serial_baud, timeout=0.05, write_timeout=0.5)
        self._rx = bytearray()
        self._pending = bytearray()
        self._reassembler = Reassembler()
        self.handshake_reply = b""
        self.last_tx_status = None   # 0x12 = bus ACKed, 0x02 = no ACK
        self.tx_ok = 0
        self.tx_fail = 0

        self._flush()
        self._port.write(bytes([0x55, CMD_STOP, 0xAA, 0x55]))   # stop any streaming
        time.sleep(0.1)
        self._flush()

        self._port.write(bytes([0x55, CMD_HANDSHAKE, 0xAA, 0x55]))
        time.sleep(1.0)
        self.handshake_reply = self._port.read(64)
        self._flush()

        self._port.write(bytes([0x55, 0x05, bitrate_index, 0xAA, 0x55]))
        time.sleep(0.05)
        self._flush()

    def _flush(self) -> None:
        self._port.reset_input_buffer()

    def send_frame(self, can_id: int, payload: bytes) -> None:
        self._port.write(build_tx_frame(can_id, payload))

    def write(self, payload: bytes) -> int:
        for index, offset in enumerate(range(0, len(payload), SEG_DATA)):
            chunk = payload[offset:offset + SEG_DATA]
            self.send_frame(TX_ID, bytes([index & 0xFF]) + chunk)
        return len(payload)

    def _drain(self) -> None:
        waiting = self._port.in_waiting
        if waiting:
            self._rx.extend(self._port.read(waiting))
        while len(self._rx) >= RX_LEN:
            start = self._rx.find(0xAA)
            if start < 0:
                self._rx.clear()
                return
            if start:
                del self._rx[:start]
            if len(self._rx) < RX_LEN:
                return
            frame = bytes(self._rx[:RX_LEN])
            if frame[0] != 0xAA or frame[15] != 0x55:
                del self._rx[0]
                continue
            del self._rx[:RX_LEN]
            command = frame[1]
            if command in (0x02, 0x12):
                # The adapter answers every forwarded frame with its bus status;
                # 0x02 means nobody ACKed (wiring / ground / bitrate).
                self.last_tx_status = command
                if command == 0x12:
                    self.tx_ok += 1
                else:
                    self.tx_fail += 1
                continue
            if command != CMD_RX_FRAME:
                continue
            (can_id,) = struct.unpack_from("<I", frame, 3)
            length = frame[2] & 0x3F
            payload = frame[7:7 + min(length, 8)]
            if can_id == RX_ID:
                complete = self._reassembler.feed(payload)
                if complete:
                    self._pending.extend(complete)

    def read(self, capacity: int = 73) -> bytes:
        deadline = time.monotonic() + 0.05
        while len(self._pending) < capacity and time.monotonic() < deadline:
            self._drain()
            if self._pending:
                break
            time.sleep(0.001)
        out = bytes(self._pending[:capacity])
        del self._pending[:capacity]
        return out

    def close(self) -> None:
        self._port.close()
