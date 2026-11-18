# Core + Motor 固件联调

适用：STM32C5A3VGT6、ST7735S 128×160、HC-08、Emm 固件步进和双编码器电机。
本轮实测及尚未通过项见[验证记录](../validation/2026-11-16-firmware-bringup.md)。

## 1. 构建与烧录

当前板已安装无线升级引导程序。日常从项目根目录执行 `./Firmware/flash-wireless.ps1`，完整说明见[蓝牙无线烧录](wireless-update.md)。

首次安装新板或修复引导程序时，从[固件入口](../../Firmware/README.md)构建合并包。当前本机 J-Link 序列号为 `69730331`，在项目根目录执行：

```powershell
STM32_Programmer_CLI -c port=JLINK sn=69730331 -w Firmware/build/debug/first_install.hex -v -g 0x08000000
```

确认输出 `Download verified successfully` 和 `Start operation achieved successfully`。
此 J-Link 后端的 `-rst` 返回过非零，不能仅凭打印了 Reset 认定程序启动；复位检查使用 J-Link Commander 的 `r`、`g`，并观察时基增长。

本板原 Option Bytes 为 `BOOT_SEL=0, BOOT0=1`，会复位进入 ROM Bootloader。
2026-11-16 已单独把 `BOOT0` 改为 `0`，`BOOTADD=0x08000000`、读保护与其他选项不变。
需要检查另一块板时先读 `-ob displ`，不要照抄修改保护或锁定位。
该启动选择采用选项位，PH2 的电平不参与启动选择。依据 [ST AN6274 启动配置](https://www.st.com/content/st_com/en/technical-documents/an6274-getting-started-with-stm32c5-mcu-hardware-development-stmicroelectronics_asset/chunk-003-topic-an6274-boot-configuration.html)。

## 2. 屏幕与按键

以按钮在上、LCD 排针在下的照片方向使用。`LCD_MADCTL=0x00`，列偏移 2、行偏移 1；首版 `0xC0` 曾造成文字倒置。
上电默认 LAP，保持停止。首圈方向检查和 RUN 操作见[循线一圈](line-following-lap.md)。
原联动测试需选择 ALL，最左侧 RUN 启动两轮和步进，再按停止；默认轮 PWM 45%、步进 30 RPM，ALL 不受单独轮测试的 3 秒期限限制。
屏幕顶部提示对应从左到右 SW5、SW4、SW3、SW2：

| 按钮 | 停止时 | 运行时 |
| --- | --- | --- |
| RUN / STOP | 启动所选测试；MONITOR、GRAY 不动作 | 停止 |
| DIR / MAP | 反转下次启动方向；LAP 切换安装方向 A/B；GRAY 不动作 | 先停止 |
| PWR / SPD / LVL / B/W | 普通轮 PWM 10/20/30/45%，步进 30/60/90 RPM，LAP 800/1800/2200/3300/4600 CPS。GRAY 切换黑线/白线目标，ALL 档位见固件说明 | 先停止 |
| TASK | LAP、MONITOR、LEFT、RIGHT、BOTH、STEPPER、ALL、GRAY 循环 | 先停止 |

LEFT/RIGHT 大数字是上电后的原始正交计数，`/S` 是每秒计数；并非 RPM。
标题右侧两个小点分别显示该路 A/B 的瞬时输入电平。
底部 `EN REQ` 仅表示 MCU 请求轮驱动使能，`J10 ?` 表示跳帽无法由软件检测。
`ENABLE LOW / PA3` 表示请求后 PA3 前端仍读到低电平，程序已停止轮任务；按[使能线路排查](wheel-enable-check.md)检查后再尝试启动。
`FAULT L:x R:x` 同时显示当前 nFAULT 和已锁存的轮故障；下一次启动请求重新检查。
ALL 会先等待步进接受速度命令再启动轮 PWM。轮驱动或步进异常会停止联动，恢复通信不自动启动；停止时等 EMM READY 后再按 RUN。
普通页灰度 1–8 青色表示滤波后的黑色。GRAY 专页提供原始值、目标极性、段数和通道偏差，见[灰度联调](grayscale-check.md)；用户已确认黑白识别正常、左到右为 1～8。
BT 点表示发送数据已排入串口队列，不代表电脑已经收到。

## 3. J10 不插时检查编码器

J10 控制两路 DRV8874 的 nSLEEP；编码器供电及 U3/U5 整形由 Core 3.3 V 提供，不经过 J10。
保持 J10 不插，先在 MONITOR 中分别手转两个轮子。
LEFT 使用 PA0/PA1、TIM5；RIGHT 使用 PB3/PA15、TIM2。右侧 A/B 对应定时器 CH2/CH1，方向符号需要实车标定。

若一侧计数不动，先比较 A/B 小点与电脑端 `encoder_ab`，再检查六针线序、编码器供电和输入通路。
断电后对换 J4/J6 的两条六针线束再手转，可区分问题是否跟随电机/线束或固定在某个板端口。
不要用软件改变线序来掩盖没有输入边沿的问题。[六针定义与电平依据](../reference/motor-interface.md)。
早期左路异常的具体位号、脚号和检查顺序见[左编码器测点](left-encoder-check.md)；用户重焊后已反馈两侧可正常读数。

轮电机运动测试需断电后插入 J10、使车轮离地，再上电选择 LEFT/RIGHT/BOTH。
当前默认 45%；需要从低速开始时，停止状态按一次 PWR 切到 10%，再按 RUN。后续可停止后升至 20%、30% 或 45%；低占空比不一定克服静摩擦。
LEFT/RIGHT/BOTH 单次 3 秒自动结束，任意键或蓝牙 `!` 可提前结束；ALL 为连续联动。方向改变须停止后再按 DIR，然后重新启动。
当前是开环 PWM 联调，不保证两侧相同转速或整车前进方向。

## 4. 步进连续旋转

驱动使用用户确认的 **Emm 固件**，UART 为 115200 8N1、地址 1、固定校验字节 `0x6B`、非多机同步。
驱动需允许串口控制，并返回命令应答；速度单位采用厂家默认 RPM，不采用 0.1 RPM 缩放模式。
状态行 `EMM READY` 后进入 STEPPER，选择 30 RPM、CW，再按 RUN。

固件先等待使能应答，再发送 `01 F6 00 00 1E 0A 00 6B`，对应 CW、30 RPM、加速度参数 10。
该速度模式不依赖微步设置，不设 3 秒期限，不写驱动“上电自动运行”参数。
屏幕 `CONT` 表示连续模式，运行时 EMM 行显示驱动回复的实际带符号 RPM。
每 200 ms 轮询一项：状态 `3A`、速度 `35`、位置 `36`，每项约 600 ms 更新一次。

任意按键或蓝牙 `!` 发停止 `FE 98`，然后失能 `F3 AB 00`。
丢应答、接收损坏或堵转状态也会尝试停止、失能；停止应答超时仍会发送失能。
通信线断开或 MCU 不能执行时，软件无法保证停止命令到达驱动；本板没有 MCU 控制的步进电源切断通道。

## 5. 电脑端接收 HC-08

这只实物 HC-08 已测得 UART 为 **57600 8N1**，固件已固定为此速率；厂家手册的 9600 默认值不适用于当前模块配置。
HC-08 使用 BLE，Windows 自带蓝牙可通过 Bleak 订阅 FFE1 特征值；J-Link 的 COM3 不是它的蓝牙串口。

建立独立 Python 环境，在项目根目录执行：

```powershell
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r Firmware/tools/requirements.txt
.venv/Scripts/python.exe Firmware/tools/monitor.py --scan
.venv/Scripts/python.exe Firmware/tools/monitor.py --ble F8:2E:0C:F3:68:37 --out c5-live.csv
```

地址来自本次扫描，换模块后使用扫描所得地址。退出其他连接此模块的 BLE 客户端，再运行接收器。
`--seconds 30` 可限定记录时间；输出文件必须尚不存在，避免覆盖。
电脑端会按换行重组任意 BLE 分片，验证版本、字段数量，打印丢帧并保留带接收时间的 CSV。
连接后没有合法帧或意外断开会返回错误，重连使用新的输出文件名。

发送一次停止并查看后续数据：

```powershell
.venv/Scripts/python.exe Firmware/tools/monitor.py --ble F8:2E:0C:F3:68:37 --stop --seconds 5 --out stopped.csv
```

BLE 通道支持停止字节 `!`、固件查询和进入无线升级，不支持远程启动电机；升级协议见[参考说明](../reference/wireless-update.md)。
若改用有线 USB-UART，连接 USART1 的实际收发和地线后指定 `--port COMx --baud 57600`。
[遥测字段及错误码](../reference/firmware-telemetry.md)用于区分计数、串口异常和驱动故障。
