# 2026-11-16 LL 固件实物联调

本记录保留初次联调结果；后续用户重焊后反馈两轮编码器均可读数。轮电机测试与当前使能异常见[后续记录](2026-11-16-wheel-drive.md)。

源码基线：Git `64c5210` 加本轮未提交工作区。初次应用文件及烧录产物 SHA-256 见[初次清单](../../Firmware/validation/2026-11-16/source-manifest-initial.json)。
保留原有硬件工作区改动，本轮没有修改原理图/PCB，也没有重跑 ERC/DRC。
编码规范采用 `kernel-guided-coding` 的通用与裸机 C 规则，文档按 `docs update` 整理。

## 实现与实测范围

| 项目 | 结果与证据 |
| --- | --- |
| C5A3 LL 构建 | Debug、Release 均成功，应用启用 `-Wall -Wextra -Werror`；没有链接 HAL |
| 占用 | Debug Flash 16232 B；Release Flash 14384 B；RAM 42660 B，另预留 8192 B 栈，栈实际峰值未测 |
| 主机测试 | CTest 2/2、Python unittest 3/3 通过；覆盖回绕、消抖、协议/停止状态机、连续运行不受原 3 秒期限限制、画面边界、BLE 分片和 v1 兼容 |
| 烧录 | J-Link 写入 Debug HEX 并校验通过，见[最终烧录日志](../../Firmware/validation/2026-11-16/flash-bt-57600.txt) |
| LCD | 实物已显示界面；首版照片文字倒置，MADCTL 从 C0 改为 00，按“按钮在上、屏幕在下”配置；新方向尚无独立照片留档 |
| 按键 | 蓝牙记录包含 MONITOR→LEFT→RIGHT→BOTH→STEPPER，用户按 RUN 后启动并确认可旋转 |
| 连续步进 | 216 个运行帧，从 33503 到 141003 ms，覆盖 107.5 秒；实际速度回复为 29–30 RPM，位置持续增加 |
| 步进停止 | 同一记录随后 `running=0, phase=IDLE, flags=2, rpm=0`，即停止后失能；最终位置 3514061 |
| BLE 接收 | 57600 8N1，FFE1 notify；[连续记录](../../Firmware/validation/2026-11-16/ble-bringup-2.csv)共 278 帧，序号无缺口、解析拒绝 0；UART errors、RX/TX drops、步进超时/错误均为 0 |
| 编码器/J10 | 用户确认 J10 未插时右轮有计数，蓝牙读取右路 -24；左路始终 0，手转时 A/B 小点不变，进一步检查待进行 |
| 左路寄存器 | PA0/PA1 已配置 AF2，TIM5 的编码器模式/滤波/通道与计数使能已设置；150 次采样读取 PA0=1、PA1=0、TIM5_CNT=0，见[输入采样](../../Firmware/validation/2026-11-16/left-inputs-sampling.txt) |
| 轮动力输出 | PWM、方向、使能和 3 秒保护已实现；保持 J10 不插期间没有验证车轮主动转动 |
| 灰度 | GPIO 原始输入已读取，当前记录为全 0；黑白极性、线序及循线尚未实测 |

界面[软件渲染预览](../../Firmware/validation/2026-11-16/ui-preview.png)来自同一 C 渲染器的示例值，不是实物照片。
左轮检查入口：[供电、U3、R16/R17、A3/A4 与 MCU 脚号](../how-to/left-encoder-check.md)。

## 失败、修正和限制

1. 初次构建遇到 C5 LL 与旧系列常量名称不同，按本地 2.1.0 头文件修正；链接脚本只读表显式使用 READONLY，消除 RWX 段警告。
2. SPI3 首次初始化触发 MODF。修正为 `LL_SPI_SetConfig` 先建立 SSI，再进入 MASTER，并复位本应用使用的外设。随后 `lcd_ok=1`，实物有完整界面。
3. Emm 在上电已失能时返回停止拒绝 E2，但后续失能应答成功。仅启动/重连清理成功时清除此错误；实际运动错误仍保留。
4. 按 HC-08 手册默认 9600 收到乱码，115200 也未解出合法帧。[速率探测](../../Firmware/validation/2026-11-16/ble-baud-probe.txt)在 57600 首次获得合法 C5 帧，已写入固件配置；没有改写蓝牙模块参数。
5. 首次 57600 接收在两帧后断开，原因未确定；重新连接取得上述完整 278 帧记录，最后断开被工具作为错误报告。工具没有静默声称持续连接成功；断电后扫描另一次未找到设备，记录原样保留。
6. 原启动项 `BOOT_SEL=0, BOOT0=1` 选择 ROM Bootloader。只修改 `BOOT0=0`，OPTSR 从 `20B0EDD8` 到 `2030EDD8`，BOOTADD 和其余选项不变；[修改日志](../../Firmware/validation/2026-11-16/options-boot-flash.txt)、[复位检查](../../Firmware/validation/2026-11-16/reset-check.txt)显示正常复位后时基持续增长。物理断电重启尚未单独留证。
7. CubeProgrammer J-Link 的 `-rst`/`-run` 流程返回过非零，早期读寄存器时 `Regs` 也提示需先 halt。最终烧录使用 `-g 0x08000000`；复位验证使用 J-Link `r`、`g`，不是用指定 PC 掩盖启动选项。
8. 旧 MX2 递归删除被自动审批拒绝，改为可恢复的归档移动：原工程和 IOC2 已移出 Firmware 到本机 `.scratch/firmware-baseline-2026-11-16`，厂商依赖迁入 `vendor` 并保留许可证。

未完成：左编码器故障分段测量、两轮带电转动、轮径/计数方向与每圈计数标定、速度闭环、电流 ADC、灰度黑白标定、整车任务、长时间机械负载及电源测试。
蓝牙远程 `!` 已实现，但本轮尚未单独记录运动期间的无线停止实测。步进断通信线或 MCU 故障时，停止命令不保证能送达，硬件没有独立步进断电输出。

## 工具与命令

- Arm GNU GCC 14.3.1，CMake 4.3.1，Ninja 1.13.2；主机 MinGW GCC 15.2.0。
- STM32CubeProgrammer 2.23.0；J-Link Commander 9.82，探针 69730331；VTref 约 3.28 V。
- CMSIS Core(M) 6.1、ST DFP 2.1.0、C5 drivers 2.1.0；Python 接收使用本机独立 venv，Bleak 3.1.0、pyserial 3.5。

在 `Firmware` 执行：

```powershell
cmake --preset debug
cmake --build --preset debug
cmake --preset release
cmake --build --preset release
cmake -S . -B build/host -G Ninja -DHOST_TESTS=ON "-DCMAKE_C_COMPILER=C:/Program Files/JetBrains/CLion 2026.2.4.1/bin/mingw/bin/gcc.exe"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

在项目根目录执行：

```powershell
.scratch/firmware-tools/Scripts/python.exe -m unittest discover -s Firmware/tests -p test_monitor.py
STM32_Programmer_CLI -c port=JLINK sn=69730331 -w Firmware/build/debug/coral_c5.hex -v -g 0x08000000
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/monitor.py --ble F8:2E:0C:F3:68:37 --seconds 240 --out Firmware/validation/2026-11-16/ble-bringup-2.csv
git diff --check
```

最后一条接收命令实际在 278 帧后因断开返回 1，不能把 `--seconds 240` 当作已完成 240 秒接收。
上列 CSV 文件已存在，复测需换新文件名。`git diff --check` 通过；既有 KiCad 文件换行提醒与本轮固件验证分开。

本机迁移前 `Firmware.zip` SHA-256：`6F97E8076D0C3CDE0FA237CBA140009D85F032321802AFC8BE58E980779E098C`。
原芯片 1 MiB Flash 备份 `device-before.bin` SHA-256：`F3DBB57B7D7E8BB5E8919FE6428D23C06832C6D5FAB5FBDAEA3352EC34B56889`。
