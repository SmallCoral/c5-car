# 2026-11-16 HC-08 无线升级验证

结论：当前 Core + Motor 实物已安装自定义引导程序，完成两次完整 BLE 固件更新，以及一次中途断开后的旧应用恢复。今后普通应用修改使用[无线烧录入口](../how-to/wireless-update.md)。两次完整上传期间没有调用 J-Link；首次引导程序安装和最后独立 Flash 回读使用了 J-Link。

## 版本与工具

- 基线 Git HEAD：`64c5210ef6b4a552aac22b4bfe8a288ffd95149f`；本轮在未提交工作区修改，实际源码和产物身份以 [SHA-256 清单](../../Firmware/validation/2026-11-16/wireless/source-manifest.json)为准。
- 修改前固件本机备份：`.scratch/ota-baseline-20261116`。没有修改原理图、PCB 或旧项目。
- Arm GCC 14.3.1、CMake 4.3.1、Ninja 1.13.2；主机测试使用 CLion MinGW GCC 15.2。
- Python 3.12.14、Bleak 3.1.0、pyserial 3.5；本机解释器 `.scratch/firmware-tools/Scripts/python.exe`。
- STM32CubeProgrammer 2.23.0；J-Link Commander/DLL 9.82，探针序列号 69730331；回读电压 3.314 V。
- CMSIS/ST DFP/LL 来源沿用 [vendor 记录](../../Firmware/vendor/README.md)，应用与引导程序均未链接 HAL。

## 构建和主机验证

在 `Firmware` 中运行 `cmake --build --preset debug`、`cmake --build --preset release`、`cmake --build build/host` 和 `ctest --test-dir build/host --output-on-failure`。

| 产物 | Flash | RAM，不含栈 | 预留栈 |
| --- | --- | --- | --- |
| Debug 应用 | 18,976 B | 43,480 B | 8,160 B |
| Release 应用 | 16,696 B | 43,472 B | 8,160 B |
| Debug 引导程序 | 17,916 B | 43,728 B | 8,160 B |
| Release 引导程序 | 16,020 B | 43,728 B | 8,160 B |

应用预留 448 KiB，引导区预留 64 KiB。两个链接脚本排除 SRAM 最后 32 B，保留软件升级请求。厂商 `SystemInit` 从实际向量表符号设置 VTOR。

CTest **3/3 通过**：原有任务/步进逻辑、UI 边界、升级与恢复。恢复测试使用跨两个 8 KiB 页的 9,349 B 镜像，覆盖 **591 个安装中断点**，包括写入到一半的 quadword 和元数据，并遍历下载写入中断。另覆盖错地址、越界、偏移错误、损坏 CRC、重复 DATA/BEGIN/END 和丢失 ACK。[CTest 输出](../../Firmware/validation/2026-11-16/wireless/ctest.txt)、[恢复测试输出](../../Firmware/validation/2026-11-16/wireless/recovery-tests.txt)。

项目根目录运行 `.scratch/firmware-tools/Scripts/python.exe -m unittest discover -s Firmware/tests -p 'test_*.py'`，**12/12 通过**，覆盖原遥测、OTA 包校验、旧链接地址拒绝、HEX 地址与校验和、任意分片、丢 ACK、断线重连及已有暂存镜像时的处理。[Python 测试输出](../../Firmware/validation/2026-11-16/wireless/python-tests.txt)。

检查 ELF 段与[反汇编](../../Firmware/validation/2026-11-16/wireless/flash-ram-disassembly.txt)：Flash 临界操作代码和字面量位于 RAM，无 Flash 函数调用；引导程序代码/数据使用独立 RX/RW 段。最初混放 RAM 代码和数据产生过 RWX 链接警告，已通过分段修正。

升级界面使用与板端相同的 `ui.c` 生成，见[像素预览](../../Firmware/validation/2026-11-16/wireless/ui-update.png)。边界测试通过；本轮没有取得新的屏幕实拍。

## 实物过程与证据

1. 通过 BLE 停止电机，收到 6 帧停止状态。[安装前遥测](../../Firmware/validation/2026-11-16/wireless/before-install.csv)。此时步进历史错误为 E0，本轮重启后清除。
2. 只读检查 Option Bytes：Flash 1 MiB，SWAP_BANK=0，BOOT_SEL=0、BOOT0=0、BOOTADD=0x08000000；SRAM1/2 在系统复位时不擦除。本轮未写入 Option Bytes。
3. 使用 `STM32_Programmer_CLI -c port=JLINK sn=69730331 -w Firmware/build/debug/first_install.hex -v -g 0x08000000` 首次安装。烧录和校验成功；通过 BLE 查询得到 Debug 应用 `4084B512 / 18976 B`。[安装日志](../../Firmware/validation/2026-11-16/wireless/first-install.txt)、[首次应用回复](../../Firmware/validation/2026-11-16/wireless/first-app-info.txt)。
4. 执行 `.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py Firmware/build/release/coral_c5.ota`。完整无线更新到 Release，重启后返回 `D7E57D4B / 16696 B`；用时 **52.5 秒，重发 8 次**。[上传日志](../../Firmware/validation/2026-11-16/wireless/upload-release.txt)。
5. 使用[实物恢复测试脚本](../../Firmware/validation/2026-11-16/wireless/interruption-check.py)，上传 Debug 文件前 1,024 B；重复上一包、发送 CRC 错误包，再断开 BLE。重新连接后 ABORT/RUN，原 Release 应用正常启动，CRC 仍为 `D7E57D4B`。这一恢复过程也未调用 J-Link。[记录](../../Firmware/validation/2026-11-16/wireless/interrupted-transfer.txt)。
6. 从项目根目录运行 `./Firmware/flash-wireless.ps1 -Preset debug`，通过 CMake `wireless_flash` 完整上传 Debug。重启后返回 `4084B512 / 18976 B`；用时 **75.5 秒，重发 10 次**。[成功日志](../../Firmware/validation/2026-11-16/wireless/upload-debug-retry.txt)。
7. 同一镜像再运行一次入口，识别已安装、未重复擦写，返回应用并再次确认 CRC。[同镜像检查](../../Firmware/validation/2026-11-16/wireless/same-image-check.txt)。
8. 用 J-Link 独立只读回查：`VTOR=0x08010000`；应用元数据与 Debug 一致；暂存提交页头部全部为 FF；`ui.duty=450`，`controls.running=0`。[板上回读](../../Firmware/validation/2026-11-16/wireless/board-readback.txt)。
9. 对回读窗口中的 17,916 B 引导镜像及 18,976 B 应用逐字节比较，均与本机对应 BIN 一致，引导程序未被两次无线更新修改。[比较结果](../../Firmware/validation/2026-11-16/wireless/readback-comparison.json)。J-Link SaveBin 的长度按十六进制解析，因此日志中的窗口为 96,534 B；比较脚本使用上述实际镜像长度，未把整个窗口与单个 BIN 混比。
10. 最后接收 6 帧蓝牙遥测：ALL、`running=0`、两轮 `0/s`、轮故障 0、步进在线、错误 00、实际 RPM 0。[更新后遥测](../../Firmware/validation/2026-11-16/wireless/after-wireless.csv)。

## 失败和验证边界

第一次尝试一条命令入口时，脚本的 Push-Location 影响了调用方日志的相对路径；已改为 CMake 显式源码/构建目录，从项目根目录复测通过。第二次尝试发生 BLE `Not connected`，构建目标正确返回非零；[失败日志](../../Firmware/validation/2026-11-16/wireless/upload-debug-final.txt)保留。随后加入有界断线重连、连接后等待与同镜像检查；重连逻辑另有主机故障注入测试。

现场蓝牙仍有包超时，20 ms 分片间隔没有消除全部重发。本轮完整传输均靠重试后校验通过，不将其记录为零丢包或零重试。独立回读时探针报告旧固件的缓存调试警告；当前引导程序保持 ICACHE 关闭，没有升级探针固件。

**已验证**：蓝牙请求进入引导程序、两次完整上传/安装/重启、最终 CRC 和逐字节回读、重复包处理、错误 CRC 包拒绝、断开后启动旧程序、默认 45% 与停止状态。

**尚未验证**：实物随机断电压力测试、Flash ECC 故障注入、448 KiB 极限镜像、按住 TASK 上电的实际持键操作、新升级界面的实拍确认。本轮没有启动电机测试；已有轮电机/步进运动证据仍见[先前联动记录](2026-11-16-wheel-drive.md)。主机断电模拟不替代实物电源故障试验。

CRC 提供完整性检测，未实现签名认证、应用健康回滚或引导程序自身的无线更新。
