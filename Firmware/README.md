# Coral-C5 LL 固件

面向 STM32C5A3VGT6 Core + Motor 实物联调，使用 ST C5 LL 与 CMSIS，未链接 HAL。
实现轮电机 PWM、双路编码器计数、Emm 连续旋转、HC-08 蓝牙遥测与无线升级、八路灰度输入和四键彩屏界面。
上电进入 LAP，保持停止。RUN 启动灰度循线，离开 A 横线后再返回横线时停车；任意键可提前停止。
LAP 默认直线目标为 3300 编码器计数/秒。弯道以 CH4/CH5 中点为目标，按灰度持续偏差和编码器反馈维持差速，出弯后恢复巡航。LAP 只驱动两轮，ALL 联调页提供两轮 45% PWM、步进 30 RPM 的连续联动。[弯道居中验证](../Documentation/validation/2026-11-18-lap-centering.md)
11 月 17 日的 2200 CPS 版本连续两圈分别为 24.333 s 和 24.262 s，用户确认回 A 自动停车。停车位置误差仍待量测，钢球控制尚待接入。[旧版实测](../Documentation/validation/2026-11-17-lap-repeat.md)

独立 Rust 迁移工程位于 [Rust](Rust/README.md)，其工具链与 RustRover 配置见 [Rust 开发说明](../Documentation/how-to/rust-development.md)。
Rust 版尚未烧录，本页的板上操作和无线升级步骤适用于已实测的 C/LL 版。

按用户照片的摆放方向：**按钮在上、屏幕在下**，左至右为 **RUN / DIR / PWR（步进页为 SPD）/ TASK**。

- TASK：LAP → MONITOR → LEFT → RIGHT → BOTH → STEPPER → ALL → GRAY → LAP。
- RUN：启动当前任务；MONITOR 中不启动电机。
- DIR：停止时切换方向；PWR：轮电机 10/20/30/45% 占空比；SPD：步进 30/60/90 RPM。
- ALL 页默认 45%/30 RPM；LVL 循环切换为 10%/30 RPM、20%/60 RPM、30%/90 RPM、45%/30 RPM。
- 运行时任意键先停止，不同时换任务或反转。ALL 与 STEPPER 不设 3 秒期限；LEFT/RIGHT/BOTH 单独测试仍每次最多 3 秒。
- ALL 等步进速度命令确认后启动两轮；任一轮驱动保护或步进通信/运动错误会停止联动，不会自动重新启动。停止流程结束、EMM READY 后可再次启动。
- J10 未插时可以手转读取编码器，轮驱动不能使能；程序不能检测跳帽是否插入。
- PA3 使能请求发出 5 ms 后仍读到低电平时，停止轮任务并显示 `ENABLE LOW / PA3`；这不是跳帽检测。检查方法见[使能线路排查](../Documentation/how-to/wheel-enable-check.md)。

LAP 页用 RUN 启停。停止时用 MAP 切换两轮安装方向组合 A/B，SPD 循环选 800、1800、2200、3300 计数/秒。
实车左轮接 J6、右轮接 J4，板层统一映射后，LAP 默认实车左正/右负电气方向（MAP A），即 J6 正、J4 负。驱动、编码器与故障位使用同一物理左右映射，见[电机接口](../Documentation/reference/motor-interface.md)和[循线一圈步骤](../Documentation/how-to/line-following-lap.md)。
控制每约 10 ms 更新。位置经过 15 ms 低通，变化率经过 35 ms 低通。中心附近限制微分产生的轮速差，超出 CH4/CH5 后提高位置增益。持续偏向同侧、编码器确认车辆正在转弯时，控制器积累与前进速度成比例的差速。共同目标依据位置趋势、实际转向和剩余 PWM 余量降速，单轮输出上限为 45%。[控制参数](include/config.h)
从 A 横线或可识别的标记边缘按 MAP A 起步，当前赛道的两处半圆弯会提前降至最高 2200 CPS，逐步加入随速度缩放的基础右转差速。灰度继续修正偏差，半轮速差最多为共同目标的 38%。普通窄线起步和 MAP B 使用灰度与轮速反馈。[起步与调速](../Documentation/how-to/line-following-lap.md)
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
当前板已安装引导程序，日常从本目录运行 `./flash-wireless.ps1`，或构建 `wireless_flash` 目标，即可编译并通过 HC-08 上传；无需接 J-Link。先关闭占用模块的遥测客户端。
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

测试覆盖消抖、计数和时间回绕、任务停止、连续速度协议、丢应答与损坏接收、画面边界和切片一致性、灰度滤波及两种极性的全部 256 种输入、实车左右映射、LAP 起停与保护、180 组循线模型、BLE 分片与 v1～v6 遥测，以及升级分包、重连、重复 ACK、损坏镜像和跨 Flash 页安装中断恢复。模型单独统计探头居中率，停车精度需在实车上量测。
主机测试不能替代电机运动、电气电平与实际屏幕方向检查。
