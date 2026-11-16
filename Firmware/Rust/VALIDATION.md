# Rust 迁移验证记录

日期：2026-11-16。原工程基线：`b9aaa695109ca046874f67017f2174d682bed09d`。本记录对应独立 `Firmware/Rust` 工作区的迁移代码；交付源码版本以包含本记录的 Git 提交为准，提交前复核见[交接整理记录](../../Documentation/validation/2026-11-16-rust-handoff.md)。

## 环境与命令

Windows，Rust 1.100.0 (`0ef1bc386`)，Cargo 1.100.0，rustfmt 1.11.0，Clippy 0.1.100。主机目标 `x86_64-pc-windows-gnu`，链接器为 CLion 2026.2.4.1 的 MinGW。MCU 目标 `thumbv8m.main-none-eabihf`，Cortex-M33，Rust 自带 LLD 链接。ARM GNU 14.3.1 的 objcopy/nm/objdump 用于离线导出和核查。

本次使用 `.scratch/rust` 隔离工具链，未修改系统 PATH；执行前通过 `.scratch/rust-env.ps1` 加载进程环境。

在 `Firmware/Rust` 执行并通过：

```powershell
cargo test --workspace --features radio --locked
cargo fmt --all --check
cargo clippy -p coral-core -p coral-host --all-targets --features radio --locked -- -D warnings
cargo clippy -p coral-device --target thumbv8m.main-none-eabihf --locked -- -D warnings
cargo clippy -p coral-device --target thumbv8m.main-none-eabihf --release --locked -- -D warnings
cargo build -p coral-device --target thumbv8m.main-none-eabihf --locked
cargo build -p coral-device --target thumbv8m.main-none-eabihf --release --locked
cargo build -p coral-host --features radio --locked
```

## 主机测试结果

共 **30 项测试通过，0 失败**。测试不创建蓝牙适配器、不打开串口。

| 测试组 | 数量 | 主要证据 |
| --- | ---: | --- |
| `core/tests/motion.rs` | 9 | 上电停止、按键切换/停止、去抖、时钟/编码器回绕、使能故障与心跳停机、Emm 启停 ACK/超时/迟到 ACK/接收丢失 |
| `core/tests/protocol.rs` | 8 | CRC/向量校验、分片/噪声/超长报文、v1/v2 遥测、容量边界、旧版协议样本、27 帧 UI 比较 |
| `core/tests/storage.rs` | 5 | 重复分包、元数据提交、错误镜像拒绝、写保护范围、下载/安装逐操作断电恢复 |
| `host/tests/exchange.rs` | 3 | ACK 丢失重试相同包、忽略旧序号、有界重试、拒绝响应/断线退出；使用模拟传输 |
| `host/tests/tools.rs` | 5 | HEX 校验和/跨 64 KiB/重叠拒绝、打包往返、错误板号/续传偏移/镜像身份、真实 CLI 离线回放/渲染 |

断电模拟使用 9349 字节镜像、8 KiB 擦除页、16 字节编程单位，并禁止未擦除的单元再次编程。下载 590 个、安装 591 个故障点，各运行“操作前失败”和“半页/半个编程单元已修改后失败”两种情形，共 **2362 种**。验证旧应用或已提交暂存镜像可恢复；这不是实物电源、电压或 Flash ECC 行为测试。

27 帧 UI 使用原 C 渲染器生成的 RGB565 CRC32 作对照，包含全部六任务、运行/停止/反向/故障与升级页。OTA 固定样本来自原 Python 实现。输入与来源见 [测试样本说明](core/tests/fixtures/README.md)。

迁移过程中发现并修正 LL 编码器滤波常量的位移、Rust 数组读回比较类型以及 Clippy 检查项；最终上述检查没有遗留编译错误或警告。

## 固件产物与内存

离线 `objcopy` 导出、Rust `package` 和 `inspect` 均通过。

| 模式 | 产物 | Flash 字节 | 静态 RAM 字节 |
| --- | --- | ---: | ---: |
| Debug | 应用 | 51112 | 43136 |
| Debug | 引导程序 | 49504 | 43328 |
| Release | 应用 | 25824 | 43128 |
| Release | 引导程序 | 24316 | 43328 |

Flash 包括向量、代码、只读数据、初始化 RAM 的加载镜像；RAM 包括 `.ramfunc`、`.data`、`.bss`，不包含运行时栈峰值。引导程序均小于 64 KiB，应用均小于 448 KiB，静态 RAM 不侵占链接器预留的 8160 字节栈或顶部升级请求区。

应用 `.bin` SHA-256：

- Debug：`930d226b9758e983df43d092e20480715d24e5b791b400bf791f4804b66b439a`
- Release：`dcedbf7096ab0997f47488eb6c24c50a3ab7a969fc923d85e8146e0b780508c7`

引导程序 `.bin` SHA-256：

- Debug：`447298ff0f53f465998a33dc7f59f2864ae8e9028b693bb8ad66cad36d9c302d`
- Release：`714aef2f34881f4a439fd4e5f4b75aed55bb0e1f02e3feccefd87133d8e0a4f5`

应用 CRC32：Debug `A38355B2`、Release `C84BC27D`。产物在被忽略的 `dist/{debug,release}`。字节数和摘要只描述本次工具链产物，不承诺其他编译器或路径下逐字节可复现。

## ELF 核查

对 Debug/Release 两个映像分别执行：

```powershell
arm-none-eabi-objdump -h <ELF>
arm-none-eabi-nm --defined-only <ELF>
arm-none-eabi-nm -u <ELF>
arm-none-eabi-objdump -d -j .ramfunc <BOOT_ELF>
```

从实际 `.bin` 比对 MSP、Reset、NMI、HardFault、SysTick、USART1/USART6 槽位与 ELF 符号；处理函数均为正确分区内的 Thumb 地址，未解析符号为空。引导程序 `.ramfunc` 位于 `0x20000000`、长度 200 字节；分支及文字常量池留在该段，无 `bl`/`blx` 调回 Flash。应用不含擦写 Flash 的 `.ramfunc`。

核查脚本为本机 `.scratch/rust_artifact_audit.py`，结果存于 `dist/artifact-audit.json`，没有执行下载器、BLE 或设备读写。原 Firmware 文件基线与本轮开始时保存的 621 项文件摘要比对一致；硬件目录中的用户改动不属于本次迁移。

## RustRover 主机目标入口修复（2026-11-16）

修复发生于上述基线之上的迁移工作区，工具链仍为 Rust/Cargo 1.100.0。修改前，`cargo check --workspace --all-targets --locked` 在 Windows 目标下复现 `app` / `boot` 模块缺失（E0433）和缺少 `panic_handler`：库仅在 ARM 下启用，两个二进制入口却无条件引用它。

两个入口现按 ARM 条件启用 `no_std`、`no_main` 和 `firmware_main`。本机入口仅提示对应 ARM 构建命令并返回失败状态。RustRover 已移除排除 `coral-device` 的检查参数，默认构建配置选择 `Coral ARM App Build`。

在 `Firmware/Rust` 通过以下检查：

```powershell
cargo check --workspace --all-targets --locked
cargo fmt --all --check
cargo clippy --workspace --all-targets --features radio --locked -- -D warnings
cargo build -p coral-device --target thumbv8m.main-none-eabihf --locked
```

在 `device` 目录，分别对 `coral-app` 和 `coral-boot` 执行 `cargo check --bin <名称> --no-default-features --tests --locked`，模拟 IDE 当前文件检查，均通过。在工作区执行 `cargo run -p coral-device --bin <名称> --locked`，两者均按预期打印 `cargo app --locked` / `cargo boot --locked` 提示并返回退出码 1；这是拒绝在电脑执行固件的预期结果，不是编译失败。未操作硬件。

RustRover 2026.3 中实际运行 `Coral ARM App Build`，控制台显示 `Finished dev profile`、退出代码 0；主程序入口保留在编辑器中。IDE 日志确认当前文件和 workspace 的 Cargo 检查均已去掉原先的设备排除参数。

本次入口源码 SHA-256：`device/src/bin/app.rs` 为 `8fabbf49ab989a5e7fdb4e88c404bdcc0cfd2930105c2a6a8798270505d29598`；`device/src/bin/boot.rs` 为 `ea657ed9cecabc5c6e431b95b717677a92190565473a08dcf50ac9fada5df8c1`。上文固件产物摘要仍为初次迁移时的产物记录。

## 未验证范围

- 未烧录 Rust 应用或引导程序，未操作电机，未连接 HC-08/串口/J-Link。
- 未验证 Rust 的实际屏幕方向、GPIO 波形、PWM/编码器、串口中断时延、Flash/ECC 和真实断电恢复。
- Windows BLE 后端已编译，协议重试有模拟测试；没有进行真实 BLE 连接、跨 C/Rust 无线升级或吞吐测试。
- 未测动态栈峰值；未验证 Linux/macOS 主机后端。编译和模型测试不能替代这些实物验证。

因此本次交付是可编译、可测试的独立 Rust 实现；原已验证 C 版仍作为硬件运行基准。
