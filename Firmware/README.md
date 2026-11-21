# Coral-C5 LL 固件

面向 STM32C5A3VGT6 Core + Motor 实物联调，使用 ST C5 LL 与 CMSIS，未链接 HAL。
11 月 20 日最后一次现场回读为第一题实测版 `8A10EDEE`。本分支 Debug 构建得到含第二题里程停车的 `D97DBD0E`。复测原版第一题应使用对应存档。[两份 OTA 与调试符号](releases/README.md) · [相机程序](26h/README.md)
实现轮电机 PWM、双路编码器计数、Emm 限位点动与转速控制、HC-08 蓝牙遥测与无线升级、八路灰度输入和四键彩屏界面。
上电或软件复位进入校赛 A 题主页。按键按实物从左到右排列为 **SW5 返回、SW4 上一项、SW3 下一项、SW2 进入或确认**。松开 SW2 后再按一次才启动。主页同时按 SW4、SW3 进入设备调试列表。

主页依次列出第一题静止移球、第二题循线停车、第三题载球半圈、第四题载球整圈、发挥一定点稳球、发挥二图传回放。每次进入第一题准备页，步进先返回标定水平，屏幕显示 `LEVEL OK`。钢球可放在管内任意位置，短按 SW2 开始计时。8 s 是比赛目标，超过后继续按原路线运行，停稳时显示实际用时。车轮保持停止，钢球依次在 O、右侧 +50 mm、左侧 −50 mm 稳住至少 0.30、0.40、0.40 s，末点计时完成后继续闭环保持。每个停留区间内，直接观测的位置误差不超过 10 mm、最大与最小球位之差不超过 4 mm，起止估计速度不超过 12 mm/s。孤立漏检两侧的真实球位足够接近时，主控扣除两帧间隔并继续计时。长时间漏检且相机仍在发包时显示 `WAIT VISION`。运动中回水平等待，接近左点且已低速时保持轴角。球位重新确认后继续当前阶段，再计未完成的停留。第二题接入 A 点顺时针一圈停车。其余四项显示要求和接入状态，发挥一支持预选 −5～+5 cm 的整数目标。[第一题实测记录](validation/2026-11-20/task1-recovery.md)

第二题从比赛首页按 SW3 选中，SW2 进入。将车放在 A 点朝顺时针，再按一次 SW2 立即起跑。八路灰度负责差速循线，双轮编码器的平均行程达到 41800 计数即停车。末弯按正常速度循线，最后 900 计数平滑收速。准备页 SW3（SPD）可换档，上电默认直道 4600、弯道最高 3100 编码器计数/秒。运行页显示灰度、双轮速度、里程和时间，30 s 限时及丢线、轮驱动保护继续生效。[第二题现场操作](../Documentation/how-to/line-following-lap.md)

设备调试页保留原 BALANCE 回中模式。进入 BALANCE 后按 SW2，或发 `C5:BAL` 后发 `C5:HOLD` 开始寻球。驱动以绝对水平读数 10260 恢复物理行程，右端最高对应轴角 −70.20°，最低为 +106.53°。按键或蓝牙 `!` 可抱停。

转轴实际运动后，主控用至少 5 个直接观测确认轨迹。这些观测需覆盖至少 95 ms，有至少 3 次同向位移，净移动达到 40 个控制像素（约 80 个相机像素）。相邻观测可隔一帧漏检，间隔最长 100 ms。主控先抱停并核对钢球仍沿同一方向滚动，再切换到 `CENTERING`。搜索间歇显示 `NO BALL TRACK`。若相机断流，电机原位保持，收到新数据后继续。

外环名义 20 ms 执行一次，第 4 题的比例增益为 0.08，对球坐标作差分的增益为 0.4，目标管角限幅 ±3°。内环名义 15 ms 执行一次，比例增益为 8，对估计管角作差分的增益为 0.7。角差在 ±0.5° 内输出零速，其余按原逻辑取整、限幅到 ±50 rpm，非零转速随有限位终点的 FD 绝对定位指令发送，转速为零时发 F6 零速。屏幕显示最近下发的方向和 rpm。主控从自动启动或接受 RUN 开始计时，包含抱持、转动与持续一秒的居中判定。[原控制源码](ports/classmate_26h/balance.c) · [与前版的比较](../Documentation/explanation/classmate-balance-reference.md)

BALANCE 和 LAP 页的五个刻度依次是 `L、-5、O、+5、R`。当前安装姿态的相机原始 x 依次为 531、400、282、160、49。实心黄点表示本次直接观测，短暂漏检用空心圈标出最近位置，最长 2 秒。大幅换位后，屏幕可从三次相邻位置的直接观测重新显示静止球。LCD 按这五个实物标记绘制。稳球控制以 x=282 为 O，像素偏差除以 2 后送入原控制环。[实物五点标定](validation/2026-11-19/camera640-and-shaft.json) · [板上验证](../.agent/main/handoff.md)

本车确认 CCW 会抬高右端。默认 MAP+ 下，静止球在右侧时发 CCW，左侧时发 CW。原程序的目标 160 在相机适配处对齐本车 O。原机构的 `3200 × 0.039` 管角换算随控制源码保留，本车驱动已读到 3200 脉冲/转，0.039 对本连杆的角度比例尚待实测。

进入稳球后，主控以实测水平为优先参考，并按当前轴位限制首次闭环校正。当步进已达到所需倾角、钢球仍持续偏在一侧时，主控根据球位修正这个参考。软限位在修正后的参考两侧各最多 8°，全程受标定的机械总行程约束。无可信新球位 120 ms 请求零速，持续 3 s 会在原行程内重新寻球。编码器 40 ms 无更新、驱动应答超时或异常会抱停。驱动抱持连杆期间拒绝无线重启。
LAP 默认选择 4600 CPS，从 A 顺时针起步时两个半圆弯的目标为 3100 CPS。控制器以 CH4/CH5 中点为目标。3300/2200 CPS 稳定档仍可从 SPD 选取，也保存了能直接刷回的完整镜像。[高速档验证](../Documentation/validation/2026-11-18-lap-fast.md)、[稳定版与恢复命令](releases/lap-3300-BF33A9F7/README.md)
11 月 17 日的 2200 CPS 版本连续两圈分别为 24.333 s 和 24.262 s，用户确认回 A 自动停车。停车位置误差仍待量测。[旧版实测](../Documentation/validation/2026-11-17-lap-repeat.md)

独立 Rust 迁移工程位于 [Rust](Rust/README.md)，其工具链与 RustRover 配置见 [Rust 开发说明](../Documentation/how-to/rust-development.md)。
Rust 版尚未烧录，本页的板上操作和无线升级步骤适用于已实测的 C/LL 版。

在 **BALANCE 待机页**，中间两键用于手动点动。短按左侧的 **SW4 / LEFT**，电机轴逆时针 1.8°。短按右侧的 **SW3 / RIGHT**，电机轴顺时针 1.8°。松开按键时执行，到位后步进继续抱持。连续点按累计目标，同时按住两键不产生点动。实测配置为 3200 脉冲/转，每次 16 个微步。屏上 `SHAFT` 和 `GOAL` 在点动时显示相对实物水平的电机轴角度。水管倾角由曲柄连杆决定。

在 BALANCE 待机页同时按下 SW4 和 SW3，或通过蓝牙发送 `C5:CAL`，可进入 **CALIBRATE**。松开后可用 SW4 / UP、SW3 / DOWN 反向点动，每步 1.8°。按住 0.45 秒后会重复走步，每次等待前一步到位；松手后完成当前一步并抱持。SW5 返回 BALANCE 并抱停，SW2 可原位停止。屏上 `SHAFT` 以实物水平为零，`ENC` 为实测轴编码器计数，`BALL X` 为相机最新实测像素。UP 和 DOWN 对应相反的轴转向，管端升降随曲柄相位变化。

待机时，按住 SW4 满 0.7 s 切换一次自动纠偏方向，按住 SW3 满 0.7 s 记录本次维护操作的参考轴位。永久机械限位仍由绝对编码器确定。自动稳球中，任意键先停车，松开后再短按方向键进入点动。点动过程中 SW5、SW2 均可停止并抱持。车轮始终停止，点动不依赖摄像头。

- 调试列表包含 BALANCE、LAP、GRAY、MONITOR 和五个电机测试页。SW5 从待机页返回列表，再按一次返回主页。
- SW2 启动所选可运行的调试项目。
- DIR：停止时切换方向；PWR：轮电机 10/20/30/45% 占空比；SPD：步进 30/60/90 RPM。
- ALL 页默认 45%/30 RPM；LVL 循环切换为 10%/30 RPM、20%/60 RPM、30%/90 RPM、45%/30 RPM。
- 自动任务运行时任意键先停止。手动点动时，两个方向键继续调整目标。ALL 与 STEPPER 不设 3 秒期限；LEFT/RIGHT/BOTH 单独测试仍每次最多 3 秒。
- ALL 等步进限位定位命令确认后启动两轮；任一轮驱动保护或步进通信/运动错误会停止联动，不会自动重新启动。停止流程结束、EMM READY 后可再次启动。
- J10 未插时可以手转读取编码器，轮驱动不能使能；程序不能检测跳帽是否插入。
- PA3 使能请求发出 5 ms 后仍读到低电平时，停止轮任务并显示 `ENABLE LOW / PA3`；这不是跳帽检测。检查方法见[使能线路排查](../Documentation/how-to/wheel-enable-check.md)。

LAP 页用 RUN 启停。停止时用 MAP 切换两轮安装方向组合 A/B，SPD 循环选 800、1800、2200、3300、4600 计数/秒。
实车左轮接 J6、右轮接 J4，板层统一映射后，LAP 默认实车左正/右负电气方向（MAP A），即 J6 正、J4 负。驱动、编码器与故障位使用同一物理左右映射，见[电机接口](../Documentation/reference/motor-interface.md)和[循线一圈步骤](../Documentation/how-to/line-following-lap.md)。
控制每约 10 ms 更新。位置经过 15 ms 低通，变化率经过 35 ms 低通。高档根据实际偏差和未来 60 ms 的预计偏差调速，黑线向外偏一个探头间距时收回额外巡航速度。两轮编码器用于维持差速，单轮 PWM 上限在 4600 档为 60%，其余档为 45%。[控制参数](include/config.h)
从 A 横线或可识别的标记边缘按 MAP A 起步，两处半圆弯会提前收速，再逐渐加入基础右转差速。灰度持续纠偏，半轮速差最多为共同目标的 38%。普通窄线起步和 MAP B 使用灰度与轮速反馈。[起步与调速](../Documentation/how-to/line-following-lap.md)
放在 A 横线及其分段边缘时允许起步，先以不超过 800 CPS 的共同目标直行驶离；屏幕显示 LEAVE A，最多 1.5 s 或 1000 计数。普通窄线稳定 100 ms 后进入正常循线。
停车检测需运行至少 2 s、平均行程至少 30000 计数，再检测四路及以上、至多两段的标记 10 ms。60000 计数仍未认到 A 则显示 NO A MARK 并停止，不将里程上限当作回到 A。窗口按当前约 6.142 m 赛道设置，未标定为毫米。
39000 计数起显示 A NEAR，共同目标最高 1100 CPS；41000～47000 内允许三路部分覆盖标记持续 30 ms 后停车，避免仅靠四路门限漏认。窗口外三路普通线不触发停车。
无目标持续 150 ms、多段/异常线持续 250 ms、编码器异常、控制周期超过 50 ms、轮驱动保护或运行 90 s 都会停车，恢复后需重新按 RUN。

GRAY 页按 CH1～CH8 显示灰度辅助板的八路数字量、滤波结果、目标数量、段数和通道偏差。
PWR 对应的 B/W 键切换黑线/白线目标；VIEW 和 DIR 不启动电机。默认黑线，单段目标显示 `LINE`，其他情况显示 `NO LINE`、`ALL` 或 `MULTI`，偏差显示 `--`。
2026-11-17 用户完成黑白校准，并确认按车辆前进方向看左到右是 CH1～CH8，偏差左负右正。校准和逐路检查见[灰度联调](../Documentation/how-to/grayscale-check.md)。

## 构建

需要 CMake ≥3.22、Ninja、Arm GNU 工具链和 Python ≥3.10；本机已验证 GCC 14.3.1。进入本目录：

```powershell
cmake --preset debug
cmake --build --preset debug
cmake --preset release
cmake --build --preset release
```

输出 `build/debug/coral_c5.elf`、`.hex`、`.bin`、无线包 `.ota` 和首次安装包 `first_install.hex`，Release 在 `build/release`。应用现在链接到 `0x08010000`。
当前板已安装引导程序。日常从本目录运行 `./flash-wireless.ps1`，或构建 `wireless_flash`，通过 HC-08 上传后回读应用的长度和 CRC。步进仍在托住水管时，先独立托稳，再用 `./flash-wireless.ps1 -PipeSupported`。有线调试和救援保留 `./flash-jlink.ps1`，默认传入 `-NoGui 1`。
核心时钟 48 MHz；参数集中在 [config.h](include/config.h)，管脚集中在 [board.c](src/board.c)。
原 MX2 工程已从本目录移出，本机备份在 `.scratch/firmware-baseline-2026-11-16`；无需 MX2 重新生成。

## 联调入口

- [无线烧录、CLion 入口与异常恢复](../Documentation/how-to/wireless-update.md)
- [无线升级的 Flash 布局和协议](../Documentation/reference/wireless-update.md)
- [2026-11-16 无线升级实测记录](../Documentation/validation/2026-11-16-wireless-update.md)
- [烧录、按键、编码器和蓝牙接收步骤](../Documentation/how-to/firmware-bringup.md)
- [遥测字段、状态与错误码](../Documentation/reference/firmware-telemetry.md)
- [八路灰度校准、页面与蓝牙观察](../Documentation/how-to/grayscale-check.md)
- [灰度循线一圈、方向核对与调速](../Documentation/how-to/line-following-lap.md)
- [2026-11-16 实物验证记录及剩余问题](../Documentation/validation/2026-11-16-firmware-bringup.md)
- [轮电机启动测试与使能异常记录](../Documentation/validation/2026-11-16-wheel-drive.md)
- [厂商库来源与许可](vendor/README.md)

`src/logic.c` 管理任务和消抖，`stepper.c` 管理 Emm 应答与停止流程，`serial.c` 使用中断环形队列。
`ui.c` 生成 128×160 RGB565 画面，`display.c` 每次只绘制并传输 2 行；跳过切片外的文字，同一帧使用固定状态快照，为灰度采样和控制留出时间。
轮驱动先保持 PWM 为零等待 5 ms 唤醒，再检查 nFAULT 与使能回读，故障位保留在屏幕直到下次启动。
SysTick 独立检查故障、使能电平、单独测试的 3 秒期限和主循环失联；LAP 使用独立任务期限，ALL 为连续运行。
轮径与编码器每转计数尚未标定，轮速目标直接使用计数/秒；没有电流采样闭环。

Debug 构建另有 SWD 单次按键入口 `debug_key_request`，用于电脑端复现相同的按键任务与停止保护。
写入 `0xc5000001 + key_id`：`key_id` 为 0 TASK、1 PWR/SPD、2 DIR、3 RUN/STOP。
地址从当前 ELF 的 `arm-none-eabi-nm` 输出取得；物理按键优先，主循环消费一次后清零。Release 不包含此入口。
Debug 的 `debug_stop_reason` 记录停止来源：低字节 1 按键、2 BLE、3 原始 nFAULT、4 应用期限、5 轮保护、6 步进停止、7 LAP；高字节记录对应故障位、`wheel_trip` 或 `lap_status`。Release 同样不含此诊断变量。
Debug 的 BLE `C5:CHECK` 只在停止时发起一次 15%/1.5 s 架空方向检查，独立于灰度；普通 LAP 仍使用 RUN。`C5:PERF` 返回主循环、显示服务、遥测服务最大耗时，单位毫秒。

## 本机测试

使用主机 GCC（Windows 可用 MinGW）单独构建可移植测试：

```powershell
cmake -S . -B build/host -G Ninja -DHOST_TESTS=ON -DCMAKE_C_COMPILER=gcc
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
```

测试覆盖消抖、计数和时间回绕、任务停止、连续速度协议、丢应答与损坏接收、画面边界和切片一致性、灰度滤波及两种极性的全部 256 种输入、实车左右映射、比赛里程停车、调试页横线停车及轮驱动保护。循线模型覆盖原有 432 组和新增 72 组无横线整圈。另有 BLE 分片、v1～v6 遥测、升级分包、重连、重复 ACK、损坏镜像及跨 Flash 页安装中断恢复测试。模型单独统计探头居中率，停车精度需在实车上量测。
主机测试不能替代电机运动、电气电平与实际屏幕方向检查。
