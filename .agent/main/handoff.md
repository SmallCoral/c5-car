# Coral-C5 交接：独立 Rust 版与当前硬件快照

更新日期：2026-11-16。仓库 `SmallCoral/c5-car`，分支 `main`。
本轮起点为 `b9aaa695109ca046874f67017f2174d682bed09d`；交付版本以包含本文件的 Git 提交为准。
本文面向没有本次对话记录的接手者，覆盖项目现状、开发方法与剩余工作。

## 目标与完成条件

校赛 A 题使用 STM32C5A3 Core + Motor，实现循线小车与钢球稳控装置。
本轮交付独立 Rust 代码、可复现构建入口、当前手工硬件快照和文档；GitHub `main` 与本地交付提交一致即完成本轮同步。
Rust 的实物联调不在本轮范围内。

## 当前状态

- **已实测，C/LL 版：** 双编码器已修复焊接问题，两轮主动转动、Emm 连续旋转、彩屏和 HC-08 联调通过。J10 接通轮驱动使能；拔下仍可手转读编码器。默认 ALL，上电停止，RUN 启动，再按任意键停止；两轮 45% PWM、步进 30 RPM。
- **已实测，C/LL 无线升级：** 板上已装引导程序，两次完整上传和传输中断恢复通过。最后记录的 Debug 应用为 18,976 B、CRC `4084B512`，地址 `0x08010000`；后续镜像身份以实际构建包和设备查询为准。
- **已离线验证，Rust 版：** `Firmware/Rust` 包含 `core`、`device`、`host`；应用、引导程序、BLE/串口工具与原协议迁移完成。30 项主机测试、Clippy、Windows workspace 检查及 ARM Debug/Release 编译通过。Rust 未烧录、未实测，不能沿用 C 版的实测结论。
- **已修复，RustRover：** Rust 1.100.0、标准库源码和 Cargo 工程已关联；`app`/`boot` 的 Windows 条件编译错误已修复。ARM 使用无标准库固件入口；误在电脑运行固件时打印正确构建命令并返回 1。三份共享 `.run` 配置随仓库维护。
- **已导出检查，硬件：** 本轮原样收录 7 个手工修改文件。Core、Motor、Transfer 原生 ERC 均为 0 错误、0 警告；Motor 检查脚本 957 项中 4 项失败，Transfer 28 项中 4 项失败，原始结果已保留。PCB DRC 未运行。
- **已确认，Transfer 用途：** 修正原电源输入正负极画反。新网表确认 `J1:1 ↔ J2:2`、`J1:2 ↔ J2:1`；嵌入符号与本地符号库不一致，PCB 同步、布线和实物极性待验证。
- **尚未实现：** 轮速闭环、灰度循线、视觉标定、摆杆角度反馈和钢球稳控。45% 是开环占空比，不是速度目标。

## 下一步

1. 按 [Rust 开发说明](../../Documentation/how-to/rust-development.md)打开 `Firmware/Rust/Cargo.toml`，运行 `Coral Host Check`、`Coral ARM App Build` 和 `Coral ARM Boot Build`，确认接手环境可用。
2. 继续 Rust 开发时保持原协议、按键启动和故障停机行为；修改协议/UI 时运行固定兼容样本测试。不要用 Rust 自己的新输出覆盖原 C/Python 样本来消除差异。
3. 先核对本轮[硬件失败项](../../Documentation/validation/2026-11-16-rust-handoff.md)：Motor UUID/PCB 关联、历史基线假设，以及 Transfer 嵌入符号差异。核实实际设计意图后再更新合同或设计，不能直接关闭检查。
4. 完成 Transfer PCB 同步、布线、原生 DRC 和实物极性检查；对最新 Core/Motor PCB 重新做 DRC。保留设计位号、UUID 和布局约束，不重跑生成器覆盖手工设计。
5. 用户另行安排 Rust 实物联调后，再逐项核查时钟、中断、屏幕方向、双编码器、PWM、Emm、遥测和无线恢复。先阅读[寄存器与运行时边界](../../Firmware/Rust/device/registers-source.md)及 Rust 未验证范围。
6. 控制开发先标定轮径、方向和编码器每转计数，再做轮速闭环、循线；确认 MaixCAM2 的 J19 线束、供电和 UART 实测数据后接入视觉与稳球。AS5600 等候选尚未形成已验证实现。

## 关键文件

| 文件 | 用途 |
| --- | --- |
| [根 README](../../README.md) | 项目状态、C/Rust 与硬件入口 |
| [Firmware/README.md](../../Firmware/README.md) | 原 C/LL 构建、按键及已实测操作 |
| [Firmware/Rust/README.md](../../Firmware/Rust/README.md) | Rust 结构、命令与工具接口 |
| [Firmware/Rust/VALIDATION.md](../../Firmware/Rust/VALIDATION.md) | 30 项测试、兼容样本、内存/ELF 检查和实物边界 |
| [Firmware/Rust/core](../../Firmware/Rust/core) | 可移植的任务、协议、OTA、遥测和 UI；禁止 unsafe |
| [Firmware/Rust/device/src/app.rs](../../Firmware/Rust/device/src/app.rs) | MCU 业务循环；入口在同目录的 `bin/app.rs`，引导入口为 `bin/boot.rs` |
| [Firmware/Rust/host/src/main.rs](../../Firmware/Rust/host/src/main.rs) | 电脑端 CLI，不是 MCU 主函数 |
| [Firmware/Rust/.run](../../Firmware/Rust/.run) | 可共享的检查与 ARM 构建配置，不含烧录命令 |
| [本轮整理记录](../../Documentation/validation/2026-11-16-rust-handoff.md) | 暂存源码独立构建、硬件快照、失败项与提交检查 |
| [硬件入口](../../Hardware/stm32c5a3/README.md) | Core/Motor/Transfer 原生设计与本地资源 |
| [接口合同](../../Hardware/stm32c5a3/Hardware/Shared/interface.csv) | 98 触点；电机六针线序另见 [motor-interface.md](../../Documentation/reference/motor-interface.md) |
| [无线升级步骤](../../Documentation/how-to/wireless-update.md) | 已实测 C 版的日常更新和恢复；[协议](../../Documentation/reference/wireless-update.md) |

## 约束与易错点

- 开始修改前读取根 `AGENTS.md` 和 `Firmware/AGENTS.md`。Rust 按用户要求单独放在 `Firmware/Rust`，当前只改代码、不实际烧录；原 C/Python 工具和厂家资料保留。
- `core` 为 no_std 可移植代码，`device` 使用 C5A3 寄存器与 ARM 启动代码，`host` 依赖电脑端 std。不要把整个 workspace 都按 ARM 目标构建。
- Rust 不链接 C LL/HAL，寄存器常量按固定 ST DFP/LL 头文件核对。MCU 运行时少量汇编和 unsafe 边界不应替换成其他 STM32 系列常量。
- 按照片方向“按钮在上、屏幕在下”，左至右 RUN / DIR / LEVEL / TASK。蓝牙为 USART1 PA6/PA7、57600 8N1；Emm 为 USART6 PC6/PC7、115200 8N1、地址 1。
- 引导区 `0x08000000`、应用 `0x08010000`、暂存 `0x08080000`，元数据 `0x080FC000` / `0x080FE000`。协议 CRC 校验完整性，不提供签名认证。Flash/ECC 和真实断电恢复未做 Rust 实测。
- `.scratch` 工具链、`.idea`、`target`、`dist`、设备原始诊断和 KiCad 本机历史不提交。共享 `.run`、Cargo.lock、固定测试样本必须保留。
- 原理图 ERC、资源/合同检查、PCB DRC、实物验证分别报告。旧报告中的日期、基线或“未实现固件”等文字是旧检查合同，不代表当前固件状态；本次执行时间和源码摘要见各板 `docs/handoff-2026-11-16/snapshot.json`。
- 旧项目 `D:/dev/qrs/stm32/Coral-C5` 只用于参考，不修改。此前交接可从 Git 历史查阅。

## 构建与验证

新机器按 [Rust 工具链步骤](../../Documentation/how-to/rust-development.md)安装 Rust 1.100.0、组件和本机链接器。
本机可先在仓库根目录执行 `. ./.scratch/rust-env.ps1` 加载隔离环境，再执行：

```powershell
cd Firmware/Rust
cargo check --workspace --all-targets --features radio --locked
cargo test --workspace --features radio --locked
cargo fmt --all --check
cargo clippy --workspace --all-targets --features radio --locked -- -D warnings
cargo app --locked
cargo boot --locked
cargo app --release --locked
cargo boot --release --locked
```

C/LL 原工程离线构建从仓库根目录执行：

```powershell
cmake --preset debug -S Firmware
cmake --build Firmware/build/debug
```

需要更新已实测 C 版时，入口仍为 `./Firmware/flash-wireless.ps1`；不要把它当作 Rust 构建或升级脚本。
本轮没有运行任何烧录或电机启动命令。
