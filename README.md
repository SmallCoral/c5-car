# Coral-C5 A 题小车

基于 STM32C5A3VGT6 的自研控制平台，当前目标是校赛 A 题“循线小车与钢球稳控装置”。
保留 Core 核心板，以 CoralC5-Motor 为小车底板；网络 Expansion 是另一种底板，不与 Motor 同时插接。

## 从这里开始

| 内容 | 入口 |
| --- | --- |
| 任务、总体架构和阶段交付 | [A 题方案大纲](Documentation/explanation/a-system-outline.md) |
| 同学历史代码、闭环步进资料及参考边界 | [同学资料索引](Documentation/reference/classmate-materials.md) |
| 两路摄像头、摆杆步进和底部灰度的资料状态 | [外接模块清单](Documentation/reference/external-modules.md) |
| 元器件用途、选型依据和采购入口 | [器件选型](Documentation/reference/parts-selection.md) |
| MG513 六针接口及电平 | [电机接口](Documentation/reference/motor-interface.md) |
| 按当前Motor布局规划核心板信号 | [GPIO分配与布线建议](Documentation/reference/motor-core-pins.md) |
| 小车 3D 总装、预览与参数 | [机械模型](Mechanical/README.md) |
| M2 扩展板功能分配 | [资源与集成计划](Documentation/reference/resource-plan.md) |
| 检查结果、问题和边界 | [布线与引脚调整记录](Documentation/validation/2026-11-03-motor-pin-remap.md) |
| 原理图编辑 | [CoralC5-Motor 工程](Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro) |
| 固件构建入口 | [固件说明](Firmware/README.md) |

## 当前进度

Motor的原理图和PCB均使用J4、J6六针电机口，J3、J5两针端子已删除。
2026-11-08 当前为 M2.8P 模块精简版：142个原理图实体、184个网络、39路GPIO；按当前模块摆放调整了25路GPIO，Core的98触点针序不变。原生ERC为0错误0警告，原理图955项核对与十页渲染检查通过。Motor PCB保留用户当前内容，尚未同步这次GPIO重分配。
Core的43个电气器件已补齐原理图关联路径，原理图、布局及铜线均未改动。未运行本轮PCB DRC或整车实测，当前不能据原理图检查判定可制板。最新结果见[接口规划与Core关联](Documentation/validation/2026-11-08-layout-pinplan.md)、[验证记录](Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)、[交接与续接](.agent/main/handoff.md)。

上述模块的电源与控制接口已画入 Motor；实际模块功能、外置步进驱动、相机程序与闭环控制尚未完成。
固件仍为系统初始化后进入空循环的生成工程。
已生成首版小车 3D 框架，包含参考轮组、电池定位框、实际 Core、99×99 mm Motor 规划外形、摆杆执行机构及双摄支架。首版机械模型仍基于旧相机占位，尚未按最新实际图传模块重建或完成装配、加工验证。
当前文件不是整车可运行版或制板发布版。

## 文件归属

`Documentation` 保存比赛原件、总体方案、器件依据和检查记录。
`Hardware/stm32c5a3/Hardware` 保存 Core、Expansion、Motor 和共享接口。
`Firmware` 保存 STM32 配置与源码。
`Mechanical` 保存原生总装、STEP、交互预览及机械参数；板内 `Models` 用于元器件外形。
现有硬件工程不搬家，避免破坏本地库、模型及用户手工设计。

第三方资源和许可见 [来源说明](Hardware/stm32c5a3/THIRD_PARTY.md)。
