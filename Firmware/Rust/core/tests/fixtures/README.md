# 原版兼容性样本

2026-11-16 从迁移基线 `b9aaa695109ca046874f67017f2174d682bed09d` 的原版实现生成。Rust 测试直接读取固定样本，不需要 C 或 Python 环境。

- `v1_header.bin`：原 `Firmware/tools/ota_protocol.py::image_header` 的输出。镜像长度 777，填充 `0x55`，前两个小端字为 `0x2003FFE0` 和 `0x08010241`。
- `v1_begin.bin`：上述头部作为 BEGIN payload，原 Python `Frame` 编码，序号 `0x89ABCDEF`。
- `ui_crc32.txt`：原 `Firmware/src/ui.c`、`logic.c`、`ota.c` 在 Windows MinGW 下生成的 27 个 RGB565 帧 CRC32。先按任务 0..5、模式 0..3 遍历，模式两位分别表示运行/反向；模式 2 左轮故障，模式 3 Emm `E2`。固定编码计数 1284/-1279、速度 456/-452、灰度 `E7`、A/B `9`、45%、30 RPM、2400 ms。最后三个是 RECEIVING 进度 0/450/900，总量 1000。完整输入由 `ui_matches_27_legacy_c_renderings` 明确列出。

生成程序与核查记录保留在本机 `.scratch/rust_oracle.py`；它不是正常构建或测试依赖。修改 UI 设计或协议时应重新审查兼容要求，不能用 Rust 自身输出覆盖样本来消除差异。
