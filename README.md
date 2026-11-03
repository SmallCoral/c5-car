# Coral-C5 A 题小车

基于 STM32C5A3VGT6 的自研控制平台，当前目标是校赛 A 题“循线小车与钢球稳控装置”。
保留 Core 核心板，以 CoralC5-Motor 为小车底板；网络 Expansion 是另一种底板，不与 Motor 同时插接。

## 从这里开始

| 内容 | 入口 |
| --- | --- |
| 任务、总体架构和阶段交付 | [A 题方案大纲](Documentation/explanation/a-system-outline.md) |
| 元器件用途、选型依据和采购入口 | [器件选型](Documentation/reference/parts-selection.md) |
| MG513 六针接口及电平 | [电机接口](Documentation/reference/motor-interface.md) |
| 按当前Motor布局规划核心板信号 | [GPIO分配与布线建议](Documentation/reference/motor-core-pins.md) |
| 机械总装与 3D 模型拆分 | [机械建模大纲](Documentation/explanation/mechanical-outline.md) |
| M2 扩展板功能分配 | [资源与集成计划](Documentation/reference/resource-plan.md) |
| 检查结果、问题和边界 | [布线与引脚调整记录](Documentation/validation/2026-11-03-motor-pin-remap.md) |
| 原理图编辑 | [CoralC5-Motor 工程](Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro) |
| 固件构建入口 | [固件说明](Firmware/README.md) |

## 当前进度

Motor的原理图和PCB均使用J4、J6六针电机口，J3、J5两针端子已删除。
已按最新布局调整Motor使用的GPIO，Core的物理针序及现有PCB走线保留；原理图/PCB一致性问题为0。
当前原生ERC为0错误、8警告；PCB仍有既有DRC问题和未连接项，详见最新记录。
详细状态以最新验证记录为准，不沿用旧的“全部通过”结论。

循迹、摆杆驱动、钢球检测和图传目前属于方案设计，尚未画入 Motor 原理图。
固件仍为系统初始化后进入空循环的生成工程。
当前文件不是整车可运行版或制板发布版。

## 文件归属

`Documentation` 保存比赛原件、总体方案、器件依据和检查记录。
`Hardware/stm32c5a3/Hardware` 保存 Core、Expansion、Motor 和共享接口。
`Firmware` 保存 STM32 配置与源码。
`Mechanical` 保存建模参数和模型任务单；板内 `Models` 仅用于元器件外形。
现有硬件工程不搬家，避免破坏本地库、模型及用户手工设计。

第三方资源和许可见 [来源说明](Hardware/stm32c5a3/THIRD_PARTY.md)。
