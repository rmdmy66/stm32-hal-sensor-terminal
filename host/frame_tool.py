#!/usr/bin/env python3
"""Frame protocol reference tool: parser, builder, serial monitor, CSV logger."""

from __future__ import annotations

import argparse
import csv
import struct
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HEAD1 = 0xAA
HEAD2 = 0x55
TAIL1 = 0x0D
TAIL2 = 0x0A
MAX_PAYLOAD = 64
OVERHEAD = 9

EVENT_NONE = "none"
EVENT_OK = "ok"
EVENT_CRC_ERROR = "crc_error"
EVENT_TAIL_ERROR = "tail_error"
EVENT_LENGTH_ERROR = "length_error"

CMD_HEARTBEAT = 0x01
CMD_DATA = 0x02
CMD_SET_PERIOD = 0x03
CMD_DEVICE_INFO = 0x04
CMD_STREAM = 0x05


def crc16_update(crc: int, byte: int) -> int:
    crc ^= byte
    for _ in range(8):
        if crc & 1:
            crc = (crc >> 1) ^ 0xA001
        else:
            crc >>= 1
    return crc & 0xFFFF


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc = crc16_update(crc, byte)
    return crc


def build_frame(cmd: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too large")
    body = bytes([len(payload), cmd & 0xFF, seq & 0xFF]) + payload
    crc = crc16(body)
    return bytes([HEAD1, HEAD2]) + body + struct.pack("<H", crc) + bytes([TAIL1, TAIL2])


@dataclass
class Frame:
    cmd: int
    seq: int
    payload: bytes

    @property
    def len(self) -> int:
        return len(self.payload)


@dataclass
class FrameParser:
    state: str = "head1"
    length: int = 0
    cmd: int = 0
    seq: int = 0
    payload: bytearray = field(default_factory=bytearray)
    crc_calc: int = 0xFFFF
    crc_recv: int = 0
    ok_count: int = 0
    crc_error_count: int = 0
    tail_error_count: int = 0
    length_error_count: int = 0

    def reset(self) -> None:
        self.state = "head1"
        self.length = 0
        self.cmd = 0
        self.seq = 0
        self.payload.clear()
        self.crc_calc = 0xFFFF
        self.crc_recv = 0

    def feed(self, byte: int) -> tuple[str, Frame | None]:
        byte &= 0xFF

        if self.state == "head1":
            if byte == HEAD1:
                self.state = "head2"

        elif self.state == "head2":
            if byte == HEAD2:
                self.state = "len"
            elif byte != HEAD1:
                self.state = "head1"

        elif self.state == "len":
            if byte > MAX_PAYLOAD:
                self.length_error_count += 1
                self.reset()
                return EVENT_LENGTH_ERROR, None
            self.length = byte
            self.crc_calc = crc16_update(0xFFFF, byte)
            self.state = "cmd"

        elif self.state == "cmd":
            self.cmd = byte
            self.crc_calc = crc16_update(self.crc_calc, byte)
            self.state = "seq"

        elif self.state == "seq":
            self.seq = byte
            self.crc_calc = crc16_update(self.crc_calc, byte)
            self.payload.clear()
            self.state = "crc_lo" if self.length == 0 else "payload"

        elif self.state == "payload":
            self.payload.append(byte)
            self.crc_calc = crc16_update(self.crc_calc, byte)
            if len(self.payload) >= self.length:
                self.state = "crc_lo"

        elif self.state == "crc_lo":
            self.crc_recv = byte
            self.state = "crc_hi"

        elif self.state == "crc_hi":
            self.crc_recv |= byte << 8
            if self.crc_recv != self.crc_calc:
                self.crc_error_count += 1
                self.reset()
                return EVENT_CRC_ERROR, None
            self.state = "tail1"

        elif self.state == "tail1":
            if byte != TAIL1:
                self.tail_error_count += 1
                self.reset()
                return EVENT_TAIL_ERROR, None
            self.state = "tail2"

        elif self.state == "tail2":
            if byte != TAIL2:
                self.tail_error_count += 1
                self.reset()
                return EVENT_TAIL_ERROR, None
            frame = Frame(self.cmd, self.seq, bytes(self.payload))
            self.ok_count += 1
            self.reset()
            return EVENT_OK, frame

        return EVENT_NONE, None

    def feed_bytes(self, data: bytes):
        for byte in data:
            event, frame = self.feed(byte)
            if event != EVENT_NONE:
                yield event, frame


def decode_data(frame: Frame) -> dict | None:
    if frame.cmd != CMD_DATA or len(frame.payload) != 12:
        return None
    tick, adc_mv, temp_x100, humi_x100, flags = struct.unpack("<IhHHH", frame.payload)
    return {
        "tick_ms": tick,
        "adc_mv": adc_mv,
        "temp_c": temp_x100 / 100.0,
        "humi_pct": humi_x100 / 100.0,
        "flags": flags,
    }


def make_data_payload(tick_ms: int, adc_mv: int, temp_c: float, humi_pct: float, flags: int = 1) -> bytes:
    return struct.pack(
        "<IhHHH",
        tick_ms & 0xFFFFFFFF,
        int(round(adc_mv)),
        int(round(temp_c * 100)),
        int(round(humi_pct * 100)),
        flags,
    )


def run_self_test() -> int:
    failures = 0

    def check(cond: bool, msg: str) -> None:
        nonlocal failures
        if not cond:
            print(f"FAIL: {msg}")
            failures += 1

    check(crc16(b"123456789") == 0x4B37, "CRC known vector")

    payload = bytes([0x11, 0x22, 0x33])
    raw = build_frame(0x02, 0x07, payload)
    check(len(raw) == OVERHEAD + len(payload), "build length")
    parser = FrameParser()
    events = list(parser.feed_bytes(raw))
    check(len(events) == 1 and events[0][0] == EVENT_OK, "round trip event")
    if events and events[0][1] is not None:
        frame = events[0][1]
        check(frame.cmd == 0x02 and frame.seq == 0x07 and frame.payload == payload, "round trip fields")

    parser = FrameParser()
    events = list(parser.feed_bytes(bytes([0x00, 0xAA, 0x00, 0xAA]) + build_frame(0x01, 0x01)))
    check(len(events) == 1 and events[0][0] == EVENT_OK, "resync after garbage")

    bad = bytearray(build_frame(0x02, 0x03, bytes([0x10, 0x20])))
    bad[5] ^= 0xFF
    parser = FrameParser()
    events = list(parser.feed_bytes(bytes(bad)))
    check(any(event == EVENT_CRC_ERROR for event, _ in events), "bad crc event")
    check(parser.crc_error_count == 1, "bad crc counter")

    bad_tail = bytearray(build_frame(0x02, 0x04, bytes([0x10])))
    bad_tail[-1] = 0x00
    parser = FrameParser()
    events = list(parser.feed_bytes(bytes(bad_tail)))
    check(any(event == EVENT_TAIL_ERROR for event, _ in events), "bad tail event")
    check(parser.tail_error_count == 1, "bad tail counter")

    parser = FrameParser()
    events = list(parser.feed_bytes(bytes([HEAD1, HEAD2, 0x41])))
    check(any(event == EVENT_LENGTH_ERROR for event, _ in events), "length error event")
    check(parser.length_error_count == 1, "length error counter")

    parser = FrameParser()
    events = list(parser.feed_bytes(build_frame(0x01, 0x01)))
    check(len(events) == 1 and events[0][0] == EVENT_OK, "example frame")

    if failures == 0:
        print("ALL TESTS PASSED")
        return 0
    print(f"{failures} TEST(S) FAILED")
    return 1


def run_demo() -> int:
    payload = make_data_payload(123456, 1650, 23.45, 51.20, 1)
    raw = build_frame(CMD_DATA, 1, payload)
    print("frame:", raw.hex(" ").upper())
    parser = FrameParser()
    for event, frame in parser.feed_bytes(raw):
        print("event:", event)
        if frame is not None:
            print("cmd:", hex(frame.cmd), "seq:", frame.seq, "payload:", frame.payload.hex(" "))
            print("decoded:", decode_data(frame))
    return 0


def run_serial(port: str, baud: int, csv_path: Path, plot: bool) -> int:
    try:
        import serial
    except ImportError:
        print("pyserial is not installed. Run: python -m pip install pyserial")
        return 1

    parser = FrameParser()
    rows = []
    csv_file = None
    writer = None
    if csv_path:
        csv_file = csv_path.open("w", newline="", encoding="utf-8")
        writer = csv.writer(csv_file)
        writer.writerow(["pc_time", "cmd", "seq", "len", "payload_hex", "temp_c", "humi_pct", "adc_mv", "flags"])

    plt = None
    fig = None
    ax_temp = None
    ax_adc = None
    if plot:
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            print("matplotlib is not installed. Run: python -m pip install matplotlib")
            plot = False

    if plot:
        plt.ion()
        fig, (ax_temp, ax_adc) = plt.subplots(2, 1, sharex=True)
        ax_temp.set_ylabel("temp (C)")
        ax_adc.set_ylabel("adc (mV)")
        ax_adc.set_xlabel("sample")

    try:
        with serial.Serial(port, baud, timeout=0.1) as ser:
            print(f"listening on {port} @ {baud}")
            while True:
                chunk = ser.read(256)
                if not chunk:
                    continue
                for event, frame in parser.feed_bytes(chunk):
                    if event != EVENT_OK or frame is None:
                        print("event:", event)
                        continue
                    decoded = decode_data(frame)
                    payload_hex = frame.payload.hex(" ")
                    if decoded is None:
                        print(f"cmd=0x{frame.cmd:02X} seq={frame.seq} len={frame.len} payload={payload_hex}")
                    else:
                        print(
                            f"tick={decoded['tick_ms']} adc={decoded['adc_mv']}mV "
                            f"temp={decoded['temp_c']:.2f}C humi={decoded['humi_pct']:.2f}% "
                            f"flags=0x{decoded['flags']:04X}"
                        )
                        rows.append(decoded)
                    if writer is not None:
                        writer.writerow([
                            time.time(),
                            f"0x{frame.cmd:02X}",
                            frame.seq,
                            frame.len,
                            payload_hex,
                            "" if decoded is None else f"{decoded['temp_c']:.2f}",
                            "" if decoded is None else f"{decoded['humi_pct']:.2f}",
                            "" if decoded is None else decoded["adc_mv"],
                            "" if decoded is None else decoded["flags"],
                        ])
                        csv_file.flush()
                    if plot and decoded is not None:
                        ax_temp.clear()
                        ax_adc.clear()
                        ax_temp.plot([r["temp_c"] for r in rows[-200:]])
                        ax_adc.plot([r["adc_mv"] for r in rows[-200:]])
                        ax_temp.set_ylabel("temp (C)")
                        ax_adc.set_ylabel("adc (mV)")
                        fig.canvas.draw_idle()
                        fig.canvas.flush_events()
    except KeyboardInterrupt:
        print("stopped")
    finally:
        if csv_file is not None:
            csv_file.close()

    if plot:
        plt.ioff()
        plt.show()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Frame protocol tool")
    parser.add_argument("--self-test", action="store_true", help="run built-in tests")
    parser.add_argument("--demo", action="store_true", help="build and parse a demo frame")
    parser.add_argument("--port", help="serial port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--csv", type=Path, help="write decoded data to CSV")
    parser.add_argument("--plot", action="store_true", help="show live plots")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.demo:
        return run_demo()
    if args.port:
        return run_serial(args.port, args.baud, args.csv, args.plot)

    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
