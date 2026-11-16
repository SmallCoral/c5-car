# 2026-11-16 轮电机启动联调

用户重焊后反馈左右轮编码器均能正常读数，并插上 J10 授权测试轮电机。
用户随后处理 PA3 附近焊点；修正固件唤醒检查时序后，两轮主动转动与编码器反馈均通过，用户确认“左右轮都看到了”。
ALL 页面按 RUN 两轮与步进连续联动，再按一次全部停止；此前 10% 档已验证连续超过 6 秒和再次启停。后续 60% 与 45% 默认档的构建和烧录状态分别记录在本文末尾。
此前步进连续旋转和蓝牙验证保留在[初次记录](2026-11-16-firmware-bringup.md)。

## 早期使能故障记录

分别测试左、右轮的 10%、20%、30% PWM，每次最多 3 秒，期间通过 BLE 记录并由固件自动停止。

| 条件 | 结果 | 证据 |
| --- | --- | --- |
| 原跳帽，10% | 两侧计数增量均为 0，nFAULT 未拉低，三秒后停止 | [汇总](../../Firmware/validation/2026-11-16/wheels-10pct-summary.json) |
| 原跳帽，20% | 两侧计数增量均为 0；所选 TIM4 CCR 为 480/2400 | [汇总](../../Firmware/validation/2026-11-16/wheels-20pct-summary.json)、[左路寄存器](../../Firmware/validation/2026-11-16/wheels-20pct-left-registers.txt) |
| 原跳帽，30% | 两侧计数增量均为 0；所选 CCR 为 720/2400；PC2/PB6 输入回读有高低变化 | [汇总](../../Firmware/validation/2026-11-16/wheels-30pct-summary.json)、[左](../../Firmware/validation/2026-11-16/wheels-30pct-left-registers.txt)/[右](../../Firmware/validation/2026-11-16/wheels-30pct-right-registers.txt)寄存器 |
| 拔下 J10 | PA3 配置为推挽输出，ODR bit3=1，但 IDR bit3=0；停止后 ODR bit3=0 | [首次读取](../../Firmware/validation/2026-11-16/j10-out-enable-test.txt) |
| J10 拔下并软件复位 | 重复短时请求，仍是 ODR=0x18、IDR=0x45D1；随后停止，排除上次运行状态残留 | [复位后读取](../../Firmware/validation/2026-11-16/j10-out-reset-enable-test.txt) |
| 用户更换跳帽，新增保护，左轮 10% | `wheel_trip=4`，左右增量 0，PWM 两路为零、使能已撤销；未继续测试右轮 | [汇总](../../Firmware/validation/2026-11-16/wheels-10pct-jumper-replaced-summary.json)、[BLE](../../Firmware/validation/2026-11-16/wheels-10pct-jumper-replaced.csv)、[寄存器](../../Firmware/validation/2026-11-16/wheels-10pct-jumper-replaced-left-registers.txt) |

更换跳帽后的正式测试接收 6 个合法帧，解析拒绝 0，UART errors/RX drops/TX drops 均为 0。烧录前的独立接收收到 2 个完整帧并拒绝 1 个片段，不能把该预检查记为零拒绝。
上述 PWM 引脚读数不能代替示波器测得的波形，也不能证明 H 桥输出或电机端电压正常。
当时故障位于 J10 前端读回，不能仅据寄存器确认具体短路位置或 GPIO 损坏。[使能线路排查](../how-to/wheel-enable-check.md)保留当时测点。

## PA3 重焊与唤醒时序修正

重焊后的首次 BLE 测试只收到 2 帧后超时，不能据此确认电机运动或 MCU 复位；SWD 检查显示时基与主循环仍在运行，输出已停止。
加入 Debug 停止来源后，读到 `debug_stop_reason=0x203`：主循环在右路 nFAULT 低时撤销了启动，见[停止来源](../../Firmware/validation/2026-11-16/pa3-stop-trace.txt)。
原程序为 PWM 留了 5 ms 延迟，但主循环和 SysTick 提前判断了 nFAULT，导致驱动尚在唤醒时就被关闭。

按 [TI DRV8874 数据手册](https://www.ti.com/lit/ds/symlink/drv8874.pdf)的 tWAKE 和 active mode 要求，保持 PWM 为零完成 5 ms 唤醒，再检查 PA3/nFAULT 并允许 PWM。
唤醒结束后及运行中的故障照常停止，错误位在界面锁存，不自动重试。此解释得到后续正常运动支持，未用示波器测量 nFAULT 脉宽。

| 10% 短时测试 | 左计数增量 | 右计数增量 | 停止与证据 |
| --- | --- | --- | --- |
| 首轮左、右分别启动 | −1231 | +1634 | 各 3 秒自动停止；[汇总](../../Firmware/validation/2026-11-16/wheels-10pct-pa3-wake-fix-summary.json) |
| 用户要求再测一次 | −1508 | +1945 | 各 3 秒自动停止，用户确认两轮均转动；[汇总](../../Firmware/validation/2026-11-16/wheels-10pct-pa3-wake-repeat-summary.json) |

两轮不是在上述单独测试中同时启动。两轮启动期间 GPIOA ODR/IDR 的 PA3 位均为高；对应 PWM CCR=240/2400。
两轮测试分别记录 24、27 帧，解析拒绝 0，运行期间 nFAULT 未报告故障，停止为 `wheel_trip=2`。
此阶段源码与产物见[唤醒修正版清单](../../Firmware/validation/2026-11-16/source-manifest-wake-fix.json)。

## ALL 连续联动与再次按键停止

用户追加要求三台电机按一下持续旋转、再按停止。新增 `TASK_ALL=5` 为默认页面，上电仍不动作。
RUN 先发起 Emm 使能/速度握手，收到速度应答后启动两轮。仅 ALL 取消两轮的 3 秒期限，LEFT/RIGHT/BOTH 保持短时测试。
任意键或 BLE `!` 停止全部；步进停止/故障会关轮使能，轮驱动保护也会发起步进停止/失能，恢复通信不会自动重启。

提醒用户暂不按键后，复测两次 RUN 启停通过，见[31 帧记录](../../Firmware/validation/2026-11-16/all-continuous-settled.csv)、[汇总](../../Firmware/validation/2026-11-16/all-continuous-settled-summary.json)。

| 周期 | 连续运行采样跨度 | 左/右计数增量 | 步进位置增量 | 实际转速 | 再按 RUN |
| --- | --- | --- | --- | --- | --- |
| 1 | 6501 ms | −4533 / +5244 | 223346 | 29–30 RPM | 轮使能 0、步进失能且回到 0 RPM |
| 2 | 1500 ms | −1491 / +1690 | 65416 | 29 RPM | 同上 |

第一次记录证明运行超过原 3 秒期限；第二次验证停止后能再启动。测试默认轮 PWM 10%、步进 30 RPM，最终三台均停止。
本次运行期间没有新的轮保护或步进错误；步进超时累计仍保留此前的 1 次，没有清零掩盖历史记录。

保留两次未完成的脚本记录：初次 ALL 第一周期通过，第二周期脚本将 5 ms 唤醒期间的瞬时 `fault_mask=3` 当作故障并主动发送停止，见[原记录](../../Firmware/validation/2026-11-16/all-continuous.txt)。
另一次重连时计数已大幅增加，停止后立即重新请求过程中出现一次步进超时及 E2，具体原因未确定，见[失败记录](../../Firmware/validation/2026-11-16/all-continuous-repeat.csv)。
测试脚本随后等待轮/步进静止并取得唤醒后的有效运动反馈，再执行上述成功复测；该调整没有更改固件保护条件。

## 固件变更与验证

- Debug 增加单次 SWD 按键入口和停止来源，经过原有任务控制路径；Release 不包含这些入口。
- 主循环在允许 PWM 前检查 PA3 输入回读，SysTick 在请求至少 5 ms 后也检查。低电平会清零 PWM、撤销使能、锁存 `WHEEL_ENABLE_LOW=4`，直到下次启动请求。保留三秒期限、nFAULT 和主循环失联保护。
- 屏幕显示 `ENABLE LOW / PA3` 和 ERR；本轮有 `lcd_ok=1` 遥测，未取得新增提示的实物照片。
- 最新 Debug/Release 编译通过，应用 `-Wall -Wextra -Werror`；Debug Flash 16980 B、RAM 42668 B，Release Flash 14880 B、RAM 42660 B，均另预留 8192 B 栈。
- CTest 2/2、Python unittest 4/4 通过；新增 ALL 不自动超时、再次 RUN 停止、停止后不自启、fault 停止与 task=5 遥测解析检查。
- 早期 PA3 负向测试得到 `wheel_trip=4` 且两路 PWM 均为零；最新正常运动/连续启停通过。ALL 中硬件故障注入、运行中使能掉低、长期负载和转向标定尚未验证。
- [最新烧录日志](../../Firmware/validation/2026-11-16/flash-all-continuous.txt)确认写入、校验及启动成功。早期保护版保留[源码清单](../../Firmware/validation/2026-11-16/source-manifest-enable-guard.json)。

源码为 Git `64c5210` 加未提交工作区，当前应用源码与产物 SHA-256 见[清单](../../Firmware/validation/2026-11-16/source-manifest.json)。
工具：Arm GNU GCC 14.3.1、CMake 4.3.1、Ninja 1.13.2、主机 MinGW GCC 15.2.0、STM32CubeProgrammer 2.23.0、J-Link Commander 9.82（探针 69730331），BLE 使用 Bleak 3.1.0。

在 `Firmware` 构建：`cmake --build --preset debug`、`cmake --build --preset release`、`cmake --build build/host`、`ctest --test-dir build/host --output-on-failure`。
从项目根目录烧录：

```powershell
STM32_Programmer_CLI -c port=JLINK sn=69730331 -w Firmware/build/debug/coral_c5.hex -v -g 0x08000000
```

本机使用 `.scratch/firmware-tools/Scripts/python.exe` 运行测试脚本：`.scratch/wheel-test/run.py --duty 100 --label pa3-wake-repeat` 与 `.scratch/wheel-test/all-run.py --label settled`。
脚本用当前 ELF 查找 SWD 按键入口，接收 BLE 后执行任务；实际按键写入命令见[单轮调试器日志](../../Firmware/validation/2026-11-16/wheel-test-jlink.txt)、[联动调试器日志](../../Firmware/validation/2026-11-16/all-continuous-jlink.txt)。
所有测试结束时轮使能为 0，步进为停止/失能状态。本轮没有修改原理图/PCB，没有替代或重跑 ERC/DRC。

## 60% 默认档（2026-11-16）

用户要求两路编码电机占空比调至 60%。轮档位扩展为 10/20/30/60%，开机选择 60%，输出允许上限同步改为 600‰；步进默认仍为 30 RPM。ALL 保持按 RUN 启动、再按停止，上电不启动。
屏幕占空比改为读取与电机及遥测相同的档位函数，避免第四档显示成 40%。ALL 档位循环为 60%/30 RPM → 10%/30 RPM → 20%/60 RPM → 30%/90 RPM；STEPPER 单独页面仍为 30/60/90 RPM。

源码基于同一未提交工作区，本次源码与产物散列见[60% 清单](../../Firmware/validation/2026-11-16/pwm60/source-manifest.json)。工具沿用上述版本，执行命令：

```powershell
cmake --build --preset debug
cmake --build --preset release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Debug/Release 编译通过，CTest 2/2 通过。覆盖默认 600‰、四档正反转、输出上限、持续运行与再次停止、UI 缓冲边界。Debug 首次构建的日志重定向路径写错，产物已生成；修正路径后的构建结果为 `no work to do`，没有把该日志当作完整重编译输出。

烧录前已通过蓝牙发送停止命令，4 帧显示 ALL 停止、两轮计数速率为 0、步进 0 RPM、故障为 0，见[停止记录](../../Firmware/validation/2026-11-16/pwm60/before-flash.csv)。
首次烧录失败：`No J-Link/Flasher probe detected`；枚举同时未检测到 ST-Link、DFU 或串口，见[首次烧录日志](../../Firmware/validation/2026-11-16/pwm60/flash.txt)。
用户重新接上下载器后，使用相同烧录命令重试，下载、校验和启动全部成功，见[重试日志](../../Firmware/validation/2026-11-16/pwm60/flash-retry.txt)。

按当前 ELF 的调试信息定位变量后，用 J-Link Commander 读取 RAM：`controls.task=5`、`controls.level=3`、`ui.duty=0x258`（600‰）、`controls.running=0`，确认实物中的默认档位及停止状态，见[回读日志](../../Firmware/validation/2026-11-16/pwm60/boot-readback.txt)。地址属于本次 ELF，后续构建必须重新查询。
烧录后首次蓝牙接收在 1 帧后断开，保留[该次记录](../../Firmware/validation/2026-11-16/pwm60/after-flash.csv)；重连后连续接收 10 帧，均为 ALL 停止、轮速计数率为 0、步进 0 RPM、故障为 0，见[重连记录](../../Firmware/validation/2026-11-16/pwm60/after-flash-retry.csv)。本轮没有主动发送 RUN，未采集 60% 运行阶段的 PWM 寄存器及遥测，不能把构建、烧录和停止状态检查当作 60% 持续运动、电流、实际转速或温升验证。

## 降至 45%（2026-11-16）

用户反馈 60% 可能过快，要求改试 45%。默认第四档与允许上限一起从 600‰ 降为 450‰，保留 10/20/30% 三档；步进默认 30 RPM，RUN 启停和所有保护流程保持原行为。屏幕和遥测仍读取同一个档位函数。

源码基于同一未提交工作区，散列见[45% 清单](../../Firmware/validation/2026-11-16/pwm45/source-manifest.json)。工具和构建命令同上一节，Debug/Release 均编译通过，现有 CTest 2/2 通过；默认档、四档正反转和 UI 预览的预期值同步为 450‰。构建日志保留在该清单同目录。

烧录前通过 BLE 发送停止命令，收到两帧两轮计数速率及步进转速为 0、无故障的[记录](../../Firmware/validation/2026-11-16/pwm45/before-flash.csv)。J-Link 69730331 在线，使用本文列出的 STM32CubeProgrammer 命令烧录，下载、校验及启动成功，见[烧录日志](../../Firmware/validation/2026-11-16/pwm45/flash.txt)。

重新从当前 ELF 查询字段地址后，用 J-Link 回读 `controls.task=5`、`controls.level=3`、`ui.duty=0x1c2`（450‰）、`controls.running=0`，见[板上参数](../../Firmware/validation/2026-11-16/pwm45/boot-readback.txt)。本轮未主动启动电机；45% 的实际运行速度与负载表现待用户按 RUN 试转，不将参数回读当作运动或温升验证。
