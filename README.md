# Coral-C5 校赛 A 题小车

STM32C5A3VGT6 主控搭配 Core、Motor 自研板，使用 MaixCAM2 测量管内钢球位置、Emm 闭环步进调整管子倾角，八路灰度和双轮编码器负责循线。接手调试请先看[当前交接](.agent/main/handoff.md)。

## 当前进度

截至 2026-11-21 整理时，最近一次实物回读与当前源码构建对应两个版本。

| 用途 | 已验证结果 | 复测入口 |
| --- | --- | --- |
| 板上第一题 · 停车移球 | 11 月 20 日 22:23 经 HC-08 回读 `8A10EDEE`。从任意球位回 O、到右 5 cm、折返并保持左 5 cm。新调相机与此固件的完整带球成绩待测。 | [烧录回读](Firmware/validation/2026-11-20/task1-restore-flash.txt) · [三轮历史轨迹](Firmware/validation/2026-11-20/task1-recovery.md) |
| 源码构建版 · 第二题循线 | Debug 为 `D97DBD0E`，保留第一题控制。第二题按 SW2 起跑，按里程停车。实车一圈 13.056 s、41808 计数，停稳距 A 的误差待尺量。 | [现场操作](Documentation/how-to/line-following-lap.md) · [实测记录](Firmware/validation/2026-11-20/task2-distance.md) |
| Motor 手工设计 | 11 月 21 日 Motor 原理图 ERC 无报项。PCB DRC 有 17 条错误、6 条警告，另有 6 条原理图一致性警告。 | [此次工程核验](Documentation/validation/2026-11-21-handoff.md) |

两份实车固件均可从仓库直接恢复。上电停在比赛首页。SW4 / SW3 上下选择，SW2 进入，松开后再按一次启动，SW5 返回。[固件存档与恢复方式](Firmware/releases/README.md)

## 项目入口

| 要做的事 | 从这里开始 |
| --- | --- |
| 接着做第一题实测 | [交接文档](.agent/main/handoff.md) · [新对话提示词](.agent/main/resume-prompt.md) |
| 构建主控、查按键、连蓝牙 | [C/LL 固件](Firmware/README.md) · [无线烧录](Documentation/how-to/wireless-update.md) · [遥测字段](Documentation/reference/firmware-telemetry.md) |
| 对照球位、相机脚本和步进行程 | [MaixCAM2 工程](Firmware/26h/README.md) · [五点与轴角标定](Firmware/validation/2026-11-19/camera640-and-shaft.json) |
| 查看赛题和实测证据 | [比赛原件及文档索引](Documentation/README.md) · [总体方案](Documentation/explanation/a-system-outline.md) · [验证索引](Documentation/validation/README.md) |
| 查看硬件、机械与采购 | [Core、Motor、Transfer](Hardware/stm32c5a3/README.md) · [机械总装与参数](Mechanical/README.md) · [Motor BOM](Documentation/reference/motor-bom.md) · [第三方资源](Hardware/stm32c5a3/THIRD_PARTY.md) |
| 查阅独立 Rust 移植 | [Rust 工程与离线验证](Firmware/Rust/README.md) · [开发环境](Documentation/how-to/rust-development.md) |

实车使用 C/LL。独立 Rust 移植的验证范围、历史电路资料和本机临时文件的归属见[项目目录说明](Documentation/reference/repository-layout.md)。
