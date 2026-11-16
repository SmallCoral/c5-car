import asyncio
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ota_protocol import APP_ADDR, BOARD_ID, DATA, Frame, Parser, image_header, read_image
from package_firmware import make_hex
from wireless_flash import DEFAULT_BLE, DEFAULT_UUID, Link, run


class ImageTests(unittest.TestCase):
    def setUp(self):
        self.image = bytearray(range(256)) * 4
        struct.pack_into("<II", self.image, 0, 0x2003FFE0, APP_ADDR + 513)

    def test_package_integrity(self):
        header = image_header(self.image)
        self.assertEqual(struct.unpack_from("<I", header, 8)[0], BOARD_ID)
        self.assertEqual(zlib.crc32(header[:28]), struct.unpack_from("<I", header, 28)[0])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "app.ota"
            path.write_bytes(header + self.image)
            self.assertEqual(read_image(path), (self.image, header))
            self.image[700] ^= 1
            path.write_bytes(header + self.image)
            with self.assertRaises(ValueError):
                read_image(path)

    def test_old_link_address_rejected(self):
        struct.pack_into("<I", self.image, 4, 0x08000201)
        with self.assertRaises(ValueError):
            image_header(self.image)

    def test_hex_addresses_and_checksum(self):
        segments = [(APP_ADDR, self.image), (0x080FC000, image_header(self.image))]
        decoded = {}
        upper = 0
        for line in make_hex(segments).splitlines():
            record = bytes.fromhex(line[1:])
            self.assertEqual(sum(record) % 256, 0)
            if record[3] == 4:
                upper = int.from_bytes(record[4:-1], "big") << 16
            elif record[3] == 0:
                address = upper + int.from_bytes(record[1:3], "big")
                for i, byte in enumerate(record[4:-1]):
                    decoded[address + i] = byte
        for address, data in segments:
            self.assertEqual(bytes(decoded[address + i] for i in range(len(data))), data)

    def test_fragmented_frames_and_resync(self):
        frame = Frame(DATA, 0x12345678, bytes(range(132)))
        encoded = frame.encode()
        for split in range(len(encoded) + 1):
            parser = Parser()
            result = list(parser.feed(b"C5,2,telemetry\r\n" + encoded[:split]))
            result += list(parser.feed(encoded[split:]))
            self.assertEqual(result, [frame])
        corrupt = bytearray(encoded)
        corrupt[40] ^= 1
        parser = Parser()
        self.assertEqual(list(parser.feed(corrupt + encoded + encoded)), [frame, frame])


class RetryTests(unittest.IsolatedAsyncioTestCase):
    async def test_already_installed_does_not_erase_or_upload(self):
        image = b"verified image"
        link = Link(None, None)
        link.enter = AsyncMock(return_value=(len(image), zlib.crc32(image)))
        link.request = AsyncMock()
        link.verify_app = AsyncMock()
        with patch("wireless_flash.asyncio.sleep", new=AsyncMock()):
            await link.upload(image, b"")
        link.request.assert_awaited_once_with(5, attempts=1)
        link.verify_app.assert_awaited_once_with(len(image), zlib.crc32(image))

    async def test_connection_drop_reconnects(self):
        class BleakError(Exception):
            pass

        class Client:
            attempts = 0

            def __init__(self, *_args, **_kwargs):
                self.services = SimpleNamespace(get_characteristic=lambda _uuid: "ffe1")

            async def __aenter__(self):
                return self

            async def __aexit__(self, *_args):
                return False

            async def start_notify(self, *_args):
                Client.attempts += 1
                if Client.attempts == 1:
                    raise BleakError("test disconnect")

        modules = {
            "bleak": SimpleNamespace(BleakClient=Client, BleakScanner=SimpleNamespace(
                find_device_by_address=AsyncMock(return_value=object()))),
            "bleak.exc": SimpleNamespace(BleakError=BleakError),
        }
        args = SimpleNamespace(image=None, ble=DEFAULT_BLE, uuid=DEFAULT_UUID,
                               info=True, exit_recovery=False)
        with patch.dict(sys.modules, modules), \
                patch("wireless_flash.asyncio.sleep", new=AsyncMock()), \
                patch.object(Link, "info", new=AsyncMock()) as info:
            await run(args)
        self.assertEqual(Client.attempts, 2)
        info.assert_awaited_once()

    async def test_matching_app_with_staged_data_does_not_blindly_run(self):
        image = b"current application"
        link = Link(None, None)
        link.received = 128
        link.enter = AsyncMock(return_value=(len(image), zlib.crc32(image)))
        link.request = AsyncMock(side_effect=RuntimeError("stage already committed"))
        with self.assertRaisesRegex(RuntimeError, "stage already committed"):
            await link.upload(image, b"new header")
        link.request.assert_awaited_once_with(2, b"new header")

    async def test_lost_ack_reuses_sequence_and_ignores_stale(self):
        class Client:
            def __init__(self):
                self.parser = Parser()
                self.requests = []

            async def write_gatt_char(self, _char, data, response):
                self.assert_no_response = not response
                for frame in self.parser.feed(data):
                    self.requests.append(frame)
                    if len(self.requests) == 2:
                        stale = Frame(DATA | 0x80, frame.sequence + 1, struct.pack("<II", 0, 128))
                        valid = Frame(DATA | 0x80, frame.sequence, struct.pack("<II", 0, 128))
                        link.notify(None, stale.encode() + valid.encode())

        client = Client()
        link = Link(client, "ffe1")
        reply = await link.request(DATA, b"data", timeout=0.01, attempts=2)
        self.assertEqual(struct.unpack("<II", reply), (0, 128))
        self.assertEqual(client.requests[0], client.requests[1])
        self.assertEqual(link.retries, 1)


if __name__ == "__main__":
    unittest.main()
