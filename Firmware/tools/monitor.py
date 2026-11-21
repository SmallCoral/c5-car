"""Receive Coral-C5 version 1 through 6 telemetry over HC-08 BLE or a wired UART."""

import argparse
import asyncio
import csv
from datetime import datetime, timezone
from pathlib import Path
import sys


FIELDS = (
    "tag", "version", "ms", "seq", "task", "running", "duty_permille",
    "left_count", "right_count", "left_cps", "right_cps", "gray_raw",
    "encoder_ab", "fault_mask", "enable_request", "step_online", "step_phase",
    "step_flags", "step_position_raw", "step_error", "uart_errors", "rx_drops",
    "tx_drops", "lcd_ok", "wheel_trip", "step_timeouts", "step_position_valid",
    "step_target_rpm", "step_rpm", "step_rpm_valid",
    "gray_black", "gray_line", "gray_count", "gray_groups", "gray_position",
    "gray_status", "gray_white",
    "lap_status", "lap_left_pwm", "lap_right_pwm", "lap_left_target", "lap_right_target",
    "lap_left_cps", "lap_right_cps", "lap_ms", "lap_counts", "lap_armed",
    "lap_position", "lap_position_rate", "lap_steering", "lap_differential",
    "lap_base_target", "lap_control_ms",
    "lap_departing", "lap_marker_hits", "lap_marker_peak",
)
TASKS = ("MONITOR", "LEFT", "RIGHT", "BOTH", "STEPPER", "ALL", "GRAY", "LAP", "BALANCE")
GRAY_STATUS = ("WAIT", "NO LINE", "LINE", "ALL", "MULTI")
LAP_STATUS = ("READY", "FOLLOW", "DONE", "STOPPED", "LOST LINE", "BAD LINE",
              "ENC ERROR", "TIMEOUT", "DRIVE ERR", "ALIGN LINE", "LOOP LATE", "DIR CHECK", "NO A MARK")
VERSION_LENGTHS = {1: 27, 2: 30, 3: 37, 4: 47, 5: 53, 6: len(FIELDS)}
BOOLEAN_FIELDS = (
    "running", "enable_request", "step_online", "lcd_ok",
    "step_position_valid", "step_rpm_valid", "gray_white", "lap_armed", "lap_departing",
)
DEFAULT_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"


class Decoder:
    """BLE notifications are arbitrary fragments, not complete CSV records."""

    def __init__(self):
        self.buffer = bytearray()
        self.invalid = 0

    def feed(self, data):
        self.buffer.extend(data)
        records = []
        while b"\n" in self.buffer:
            line, _, self.buffer = self.buffer.partition(b"\n")
            try:
                values = line.decode("ascii").strip().split(",")
                version = int(values[1]) if len(values) > 1 else 0
                if values[0] != "C5" or len(values) != VERSION_LENGTHS.get(version):
                    raise ValueError("invalid record header")
                values.extend("0" for _ in range(len(FIELDS) - len(values)))
                record = dict(zip(FIELDS[2:], map(int, values[2:])))
                record["version"] = int(values[1])
                if not 0 <= record["task"] < len(TASKS):
                    raise ValueError("invalid task")
                if not 0 <= record["gray_status"] < len(GRAY_STATUS):
                    raise ValueError("invalid grayscale status")
                if not 0 <= record["lap_status"] < len(LAP_STATUS):
                    raise ValueError("invalid lap status")
                # Lost BLE fragments can splice two rows into a valid column
                # count. Reject impossible flags instead of charting them.
                if any(record[field] not in (0, 1) for field in BOOLEAN_FIELDS):
                    raise ValueError("invalid boolean field")
                records.append(record)
            except (ValueError, UnicodeError):
                self.invalid += 1
        if len(self.buffer) > 1024:
            self.buffer.clear()
            self.invalid += 1
        return records


class Sink:
    def __init__(self, output):
        self.decoder = Decoder()
        self.output = output
        self.writer = csv.writer(output)
        self.writer.writerow(("received_utc", *FIELDS))
        self.last_seq = None
        self.last_ms = None
        self.count = 0
        self.bytes_received = 0

    def accept(self, data):
        self.bytes_received += len(data)
        for record in self.decoder.feed(data):
            sequence = record["seq"]
            if self.last_seq is not None:
                gap = (sequence - self.last_seq - 1) & 0xffffffff
                if record["ms"] < self.last_ms and sequence < self.last_seq:
                    print("MCU restarted or counters wrapped", file=sys.stderr)
                elif gap:
                    print(f"Telemetry gap: {gap} frame(s)", file=sys.stderr)
            self.last_seq = sequence
            self.last_ms = record["ms"]
            self.count += 1
            self.writer.writerow((
                datetime.now(timezone.utc).isoformat(), "C5", record["version"],
                *(record[field] for field in FIELDS[2:]),
            ))
            self.output.flush()
            print(
                f'{record["ms"]:9} ms  {TASKS[record["task"]]:7} '
                f'run={record["running"]}  '
                f'L={record["left_count"]:7} ({record["left_cps"]:+6}/s)  '
                f'R={record["right_count"]:7} ({record["right_cps"]:+6}/s)  '
                f'gray(CH1..8)={"".join(str((record["gray_raw"] >> ch) & 1) for ch in range(8))} '
                f'{GRAY_STATUS[record["gray_status"]]} '
                f'pos={record["gray_position"] if record["gray_status"] == 2 else "--"}  '
                f'lap={LAP_STATUS[record["lap_status"]]} '
                f'pwm={record["lap_left_pwm"]}/{record["lap_right_pwm"]} '
                f'fault={record["fault_mask"]} '
                f'emm={record["step_online"]}/{record["step_error"]:02X} '
                f'rpm={record["step_rpm"] if record["step_rpm_valid"] else "?"}'
            )


async def receive_ble(args, sink):
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as exc:
        raise RuntimeError("Install BLE support: python -m pip install bleak") from exc

    if args.scan:
        devices = await BleakScanner.discover(timeout=8)
        for device in devices:
            print(f"{device.address}  {device.name or '(unnamed)'}")
        return

    device = await BleakScanner.find_device_by_address(args.ble, timeout=15)
    if device is None:
        raise RuntimeError("BLE device not found; check power and close other BLE clients")
    disconnected = asyncio.Event()
    async with BleakClient(device, disconnected_callback=lambda _: disconnected.set()) as client:
        characteristic = client.services.get_characteristic(args.uuid)
        if characteristic is None:
            raise RuntimeError(f"Characteristic {args.uuid} is absent; inspect module UUID settings")
        await client.start_notify(characteristic, lambda _, data: sink.accept(data))
        if args.stop or args.gray or args.lap:
            command = b"!\nC5:GRAY\n" if args.gray else b"!\nC5:LAP\n" if args.lap else b"!"
            await client.write_gatt_char(characteristic, command,
                                         response=False)
        print("BLE connected; receiving. Ctrl+C exits. J10 state is not sensed.")
        try:
            await asyncio.wait_for(disconnected.wait(), timeout=args.seconds or None)
        except asyncio.TimeoutError:
            pass
        if disconnected.is_set():
            raise RuntimeError("BLE disconnected")
        if not sink.count:
            raise RuntimeError("Connected but no valid C5 frames received; check MCU firmware and UART baud")


def receive_serial(args, sink):
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("Install UART support: python -m pip install pyserial") from exc
    import time

    deadline = time.monotonic() + args.seconds if args.seconds else None
    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        if args.stop or args.gray or args.lap:
            port.write(b"!\nC5:GRAY\n" if args.gray else b"!\nC5:LAP\n" if args.lap else b"!")
        while deadline is None or time.monotonic() < deadline:
            sink.accept(port.read(256))
    if not sink.count:
        raise RuntimeError("No valid C5 frames received")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--scan", action="store_true", help="list nearby BLE devices")
    mode.add_argument("--ble", metavar="ADDRESS", help="HC-08 address from --scan")
    mode.add_argument("--port", help="wired UART port, e.g. COM3 (not a BLE virtual port)")
    mode.add_argument("--replay", type=Path, help="decode a raw telemetry capture offline")
    parser.add_argument("--uuid", default=DEFAULT_UUID)
    parser.add_argument("--baud", type=int, default=57600)
    parser.add_argument("--seconds", type=float, default=0, help="0 listens until Ctrl+C")
    parser.add_argument("--stop", action="store_true", help="send one stop byte after connecting")
    page = parser.add_mutually_exclusive_group()
    page.add_argument("--gray", action="store_true", help="stop motors and select the GRAY page (v3 firmware)")
    page.add_argument("--lap", action="store_true", help="stop motors and select LAP; physical RUN starts (v4)")
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()
    if args.seconds < 0 or args.baud <= 0:
        parser.error("seconds must be nonnegative; baud must be positive")
    if args.scan:
        asyncio.run(receive_ble(args, None))
        return
    output = args.out or Path(datetime.now().strftime("c5-%Y%m%d-%H%M%S.csv"))
    # Exclusive creation prevents an accidental overwrite of a prior capture.
    with output.open("x", newline="", encoding="utf-8") as stream:
        sink = Sink(stream)
        try:
            if args.ble:
                asyncio.run(receive_ble(args, sink))
            elif args.port:
                receive_serial(args, sink)
            else:
                with args.replay.open("rb") as source:
                    while data := source.read(256):
                        sink.accept(data)
        finally:
            print(f"Saved {sink.count} frame(s) to {output}; "
                  f"received {sink.bytes_received} byte(s); rejected {sink.decoder.invalid}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
