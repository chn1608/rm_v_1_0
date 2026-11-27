"""Unit tests for the CAN segmentation used by the PC tool.

They run without hardware through FakeBus, and mirror the firmware's
segmentation rules (7 payload bytes per frame, sequence starts at 0).
"""
import unittest

from can_link import CanLink, FakeBus, Reassembler, RX_ID, TX_ID, segment

from protocol import Frame, STATUS, encode, decode_status


class SegmentTests(unittest.TestCase):
    def test_short_frame_fits_one_can_message(self):
        payload = bytes(range(9))          # 9-byte protocol frame (0 payload)
        frames = segment(payload)
        self.assertEqual(len(frames), 2)   # 7 + 2
        self.assertEqual(frames[0].arbitration_id, TX_ID)
        self.assertEqual(frames[0].data[0], 0)
        self.assertEqual(bytes(frames[0].data[1:]), payload[:7])
        self.assertEqual(frames[1].data[0], 1)
        self.assertEqual(bytes(frames[1].data[1:3]), payload[7:9])

    def test_max_frame_needs_eleven_messages(self):
        payload = bytes(73)
        self.assertEqual(len(segment(payload)), 11)

    def test_segment_rejects_empty(self):
        with self.assertRaises(Exception):
            segment(b"")

    def test_reassembly_round_trip(self):
        payload = bytes(range(36))
        reassembler = Reassembler()
        out = b""
        for message in segment(payload):
            out += reassembler.feed(message)
        self.assertEqual(out, payload)

    def test_sequence_gap_drops_partial(self):
        payload = bytes(30)
        frames = segment(payload)
        reassembler = Reassembler()
        self.assertEqual(reassembler.feed(frames[0]), b"")
        self.assertEqual(reassembler.feed(frames[2]), b"")   # gap
        self.assertEqual(reassembler.resyncs, 1)
        # After the gap the stream restarts cleanly.
        out = b"".join(reassembler.feed(m) for m in segment(payload))
        self.assertEqual(out, payload)


class LinkTests(unittest.TestCase):
    def test_write_segments_and_read_reassembles(self):
        local, remote = FakeBus(), FakeBus()
        link = CanLink(local, timeout=0.0)

        status = Frame(STATUS, 0, b"\x01\x02\x03")
        wire = encode(status)
        self.assertEqual(link.write(wire), len(wire))
        self.assertEqual(len(local.sent), 2)

        # Feed the board's reply into the link and read it back.
        for message in segment(wire):
            remote.push(RX_ID, bytes(message.data))
        # The link reads from its own bus, so bridge: copy inbox into local.
        local._inbox.extend(remote._inbox)
        self.assertEqual(link.read(64), wire)

    def test_ignores_other_ids(self):
        bus = FakeBus()
        link = CanLink(bus, timeout=0.0)
        bus.push(0x123, bytes([0, 1, 2, 3, 4, 5, 6, 7]))
        self.assertEqual(link.read(64), b"")


if __name__ == "__main__":
    unittest.main()
