# Coral-C5 Rust 版

独立迁移 STM32C5A3VGT6 应用、无线升级引导程序和电脑端工具。原有 `Firmware` C 工程与 Python 工具保留，本目录使用自己的 Cargo workspace、依赖锁文件和构建输出。

**当前为代码迁移版：主机测试、ARM 编译和静态检查已完成，未烧录、未进行 Rust 版实物验证。** 默认构建、测试和离线工具不会连接蓝牙或操作电机。验证范围见 [VALIDATION.md](VALIDATION.md)。

新环境安装、共享构建配置和入口选择见 [RustRover 开发说明](../../Documentation/how-to/rust-development.md)；接续任务见 [项目交接](../../.agent/main/handoff.md)。

## 代码结构

| 目录 | 职责 |
| --- | --- |
| `core` | 无堆分配的任务/按键逻辑、Emm 协议、OTA 协议与恢复流程、遥测、128×160 UI；`no_std`，禁止 `unsafe` |
| `device` | C5A3 GPIO、PWM、编码器、串口、SPI、Flash、中断、启动代码；生成 `coral-app` 和 `coral-boot` |
| `host` | Rust 电脑端 CLI：BLE/串口遥测、无线升级、固件打包、离线解析和界面预览 |

固件不链接 C LL/HAL 或 C 运行库。C5A3 外设通过集中封装的寄存器访问实现，寄存器取值与现有 ST DFP/LL 头文件核对。启动和 Flash 临界操作保留必要的 ARM 汇编；来源、内存边界和 `unsafe` 范围见 [device/registers-source.md](device/registers-source.md)。

## 构建与检查

在本目录执行命令。`rust-toolchain.toml` 固定 Rust **1.100.0** 和 `thumbv8m.main-none-eabihf`，`Cargo.lock` 固定依赖版本。首次使用需要 rustup 和本机链接器；本次 Windows 验证使用 GNU Rust 工具链与 CLion 自带 MinGW。

```powershell
cd Firmware/Rust
cargo check --workspace --all-targets --features radio --locked
cargo test --workspace --features radio --locked
cargo fmt --all --check
cargo clippy -p coral-core -p coral-host --all-targets --features radio --locked -- -D warnings
cargo clippy -p coral-device --target thumbv8m.main-none-eabihf --locked -- -D warnings
cargo app --locked
cargo boot --locked
cargo app --release --locked
cargo boot --release --locked
cargo build -p coral-host --features radio --locked
```

`cargo app`、`cargo boot` 是本目录 `.cargo/config.toml` 的别名。嵌入式产物在 `target/thumbv8m.main-none-eabihf/{debug,release}/`，电脑端程序在 `target/debug/coral-host.exe`。普通 `cargo build` 只构建主机可用的默认成员；不要把整个 workspace 用主机目标构建成 MCU 程序。

本机还准备了被 Git 忽略的隔离工具链。在项目根目录执行 `. .scratch/rust-env.ps1` 可为**当前 PowerShell 进程**加载它，然后进入本目录运行上述命令。它不是克隆项目后的必需文件，也没有改系统 PATH 或原 C 工具链。

IDE 打开本目录 `Cargo.toml`。主机目标下可以检查整个 workspace；`coral-app`、`coral-boot` 的本机入口只打印正确构建命令并以失败状态退出，不执行固件代码。实际固件使用 `cargo app --locked` / `cargo boot --locked`，或 RustRover 右上角的 `Coral ARM App Build` / `Coral ARM Boot Build` 共享配置。它们只编译 ARM 固件，不在电脑上运行或烧录。

`host/src/main.rs` 是电脑端入口；`device/src/bin/app.rs` 是固件入口，业务循环在 `device/src/app.rs`。查看设备代码时选择 `thumbv8m.main-none-eabihf` 目标；运行主机测试时使用电脑目标。两者有不同的标准库和链接方式。

## 已迁移的行为

- 上电停止，默认 `ALL`：两轮 45% 占空比、Emm 步进 30 RPM。按 RUN 连续运行，再按任意键停止；ALL 等待步进速度命令确认后再启动轮子。
- 照片方向“按钮在上、屏幕在下”，左至右 RUN / DIR / LVL / TASK。屏幕延续已修正的 `MADCTL=0x00`、偏移和配色，显示编码计数、A/B 电平、灰度、Emm 状态及故障。
- MONITOR、LEFT、RIGHT、BOTH、STEPPER、ALL 六个任务。单轮/双轮测试 3 秒，STEPPER / ALL 连续运行。按键去抖 25 ms，上电按住的键必须先释放。
- 20 kHz PWM；使能等待 5 ms；PA3 回读低、驱动故障或主循环心跳超过 100 ms 会停止轮子。故障不会自动重启电机。
- HC-08 UART 57600、Emm UART 115200、地址 1。保留遥测 v2、远程 `!` 停机、`C5:INFO` 查询、`C5:OTA` 升级入口。
- OTA v1 的分区、CRC、128 字节分包、重复包确认、暂存区提交和安装恢复流程保持兼容。

## 离线工具

以下命令只处理电脑文件，不连接硬件：

```powershell
cargo run -p coral-host -- inspect dist/release/coral_c5.ota
cargo run -p coral-host -- replay capture.txt --out telemetry.csv
cargo run -p coral-host -- render --out ui.ppm
```

`replay` 输入为原始串口文本；兼容 v1/v2 帧，CSV 固定补齐速度字段。首列为 `received_unix_ms`，表示解析时的电脑时间，区别于原 Python 工具的时间字符串。过长或损坏的整行丢弃，下一行继续解析。

构建后可用现有 ARM binutils 导出并打包，仍然不会烧录：

```powershell
New-Item -ItemType Directory -Force dist/release | Out-Null
arm-none-eabi-objcopy -O binary target/thumbv8m.main-none-eabihf/release/coral-app dist/release/coral-app.bin
arm-none-eabi-objcopy -O binary target/thumbv8m.main-none-eabihf/release/coral-boot dist/release/coral-boot.bin
cargo run -p coral-host -- package --app dist/release/coral-app.bin --boot dist/release/coral-boot.bin --out dist/release
```

输出 `coral_c5.ota` 和带镜像元数据的 `first_install.hex`。`target`、`dist` 已忽略，不作为源码提交。

## 蓝牙工具接口

开启 `radio` 特性后提供 `scan`、`monitor`、`upload`、`info`、`exit-recovery`；参数见 `coral-host help`。默认 HC-08 地址沿用本车的 `F8:2E:0C:F3:68:37`，可通过 `--ble` 指定，服务特征默认 FFE1。

只有显式调用这些命令才访问硬件；`upload` 会写入设备。**本轮没有运行这些硬件命令。** Rust 引导程序目前也未装入板子，板上仍是原有版本；跨语言实物升级与真实断电恢复尚未验证。OTA CRC 用于完整性检查，不提供固件签名认证。

原版连接说明和协议表分别在 [无线升级使用说明](../../Documentation/how-to/wireless-update.md) 与 [协议参考](../../Documentation/reference/wireless-update.md)。这些资料中的原 C/Python 构建命令不替代本目录的 Cargo 命令。
