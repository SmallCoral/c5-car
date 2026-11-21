# Coral-C5 校赛 A 题交接

更新于 2026-11-21，分支 `main`。下一轮优先验证现场新调的相机能否让第一题从任意位置可靠完成，并在左 5 cm 持续保持。比赛目标为 8 s，达标后继续闭环保持，超时仍记录实际完成时刻。[题目与评分口径](../../Documentation/explanation/a-system-outline.md)

## 主控和相机的版本

| 对象 | 最近一次核实的状态 |
| --- | --- |
| 实车 STM32 C/LL | 11 月 20 日 22:23，HC-08 重连回读 `C5APP,1,8A10EDEE,60804`。当时首页选中第一题，运行位与车轮输出均为 0。恢复后仅做了读数检查。[烧录回读](../../Firmware/validation/2026-11-20/task1-restore-flash.txt) |
| 当前源码 | 11 月 21 日从干净目录构建 Debug，输出与 `D97DBD0E` 比赛版存档逐字节相同，含第二题按里程停车。两份 OTA 已归档，调试板上第一题使用 `8A10EDEE` 对应的 ELF。[固件与符号](../../Firmware/releases/README.md) |
| MaixCAM2 | 用户已在现场重新调过识别。仓库 `main.py`、旧版安装包和现场最后一次运行的脚本要分别核对。最后一次主控抽查的相机帧计数 274 → 292 → 310，三次 `flags=0x00`。当时是否有球入镜及设备上最终脚本的哈希仍待确认。[源码与对照记录](../../Firmware/26h/README.md) |
| 第二题 | `D97DBD0E` 从 A 朝顺时针按 SW2 起跑，纵向黑线循线，双轮平均里程达到 41800 计数停车。现场已跑到 13.056 s、41808 计数，A 点停止偏差需实测是否 ≤3 cm。[轨迹与末段速度](../../Firmware/validation/2026-11-20/task2-distance.md) |

## 下一轮怎样测第一题

1. 先读取 `C5:INFO`、`C5:MENU?`、`C5:BALL?`。将球放在实物 O 附近，观察序号更新、`flags=0x03` 的新鲜直接测量以及原始 x。再看右、左 5 cm 标线。记录设备实际运行的相机脚本与模型版本。[蓝牙命令与字段](../../Documentation/reference/firmware-telemetry.md)
2. 由现场操作者在首页按 SW2 进入第一题，等 `LEVEL OK`，松键后再按 SW2 并让球自由滚动。进入准备页会回标定水平。计时从确认启动开始，包含启动后的水平确认。
3. 记录起点、O 停够 0.30 s、右 5 cm 停够 0.40 s、左 5 cm 停够 0.40 s 的时刻，以及之后约 10 s 的左点漂移。出现超时或丢球时保留该轮轨迹，记录相机 flags、age、x、实际球位和步进反馈。从左右挡杆分别起跑可检验任意起点。
4. 旧相机配同一主控的三轮成绩为 5.441、7.512、500.518 s。第三轮长时间等不到可信球位，500.518 s 是主控最后确认停稳的时刻。复测新识别时沿用相同计时口径。[三轮原始数据](../../Firmware/validation/2026-11-20/task1-recovery.md)

## 标定与现场约定

| 实物位置，从左到右 | 左挡杆 | O 左 5 cm | O | O 右 5 cm | 右挡杆 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 640 宽图像的原始 x | 531 | 400 | 282 | 160 | 49 |

钢球向实物右侧移动时 x 减小。第一题取绝对 x 换算位置。水平绝对编码器参考为 10260，右端最高时电机轴相对水平为 −70.20°，最低为 +106.53°。这些数值描述电机轴角。显示 `SHAFT LIMIT` 时核对实物位置和 `C5:LIMIT?` 的回读。[标定原始记录](../../Firmware/validation/2026-11-19/camera640-and-shaft.json)

日常主控更新用 HC-08。烧录会释放正在托住水管的步进，独立托稳后才能使用 `--pipe-supported`。J-Link 有线更新使用项目脚本的 `-NoGui 1`。现场按键启动，远程启动按当轮操作者授权执行。J6 接左轮，J4 接右轮，灰度按实物 CH1～CH8 从左向右。[更新说明](../../Documentation/how-to/wireless-update.md) · [实车接线约定](../../AGENTS.md)

## 代码与验证入口

需要修改板上第一题时，使用标签 `task1-8A10EDEE` 建独立工作目录。需要第二题里程停车时，使用 `task2-D97DBD0E`。两份标签各自对应一个可恢复的实车版本。[切换与恢复](../../Firmware/releases/README.md)

- `Firmware/src/contest.c` 和 `Firmware/include/contest.h` 定义第一题路线、停留和漏检恢复。`Firmware/src/main.c` 负责菜单、回水平和执行器。[第一题实现](../../Firmware/src/contest.c) · [主程序](../../Firmware/src/main.c)
- `Firmware/src/ball.c` 筛选 V2 实测，`Firmware/src/stepper.c` 处理驱动保护与限位。`Firmware/src/lap.c` 实现巡线与里程停车。[相机接收](../../Firmware/src/ball.c) · [步进](../../Firmware/src/stepper.c) · [巡线](../../Firmware/src/lap.c)
- 11 月 21 日主机 CTest 9/9、Python 19/19 通过。Motor ERC 无报项，资源检查 957 项中 4 项失败，PCB DRC 保留 17 条错误。Rust 是独立的离线移植。第三、四题与发挥项仍待整车验证。[本轮核验与命令](../../Documentation/validation/2026-11-21-handoff.md) · [历次现场交接](archive/2026-11-20-field-handoff.md)
