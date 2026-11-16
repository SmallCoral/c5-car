# Rust 构建与 RustRover 配置

适用于独立的 `Firmware/Rust` 工程。
Rust 版目前只完成代码迁移、主机测试和 ARM 编译，尚未烧录；板上运行的是原 C/LL 固件。
功能与验证范围见 [Rust 说明](../../Firmware/Rust/README.md)和[验证记录](../../Firmware/Rust/VALIDATION.md)。

## 准备工具链

先安装 rustup，并准备本机 Rust 目标所需的链接器。
仓库的 `rust-toolchain.toml` 固定 Rust 1.100.0、`rust-src`、rustfmt、Clippy 和 `thumbv8m.main-none-eabihf` 标准库。
Windows 本次使用 GNU 工具链和 CLion 自带 MinGW；在新的 Windows 环境可执行：

```powershell
cd Firmware/Rust
rustup toolchain install 1.100.0-x86_64-pc-windows-gnu --profile minimal --component rust-src --component rustfmt --component clippy --target thumbv8m.main-none-eabihf
rustup override set 1.100.0-x86_64-pc-windows-gnu
rustc -Vv
cargo -V
```

将实际 MinGW `bin` 目录加入当前终端或 IDE 项目环境的 `PATH`。
本机已验证路径为 `C:/Program Files/JetBrains/CLion 2026.2.4.1/bin/mingw/bin`；其他机器按实际安装位置填写。
使用 MSVC 主机目标时需要对应的 Visual Studio C++ 链接工具，本轮没有验证该组合。

本机另有被 Git 忽略的隔离工具链，已有环境可从项目根目录加载：

```powershell
. ./.scratch/rust-env.ps1
cd Firmware/Rust
rustc -Vv
cargo -V
```

`.scratch` 不随仓库分发，新机器按上面的 rustup 流程准备环境。

## 在 RustRover 中打开

1. 打开 `Firmware/Rust/Cargo.toml`，确认它已关联为 Cargo workspace。
2. 在 Rust 设置中选择包含 `cargo`、`rustc` 的工具链目录；`rustup which rustc` 可显示当前编译器位置。
3. 确认标准库源码已识别，必要时执行 `rustup component add rust-src`。
4. 使用本机隔离工具链时，在 Rust 项目环境中配置 `CARGO_HOME`、`RUSTUP_HOME` 和包含 MinGW 的 `PATH`；不要写入全局环境或提交机器绝对路径。
5. 从右上角选择下表配置，再点击运行按钮执行检查或构建。

| 共享配置 | 行为 |
| --- | --- |
| `Coral Host Check` | 按电脑目标检查全部 workspace 和测试目标，启用 `radio` |
| `Coral ARM App Build` | 编译 ARM 主程序 `coral-app`，不启动设备 |
| `Coral ARM Boot Build` | 编译 ARM 引导程序 `coral-boot`，不启动设备 |

这些配置保存在 [`.run`](../../Firmware/Rust/.run)，随 Git 提交。
`.idea` 中的工具链路径、索引和界面状态仍只保存在本机。
共享配置使用项目目录和当前已配置的工具链，不包含本机路径或烧录步骤。

## 选择正确入口与目标

| 文件 | 用途 |
| --- | --- |
| [host/src/main.rs](../../Firmware/Rust/host/src/main.rs) | 电脑端 CLI：遥测、打包、离线工具及可选 BLE |
| [device/src/bin/app.rs](../../Firmware/Rust/device/src/bin/app.rs) | MCU 主程序入口 |
| [device/src/app.rs](../../Firmware/Rust/device/src/app.rs) | MCU 外设初始化与业务循环 |
| [device/src/bin/boot.rs](../../Firmware/Rust/device/src/bin/boot.rs) | MCU 引导程序入口 |

在本机目标下，设备库不编译寄存器访问代码；两个固件二进制的本机入口只打印 ARM 构建提示并返回退出码 1。
这个提示表示把固件当作电脑程序运行了，应选择 ARM 构建配置。
ARM 目标下保留 `no_std`、启动代码和 `firmware_main`。
之前的 `cannot find app in coral_device` 已通过入口条件编译修复，无需排除整个设备包或隐藏检查错误。

固件代码在本机分析目标下显示灰色属于条件编译未启用。
查看 ARM 分支时可切换分析目标到 `thumbv8m.main-none-eabihf`；电脑端程序和测试仍使用本机目标。
不要用 `--workspace --target thumbv8m.main-none-eabihf` 编译依赖 `std` 的电脑端工具。

## 验证与产物

在 `Firmware/Rust` 执行：

```powershell
cargo check --workspace --all-targets --features radio --locked
cargo test --workspace --features radio --locked
cargo fmt --all --check
cargo clippy --workspace --all-targets --features radio --locked -- -D warnings
cargo app --locked
cargo boot --locked
```

`cargo app`、`cargo boot` 是 `.cargo/config.toml` 的构建别名。
ARM ELF 位于 `target/thumbv8m.main-none-eabihf/debug/`，Release 构建加 `--release`。
构建与测试不会连接 J-Link、BLE 或电机；无线升级工具的硬件命令需另行安排验证。
