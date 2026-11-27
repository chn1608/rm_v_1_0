"""End-to-end check of the CAN path without hardware.

It builds the same 36-byte status payload the firmware emits, wraps it in a
V1.0 protocol frame, runs it through the CAN segmentation used on the wire and
decodes it again. This proves the PC side of the link offline; the only thing
left for a bench test is the adapter itself.
"""
import struct
import unittest

from can_link import CanLink, FakeBus, RX_ID, segment
from protocol import Frame, STATUS_REPLY, decode_status, STATUS_LAYOUT


def firmware_status(**over):
    """Mirror of snapshot_encode_status() in Firmware/Service/snapshot.c."""
    fields = dict(layout=1, state=4, owner=3, flags=0x2B,
                  left_cmd=120, right_cmd=-80, left_out=100, right_out=-60,
                  servo_us=1520, servo_decideg=45, battery_mv=14800,
                  current_ma=0, faults=0x00000010, uptime_s=42,
                  good=17, bad=0, loop_us=1800,
                  battery_src=2, current_src=5)
    fields.update(over)
    return STATUS_LAYOUT.pack(
        fields["layout"], fields["state"], fields["owner"], fields["flags"],
        fields["left_cmd"], fields["right_cmd"], fields["left_out"], fields["right_out"],
        fields["servo_us"], fields["servo_decideg"], fields["battery_mv"],
        fields["current_ma"], fields["faults"], fields["uptime_s"],
        fields["good"], fields["bad"], fields["loop_us"],
        fields["battery_src"], fields["current_src"])


class EndToEndTests(unittest.TestCase):
    def test_status_frame_survives_can_round_trip(self):
        payload = firmware_status()
        self.assertEqual(len(payload), 36)
        wire = Frame(STATUS_REPLY, 7, payload).encode()
        self.assertEqual(len(wire), 36 + 9)

        bus = FakeBus()
        link = CanLink(bus, timeout=0.0)
        link.write(wire)
        self.assertEqual(len(bus.sent), 7)          # 45 bytes / 7 per frame

        # Put the frames back as if they came from the board and read the stream.
        for message in segment(wire):
            bus.push(RX_ID, bytes(message.data))
        stream = link.read(73)
        self.assertEqual(stream, wire)

        from protocol import Decoder
        frames = list(Decoder().feed(stream, 0))
        self.assertEqual(len(frames), 1)
        row = decode_status(frames[0])
        self.assertEqual(row["left_cmd"], 120)
        self.assertEqual(row["right_cmd"], -80)
        self.assertEqual(row["bus_mv"], 14800)
        self.assertEqual(row["bus_source"], "command")
        self.assertEqual(row["uptime_s"], 42)
        self.assertEqual(row["faults"], 0x10)
        self.assertTrue(row["motion_permitted"])
        self.assertTrue(row["pc_link"])

    def test_invalid_battery_flag_hides_value(self):
        payload = firmware_status(flags=0x2B & ~0x02)
        row = decode_status(Frame(STATUS_REPLY, 1, payload))
        self.assertIsNone(row["bus_mv"])

    def test_corrupted_crc_is_rejected(self):
        wire = bytearray(Frame(STATUS_REPLY, 3, firmware_status()).encode())
        wire[-1] ^= 0xFF
        from protocol import Decoder
        self.assertEqual(list(Decoder().feed(bytes(wire), 0)), [])


if __name__ == "__main__":
    unittest.main()
