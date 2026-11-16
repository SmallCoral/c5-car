# 项目目录与提交范围

当前使用 Core + Motor 进行校赛 A 题实物联调。用途与状态从[根 README](../../README.md)进入，具体任务从[文档索引](../README.md)进入。

| 目录 | 内容与维护入口 |
| --- | --- |
| `Firmware` | 当前 LL 应用、无线引导程序、固定厂商依赖、主机测试和 BLE 工具；[固件说明](../../Firmware/README.md) |
| `Firmware/validation` | 实物遥测、烧录输出、测试日志、源码/产物哈希；按日期保存，历史记录不改写成当前结果 |
| `Hardware/stm32c5a3/Hardware` | Core、Motor、Transfer 的 KiCad 原生工程、随板库和模型；[硬件说明](../../Hardware/stm32c5a3/README.md) |
| `Hardware/.../Shared` | Core/Motor 的 98 触点合同、共用参考与设计工具 |
| `Mechanical` | 机械参考总装、模型、参数、生成器及验证；[机械说明](../../Mechanical/README.md) |
| `Documentation/how-to` | 编译、接线、联调、故障定位和无线升级操作步骤 |
| `Documentation/reference` | 接口、GPIO、遥测、升级协议、采购和资料索引 |
| `Documentation/explanation` | 比赛总体方案、机械与控制设计依据 |
| `Documentation/validation` | 按日期记录工具、版本、检查结果与未验证项；[验证索引](../validation/README.md) |
| `Documentation` 其他资料目录 | 赛题原件、厂家文档和同学参考代码；保留原始来源与历史边界 |
| `Images` | 项目图片；[图片索引](../../Images/README.md) |
| `.agent/main` | 当前交接和历史交接；[继续开发](../../.agent/main/handoff.md) |

## 当前固件入口

旧 MX2 生成工程已由 LL 工程取代，不再作为构建入口；旧版本仍可从 Git 历史恢复。不要重新生成 MX2 工程覆盖当前 `Firmware`。

普通构建与测试不会连接板子；`flash-wireless.ps1` 和 CMake `wireless_flash` 才会进行无线烧录。首次安装和引导程序救援使用合并包，见[无线升级说明](../how-to/wireless-update.md)。

## 不纳入 Git 的本机文件

`.gitignore` 排除构建目录、Python 环境、`.scratch`、IDE 会话、KiCad `.kicad_prl`/锁文件/自动历史，以及 `.agent/diagnostics` 中的原始 USB 扫描和 Flash 备份。这些本机文件仍保留在磁盘上。

KiCad 自动历史、`.kicad_prl` 和两份 Python 字节码曾被跟踪，2026-11-16 起停止跟踪；旧提交中的历史不改写。正式设计、模型、BOM、厂家资料以及用于解释检查结论的日期记录继续提交。

## 验证边界

工程构建、主机测试、原理图 ERC、PCB DRC 和实物运行分别记录。整理文档或保存硬件文件不意味着完成新的硬件验证。采购表保留查询日期，历史快照不代表当前库存。
