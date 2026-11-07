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
2026-11-07 当前为 M2.7：MaixCAM2 负责识别，J19 改为同序六针；Action 4 负责图传，J28 只提供 Type-C 电源。扩展板合并为一路 8A 等级的 5V 总降压，另保留 TFT、蓝牙共用的 3.3V LDO。核心板经 A48/B48 输入 5V，并用本板 AP63203 产生 3.3V；USB 隔离保留。
原生 ERC 为 0 错误、0 警告，2821 项原理图检查通过。PCB 已核对并补齐同步，一致性问题为 0；154 段走线及过孔、现有摆放保留。PCB 仍有 18 条 DRC 违规和 305 项未连接，尚未完成布线。固件未修改。
详细接线见 [M2 接口](Documentation/reference/m2-interfaces.md)，电流、MaixCAM2 六针座 1A 限制及检查边界见 [M2.7 验证](Documentation/validation/2026-11-07-m27-unified-power.md)。

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
