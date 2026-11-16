import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "monitor", Path(__file__).parents[1] / "tools" / "monitor.py"
)
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)


class MonitorTest(unittest.TestCase):
    def frame(self):
        return ("C5,2," + ",".join("0" for _ in monitor.FIELDS[2:]) + "\r\n").encode()

    def test_previous_version(self):
        frame = ("C5,1," + ",".join("0" for _ in monitor.FIELDS[2:-3]) + "\r\n").encode()
        record, = monitor.Decoder().feed(frame)
        self.assertEqual(record["version"], 1)
        self.assertEqual(record["step_rpm_valid"], 0)

    def test_combined_motion_task(self):
        values = self.frame().decode().strip().split(",")
        values[monitor.FIELDS.index("task")] = "5"
        values[monitor.FIELDS.index("running")] = "1"
        values[monitor.FIELDS.index("duty_permille")] = "100"
        values[monitor.FIELDS.index("step_target_rpm")] = "30"
        record, = monitor.Decoder().feed((",".join(values) + "\n").encode())
        self.assertEqual(monitor.TASKS[record["task"]], "ALL")
        self.assertEqual(record["duty_permille"], 100)
        self.assertEqual(record["step_target_rpm"], 30)

    def test_every_fragment_boundary(self):
        frame = self.frame()
        for split in range(len(frame)):
            decoder = monitor.Decoder()
            self.assertEqual(decoder.feed(frame[:split]), [])
            self.assertEqual(len(decoder.feed(frame[split:])), 1)

    def test_noise_multiple_frames_and_recovery(self):
        decoder = monitor.Decoder()
        self.assertEqual(len(decoder.feed(b"noise\n" + self.frame() * 2)), 2)
        self.assertEqual(decoder.invalid, 1)
        decoder.feed(b"X" * 2048)
        self.assertLessEqual(len(decoder.buffer), 1024)
        self.assertEqual(len(decoder.feed(self.frame())), 1)
        self.assertEqual(decoder.feed(b"\xff\n"), [])


if __name__ == "__main__":
    unittest.main()
