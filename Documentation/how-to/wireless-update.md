# 蓝牙无线烧录

适用当前 STM32C5A3VGT6 + HC-08 实物。2026-11-16 已安装自定义引导程序，并完成真实 BLE 更新。日常修改应用无需接 J-Link；板子仍需正常供电。

## 一条命令编译并更新

关闭正在接收 HC-08 的遥测工具或手机客户端，在项目根目录运行：

```powershell
./Firmware/flash-wireless.ps1
```

默认编译并上传 Debug；Release 使用 `./Firmware/flash-wireless.ps1 -Preset release`。脚本使用 CMake、Ninja、Arm GCC 和 Python；当前电脑的工具链与 BLE Python 环境已配置。

工具连接地址 `F8:2E:0C:F3:68:37`，先停止电机，再进入升级界面、分包上传、校验、安装和重启。完成后应看到 `Verified running application`，包含与上传文件一致的长度和 CRC。重启后程序进入 LAP，按 RUN 才启动。[当前速度档和操作](line-following-lap.md)

屏幕按“按钮在上、屏幕在下”显示 UPDATE、当前阶段和进度条。安装阶段保持供电。镜像大小随构建变化，上传时间取决于蓝牙重试。[最近一次应用更新](../validation/2026-11-18-lap-fast.md)、[3300 稳定版恢复命令](../../Firmware/releases/lap-3300-BF33A9F7/README.md)、[引导程序与恢复测试](../validation/2026-11-16-wireless-update.md)

## CLion

打开 `Firmware` 工程，Reload CMake Project，使用已有 `debug` 或 `release` preset。构建 **`wireless_flash`** 目标即可编译并无线更新；也可在 CLion Terminal 运行 `./flash-wireless.ps1`。普通 `coral_c5` 目标只编译应用。无线上传不提供断点或单步调试，这些仍使用 J-Link。

## 上传已有文件或换电脑

安装 Python 3.10 或更新版本并建立独立环境。在项目根目录：

```powershell
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r Firmware/tools/requirements.txt
.venv/Scripts/python.exe Firmware/tools/wireless_flash.py Firmware/build/debug/coral_c5.ota
```

构建打包只需要 Python 标准库；无线上传需要 Bleak。更换构建解释器时，在 `Firmware` 目录配置 `cmake --preset debug -DPython3_EXECUTABLE=C:/path/to/python.exe`。

当前本机解释器也可使用 `.scratch/firmware-tools/Scripts/python.exe`。CMake 优先采用显式指定的解释器，否则使用已有的本机环境，再查找系统 Python。

支持 `.ota` 和链接到 `0x08010000` 的应用 `.bin`。不要传入 bootloader、旧版 `0x08000000` 应用、ELF 或 HEX；工具会拒绝不匹配的镜像。

更换 HC-08 后，用 `monitor.py --scan` 查询地址，再给 `wireless_flash.py` 加 `--ble 新地址`。模块 UART 必须保持当前实测 **57600 8N1**，BLE 特征值为 FFE1。

只查询运行固件：

```powershell
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
```

## 中断和恢复

- **上传中断**：再次执行上传命令。引导程序尚未复位且镜像相同时会从已确认的偏移继续；若断电，重新上传暂存镜像，旧应用仍保留。
- **想取消未完成上传并运行旧程序**：执行 `wireless_flash.py --exit-recovery`。已经完整提交的镜像会在下一次重启时安装，不再当作未完成下载取消。
- **安装时断电或复位**：引导程序在下次启动时检查已提交的暂存镜像，重新复制、校验并启动。软件模拟已覆盖擦除/写入中断位置；尚未做实物随机断电压力测试。
- **应用异常，无法响应蓝牙命令**：按住最右侧 TASK 上电，看到 UPDATE 后松开，重新运行上传命令。物理按键强制入口已实现，板上持键验证范围见验证记录。
- **蓝牙暂时断开**：工具会重连两次；单个命令超时重发最多三次。仍失败时保留供电、关闭其他 BLE 客户端后重试，错误不会被当作成功。

升级前会发送停止和失能命令，轮驱动保持关闭。步进串口断线时，软件仍无法保证外部步进驱动收到停止命令；它没有 MCU 控制的断电通道。

## 首次安装或修复引导程序

这一步已在当前板完成，日常更新无需重复。新板或引导程序损坏时，从项目根目录执行：

```powershell
STM32_Programmer_CLI -c port=JLINK sn=69730331 -w Firmware/build/debug/first_install.hex -v -g 0x08000000
```

`first_install.hex` 包含引导程序、应用、匹配的应用元数据及清空的暂存提交标记。它与 `coral_c5.hex` 不同；单独烧入应用会使元数据与程序不匹配。

保持 `BOOTADD=0x08000000`、`BOOT_SEL=0`、`BOOT0=0`，让复位先进入自定义引导程序。当前 SRAM1/2 系统复位不擦除，用于一次性软件升级请求。不要为日常升级更改保护位或锁定位。

实现细节见[布局和协议](../reference/wireless-update.md)，实测证据见[无线升级验证](../validation/2026-11-16-wireless-update.md)。
