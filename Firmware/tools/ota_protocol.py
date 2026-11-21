"""Coral-C5 v1 镜像与传输格式，整数按小端排列，校验使用 CRC-32/ISO-HDLC。"""

import struct
import zlib
from dataclasses import dataclass

BOARD_ID = 0xC5A30001
APP_ADDR = 0x08010000
IMAGE_MAX = 448 * 1024
APP_META = 0x080FC000
STAGE_META = 0x080FE000
REQUEST_ADDR = 0x2003FFE0
HEADER_MAGIC = 0x3541544F
MAGIC = bytes.fromhex("c5 5a b0 07")
CHUNK = 128
PAYLOAD_MAX = CHUNK + 4
HELLO, BEGIN, DATA, END, RUN, ABORT = range(1, 7)
HEADER = struct.Struct("<8I")
STATUS = ("OK", "BAD_ARGUMENT", "BAD_STATE", "BAD_OFFSET", "FLASH_ERROR", "BAD_IMAGE")


# 检查应用长度、初始栈顶与复位入口是否适用于本板。
def validate_image(image):
    if not 512 <= len(image) <= IMAGE_MAX:
        raise ValueError(f"Application must be 512..{IMAGE_MAX} bytes")
    sp, entry = struct.unpack_from("<II", image)
    if not 0x20000008 <= sp <= REQUEST_ADDR or sp % 8:
        raise ValueError("Invalid initial stack pointer; rebuild with the OTA linker script")
    if not entry & 1 or not APP_ADDR <= entry < APP_ADDR + len(image):
        raise ValueError("Application must be linked at 0x08010000, not 0x08000000")


# 为已验证应用生成含目标地址、长度和 CRC 的镜像头。
def image_header(image):
    validate_image(image)
    first = struct.pack("<7I", HEADER_MAGIC, 1, BOARD_ID, APP_ADDR,
                        len(image), zlib.crc32(image), 0)
    return first + struct.pack("<I", zlib.crc32(first))


# 识别裸应用或 OTA 包并验证后返回应用数据和元数据。
def read_image(path):
    data = path.read_bytes()
    if path.suffix.lower() == ".ota":
        if len(data) < HEADER.size:
            raise ValueError("Truncated OTA package")
        header, image = data[:HEADER.size], data[HEADER.size:]
        if header != image_header(image):
            raise ValueError("OTA package header/CRC/board mismatch")
        return image, header
    if path.suffix.lower() != ".bin":
        raise ValueError("Use coral_c5.ota or coral_c5.bin; bootloader/HEX/ELF are not OTA images")
    return data, image_header(data)


# 保存一帧升级命令的操作码、序号及载荷。
@dataclass(frozen=True)
class Frame:
    command: int
    sequence: int
    payload: bytes = b""

    # 按协议拼接帧头、命令、数据及 CRC。
    def encode(self):
        if len(self.payload) > PAYLOAD_MAX:
            raise ValueError("Oversized payload")
        packet = MAGIC + struct.pack("<BBHI", 1, self.command, len(self.payload), self.sequence)
        packet += self.payload
        return packet + struct.pack("<I", zlib.crc32(packet))


# 从可跨通知分片的字节流中寻找完整升级帧。
class Parser:
    # 初始化跨次接收共用的字节缓冲。
    def __init__(self):
        self.buffer = bytearray()

    # 累积数据并跳过坏帧，收齐且 CRC 正确才返回一帧。
    def feed(self, data):
        self.buffer.extend(data)
        while len(self.buffer) >= 4:
            if not self.buffer.startswith(MAGIC):
                del self.buffer[0]
                continue
            if len(self.buffer) < 12:
                return
            version, command, length, sequence = struct.unpack_from("<BBHI", self.buffer, 4)
            if version != 1 or length > PAYLOAD_MAX:
                del self.buffer[0]
                continue
            if len(self.buffer) < length + 16:
                return
            packet = self.buffer[:length + 16]
            if zlib.crc32(packet[:-4]) != struct.unpack_from("<I", packet, len(packet) - 4)[0]:
                del self.buffer[0]
                continue
            del self.buffer[:length + 16]
            yield Frame(command, sequence, bytes(packet[12:-4]))
