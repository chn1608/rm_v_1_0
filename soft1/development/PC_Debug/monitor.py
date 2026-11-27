"""Read-only MC02 status monitor with serial, C simulation, and binary replay."""
import argparse
from contextlib import ExitStack
import csv
import json
from pathlib import Path
import subprocess
import time

from protocol import Decoder, Frame, STATUS, STATUS_REPLY, decode_status

ROOT = Path(__file__).resolve().parents[3]


def replay(stream, decoder):
    for chunk in iter(lambda: stream.read(4096), b""):
        for frame in decoder.feed(chunk, 0):
            if frame.kind == STATUS_REPLY:
                yield decode_status(frame)


def requests(read, write, decoder, samples, rate, raw=None):
    for i in range(samples):
        start = time.monotonic()
        sequence = i & 0xFFFF
        write(Frame(STATUS, sequence).encode())
        deadline = start + 2.0
        while time.monotonic() < deadline:
            data = read()
            if raw and data:
                raw.write(data)
            answers = list(decoder.feed(data, int(time.monotonic() * 1000)))
            reply = next((f for f in answers if f.kind == STATUS_REPLY and f.sequence == sequence), None)
            if reply is not None:
                yield decode_status(reply)
                break
        else:
            raise TimeoutError(f"No status response for sequence {sequence}")
        if i + 1 < samples:
            time.sleep(max(0.0, rate / 1000 - (time.monotonic() - start)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--sim", action="store_true")
    source.add_argument("--serial", metavar="PORT")
    source.add_argument("--list-ports", action="store_true")
    source.add_argument("--replay", type=Path)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--samples", type=int, default=20)
    p.add_argument("--period-ms", type=int, default=50)
    p.add_argument("--csv", type=Path)
    p.add_argument("--record", type=Path, help="Capture inbound bytes for replay")
    args = p.parse_args()
    if args.samples < 1 or not 20 <= args.period_ms <= 1000:
        p.error("samples >= 1 and 20 <= period-ms <= 1000 required")
    active_paths = [path.resolve() for path in (args.replay, args.csv, args.record) if path]
    if len(set(active_paths)) != len(active_paths) or (args.replay and args.record):
        p.error("input and output paths must be distinct; --record requires a live source")

    decoder = Decoder()
    with ExitStack() as stack:
        if args.serial or args.list_ports:
            try:
                import serial
                from serial.tools import list_ports
            except ImportError:
                p.error("install PySerial in a project virtual environment to use physical ports")
            if args.list_ports:
                for entry in sorted(list_ports.comports()):
                    print(f"{entry.device}\t{entry.description}")
                return
        raw = stack.enter_context(args.record.open("xb")) if args.record else None
        if args.replay:
            results = replay(stack.enter_context(args.replay.open("rb")), decoder)
        elif args.sim:
            exe = ROOT / "build/host/SIM/sim_link.exe"
            if not exe.is_file():
                p.error("build the C simulator first with Tools/build.py host")
            sim = stack.enter_context(subprocess.Popen([exe], stdin=subprocess.PIPE,
                                                        stdout=subprocess.PIPE, bufsize=0))
            # Windows anonymous pipes require a separate reader for a bounded response timeout.
            from queue import Empty, Queue
            from threading import Thread
            inbox = Queue(maxsize=16)
            def pump():
                while (chunk := sim.stdout.read(73)):
                    inbox.put(chunk, timeout=2.0)
            worker = Thread(target=pump, daemon=True)
            worker.start()
            def read():
                try:
                    return inbox.get(timeout=0.1)
                except Empty:
                    return b""
            def write(data):
                sim.stdin.write(data)
                sim.stdin.flush()
            results = requests(read, write, decoder, args.samples, args.period_ms, raw)
        else:
            conn = stack.enter_context(serial.Serial(args.serial, args.baud, timeout=0.05,
                                                      write_timeout=0.5))
            results = requests(lambda: conn.read(73), conn.write, decoder, args.samples,
                               args.period_ms, raw)
        output = stack.enter_context(args.csv.open("x", encoding="utf-8", newline="")) if args.csv else None
        writer = None
        for row in results:
            print(json.dumps(row))
            if output:
                if writer is None:
                    writer = csv.DictWriter(output, fieldnames=row)
                    writer.writeheader()
                writer.writerow(row)
        if args.sim:
            sim.stdin.close()
            if sim.wait(timeout=2.0) != 0:
                raise RuntimeError("SIM transport exited with an error")


if __name__ == "__main__":
    main()
