"""Build a validated OTA file and a combined bootloader/application first-install HEX."""

import argparse
from pathlib import Path
import struct

from ota_protocol import APP_ADDR, APP_META, STAGE_META, image_header


def hex_record(address, kind, data):
    record = bytes((len(data), address >> 8, address & 255, kind)) + data
    return ":" + (record + bytes((-sum(record) & 255,))).hex().upper()


def make_hex(segments):
    lines = []
    upper = None
    for address, data in sorted(segments):
        for offset in range(0, len(data), 16):
            current = address + offset
            if current >> 16 != upper:
                upper = current >> 16
                lines.append(hex_record(0, 4, upper.to_bytes(2, "big")))
            lines.append(hex_record(current & 65535, 0, data[offset:offset + 16]))
    return "\n".join(lines + [hex_record(0, 1, b"")]) + "\n"


def package(app_path, boot_path, out):
    app = app_path.read_bytes()
    boot = boot_path.read_bytes()
    header = image_header(app)
    if not 512 <= len(boot) <= 65536:
        raise ValueError("Bootloader must fit the 64 KiB partition")
    if not 0x08000000 <= struct.unpack_from("<I", boot, 4)[0] < 0x08000000 + len(boot):
        raise ValueError("Invalid bootloader reset vector")
    out.mkdir(parents=True, exist_ok=True)
    (out / "coral_c5.ota").write_bytes(header + app)
    update = [(APP_ADDR, app), (APP_META, header), (STAGE_META, b"\xff" * 32)]
    (out / "app_update.hex").write_text(make_hex(update), encoding="ascii")
    (out / "first_install.hex").write_text(make_hex([(0x08000000, boot), *update]), encoding="ascii")
    crc = struct.unpack_from("<I", header, 20)[0]
    print(f"OTA: {len(app)} bytes, CRC32={crc:08X}; first_install.hex includes bootloader")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--boot", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    package(args.app, args.boot, args.out)
