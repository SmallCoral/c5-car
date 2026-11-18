# Coral-C5 交接 · 弯道居中与提速

2026-11-18 更新于 `SmallCoral/c5-car` 的 `main` 分支。高速档基于已归档的稳定版提交 `289a85a`。板上应用对应工作区的[源码与镜像散列](../../Firmware/validation/2026-11-18/lap-fast/source-manifest.json)。

## 目标与完成条件

校赛 A 题使用 STM32C5A3 Core + Motor 实现循线小车与钢球稳控。本轮同时提高直道、半圆弯的速度，观察 CH4/CH5 居中率、全白样本、实测轮速、圈时和回 A 结果。

## 当前状态

- 上电默认 4600 CPS，识别 A、使用 MAP A 时两段半圆弯最高 3100 CPS，直道和弯道目标分别比 3300 档提高 39% 和 41%。高速档单轮 PWM 上限 60%，SPD 提供 800、1800、2200、3300 和 4600。
- 当前 Debug 应用为 26,432 B、CRC `D2FDE38F`，通过 HC-08 上传并核对运行应用。Release 为 22,924 B、CRC `BBD734DE`。高速初版第一弯有摆动。按两弯实测调整基础差速后，完整采集两圈 14.712 s、14.612 s，现场确认都回 A、过弯稳定。另回读到一次 14.602 s 的 DONE。直道、弯道实测均速为 4366、2952 CPS，弯道居中率为 62/95（65.3%）。[高速档验证](../../Documentation/validation/2026-11-18-lap-fast.md)
- 稳定标签 `lap-stable-3300-BF33A9F7` 含源码、实车记录及可直接恢复的 OTA。该版两圈分别为 18.872 s / 41939 计数和 18.905 s / 41872 计数，均为 DONE，现场确认回 A、过弯稳定。第一圈正常区间 107/144 个窄线样本居中，弯道内部 36/65 个居中，直道均速 3154 CPS，弯道均速 2122 CPS。[恢复命令](../../Firmware/releases/lap-3300-BF33A9F7/README.md)、[稳定版实测](../../Documentation/validation/2026-11-18-lap-centering.md)
- 主机 CTest 6 项、Python 16 项及双配置 ARM 构建通过。两档各有 180 组回归，另有 ±2% 里程尺度的 72 组高速整圈，总计 432 组模型。
- 旧版 2200 CPS 在同一赛道两次回 A，分别为 24.333 s / 43059 计数、24.262 s / 42898 计数。停车误差仍待量测。C/LL 遥测为 v6、56 列，运行时约 10 Hz。[旧版两圈记录](../../Documentation/validation/2026-11-17-lap-repeat.md)
- **Rust 保持独立：** `Firmware/Rust` 仍为代码迁移版，遥测 v2，未同步本轮灰度、循线和左右映射，未烧录。11 月 16 日主机 30 项测试及 ARM 构建证据见 [Rust 验证](../../Firmware/Rust/VALIDATION.md)，不能替代 C/LL 当前结果。
- **硬件范围：** 本次未编辑硬件，也未重跑 ERC/DRC。整理时发现 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/05_Right_Motor.kicad_sch` 有用户手工改动，保留本地、未纳入本次代码文档提交。11 月 16 日快照的 Motor/Transfer 脚本各 4 项失败及未运行 DRC 的边界仍见[历史交接检查](../../Documentation/validation/2026-11-16-rust-handoff.md)。

## 后续步骤

1. 从 A 朝 B 放车，在 LAP 按 RUN；自动停稳后原地再次 RUN。记录一圈时间、停止提示、实际停车位置及相对车体基准点的误差，覆盖不同电量和重复次数。操作见[循线一圈](../../Documentation/how-to/line-following-lap.md)。
2. 标定轮径、编码器每转计数和实际圈长；更换场地、轮胎或起点时重测 A 窗口。约 43000 计数/圈只是本车本赛道经验，不能换算成精确位置。保护停车不在 A 时先把车放回 A。
3. 如仍摆动或漏停，先保留遥测中的位置、变化率、轮速差、转向目标、控制间隔和标记峰值；与基线比较后调整有依据的参数，不关闭丢线、编码器、超时或行程保护。
4. 视觉、摆杆角度反馈和钢球稳控尚未实现。确认 J19 线束、单一供电与 UART 数据后再接入；AS5600 等候选未形成已验证实现。手工硬件改动另行保存基线、检查合同/网表/ERC/DRC，保留位号、UUID 和布局。
5. Rust 后续工作先按[开发说明](../../Documentation/how-to/rust-development.md)迁移本轮控制与协议、做离线验证；用户当前要求只改代码、不实际烧录，不得用 C/LL 板上结论替代 Rust 实测。

## 关键文件

| 路径 | 用途 |
| --- | --- |
| [Firmware/include/config.h](../../Firmware/include/config.h) | 速度、滤波、转向、标记窗口、超时与实车映射参数 |
| [Firmware/src/lap.c](../../Firmware/src/lap.c)、[lap.h](../../Firmware/include/lap.h) | 起步/循线/停车状态、轮速 PI、转向阻尼、行程与保护 |
| [Firmware/src/grayscale.c](../../Firmware/src/grayscale.c)、[grayscale.h](../../Firmware/include/grayscale.h) | 逐路滤波、目标段和偏差 |
| [Firmware/src/board.c](../../Firmware/src/board.c)、[board.h](../../Firmware/include/board.h)、[gray_map.h](../../Firmware/include/gray_map.h)、[wheel_map.h](../../Firmware/include/wheel_map.h) | 实车输入输出归一化与轮驱动保护 |
| [Firmware/src/main.c](../../Firmware/src/main.c)、[logic.c](../../Firmware/src/logic.c)、[logic.h](../../Firmware/include/logic.h) | LAP 调度、按键任务、诊断和 v6 发送 |
| [Firmware/src/display.c](../../Firmware/src/display.c)、[ui.c](../../Firmware/src/ui.c)、[ui.h](../../Firmware/include/ui.h) | 两行切片、片外文字裁剪、LAP/GRAY 页面 |
| [Firmware/tools/monitor.py](../../Firmware/tools/monitor.py) | BLE/串口接收，兼容 v1～v6，拒绝错位布尔字段 |
| [Firmware/tests](../../Firmware/tests)、[CMakeLists.txt](../../Firmware/CMakeLists.txt) | 灰度、控制/模型、左右映射、UI 切片、接收与恢复测试 |
| [固件说明](../../Firmware/README.md)、[遥测字段](../../Documentation/reference/firmware-telemetry.md) | 构建、按钮和协议入口 |
| [本轮记录](../../Documentation/validation/2026-11-18-lap-fast.md)、[原始记录](../../Firmware/validation/2026-11-18/lap-fast) | 当前镜像、居中模型、无线上传与实车证据 |
| [摆动排查](../../Documentation/validation/2026-11-17-lap-damping.md)、[前版起停及漏停](../../Documentation/validation/2026-11-17-lap-marker.md) | 调参依据、LCD 调度问题、单次成功后重跑失败 |
| [验证总索引](../../Documentation/validation/README.md) | 灰度校准、方向修正、旧版本和硬件记录 |

## 决策与注意事项

- **实车方向：** J6 为左轮、J4 为右轮，`WHEEL_SWAP_SIDES=1` 同时映射驱动、编码器和故障位。MAP A 前进为 J6 正/J4 负，两路前进计数均为正；原理图组名和旧 ALL 同向电气输出不能用来推断实车前进。
- **灰度方向：** 探头在轮轴前方，黑 0/白 1；`GRAY_REVERSE_CHANNELS=1` 后 PE14 为物理最左 CH1，PE7 为最右 CH8。位置左负右正，1000 单位/探头间距。MAP 只改轮方向，不改灰度左右。
- 控制每 10 ms 更新。位置滤波 15 ms，变化率滤波 35 ms。中心附近轻调方向，线超出中间两路后增加位置增益。高速档加速上限 4300 CPS/s，其他档 3200，各档减速斜率 8000。外轮目标受限时优先保留差速。[实现与参数](../../Firmware/README.md)
- **A 识别：** LEAVE A 最高共同目标 800 CPS，窄线稳定 100 ms 后退出，最多 1.5 s 或 1000 计数。离开起点且运行 ≥2 s、行程 ≥30000 后允许四路标记保持 10 ms 停车；39000 起共同目标限至 1100；41000～47000 内三路、至多两段保持 30 ms 也可 DONE。60000 仍无标记为 NO A MARK；里程本身不产生 DONE。窗口内类似黑色区域有误判可能，需现场确认位置。
- **操作：** 按钮在上、屏幕在下。上电 LAP 停止，左到右 RUN / MAP / SPD / TASK；其他页按页面标签操作。LAP 不启动步进；ALL 保留两轮 45%/步进 30 RPM。运行任意键或 BLE `!` 停止，保护解除后不自动重启。J10 只控制驱动使能，拔下仍能手转读编码器，软件不能检测跳帽。
- 运行中用 `monitor.py` 被动接收遥测。`--lap` 会先停止并切页，`C5:CHECK` 会主动转动车轮。LAP 单独驱动两轮。
- C/LL 日常使用 HC-08，应用地址为 `0x08010000`。上传前关闭占用 BLE 的监测程序，重启后从 LAP 停止态开始。[无线升级](../../Documentation/how-to/wireless-update.md)
- **仓库：** `.scratch`、工具链、构建产物与本机 IDE 状态不提交；选定的 `Firmware/validation` 文本/CSV 是需保留的实测证据。旧项目 `D:/dev/qrs/stm32/Coral-C5` 只读参考。Transfer 是电源输入极性修正转接，PCB 同步与实物极性仍待核验。

## 构建与验证

从仓库根目录执行。要求 CMake ≥3.22、Ninja、Python ≥3.10、主机 GCC 和 Arm GNU；本轮使用 CMake 4.3.1、Python 3.12.14、主机 GCC 15.2.0、ARM GCC 14.3.1。两个 GCC 入口均须能在当前进程 PATH 中找到，主机 `gcc` 不可误指 ARM 编译器。

```powershell
cmake -S Firmware -B Firmware/build/host -G Ninja -DHOST_TESTS=ON -DCMAKE_C_COMPILER=gcc
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
python -m unittest discover -s Firmware/tests -p 'test_*.py'
cmake --preset debug -S Firmware
cmake --build Firmware/build/debug
cmake --preset release -S Firmware
cmake --build Firmware/build/release
```

Python 无线工具依赖见 `Firmware/tools/requirements.txt`；本机已有 `.scratch/firmware-tools/Scripts/python.exe`，克隆后按[无线升级步骤](../../Documentation/how-to/wireless-update.md)安装。需要部署新 C/LL 修改时执行 `./Firmware/flash-wireless.ps1`，确认返回镜像长度和 CRC；此命令不属于离线验证，也不是 Rust 烧录入口。
Rust 构建命令与隔离环境统一从 [Rust 开发说明](../../Documentation/how-to/rust-development.md)进入，不对整个 workspace 强设 ARM target。旧交接可从 Git 历史查阅。
