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
已按最新布局调整Motor使用的GPIO，Core的物理针序及现有PCB走线保留；此前原理图/PCB一致性问题为0；新增 M2 原理图尚未同步 PCB。
2026-11-06 已完成 M2 接口原理图：XT60、HC-08、TFT 与双安装孔、四键、灰度、双摄和外置步进驱动接口。
当前原理图为 M2.4：蓝牙固定三伏三、删除选择跳帽，J10 为两针硬件使能跳帽；其余 M2.3 电源、8P 步进和 0603 统一方案保留。
原生 ERC 为 0 错误、0 警告，原理图合同检查 1571 项通过；本轮未检查或修改 PCB。
接线见 [M2 接口](Documentation/reference/m2-interfaces.md)，[最新验证](Documentation/validation/2026-11-06-m24-fixed-bluetooth.md)及[电流预算](Documentation/validation/2026-11-06-m23-power-connectors.md)保留结果与边界。
详细状态以最新验证记录为准，不沿用旧的“全部通过”结论。

上述模块的电源与控制接口已画入 Motor；实际模块功能、外置步进驱动、相机程序与闭环控制尚未完成。
固件仍为系统初始化后进入空循环的生成工程。
已生成首版小车 3D 框架，包含参考轮组、电池定位框、实际 Core、99×99 mm Motor 规划外形、摆杆执行机构及双摄支架。检测端按用户确认的 MaixCAM 规划，图传端沿用 OV2640 方案；机械布局尚未完成实物装配与加工验证。
当前文件不是整车可运行版或制板发布版。

## 文件归属

`Documentation` 保存比赛原件、总体方案、器件依据和检查记录。
`Hardware/stm32c5a3/Hardware` 保存 Core、Expansion、Motor 和共享接口。
`Firmware` 保存 STM32 配置与源码。
`Mechanical` 保存原生总装、STEP、交互预览及机械参数；板内 `Models` 用于元器件外形。
现有硬件工程不搬家，避免破坏本地库、模型及用户手工设计。

第三方资源和许可见 [来源说明](Hardware/stm32c5a3/THIRD_PARTY.md)。
