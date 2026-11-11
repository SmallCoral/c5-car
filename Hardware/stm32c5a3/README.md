# Coral-C5 硬件

当前校赛 A 题使用 CoralC5-Core 核心板和 CoralC5-Motor 底板。
总体状态从[项目入口](../../README.md)查看。

| 内容 | 入口 |
| --- | --- |
| Motor 原理图和 PCB | [工程](Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro)、[说明](Hardware/CoralC5-Motor/README.md) |
| Motor 当前 PCB 问题 | [2026-11-10 复查](../../Documentation/validation/2026-11-10-motor-pcb-review.md) |
| Motor 采购表 | [完整 BOM 与精简采购表](../../Documentation/reference/motor-bom.md) |
| Core 原理图和 PCB | [工程](Hardware/CoralC5-Core/CoralC5-Core.kicad_pro)、[说明](Hardware/CoralC5-Core/README.md) |
| 两板触点定义 | [接口说明](Hardware/Shared/interface.md)、[98 触点合同](Hardware/Shared/interface.csv) |
| Motor 外接接口和 GPIO | [接线表](../../Documentation/reference/m2-interfaces.md)、[GPIO 分配](../../Documentation/reference/motor-core-pins.md) |
| 固件 | [配置与构建](../../Firmware/README.md) |
| 第三方资源 | [来源与许可](THIRD_PARTY.md) |

Motor 的 J1 是 XT60 十二伏输入，使用双路 AP64500 五伏降压和双路 DRV8874 电机驱动。
Core 经 C5-EDGE-3 的 98 触点与底板连接；这是自定义 GPIO/电源接口，不能插入电脑 PCIe 插槽。
各工程的本地符号、封装、Models 和库表需一同保留。

网络 Expansion 属于历史方案，当前仓库不含其 KiCad 工程。
历史 Core/Expansion 的检查结果保留在 Shared 和 Core 的记录中，不能代替当前 Motor 的 PCB 检查。
本轮项目整理未修改 Core、Motor 电路、PCB 布局布线、固件或机械设计。
