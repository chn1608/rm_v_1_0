"""Validate Python protocol framing against the shared wire vectors."""
from pathlib import Path
import unittest
from protocol import Decoder, Frame

VECTORS = Path(__file__).resolve().parents[1] / "Protocol/vectors.txt"


class ProtocolVectors(unittest.TestCase):
    def test_gold_frames(self):
        rows = [line.split() for line in VECTORS.read_text().splitlines()
                if line and not line.startswith("#")]
        self.assertGreater(len(rows), 0)
        for command, sequence, payload, wire in rows:
            with self.subTest(command=command, sequence=sequence):
                frame = Frame(int(command, 16), int(sequence, 16),
                              bytes.fromhex(payload) if payload != "-" else b"")
                binary = bytes.fromhex(wire)
                self.assertEqual(frame.encode(), binary)
                d = Decoder()
                self.assertEqual(list(d.feed(binary[:4], 0)), [])
                self.assertEqual(list(d.feed(binary[4:] + binary, 10)), [frame, frame])
                self.assertEqual(list(d.feed(binary[:-1] + bytes([binary[-1] ^ 0x10]), 11)), [])
                self.assertEqual(d.bad, 1)


if __name__ == "__main__":
    unittest.main()
