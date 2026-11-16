"""Update Coral-C5 over HC-08 BLE; no J-Link is used by this tool."""

import argparse
import asyncio
from pathlib import Path
import struct
import sys
import time
import zlib

from ota_protocol import (ABORT, APP_ADDR, BEGIN, BOARD_ID, CHUNK, DATA, END,
                          HELLO, IMAGE_MAX, RUN, STATUS, Frame, Parser, read_image)

DEFAULT_BLE = "F8:2E:0C:F3:68:37"
DEFAULT_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"


class Link:
    def __init__(self, client, characteristic):
        self.client = client
        self.characteristic = characteristic
        self.parser = Parser()
        self.responses = asyncio.Queue(maxsize=64)
        self.lines = asyncio.Queue(maxsize=64)
        self.text = bytearray()
        self.sequence = 0
        self.retries = 0
        self.received = 0

    @staticmethod
    def enqueue(queue, value):
        if queue.full():
            queue.get_nowait()
        queue.put_nowait(value)

    def notify(self, _sender, data):
        for frame in self.parser.feed(data):
            self.enqueue(self.responses, frame)
        for byte in data:
            if byte == 10:
                self.enqueue(self.lines, self.text.decode("ascii", errors="ignore").strip())
                self.text.clear()
            elif byte in (13,) or 32 <= byte < 127:
                self.text.append(byte)
                if len(self.text) > 512:
                    self.text.clear()
            else:
                self.text.clear()

    async def write(self, data):
        # HC-08 firmware uses a 20-byte BLE payload and a 57600-baud UART bridge.
        for offset in range(0, len(data), 20):
            await self.client.write_gatt_char(self.characteristic, data[offset:offset + 20],
                                              response=False)
            await asyncio.sleep(0.020)

    async def request(self, command, payload=b"", timeout=3, attempts=4):
        self.sequence += 1
        frame = Frame(command, self.sequence, payload)
        for attempt in range(attempts):
            if attempt:
                self.retries += 1
                print(f"Retry command {command}, sequence {frame.sequence}", flush=True)
            await self.write(frame.encode())
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                try:
                    response = await asyncio.wait_for(self.responses.get(), deadline - time.monotonic())
                except asyncio.TimeoutError:
                    break
                if response.command != command | 0x80 or response.sequence != frame.sequence:
                    continue
                if len(response.payload) < 8:
                    raise RuntimeError("Truncated bootloader reply")
                status = struct.unpack_from("<I", response.payload)[0]
                if status:
                    name = STATUS[status] if status < len(STATUS) else str(status)
                    raise RuntimeError(f"Bootloader rejected command {command}: {name}")
                return response.payload
        raise TimeoutError(f"No reply to command {command}; keep power on and retry, "
                           "or hold TASK while powering on to enter recovery")

    async def enter(self):
        await self.write(b"!\nC5:OTA\n")
        await asyncio.sleep(2)
        reply = await self.request(HELLO)
        if len(reply) != 32:
            raise RuntimeError("Unsupported bootloader HELLO reply")
        _, offset, board, address, limit, chunk, old_size, old_crc = struct.unpack("<8I", reply)
        if (board, address, limit, chunk) != (BOARD_ID, APP_ADDR, IMAGE_MAX, CHUNK):
            raise RuntimeError("Board or partition contract does not match this uploader")
        self.received = offset
        print(f"Bootloader ready; current app {old_size} bytes CRC32={old_crc:08X}; "
              f"received={offset}", flush=True)
        return old_size, old_crc

    async def verify_app(self, size, crc, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            await self.write(b"\nC5:INFO\n")
            stop = min(deadline, time.monotonic() + 1.5)
            while time.monotonic() < stop:
                try:
                    line = await asyncio.wait_for(self.lines.get(), stop - time.monotonic())
                except asyncio.TimeoutError:
                    break
                if not line.startswith("C5APP,1,"):
                    continue
                parts = line.split(",")
                if len(parts) != 4:
                    continue
                if (int(parts[3]), int(parts[2], 16)) != (size, crc):
                    raise RuntimeError(f"Application identity mismatch: {line}")
                print(f"Verified running application: {size} bytes, CRC32={crc:08X}", flush=True)
                return
        raise TimeoutError("Application confirmation missing; image may already be installed. "
                           "Reconnect and use --info before retrying.")

    async def info(self):
        await self.write(b"\nC5:INFO\n")
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            try:
                line = await asyncio.wait_for(self.lines.get(), deadline - time.monotonic())
            except asyncio.TimeoutError:
                break
            if line.startswith("C5APP,1,"):
                print(line, flush=True)
                return
        reply = await self.request(HELLO, attempts=2)
        print("Bootloader: " + ",".join(f"{word:08X}" for word in struct.unpack("<8I", reply)))

    async def upload(self, image, header):
        started = time.monotonic()
        old_size, old_crc = await self.enter()
        if not self.received and (old_size, old_crc) == (len(image), zlib.crc32(image)):
            print("Requested image is already installed; returning to application", flush=True)
            await self.request(RUN, attempts=1)
            await asyncio.sleep(2)
            await self.verify_app(old_size, old_crc)
            return
        reply = await self.request(BEGIN, header)
        offset = struct.unpack_from("<I", reply, 4)[0]
        if offset > len(image) or (offset % CHUNK and offset != len(image)):
            raise RuntimeError("Invalid resume offset")
        report = -1
        while offset < len(image):
            chunk = image[offset:offset + CHUNK]
            reply = await self.request(DATA, struct.pack("<I", offset) + chunk)
            received = struct.unpack_from("<I", reply, 4)[0]
            if received != offset + len(chunk):
                raise RuntimeError(f"Unexpected ACK offset {received}, expected {offset + len(chunk)}")
            offset = received
            percent = offset * 100 // len(image)
            if percent // 10 != report:
                report = percent // 10
                print(f"{percent:3d}%  {offset}/{len(image)} bytes", flush=True)
        await self.request(END)
        print("Image verified in staging; installing...", flush=True)
        try:
            await self.request(RUN, attempts=1)
        except TimeoutError:
            # Reset may have reached the module before its final ACK was delivered.
            pass
        await asyncio.sleep(2)
        await self.verify_app(len(image), zlib.crc32(image))
        print(f"Wireless update complete in {time.monotonic() - started:.1f}s; "
              f"retries={self.retries}; motors remain stopped", flush=True)


async def run(args):
    try:
        from bleak import BleakClient, BleakScanner
        from bleak.exc import BleakError
    except ImportError as exc:
        raise RuntimeError("Install BLE support: python -m pip install -r tools/requirements.txt") from exc
    image, header = read_image(args.image) if args.image else (None, None)
    for attempt in range(3):
        try:
            device = await BleakScanner.find_device_by_address(args.ble, timeout=15)
            if device is None:
                raise TimeoutError("HC-08 not found; power the board and close the telemetry monitor")
            async with BleakClient(device, timeout=15) as client:
                characteristic = client.services.get_characteristic(args.uuid)
                if characteristic is None:
                    raise RuntimeError("HC-08 FFE1 characteristic not found")
                link = Link(client, characteristic)
                await client.start_notify(characteristic, link.notify)
                # Give the UART bridge time to settle after establishing the BLE connection.
                await asyncio.sleep(0.5)
                if args.info:
                    await link.info()
                elif args.exit_recovery:
                    size, crc = await link.enter()
                    if not size:
                        raise RuntimeError("No valid application; upload a firmware image")
                    await link.request(ABORT)
                    await link.request(RUN, attempts=1)
                    await asyncio.sleep(2)
                    await link.verify_app(size, crc)
                else:
                    await link.upload(image, header)
                return
        except (BleakError, OSError, TimeoutError) as exc:
            if attempt == 2:
                raise
            print(f"BLE connection interrupted ({exc}); reconnecting {attempt + 1}/2...", flush=True)
            await asyncio.sleep(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", nargs="?", type=Path, help="coral_c5.ota or coral_c5.bin")
    parser.add_argument("--ble", default=DEFAULT_BLE, help="HC-08 MAC address")
    parser.add_argument("--uuid", default=DEFAULT_UUID)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--info", action="store_true", help="read the installed image identity")
    mode.add_argument("--exit-recovery", action="store_true", help="abort an incomplete transfer and boot old app")
    args = parser.parse_args()
    if not (args.info or args.exit_recovery) and args.image is None:
        parser.error("provide an application .ota/.bin file")
    if args.image and (args.info or args.exit_recovery):
        parser.error("image cannot be combined with --info/--exit-recovery")
    asyncio.run(run(args))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Upload interrupted. Re-run to retry; --exit-recovery boots the previous app.", file=sys.stderr)
        sys.exit(130)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
