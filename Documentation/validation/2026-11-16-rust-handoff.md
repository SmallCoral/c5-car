# 2026-11-16 Rust、硬件快照与交接整理

起点提交：`b9aaa695109ca046874f67017f2174d682bed09d`，分支 `main`。
本轮按要求整理并推送独立 Rust 版、文档和 7 个手工硬件文件；源码版本以包含本记录的 Git 提交为准。
交接入口为 [`.agent/main/handoff.md`](../../.agent/main/handoff.md)。

## 整理结果

| 范围 | 结果 |
| --- | --- |
| Rust 源码 | `Firmware/Rust` 独立保存应用、引导程序、电脑端工具和固定测试样本；原 C/Python 源码保留 |
| RustRover | 三份可移植 `.run` 配置，工具链声明补齐 `rust-src`；修复 Windows 下固件入口错误 |
| 文档 | 根/固件/硬件/文档索引、Rust 开发说明、验证记录和完整交接同步更新 |
| 硬件 | 原样保存 7 个手工修改文件，整理前后逐文件 SHA-256 不变；仅在副本中导出和检查 |
| 本机状态 | `.scratch`、`.idea`、Rust `target`/`dist`、下载器和蓝牙会话不纳入提交 |

文档采用 `docs update`，交接采用 `handoff` 的完整模式，共六节。
未修改电路、PCB 布局布线、机械模型或厂家资料，也未执行烧录或电机操作。

## Rust 暂存源码独立验证

工具：Rust/Cargo 1.100.0、rustfmt 1.11.0、Clippy 0.1.100、Python 3.12.14。
主机为 `x86_64-pc-windows-gnu`，使用 CLion 2026.2.4.1 的 MinGW；ARM 为 `thumbv8m.main-none-eabihf`，使用 Rust LLD。
从 Git 暂存区导出 `Firmware/Rust` 到新目录，使用空构建目录、已安装的编译器和依赖缓存执行以下检查。
导出不含 `.scratch`、`.idea`、已有产物，也不依赖原 C/Python 源码生成样本。

| 检查 | 结果与证据 |
| --- | --- |
| 全 workspace 本机检查，启用 `radio` | 通过，[日志](2026-11-16-rust-handoff/host-check.txt) |
| 主机测试 | 30 项通过、0 失败，[日志](2026-11-16-rust-handoff/host-tests.txt) |
| rustfmt | 通过，[日志](2026-11-16-rust-handoff/format.txt) |
| 主机与 ARM Clippy，`-D warnings` | 通过，[主机](2026-11-16-rust-handoff/host-clippy.txt)、[ARM](2026-11-16-rust-handoff/arm-clippy.txt) |
| ARM Debug 应用和引导程序 | 通过，[日志](2026-11-16-rust-handoff/arm-debug.txt) |
| ARM Release 应用和引导程序 | 通过，[日志](2026-11-16-rust-handoff/arm-release.txt) |

精确命令与退出码见 [build-results.json](2026-11-16-rust-handoff/build-results.json)。
之前的 27 帧 C UI 对照、OTA 固定样本、恢复故障注入与 ELF 检查见 [Rust 验证记录](../../Firmware/Rust/VALIDATION.md)。
本轮只验证代码与构建；Rust 版尚未实际烧录、运行电机或执行 BLE 升级。

## 硬件快照与检查

7 个原样纳入的文件：

- Core：`02_Power_USB.kicad_sch`、`03_MCU.kicad_sch`、`04_Interface.kicad_sch`、`CoralC5-Core.kicad_sch`、`CoralC5-Core.kicad_pcb`。
- Motor：`10_Lichee_Interface.kicad_sch`。
- Transfer：`Transfer.kicad_sch`。

使用 KiCad CLI 10.0.6 对复制出的三个完整原理图层级导出原生网表并运行 ERC。
Motor/Transfer 的 `tools/verify_schematic.py` 在副本中读取新网表和 ERC，生成报告；原工程中既有日期记录未被覆盖。
Core 无独立的 `tools/verify_schematic.py`；Shared 中的旧 `verify_split.py` 依赖当前不存在的 Expansion 工程，不冒充当前 Core 的资源验证。

| 工程 | 原生 ERC | 现有检查脚本 | 本次来源与命令 |
| --- | --- | --- | --- |
| Core | 0 错误、0 警告，退出 0 | 无当前独立脚本 | [快照](../../Hardware/stm32c5a3/Hardware/CoralC5-Core/docs/handoff-2026-11-16/snapshot.json)、[ERC](../../Hardware/stm32c5a3/Hardware/CoralC5-Core/docs/handoff-2026-11-16/erc.json) |
| Motor | 0 错误、0 警告，退出 0 | 957 项，4 项失败，退出 1 | [快照](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/handoff-2026-11-16/snapshot.json)、[检查报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/handoff-2026-11-16/verification.json) |
| Transfer | 0 错误、0 警告，退出 0 | 28 项，4 项失败，退出 1 | [快照](../../Hardware/stm32c5a3/Hardware/Transfer/docs/handoff-2026-11-16/snapshot.json)、[检查报告](../../Hardware/stm32c5a3/Hardware/Transfer/docs/handoff-2026-11-16/verification.json) |

Motor 失败项为 Core MCU 原理图应与旧基线相同、Motor PCB 应与旧基线相同、“只有 XT60 精确模型缺失”的旧假设、全部 Motor UUID 关联保留。
前三项包含历史合同与当前手工保存状态的差异；UUID/PCB 关联仍需单独核对，不能当作已解决。
没有修改脚本或设计来消除这些失败。

Transfer 新网表确认 `J1:1 ↔ J2:2`、`J1:2 ↔ J2:1`。
旧脚本仍要求不连线且恰有四个悬空引脚；此外 J1/J2 嵌入符号与本地符号库不一致，四项失败均保留。
PCB 同步、布线与实物极性验证尚未完成，详见 [Transfer 当前说明](../../Hardware/stm32c5a3/Hardware/Transfer/README.md)。

原生网表、ERC、退出码、stdout、源码摘要与脚本输出分别存于各工程 `docs/handoff-2026-11-16`。
脚本报告中的日期、源码基线和限制文字部分属于历史合同；本轮身份以同目录 `snapshot.json` 为准。
首次复制快照遇到 Windows 长路径错误，缩短临时目录并使用扩展路径后完成；此失败发生在检查之前，没有更改原设计。
本轮没有运行 PCB DRC，也没有做新的电气、极性或机械实物验证。

## 提交检查与交付

提交前检查 Git 暂存区的当前文档本地链接、常见凭据特征、被忽略文件、差异空白和 7 个硬件文件的保护摘要。
记录见 [repository-audit.json](2026-11-16-rust-handoff/repository-audit.json)；历史资料的既有缺失引用保留，不视为当前入口通过。
检查不验证外部网页可用性和页内锚点，也不等同于覆盖所有敏感信息类型。

当前项目文档 638 个本地目标均可解析，4571 个暂存文本文件未匹配常见凭据特征，索引中没有被忽略文件。
历史归档仍有 171 个、厂商说明仍有 4 个既有缺失引用，保留原始资料。
完整 `git diff --cached --check` 返回 2，仅原始 `host-tests.txt` 的末尾空行产生 1 项提示；排除该日志后返回 0，源码与文档没有空白错误。

共享 RustRover 配置只保留 Cargo 命令与 `$PROJECT_DIR$`，具体工具链和环境仍由本机配置。
交接文档已写入文件；Windows 环境无 `pbcopy`，未修改剪贴板。
推送完成以本地 `HEAD` 与 `git ls-remote origin refs/heads/main` 一致为准，不改写既有 Git 历史。
