# 通过模拟蓝牙会话核对打包格式、快速分片、丢包重试及步进抱持处理。

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
from ota_protocol import APP_ADDR, BOARD_ID, CHUNK, DATA, HELLO, IMAGE_MAX, Frame, Parser, image_header, read_image
from package_firmware import make_hex
from wireless_flash import DEFAULT_BLE, DEFAULT_UUID, PACED_RECOVERY_BLOCKS, Link, run


# 检查镜像与传输帧的静态格式。
class ImageTests(unittest.TestCase):
    # 建立本次测试的模拟应用数据。
    def setUp(self):
        self.image = bytearray(range(256)) * 4
        struct.pack_into("<II", self.image, 0, 0x2003FFE0, APP_ADDR + 513)

    # 对照载荷与校验检查打包完整性。
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

    # 输入旧链接地址，验证打包器拒绝错误应用。
    def test_old_link_address_rejected(self):
        struct.pack_into("<I", self.image, 4, 0x08000201)
        with self.assertRaises(ValueError):
            image_header(self.image)

    # 逐条核对 Intel HEX 的地址与校验和。
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

    # 拆分并损坏帧，检查解析器恢复。
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


# 模拟蓝牙异步链路中的超时、重试和抱持处理。
class RetryTests(unittest.IsolatedAsyncioTestCase):
    # 连续丢包后核对上传器是否降低发送节拍。
    async def test_data_burst_falls_back_on_repeated_loss(self):
        link = Link(None, None)
        sends = []
        # 模拟发包并按场景记录重试与应答。
        async def send(data, *, paced):
            frame, = Parser().feed(data)
            sends.append((frame.sequence, paced))
            if paced:
                ack = Frame(DATA | 0x80, frame.sequence, struct.pack("<II", 0, 128))
                link.notify(None, ack.encode())
        link.write = send
        for _ in range(3):
            await link.request(DATA, bytes(132), timeout=0.01)
        self.assertEqual(sends, [(1, False), (1, True), (2, False), (2, True), (3, True)])
        for _ in range(PACED_RECOVERY_BLOCKS - 1):
            await link.request(DATA, bytes(132), timeout=0.01)
        self.assertTrue(all(paced for _, paced in sends[4:]))
        await link.request(DATA, bytes(132), timeout=0.01)
        self.assertEqual([paced for _, paced in sends[-2:]], [False, True])

    # 确认快速写仍逐片等待系统，不额外插入片间延时。
    async def test_burst_awaits_all_eight_writes_without_extra_sleep(self):
        client = SimpleNamespace(write_gatt_char=AsyncMock())
        link = Link(client, "ffe1")
        packet = Frame(DATA, 7, bytes(132)).encode()
        with patch("wireless_flash.asyncio.sleep", new=AsyncMock()) as sleep:
            await link.write(packet, paced=False)
            sleep.assert_not_awaited()
        writes = client.write_gatt_char.await_args_list
        self.assertEqual([len(call.args[1]) for call in writes], [20] * 7 + [8])
        self.assertEqual(b"".join(call.args[1] for call in writes), packet)
        self.assertTrue(all(call.kwargs == {"response": False} for call in writes))

    # 检测到水管正在托管时，应在进入引导前停止流程。
    async def test_holding_pipe_stops_before_bootloader_exchange(self):
        link = Link(None, None)
        link.write = AsyncMock(side_effect=lambda _: link.notify(None, b"C5OTA,BEAM HELD\r\n"))
        link.request = AsyncMock()
        with patch("wireless_flash.asyncio.sleep", new=AsyncMock()), \
                self.assertRaisesRegex(RuntimeError, "supporting the pipe"):
            await link.enter()
        link.request.assert_not_awaited()
        link.write.assert_awaited_once_with(b"!\nC5:OTA\n")

    # 已确认外部支撑时，先解除抱持再进入升级。
    async def test_supported_pipe_is_released_before_requesting_bootloader(self):
        link = Link(None, None)
        messages = []
        # 模拟设备按命令返回驱动状态。
        def write(data):
            messages.append(data)
            if len(messages) == 1:
                link.notify(None, b"C5OTA,BEAM HELD\r\n")
            elif b"C5:BAL?" in data:
                link.notify(None, b"C5BAL,1,23,0,0,0,1,1,500,0,0,0,0,0,3200,1,0,2,0,0\r\n")
        link.write = AsyncMock(side_effect=write)
        link.request = AsyncMock(return_value=struct.pack("<8I", 0, 0, BOARD_ID, APP_ADDR,
                                                          IMAGE_MAX, CHUNK, 60804, 123))
        with patch("wireless_flash.asyncio.sleep", new=AsyncMock()):
            self.assertEqual(await link.enter(pipe_supported=True), (60804, 123))
        self.assertEqual(messages, [b"!\nC5:OTA\n", b"\nC5:BAL\n", b"C5:FREE\nC5:BAL?\n", b"!\nC5:OTA\n"])
        link.request.assert_awaited_once_with(HELLO)

    # 缺少驱动失能确认时不得继续升级。
    async def test_supported_pipe_requires_drive_confirmation(self):
        link = Link(None, None)
        # 模拟请求已发出但驱动尚未确认释放。
        def write(data):
            if b"C5:OTA" in data:
                link.notify(None, b"C5OTA,BEAM HELD\r\n")
            else:
                link.notify(None, b"C5BAL,1,23,0,0,0,1,1,500,0,0,0,0,0,3200,10,0,3,0,0\r\n")
        link.write = AsyncMock(side_effect=write)
        link.request = AsyncMock()
        with patch("wireless_flash.asyncio.sleep", new=AsyncMock()), self.assertRaisesRegex(RuntimeError, "No confirmation"):
            await link.enter(pipe_supported=True)
        link.request.assert_not_awaited()

    # 当前运行镜像已匹配时应直接返回，避免擦写。
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

    # 模拟断连后重新发现设备并续传。
    async def test_connection_drop_reconnects(self):
        # 提供测试用蓝牙异常类型。
        class BleakError(Exception):
            pass

        # 代替实际 Bleak 客户端控制连接结果。
        class Client:
            attempts = 0

            # 建立该次模拟连接的状态。
            def __init__(self, *_args, **_kwargs):
                self.services = SimpleNamespace(get_characteristic=lambda _uuid: "ffe1")

            # 进入测试连接并记录连接尝试。
            async def __aenter__(self):
                return self

            # 退出上下文时释放模拟连接。
            async def __aexit__(self, *_args):
                return False

            # 记录订阅回调，供测试注入设备通知。
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

    # 镜像匹配但存在暂存下载时，核对升级器仍按会话状态处理。
    async def test_matching_app_with_staged_data_does_not_blindly_run(self):
        image = b"current application"
        link = Link(None, None)
        link.received = 128
        link.enter = AsyncMock(return_value=(len(image), zlib.crc32(image)))
        link.request = AsyncMock(side_effect=RuntimeError("stage already committed"))
        with self.assertRaisesRegex(RuntimeError, "stage already committed"):
            await link.upload(image, b"new header")
        link.request.assert_awaited_once_with(2, b"new header")

    # 丢 ACK 时须重用序号，迟到的旧应答不得推进传输。
    async def test_lost_ack_reuses_sequence_and_ignores_stale(self):
        # 用假的 GATT 后端注入超时与旧应答。
        class Client:
            # 建立写入记录及默认通知状态。
            def __init__(self):
                self.parser = Parser()
                self.requests = []

            # 捕获 GATT 分片并按测试计划返回通知。
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
