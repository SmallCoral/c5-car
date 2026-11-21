# 目录与维护入口

| 目录 | 用途 |
| --- | --- |
| [Firmware](../../Firmware/README.md) | 实车 C/LL 应用、引导程序、固定版本厂商库和编译配置 |
| [Firmware/tests](../../Firmware/tests) | 自动回归和两条实车回放输入，比赛镜像构建会运行主机 C 测试 |
| [Firmware/releases](../../Firmware/releases/README.md) | 三个已接入题目的实车恢复包及调试符号 |
| [Firmware/26h](../../Firmware/26h/README.md) | MaixCAM2 识别与图传。设备模型单独部署 |
| [Firmware/Rust](../../Firmware/Rust/README.md) | 独立 Cargo 工作区、离线测试和主机端工具 |
| [Hardware](../../Hardware/stm32c5a3/README.md) | Core、Motor、Transfer 三个 KiCad 工程，板级库、设计检查输入、接口合同与采购表 |
| [Mechanical](../../Mechanical/README.md) | 参数、FreeCAD 源模型、STEP 和装配预览 |
| [Documentation](../README.md) | 赛题、接线、操作、协议及主要实车记录 |
| [.agent/main](../../.agent/main/handoff.md) | 继续实车开发要用的标定与最新状态 |

本机虚拟环境、IDE 状态、构建产物、热点配置、一次性抓包和临时工作副本由 Git 忽略。新题在合并固件上开发，增加题目入口时同步补充回归。实车定版后保留恢复包，记录应用 CRC32、长度和实际测量条件。
