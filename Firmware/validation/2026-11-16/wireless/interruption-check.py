import asyncio
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path('Firmware/tools').resolve()))
from bleak import BleakClient, BleakScanner
from ota_protocol import BEGIN, DATA, Frame, HELLO, read_image
from wireless_flash import DEFAULT_BLE, DEFAULT_UUID, Link

async def main():
    image, header = read_image(Path('Firmware/build/debug/coral_c5.ota'))
    device = await BleakScanner.find_device_by_address(DEFAULT_BLE, timeout=15)
    async with BleakClient(device) as client:
        link = Link(client, client.services.get_characteristic(DEFAULT_UUID))
        await client.start_notify(link.characteristic, link.notify)
        old_size, old_crc = await link.enter()
        await link.request(BEGIN, header)
        for offset in range(0, 1024, 128):
            await link.request(DATA, struct.pack('<I', offset) + image[offset:offset + 128])
        # A duplicate packet must be acknowledged without a second flash program.
        reply = await link.request(DATA, struct.pack('<I', 896) + image[896:1024])
        assert struct.unpack_from('<I', reply, 4)[0] == 1024
        # A corrupt frame is ignored; the following valid HELLO must resynchronize.
        corrupt = bytearray(Frame(DATA, 7000, struct.pack('<I', 1024) + image[1024:1152]).encode())
        corrupt[-1] ^= 1
        await link.write(corrupt)
        reply = await link.request(HELLO)
        assert struct.unpack_from('<I', reply, 4)[0] == 1024
        assert struct.unpack_from('<II', reply, 24) == (old_size, old_crc)
        print(f'PASS: duplicate packet, corrupt CRC rejected; old app {old_crc:08X} intact; disconnect at 1024 bytes', flush=True)
    # A new client connection has no persistent local state. Abort and RUN must boot old app.
    async with BleakClient(device) as client:
        link = Link(client, client.services.get_characteristic(DEFAULT_UUID))
        await client.start_notify(link.characteristic, link.notify)
        await link.request(6)
        await link.request(5)
        await asyncio.sleep(2)
        await link.verify_app(old_size, old_crc)
        print('PASS: reconnect after incomplete transfer; previous app booted without J-Link', flush=True)

asyncio.run(main())
