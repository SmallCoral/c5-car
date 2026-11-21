# 验证遥测工具在旧版、坏行及任意分片边界下仍可还原有效数据。

import csv
import importlib.util
import io
import zlib
from contextlib import redirect_stdout
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "monitor", Path(__file__).parents[1] / "tools" / "monitor.py"
)
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)


# 覆盖遥测版本兼容、丢包恢复和派生字段。
class MonitorTest(unittest.TestCase):
    # 生成一条旧版遥测供兼容性对照。
    def frame(self, version=6):
        length = monitor.VERSION_LENGTHS[version]
        return (f"C5,{version}," + ",".join("0" for _ in monitor.FIELDS[2:length]) + "\r\n").encode()

    # 检查旧字段集仍能解析。
    def test_previous_version(self):
        for version in (1, 2, 3, 4, 5):
            record, = monitor.Decoder().feed(self.frame(version))
            self.assertEqual(record["version"], version)
            self.assertEqual(record["step_rpm_valid"], 0)
            self.assertEqual(record["gray_status"], 0)
            self.assertEqual(record["lap_status"], 0)
            self.assertEqual(record["lap_control_ms"], 0)
            self.assertEqual(record["lap_marker_hits"], 0)

    # 用相邻轮速与球位核对有符号加速度和偏差。
    def test_offset_acceleration_and_signed_error(self):
        # 构造带版本和校验字段的测试行。
        def packet(seq, at, speed, tracked):
            values = self.frame(7).decode().strip().split(",")
            fields = {"seq": seq, "ms": at, "offset_phase": 3, "target_mm": -18, "ball_mm": -23,
                      "ball_status": 2 if tracked else 1, "wheel_sample_ms": at,
                      "left_cps": speed, "right_cps": speed}
            for field, value in fields.items():
                values[monitor.FIELDS.index(field)] = str(value)
            payload = ",".join(values[:-1]).encode()
            return payload + f",{zlib.crc32(payload):08X}\r\n".encode()

        output = io.StringIO()
        sink = monitor.Sink(output)
        good = packet(1, 1000, 100, True)
        with redirect_stdout(io.StringIO()):
            sink.accept(good[:17])
            sink.accept(good[17:] + packet(2, 1100, 130, False))
            sink.accept(packet(3, 1200, 110, True).replace(b"-18", b"-19"))
            sink.accept(packet(4, 1300, 90, True))
        a, b, c = list(csv.DictReader(io.StringIO(output.getvalue())))
        self.assertEqual((a["ball_error_mm"], a["mean_accel_cps2"]), ("-5", ""))
        self.assertEqual((b["ball_mm"], b["ball_error_mm"], b["mean_accel_cps2"]), ("", "", "300"))
        self.assertEqual(c["mean_accel_cps2"], "")
        self.assertEqual(sink.decoder.invalid, 1)

    # 校验灰度字段的解码。
    def test_gray_frame(self):
        values = self.frame().decode().strip().split(",")
        fields = {"task": 6, "gray_raw": 231, "gray_black": 24, "gray_line": 24,
                  "gray_count": 2, "gray_groups": 1, "gray_position": 0, "gray_status": 2}
        for field, value in fields.items():
            values[monitor.FIELDS.index(field)] = str(value)
        record, = monitor.Decoder().feed((",".join(values) + "\n").encode())
        self.assertEqual(monitor.TASKS[record["task"]], "GRAY")
        for field, value in fields.items():
            self.assertEqual(record[field], value)

    # 将版本与列数错配，验证解析器拒绝坏行。
    def test_version_length_mismatch(self):
        decoder = monitor.Decoder()
        self.assertEqual(decoder.feed(self.frame().replace(b"C5,6", b"C5,5")), [])
        self.assertEqual(decoder.feed(self.frame(5).replace(b"C5,5", b"C5,6")), [])
        self.assertEqual(decoder.invalid, 2)

    # 核对循线扩展字段。
    def test_lap_frame(self):
        values = self.frame().decode().strip().split(",")
        for field, value in {"task": 7, "running": 1, "lap_status": 1,
                             "lap_left_pwm": 120, "lap_right_pwm": 160,
                             "lap_position": -320, "lap_position_rate": 4500,
                             "lap_steering": -120, "lap_differential": 80,
                             "lap_base_target": 1700, "lap_control_ms": 12,
                             "lap_departing": 1, "lap_marker_hits": 2, "lap_marker_peak": 8,
                             "lap_ms": 2300, "lap_counts": 2200, "lap_armed": 1}.items():
            values[monitor.FIELDS.index(field)] = str(value)
        record, = monitor.Decoder().feed((",".join(values) + "\n").encode())
        self.assertEqual(monitor.TASKS[record["task"]], "LAP")
        self.assertEqual(record["lap_status"], 1)
        self.assertEqual(record["lap_right_pwm"], 160)
        self.assertEqual(record["lap_position_rate"], 4500)
        self.assertEqual(record["lap_steering"], -120)
        self.assertEqual(record["lap_control_ms"], 12)
        self.assertEqual(record["lap_departing"], 1)
        self.assertEqual(record["lap_marker_hits"], 2)
        self.assertEqual(record["lap_marker_peak"], 8)

    # 检查联合运动任务的遥测值。
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

    # 构造列数正确但标志不合法的拼接数据。
    def test_spliced_frame_with_valid_column_count(self):
        # 蓝牙曾把两行 v6 数据拼接成一行，出现 armed=1313、departing=97 等异常字段。
        for field, value in (("lap_armed", 1313), ("lap_departing", 97)):
            values = self.frame().decode().strip().split(",")
            values[monitor.FIELDS.index(field)] = str(value)
            decoder = monitor.Decoder()
            self.assertEqual(decoder.feed((",".join(values) + "\n").encode()), [])
            self.assertEqual(decoder.invalid, 1)
            self.assertEqual(len(decoder.feed(self.frame())), 1)

    # 遍历可能的分片边界，检查解析与一次性输入一致。
    def test_every_fragment_boundary(self):
        frame = self.frame()
        for split in range(len(frame)):
            decoder = monitor.Decoder()
            self.assertEqual(decoder.feed(frame[:split]), [])
            self.assertEqual(len(decoder.feed(frame[split:])), 1)

    # 混合噪声、连续帧及损坏数据，检查能恢复到下一帧。
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
