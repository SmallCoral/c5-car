# Coral-C5 A 题小车

基于 STM32C5A3VGT6 的自研控制平台，当前目标是校赛 A 题“循线小车与钢球稳控装置”。
使用 Core 核心板和 CoralC5-Motor 小车底板，包含硬件设计、LL 固件、机械参考和实物验证记录。

## 从这里开始

| 内容 | 入口 |
| --- | --- |
| 编译、按钮操作、当前固件 | [固件说明](Firmware/README.md) |
| 日常无线烧录、CLion 与恢复 | [HC-08 无线升级](Documentation/how-to/wireless-update.md) |
| 原理图、PCB、模型与 Transfer 电源转接板 | [硬件入口](Hardware/stm32c5a3/README.md) |
| 小车 3D 总装、预览与参数 | [机械模型](Mechanical/README.md) |
| 比赛任务与控制方案 | [A 题大纲](Documentation/explanation/a-system-outline.md) |
| 接口、采购、厂家及同学资料 | [文档总索引](Documentation/README.md) |
| 已验证结果和剩余限制 | [验证记录](Documentation/validation/README.md) |
| 目录归属与提交范围 | [项目结构](Documentation/reference/repository-layout.md) |

## 当前进度

更新日期：2026-11-16。
用户已完成 Core、Motor 焊接，接入屏幕、HC-08、Emm 步进、灰度和两只编码电机，进入实物联调。
LL 固件支持四键彩屏、轮 PWM/编码器、步进连续旋转、灰度输入和蓝牙遥测。上电进入 ALL 并保持停止；最左侧 RUN 按一次联动，再按一次停止。默认轮 PWM 45%、步进 30 RPM；运动实测范围见[联动记录](Documentation/validation/2026-11-16-wheel-drive.md)。

当前板已安装无线升级引导程序，两次 HC-08 完整上传与传输中断恢复通过实测；日常更新使用[无线烧录](Documentation/how-to/wireless-update.md)，J-Link 保留用于首次安装、调试与救援。当前尚未完成轮速闭环、循线、视觉标定和钢球稳控。

三份采购表已归档，完整表与精简表的范围、库存快照日期和未落实替代件见[BOM 说明](Documentation/reference/motor-bom.md)。
11 月 10 日 [PCB 复查](Documentation/validation/2026-11-10-motor-pcb-review.md)描述当时版本。Motor PCB 此后由用户继续保存修改，本轮整理原样纳入，未重新运行 DRC；实物联调不能替代 PCB 发布验证。

## 文件归属

`Documentation` 保存比赛原件、总体方案、器件依据和检查记录。
`Hardware/stm32c5a3/Hardware` 保存 Core、Motor 和共享接口。
`Firmware` 保存 LL 固件、固定厂商库、测试与电脑接收工具。
`Mechanical` 保存原生总装、STEP、交互预览及机械参数；板内 `Models` 用于元器件外形。
现有硬件工程与本地资源保持原路径。网络 Expansion 是历史方案，当前仓库不含其工程；Transfer 用于修正原电源输入正负极画反的问题，仓库中的设计仍待完成连线与验证。
当前硬件检查与采购状态从 [Motor 说明](Hardware/stm32c5a3/Hardware/CoralC5-Motor/README.md)进入，历史记录见[验证索引](Documentation/validation/README.md)，后续工作见[交接](.agent/main/handoff.md)。

第三方资源和许可见 [来源说明](Hardware/stm32c5a3/THIRD_PARTY.md)。
