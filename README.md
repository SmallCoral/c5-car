# Coral-C5 A 题小车

基于 STM32C5A3VGT6 的自研控制平台，当前目标是校赛 A 题“循线小车与钢球稳控装置”。
使用 Core 核心板和 CoralC5-Motor 小车底板。
网络 Expansion 是历史方案，当前仓库没有其工程文件。

## 从这里开始

| 内容 | 入口 |
| --- | --- |
| 任务、总体架构和阶段交付 | [A 题方案大纲](Documentation/explanation/a-system-outline.md) |
| 同学历史代码、闭环步进资料及参考边界 | [同学资料索引](Documentation/reference/classmate-materials.md) |
| 两路摄像头、摆杆步进和底部灰度的资料状态 | [外接模块清单](Documentation/reference/external-modules.md) |
| Motor 完整 BOM、精简采购表和替代件边界 | [BOM 说明](Documentation/reference/motor-bom.md) |
| 元器件用途与历史选型依据 | [器件选型](Documentation/reference/parts-selection.md) |
| MG513 六针接口及电平 | [电机接口](Documentation/reference/motor-interface.md) |
| 按当前Motor布局规划核心板信号 | [GPIO分配与布线建议](Documentation/reference/motor-core-pins.md) |
| 小车 3D 总装、预览与参数 | [机械模型](Mechanical/README.md) |
| M2 扩展板功能分配 | [资源与集成计划](Documentation/reference/resource-plan.md) |
| 当前 PCB 问题与打板边界 | [2026-11-10 Motor PCB 复查](Documentation/validation/2026-11-10-motor-pcb-review.md) |
| 原理图编辑 | [CoralC5-Motor 工程](Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro) |
| 固件构建入口 | [固件说明](Firmware/README.md) |

## 当前进度

整理日期：2026-11-11。
Motor 使用 M2.8P2 精简电路，PCB 已完成主要布线；11 月 10 日复查的有效焊盘网络与原理图匹配，未连接项为零。
DRC、定位孔和电源布局仍有待处理项，当前尚未达到打板发布条件；具体结果以[PCB 复查](Documentation/validation/2026-11-10-motor-pcb-review.md)为准。

三份采购表已归档，完整表与精简表的范围、库存快照日期和未落实替代件见[BOM 说明](Documentation/reference/motor-bom.md)。
固件仍为初始化后进入空循环的生成工程；整车机械仍使用历史框架，尚未完成装配及实物验证。

## 文件归属

`Documentation` 保存比赛原件、总体方案、器件依据和检查记录。
`Hardware/stm32c5a3/Hardware` 保存 Core、Motor 和共享接口。
`Firmware` 保存 STM32 配置与源码。
`Mechanical` 保存原生总装、STEP、交互预览及机械参数；板内 `Models` 用于元器件外形。
现有硬件工程与本地资源保持原路径。
当前硬件检查与采购状态从 [Motor 说明](Hardware/stm32c5a3/Hardware/CoralC5-Motor/README.md)进入，历史记录见[验证索引](Documentation/validation/README.md)，后续工作见[交接](.agent/main/handoff.md)。

第三方资源和许可见 [来源说明](Hardware/stm32c5a3/THIRD_PARTY.md)。
