# Coral-C5 当前交接

更新日期：2026-11-16。仓库 `SmallCoral/c5-car`，分支 `main`。
本轮整理起点为 `64c5210ef6b4a552aac22b4bfe8a288ffd95149f`；交付提交与远端状态以 Git 记录为准。

## 当前状态

- 校赛 A 题使用 STM32C5A3 Core + Motor，用户已完成焊接。左右编码器焊接及 PA3 使能问题已处理，两轮主动转动、连续步进和彩屏/蓝牙联调通过。
- `Firmware` 已替换旧 MX2 空工程，使用 C5 LL/CMSIS。默认 ALL，轮 PWM 45%、步进 30 RPM；上电停止，按 RUN 联动，再按停止。界面方向为按钮在上、屏幕在下，左至右 RUN / DIR / LVL / TASK。
- 已安装无线引导程序，日常更新默认使用 HC-08。当前板最后验证的 Debug 应用为 CRC `4084B512`、18,976 B，应用位于 `0x08010000`；新构建的实际身份以生成包和查询回复为准。
- HC-08 USART1 PA6/PA7 使用 57600 8N1；Emm USART6 PC6/PC7 使用 115200 8N1、地址 1。蓝牙升级会先停止电机，未提供远程启动。
- Motor PCB 在 11 月 10 日检查后由用户继续保存修改。本轮原样纳入，未重跑 DRC。旧报告的 17 错误、6 警告及 6 一致性警告只代表当时版本。
- Transfer 是电源输入转接板，用来修正此前输入正负极画反的问题（用户于 11 月 16 日确认）。仓库快照已有 XT60PW 公母头、封装与模型，但原理图未连线、ERC 有 4 个错误，PCB 尚无布线，极性修正未在该快照中实现或验证。
- 机械总装保留历史参考框架。循线、轮速闭环、视觉标定、摆杆角度反馈和钢球稳控尚未实现。

## 从这里继续

| 内容 | 入口 |
| --- | --- |
| 项目与目录 | [首页](../../README.md)、[目录归属](../../Documentation/reference/repository-layout.md) |
| 固件与操作 | [构建和测试](../../Firmware/README.md)、[按键联调](../../Documentation/how-to/firmware-bringup.md) |
| 无线升级 | [一键脚本与恢复](../../Documentation/how-to/wireless-update.md)、[协议](../../Documentation/reference/wireless-update.md)、[实测记录](../../Documentation/validation/2026-11-16-wireless-update.md) |
| 电机与遥测 | [联动记录](../../Documentation/validation/2026-11-16-wheel-drive.md)、[遥测字段](../../Documentation/reference/firmware-telemetry.md) |
| 硬件 | [Core/Motor/Transfer 入口](../../Hardware/stm32c5a3/README.md)、[现状和待完成项](../../Documentation/reference/pending-hardware-changes.md) |
| 接口与采购 | [接线](../../Documentation/reference/m2-interfaces.md)、[GPIO](../../Documentation/reference/motor-core-pins.md)、[BOM](../../Documentation/reference/motor-bom.md) |
| 本次整理 | [提交前检查](../../Documentation/validation/2026-11-16-project-cleanup.md) |

普通改程序后，在项目根目录运行 `./Firmware/flash-wireless.ps1`；Release 加 `-Preset release`。关闭其他 BLE 客户端后再上传。J-Link 用于首次安装、引导程序修复及调试；不要单独下载应用而遗漏匹配的元数据。

## 后续开发边界

1. 先标定轮径、方向和编码器每转计数，再实现轮速闭环与灰度循线。当前 45% 为开环 PWM，不能当作速度设定值。
2. 核对 MaixCAM2 J19 线束、供电与 UART 数据，再接入视觉坐标和稳球控制；现有资料不代表视觉链路已验证。
3. 完成机械、负载与整车测试。AS5600 仍是未集成候选，采购替换仍需核对。
4. 对最新 Motor PCB 重新进行原生 DRC 和功率布局检查；实物外设运行不替代设计发布验证。
5. 无线升级已经完成真实上传和传输中断恢复；实物随机断电、Flash ECC 注入、极限镜像以及持 TASK 上电仍未做现场验证，详情见无线记录。

继续修改前读取根 AGENTS.md 和 Firmware/AGENTS.md。保留用户手工设计，不运行旧硬件生成器覆盖布局；不修改只供参考的旧项目。
原始诊断、Flash 备份、IDE 会话和 KiCad 自动历史保留本机并忽略。正式设计、日期验证记录、厂商依赖与模型均随仓库维护；先前交接可从 Git 历史查看。
