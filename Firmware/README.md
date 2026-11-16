# Coral-C5 LL 固件

面向 STM32C5A3VGT6 Core + Motor 实物联调，使用 ST C5 LL 与 CMSIS，未链接 HAL。
实现轮电机 PWM、双路编码器计数、Emm 连续旋转、HC-08 蓝牙遥测与无线升级、八路灰度输入和四键彩屏界面。
上电进入 ALL，电机不自动启动。按最左侧 RUN 后，两轮和步进持续旋转；再次按 RUN 全部停止。
默认两轮 PWM 45%、步进 30 RPM。当前不是循线或钢球控制程序。

按用户照片的摆放方向：**按钮在上、屏幕在下**，左至右为 **RUN / DIR / PWR（步进页为 SPD）/ TASK**。

- TASK：ALL → MONITOR → LEFT → RIGHT → BOTH → STEPPER → ALL。
- RUN：启动当前任务；MONITOR 中不启动电机。
- DIR：停止时切换方向；PWR：轮电机 10/20/30/45% 占空比；SPD：步进 30/60/90 RPM。
- ALL 页默认 45%/30 RPM；LVL 循环切换为 10%/30 RPM、20%/60 RPM、30%/90 RPM、45%/30 RPM。
- 运行时任意键先停止，不同时换任务或反转。ALL 与 STEPPER 不设 3 秒期限；LEFT/RIGHT/BOTH 单独测试仍每次最多 3 秒。
- ALL 等步进速度命令确认后启动两轮；任一轮驱动保护或步进通信/运动错误会停止联动，不会自动重新启动。停止流程结束、EMM READY 后可再次启动。
- J10 未插时可以手转读取编码器，轮驱动不能使能；程序不能检测跳帽是否插入。
- PA3 使能请求发出 5 ms 后仍读到低电平时，停止轮任务并显示 `ENABLE LOW / PA3`；这不是跳帽检测。检查方法见[使能线路排查](../Documentation/how-to/wheel-enable-check.md)。

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
- [2026-11-16 实物验证记录及剩余问题](../Documentation/validation/2026-11-16-firmware-bringup.md)
- [轮电机启动测试与使能异常记录](../Documentation/validation/2026-11-16-wheel-drive.md)
- [厂商库来源与许可](vendor/README.md)

`src/logic.c` 管理任务和消抖，`stepper.c` 管理 Emm 应答与停止流程，`serial.c` 使用中断环形队列。
`ui.c` 生成 128×160 RGB565 画面，`display.c` 分条传输，避免整屏传输长时间占用主循环。
轮驱动先保持 PWM 为零等待 5 ms 唤醒，再检查 nFAULT 与使能回读，故障位保留在屏幕直到下次启动。
SysTick 独立检查故障、使能电平、单独测试的 3 秒期限和主循环失联；ALL 只取消运行时长限制。没有 PID、轮径标定或电流采样闭环。

Debug 构建另有 SWD 单次按键入口 `debug_key_request`，用于电脑端复现相同的按键任务与停止保护。
写入 `0xc5000001 + key_id`：`key_id` 为 0 TASK、1 PWR/SPD、2 DIR、3 RUN/STOP。
地址从当前 ELF 的 `arm-none-eabi-nm` 输出取得；物理按键优先，主循环消费一次后清零。Release 不包含此入口。
Debug 的 `debug_stop_reason` 记录停止来源：低字节 1 按键、2 BLE、3 原始 nFAULT、4 应用期限、5 轮保护、6 步进停止；高字节记录对应故障位或 `wheel_trip`。Release 同样不含此诊断变量。

## 本机测试

使用主机 GCC（Windows 可用 MinGW）单独构建可移植测试：

```powershell
cmake -S . -B build/host -G Ninja -DHOST_TESTS=ON -DCMAKE_C_COMPILER=gcc
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
```

测试覆盖消抖、计数和时间回绕、任务停止、连续速度协议、丢应答与损坏接收、画面边界、BLE 分片与旧版遥测，以及升级分包、重连、重复 ACK、损坏镜像和跨 Flash 页安装中断恢复。
主机测试不能替代电机运动、电气电平与实际屏幕方向检查。
