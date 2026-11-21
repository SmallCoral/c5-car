# Coral-C5 校赛 A 题小车

STM32C5A3VGT6 主控用八路灰度和双轮编码器循线。MaixCAM2 上报管内钢球位置，主控根据球位和球速控制 Emm 步进电机，调节管子倾角。

## 比赛版

2026-11-21 最后一次实车回读的应用为 **EC2ED1B4**。第一题、第二题和发挥一共用这个比赛菜单。开发新题从合并工程构建，主机回归会检查已有题目。[按键与构建](Firmware/README.md) · [可恢复的实车固件](Firmware/releases/README.md)

| 已接入题目 | 操作 | 对应版本的实车成绩 |
| --- | --- | --- |
| 第一题 · 停车移球 | 等 `LEVEL OK`，按 SW2。球依次稳在 O、右 5 cm、左 5 cm | `8A10EDEE` 从右挡杆起步，用时 9.948 s，超过 8 s 目标 |
| 第二题 · 循线停车 | 车头从 A 点朝顺时针，按 SW2 起跑 | `D97DBD0E` 用时 13.203 s，现场估计提前 1～3 cm 停车 |
| 发挥一 · 定点稳球 | SW4/SW3 选点，SW2 原地稳球，此时仍可选点。再按 SW2 锁点起跑 | `EC2ED1B4` 选右 23 mm，用时 32.468 s，970 帧偏差均在 ±17 mm 内，现场估计提前约 1 cm 停车 |

[实车计时、测量口径和原始数据](Documentation/validation/README.md)

## 从哪里开始

| 要做的事 | 入口 |
| --- | --- |
| 编译、读源码、看实车按钮 | [主控 C/LL](Firmware/README.md) · [MaixCAM2](Firmware/26h/README.md) · [独立 Rust 移植](Firmware/Rust/README.md) |
| 烧录、采数、调参 | [无线升级](Documentation/how-to/wireless-update.md) · [蓝牙遥测字段](Documentation/reference/firmware-telemetry.md) · [球位和轴角标定](Firmware/calibration/camera640-and-shaft.json) |
| 查比赛要求和接线 | [赛题与设计文档](Documentation/README.md) · [现场交接](.agent/main/handoff.md) |
| 改板或改机械 | [Core、Motor、Transfer 原生 KiCad 工程](Hardware/stm32c5a3/README.md) · [机械总装与尺寸](Mechanical/README.md) |

Motor 最近一次原理图 ERC 为 0 错误、0 警告。PCB DRC 仍有 17 条错误、6 条警告，原理图一致性另有 6 条警告。[硬件检查记录](Documentation/validation/2026-11-21-handoff.md)
