# ESP32-S3 MicroPython: ASRPRO UART -> HC-08 BLE central.
# UART TX=GPIO17, RX=GPIO16, 9600 8N1.
# BLE starts an event with S,run,task + digit lines and receives D/F reports.
from machine import UART, Pin
import bluetooth
import time

PROGRAM_VERSION = "BALL-V3-DUPLEX-CCCD2"

TARGET_NAME = b"HC-08"
# Optional: six address bytes in scan-result order, e.g. bytes.fromhex("aabbccddeeff").
# Set this when several HC-08 modules are nearby; MAC takes precedence over name.
TARGET_MAC = bytes.fromhex("f82e0cf36837")
SERVICE_UUID = bluetooth.UUID(0xFFE0)
DATA_UUID = bluetooth.UUID(0xFFE1)


def adv_name(payload):
    i = 0
    while i < len(payload):
        n = payload[i]
        if not n or i + n >= len(payload):
            break
        if payload[i + 1] in (8, 9):
            return payload[i + 2:i + n + 1]
        i += n + 1
    return None


# ASR<->ESP: A5 5A TYPE SEQ VALUE XOR 0D; XOR=TYPE^SEQ^VALUE.
# Only the local UART uses these packets. BLE uses LF-terminated ASCII lines.
TYPE_COMMAND = 0x01
TYPE_ASR_HELLO = 0x10
TYPE_RX_ACK = 0x81
TYPE_BLE_STATE = 0x82
TYPE_STATE_ACK = 0x83


def make_packet(kind, seq, value):
    return bytes((0xA5, 0x5A, kind, seq, value, kind ^ seq ^ value, 0x0D))


class PacketParser:
    def __init__(self):
        # Immutable bytes: no bytearray deletion/resize operations.
        self.buf = b""
        self.bad_frames = 0
        self.last_rx = time.ticks_ms()

    def feed(self, data):
        now = time.ticks_ms()
        if time.ticks_diff(now, self.last_rx) > 300:
            self.buf = b""
        self.last_rx = now
        packets = []
        for byte in data:
            self.buf = self.buf + bytes((byte,))
            while self.buf:
                if self.buf[0] != 0xA5:
                    self.buf = self.buf[1:]
                elif len(self.buf) < 2:
                    break
                elif self.buf[1] != 0x5A:
                    self.buf = self.buf[1:]
                elif len(self.buf) < 7:
                    break
                elif (self.buf[6] != 0x0D or
                      self.buf[5] != (self.buf[2] ^ self.buf[3] ^ self.buf[4])):
                    self.bad_frames += 1
                    self.buf = self.buf[1:]
                else:
                    packets.append((self.buf[2], self.buf[3], self.buf[4]))
                    self.buf = self.buf[7:]
        return packets

class ASRLink:
    def __init__(self, uart):
        self.uart = uart
        self.parser = PacketParser()
        self.tx = []
        self.tx_pending = b""
        self.ble_ready = False
        self.link_seq = 0
        self.link_confirmed = False
        self.next_link_notice = time.ticks_ms()
        self.seen = []
        self.report_ack = None
        self.command_seqs = []
        self.send(TYPE_BLE_STATE, 0, 0)  # Clear ASR state after an ESP-only restart.

    def send(self, kind, seq, value):
        if len(self.tx) >= 16:
            print("UART reply queue full; reply dropped")
            return False
        self.tx.append(make_packet(kind, seq, value))
        return True

    def set_ble_ready(self, ready):
        if ready == self.ble_ready:
            return
        self.ble_ready = ready
        self.link_confirmed = False
        if ready:
            self.link_seq = (self.link_seq % 255) + 1
            self.next_link_notice = time.ticks_ms()
        else:
            # Suppress queued stale ready notices after a disconnect.
            self.tx = [p for p in self.tx if p[2] != TYPE_BLE_STATE]
            self.send(TYPE_BLE_STATE, self.link_seq, 0)

    def receive(self, data):
        commands = []
        self.command_seqs = []
        bad_before = self.parser.bad_frames
        for kind, seq, value in self.parser.feed(data):
            if kind == TYPE_COMMAND and seq != 0 and 1 <= value <= 8:
                print("UART CHECK OK: seq=", seq, "command=", value)
                # ACK means that ESP received and checked UART data, independent of BLE.
                self.send(TYPE_RX_ACK, seq, value)
                key = (seq, value)
                if key in self.seen:
                    print("UART duplicate: ACK only; no BLE replay")
                    continue
                self.seen.append(key)
                if len(self.seen) > 16:
                    self.seen = self.seen[1:]
                commands.append(bytes((48 + value, 10)))
                self.command_seqs.append(seq)
            elif kind == TYPE_ASR_HELLO and seq == 0 and value == 0:
                if self.ble_ready:
                    self.link_confirmed = False
                    self.next_link_notice = time.ticks_ms()
            elif kind == 0x95 and value == 1:
                self.report_ack = seq
            elif kind == TYPE_STATE_ACK and value == 1:
                if self.ble_ready and seq == self.link_seq:
                    if not self.link_confirmed:
                        print("ASR confirmed handshake notice; UART round trip OK")
                    self.link_confirmed = True
        if self.parser.bad_frames != bad_before:
            print("UART checksum/frame error: packet ignored; no ACK")
        return commands

    def poll(self):
        now = time.ticks_ms()
        if self.ble_ready and not self.link_confirmed:
            if time.ticks_diff(now, self.next_link_notice) >= 0:
                self.send(TYPE_BLE_STATE, self.link_seq, 1)
                self.next_link_notice = time.ticks_add(now, 2000)
        if not self.tx_pending and self.tx:
            self.tx_pending = self.tx.pop(0)
        if self.tx_pending:
            try:
                sent = self.uart.write(self.tx_pending)
                if sent:
                    self.tx_pending = self.tx_pending[sent:]
            except OSError as exc:
                print("UART TX error:", exc)
                self.tx_pending = b""


class HC08Central:
    def __init__(self):
        self.ble = bluetooth.BLE()
        self.ble.active(True)
        # Keep firmware default BLE buffers; rxbuf config is not universal.
        self.events = []
        self.event_overflow = False
        self.ble.irq(self._irq)
        self.conn = None
        self.state = "idle"
        self.service = None
        self.handle = None
        self.mode = None
        self.queue = []
        self.char_end = None
        self.props = 0
        self.cccd = None
        self.identity_lines = TextLines()
        self.next_probe = 0
        self.received = []
        self.waiting = False
        self.next_scan = time.ticks_ms()
        self.last_send = time.ticks_ms()
        self.deadline = self.next_scan

    def _irq(self, event, data):
        # Copy borrowed memoryviews while inside the callback.
        if event == 5:  # scan result
            at, addr, kind, rssi, payload = data
            address = bytes(addr)
            name = adv_name(bytes(payload))
            match = (address == TARGET_MAC) if TARGET_MAC is not None else (name == TARGET_NAME)
            if not match:
                return
            item = (event, (at, address))
        elif event == 7:  # peripheral connected
            item = (event, (data[0],))
        elif event == 18:
            item = (event, (data[0], data[1], bytes(data[2])))
        elif event == 13:
            item = (event, (data[0], data[1], bluetooth.UUID(data[2])))
        elif event == 8:
            item = (event, (data[0],))
        elif event in (10, 12, 14):
            item = (event, (data[0], data[1]))
        elif event == 17:
            item = (event, (data[0], data[1], data[2]))
        elif event == 6:
            item = (event, ())
        elif event == 9:
            c, start, end, uuid = data
            item = (event, (c, start, end, bluetooth.UUID(uuid)))
        elif event == 11:
            c, end, value, props, uuid = data
            item = (event, (c, end, value, props, bluetooth.UUID(uuid)))
        else:
            return
        if len(self.events) < 64:
            self.events.append(item)
        else:
            self.event_overflow = True

    def _stage(self, state, timeout=12000):
        self.state = state
        self.deadline = time.ticks_add(time.ticks_ms(), timeout)

    def _reset(self, reason):
        print("BLE reset:", reason)
        old = self.conn
        self.conn = None
        self.state = "idle"
        self.handle = None
        self.service = None
        self.mode = None
        self.queue.clear()
        self.char_end = None
        self.props = 0
        self.cccd = None
        self.received = []
        self.waiting = False
        self.next_scan = time.ticks_add(time.ticks_ms(), 1500)
        try:
            self.ble.gap_scan(None)
        except OSError:
            pass
        if old is not None:
            try:
                self.ble.gap_disconnect(old)
            except OSError:
                pass

    def enqueue(self, frame):
        if self.state != "ready":
            print("DROP (HC-08 not ready):", frame)
            return False
        if len(self.queue) >= 8:
            print("DROP (queue full):", frame)
            return False
        # Use <=20-byte ATT writes; preserve text stream order across chunks.
        parts = [frame[i:i+20] for i in range(0, len(frame), 20)]
        if len(self.queue) + len(parts) > 8:
            return False
        self.queue.extend(parts)
        return True

    def _event(self, event, d):
        if event == 5 and self.state == "scan":
            self._stage("connect")
            self.ble.gap_scan(None)
            print("Connecting HC-08:", d[1].hex())
            self.ble.gap_connect(d[0], d[1], 8000)
        elif event == 6 and self.state == "scan":
            self._reset("HC-08 not found; check power/name")
        elif event == 7:
            if self.state != "connect":
                self.ble.gap_disconnect(d[0])
                return
            self.conn = d[0]
            self.service = None
            self._stage("service")
            self.ble.gattc_discover_services(self.conn, SERVICE_UUID)
        elif event == 8 and d[0] == self.conn:
            self._reset("disconnected; pending commands cleared")
        elif event == 9 and self.state == "service" and d[0] == self.conn:
            if d[3] == SERVICE_UUID:
                self.service = (d[1], d[2])
        elif event == 10 and self.state == "service" and d[0] == self.conn:
            if d[1] != 0 or self.service is None:
                self._reset("FFE0 service missing")
                return
            self._stage("characteristic")
            self.ble.gattc_discover_characteristics(self.conn, *self.service)
        elif event == 11 and self.state == "characteristic" and d[0] == self.conn:
            if d[4] == DATA_UUID:
                self.char_end = d[1]
                self.props = d[3]
                # Prefer acknowledged GATT writes if available.
                if d[3] & 0x08:
                    self.handle, self.mode = d[2], 1
                elif d[3] & 0x04:
                    self.handle, self.mode = d[2], 0
        elif event == 12 and self.state == "characteristic" and d[0] == self.conn:
            if d[1] != 0 or self.handle is None:
                self._reset("writable FFE1 characteristic missing")
                return
            if not (self.props & 0x10):
                self._reset("FFE1 has no Notify; telemetry unavailable")
                return
            if self.handle >= self.char_end:
                self._reset("no descriptor space for FFE1 CCCD")
                return
            self._stage("descriptor")
            # NimBLE begins its ATT search *after* the characteristic value handle.
            self.ble.gattc_discover_descriptors(self.conn, self.handle, self.char_end)
        elif event == 13 and self.state == "descriptor" and d[0] == self.conn:
            if d[2] == bluetooth.UUID(0x2902):
                self.cccd = d[1]
        elif event == 14 and self.state == "descriptor" and d[0] == self.conn:
            if d[1] != 0 or self.cccd is None:
                self._reset("FFE1 CCCD missing")
                return
            self._stage("subscribe")
            self.ble.gattc_write(self.conn, self.cccd, b"\x01\x00", 1)
        elif event == 17 and self.state == "subscribe" and d[0] == self.conn:
            if d[1] == self.cccd:
                if d[2] != 0:
                    self._reset("Notify subscription rejected")
                    return
                self.identity_lines = TextLines()
                self.next_probe = time.ticks_ms()
                self._stage("verify STM32", 4000)
        elif event == 18 and d[0] == self.conn and d[1] == self.handle:
            if self.state == "verify STM32":
                for line in self.identity_lines.feed(d[2]):
                    fields = line.split(b",")
                    if (len(fields) == 4 and fields[:2] == [b"C5APP", b"1"] and
                            len(fields[2]) == 8 and all(c in b"0123456789abcdefABCDEF" for c in fields[2]) and
                            1 <= len(fields[3]) <= 7 and all(48 <= c <= 57 for c in fields[3]) and int(fields[3]) > 0):
                        self.state = "ready"
                        print("BLE READY: duplex HC-08, STM32=", fields[2].decode(), "handle=", self.handle,
                              "CCCD=", self.cccd, "write mode=", self.mode)
                        break
            elif self.state == "ready":
                if len(self.received) >= 32:
                    self._reset("telemetry backlog overflow")
                else:
                    self.received.append(d[2])
        elif event == 17 and self.state == "ready" and d[0] == self.conn:
            if d[1] == self.handle and self.waiting:
                self.waiting = False
                if d[2] != 0:
                    self._reset("GATT write rejected")
                else:
                    print("GATT write acknowledged (STM32 execution not confirmed)")

    def poll(self):
        if self.event_overflow:
            self.events.clear()
            self.event_overflow = False
            self._reset("event queue overflow")
        while self.events:
            event, data = self.events.pop(0)
            try:
                self._event(event, data)
            except OSError as exc:
                self._reset(exc)
        now = time.ticks_ms()
        if self.state == "idle":
            if time.ticks_diff(now, self.next_scan) >= 0:
                try:
                    self._stage("scan", 7000)
                    self.ble.gap_scan(5000, 30000, 30000, True)
                except OSError as exc:
                    self._reset(exc)
        elif self.state != "ready":
            if time.ticks_diff(now, self.deadline) >= 0:
                self._reset("timeout: " + self.state)
            elif self.state == "verify STM32" and time.ticks_diff(now, self.next_probe) >= 0:
                self.next_probe = time.ticks_add(now, 1000)
                try:
                    self.ble.gattc_write(self.conn, self.handle, b"\nC5:INFO\n", self.mode)
                except OSError as exc:
                    self._reset(exc)
        elif self.waiting:
            if time.ticks_diff(now, self.deadline) >= 0:
                self._reset("write timeout; not replaying")
        elif self.queue and time.ticks_diff(now, self.last_send) >= 40:
            frame = self.queue.pop(0)
            self.waiting = self.mode == 1
            self.deadline = time.ticks_add(now, 4000)
            self.last_send = now
            try:
                self.ble.gattc_write(self.conn, self.handle, frame, self.mode)
                print("BLE TX:", frame)
            except OSError as exc:
                # Delivery may be uncertain. Do not retry an execution command.
                self._reset(exc)


import math

# No unit was supplied. Set 1=millimetres, 2=pixels, 3=centimetres; 0=omit unit.
UNIT_CODE = 0
FINAL_WAIT_MS = 1500
MAX_VALUE = 99999.99


class TextLines:
    def __init__(self):
        self.buf = b""
        self.overflow = False

    def feed(self, data):
        lines = []
        for b in data:
            if b == 10:
                if not self.overflow:
                    lines.append(self.buf.rstrip(b"\r"))
                self.buf = b""
                self.overflow = False
            elif not self.overflow:
                if len(self.buf) < 80:
                    self.buf += bytes((b,))
                else:
                    self.buf = b""
                    self.overflow = True
        return lines


def decimal_value(raw):
    # Finite ordinary decimal only; reject NaN/Inf and exponent syntax.
    s = raw.decode('ascii')
    if s[:1] in ('+', '-'):
        body = s[1:]
    else:
        body = s
    if not body or body.count('.') > 1 or not any('0' <= c <= '9' for c in body):
        raise ValueError('bad decimal')
    if any(c != '.' and not ('0' <= c <= '9') for c in body):
        raise ValueError('bad decimal')
    v = float(s)
    if not (-MAX_VALUE <= v <= MAX_VALUE):
        raise ValueError('out of range')
    return v


class TaskStats:
    def __init__(self):
        self.run = None
        self.task = None
        self.active = False
        self.closing = False
        self.exit_seq = 0
        self.maximum = None
        self.samples = 0
        self.final_received = False
        self.incomplete = False
        self.deadline = 0
        self.lines = TextLines()

    def start(self, run, task):
        self.run, self.task = run, task
        self.active = True
        self.closing = False
        self.maximum = None
        self.samples = 0
        self.final_received = False
        self.incomplete = False
        self.lines = TextLines()

    def exit(self, seq):
        self.exit_seq = seq
        self.closing = True
        self.deadline = time.ticks_add(time.ticks_ms(), FINAL_WAIT_MS)
        if not self.active:
            self.maximum = None
            self.samples = 0
            self.final_received = False
            self.deadline = time.ticks_ms()

    def feed(self, data):
        for line in self.lines.feed(data):
            try:
                fields = line.split(b',')
                if len(fields) not in (3, 4) or fields[0] not in (b'D', b'F'):
                    raise ValueError('line type/length')
                if not fields[1] or any(c < 48 or c > 57 for c in fields[1]):
                    raise ValueError('bad run')
                run = int(fields[1])
                if not self.active or run != self.run:
                    print('Ignoring stale/inactive telemetry:', line)
                    continue
                if fields[0] == b'F':
                    if len(fields) != 3 or not self.closing:
                        raise ValueError('unexpected final')
                    magnitude = decimal_value(fields[2])
                    if magnitude < 0:
                        raise ValueError('negative maximum')
                    self.final_received = True
                elif len(fields) == 3:
                    magnitude = abs(decimal_value(fields[2]))
                else:
                    x, y = decimal_value(fields[2]), decimal_value(fields[3])
                    magnitude = math.sqrt(x * x + y * y)
                    if magnitude > MAX_VALUE:
                        raise ValueError('distance out of range')
                self.maximum = magnitude if self.maximum is None else max(self.maximum, magnitude)
                self.samples += fields[0] == b'D'
                print('STM32 RX:', line, 'max=', self.maximum)
            except (ValueError, UnicodeError, OverflowError) as exc:
                print('Bad STM32 telemetry:', line, exc)

    def result(self):
        if not self.closing or time.ticks_diff(time.ticks_ms(), self.deadline) < 0:
            return None
        self.closing = False
        self.active = False
        # 0=no data, 1=STM32 final received, 2=observed-only or a link gap.
        status = 0 if self.maximum is None else (1 if self.final_received and not self.incomplete else 2)
        centi = 0 if self.maximum is None else int(self.maximum * 100 + 0.5)
        return (self.exit_seq, status, centi)


class ExitReport:
    def __init__(self, link):
        self.link = link
        self.pending = None
        self.next_send = 0
        self.expires = 0

    def start(self, result):
        self.pending = result
        self.next_send = time.ticks_ms()
        self.expires = time.ticks_add(self.next_send, 15000)
        self.link.report_ack = None

    def cancel(self):
        self.pending = None
        self.link.tx = [p for p in self.link.tx if not (0x90 <= p[2] <= 0x94)]

    def poll(self):
        if self.pending is None:
            return
        seq, status, n = self.pending
        now = time.ticks_ms()
        if self.link.report_ack == seq or time.ticks_diff(now, self.expires) >= 0:
            self.pending = None
            return
        if time.ticks_diff(now, self.next_send) >= 0 and len(self.link.tx) <= 10:
            for kind, value in ((0x90, status), (0x91, n & 255), (0x92, (n >> 8) & 255),
                                (0x93, (n >> 16) & 255), (0x94, UNIT_CODE)):
                self.link.send(kind, seq, value)
            self.next_send = time.ticks_add(now, 2000)


asr_uart = None


def send_asr(data):
    """Raw UART debug API; running protocol replies are sent via ASRLink."""
    if asr_uart is None:
        raise RuntimeError("UART not initialized")
    if not isinstance(data, (bytes, bytearray)):
        raise TypeError("Use bytes or bytearray")
    return asr_uart.write(data)


def main():
    global asr_uart
    import urandom
    run_counter = urandom.getrandbits(16) or 1
    print("PROGRAM VERSION:", PROGRAM_VERSION)
    asr_uart = UART(1, baudrate=9600, bits=8, parity=None, stop=1,
                    tx=Pin(17), rx=Pin(16), rxbuf=512, timeout=0)
    link = ASRLink(asr_uart)
    bridge = HC08Central()
    stats = TaskStats()
    reports = ExitReport(link)
    was_ready = False
    print("UART ready: ASR PA2 -> S3 GPIO16; S3 GPIO17 -> ASR PA3")
    while True:
        bridge.poll()
        ready = bridge.state == "ready"
        if was_ready and not ready and stats.active:
            stats.incomplete = True
            stats.lines = TextLines()
        was_ready = ready
        link.set_ble_ready(ready)
        while bridge.received:
            stats.feed(bridge.received.pop(0))
        data = asr_uart.read()
        if data:
            commands = link.receive(data)
            for frame, seq in zip(commands, link.command_seqs):
                cmd = frame[0] - 48
                if stats.closing:
                    print("DROP: waiting for exit report; repeat command after report")
                    continue
                if 1 <= cmd <= 6:
                    run_counter = run_counter % 65535 + 1
                    # S configures the session; only the following digit executes the task.
                    start = ("S,%d,%d\n" % (run_counter, cmd)).encode() + frame
                    if bridge.enqueue(start):
                        reports.cancel()
                        stats.start(run_counter, cmd)
                        print("TASK START: run=", run_counter, "task=", cmd)
                    else:
                        print("DROP: BLE not ready; task not started")
                elif cmd == 7:
                    if not stats.active:
                        print("DROP: no active task to continue")
                    else:
                        bridge.enqueue(frame)
                elif cmd == 8:
                    reports.cancel()
                    if bridge.enqueue(frame):
                        stats.exit(seq)
                    else:
                        # Never pretend an exit was sent when the BLE link is down.
                        reports.start((seq, 3, 0))
                        stats.active = False
                        print("EXIT NOT SENT: BLE unavailable")
        result = stats.result()
        if result is not None:
            print("EXIT REPORT:", result)
            reports.start(result)
        reports.poll()
        link.poll()
        time.sleep_ms(5)


if __name__ == "__main__":
    main()
