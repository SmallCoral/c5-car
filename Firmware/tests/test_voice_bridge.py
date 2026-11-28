"""按 MicroPython/NimBLE 的 IRQ 参数回放 HC-08 发现、订阅及主控身份确认。"""
import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


class VoiceBridgeTest(unittest.TestCase):
    def setUp(self):
        self.radio = Mock()
        modules = {
            "bluetooth": SimpleNamespace(BLE=lambda: self.radio, UUID=int),
            "machine": SimpleNamespace(UART=Mock(), Pin=Mock()),
        }
        spec = importlib.util.spec_from_file_location(
            "voice_bridge", Path(__file__).resolve().parents[1] / "voice" / "xiaosai.py")
        self.app = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, modules):
            spec.loader.exec_module(self.app)
        self.now = 0
        self.app.time = SimpleNamespace(ticks_ms=lambda: self.now, ticks_diff=lambda a, b: a - b,
                                        ticks_add=lambda a, b: a + b)
        self.bridge = self.app.HC08Central()

    def event(self, number, *fields):
        self.bridge._irq(number, fields)
        self.bridge.poll()

    def test_descriptor_and_identity(self):
        b = self.bridge
        b.poll()
        self.event(5, 0, bytes(6), 0, -60, b"\x06\x09HC-08")
        self.radio.gap_connect.assert_not_called()
        self.event(5, 0, self.app.TARGET_MAC, 0, -60, b"")
        self.radio.gap_connect.assert_called_once_with(0, self.app.TARGET_MAC, 8000)
        self.event(7, 1, 0, self.app.TARGET_MAC)
        self.event(9, 1, 51, 60, 0xFFE0)
        self.event(10, 1, 0)
        self.event(11, 1, 56, 54, 0x16, 0xFFE1)
        self.event(11, 1, 60, 58, 0x02, 0x1234)
        self.event(12, 1, 0)
        self.radio.gattc_discover_descriptors.assert_called_once_with(1, 54, 56)
        self.event(13, 1, 55, 0x2902)
        self.event(14, 1, 0)
        self.radio.gattc_write.assert_called_once_with(1, 55, b"\x01\x00", 1)
        self.event(17, 1, 55, 0)
        self.radio.gattc_write.assert_called_with(1, 54, b"\nC5:INFO\n", 0)
        self.event(18, 1, 54, b"C5APP,1,invalid,0\r\nC5APP,1,690C")
        self.assertFalse(b.enqueue(b"1\n"))
        self.event(18, 1, 54, b"8900,69040\r\n")
        self.assertEqual(b.state, "ready")
        self.assertTrue(b.enqueue(b"S,42,1\n1\n"))
        self.now = 50
        b.poll()
        self.radio.gattc_write.assert_called_with(1, 54, b"S,42,1\n1\n", 0)
        self.event(8, 1, 0, self.app.TARGET_MAC)
        self.assertEqual(b.state, "idle")

    def test_no_stm32_no_ready_notice(self):
        b = self.bridge
        b.conn, b.handle, b.cccd, b.mode = 1, 54, 55, 0
        b._stage("subscribe")
        self.event(17, 1, 55, 0)
        self.now = 4001
        b.poll()
        self.assertEqual(b.state, "idle")
        self.assertFalse(b.enqueue(b"1\n"))
