# 2026-11-17 全程摆动与转向响应修正

用户确认灰度循线已经能运行，但直线、弯道均明显左右摆动。本轮只修改 C/LL 控制和 Python 遥测；实车灰度/电机左右映射保留，Rust 和硬件设计未改。

## 证据与判断

- 修改前重新查询到板上 `C5APP,1,B1A813D4,24324`，与上一版灰度映射修正一致。见[运行身份](../../Firmware/validation/2026-11-17/lap-damping/before-info.txt)。
- [上一轮原始记录](../../Firmware/validation/2026-11-17/lap-gray-map/track-gray-map-1.csv)中，位置在 −2000、+2000 等坐标反复换边。运行 2044 ms 时，位置已为 −1500，左/右目标 1592/1866 CPS，但反馈仍为 1976/1667 CPS；该时刻车辆轮速差仍朝上一方向。9084 ms 后以 `LOST LINE` 停车。
- [本轮修改前读取](../../Firmware/validation/2026-11-17/lap-damping/before-live.csv)时已停止，最近一次任务 2108 ms、2868 计数、`LOST LINE`；`fault_mask=0`、`wheel_trip=0`。这是另一轮运行留下的状态，不能与上述 9084 ms 混作同一圈。读取期间没有发送启动命令。
- 旧代码对灰度位置低通后，又先限转向变化、再分别限制两个轮目标。起步时两个轮目标容易同时撞到相同加速限制，暂时抹掉差速；回中时两层限速增加转向撤回延迟。
- 旧 D 项使用 `Δ位置 × 10 / Δ毫秒` 后乘 `140/1000`，相当于对每秒变化率乘 0.0014；P 为 0.22，等效微分时间仅约 6.4 ms。参数名字里的 D=140 并不表示有 140 ms 的提前抑制。

这些证据支持控制响应滞后是需要修正的因素；旧遥测只有约 2 Hz，无法据此量化摆动频率，也不能排除探头高度、机械间隙、轮胎打滑等影响。当前没有接入 IMU 角速度反馈。

## 修改后的控制

灰度提供相对线位置，编码器提供轮速和正在发生的差速。两者共同决定转向，未积分或宣称得到绝对航向。

| 项目 | 当前实现 |
| --- | --- |
| 灰度位置 | 20 ms 一阶低通；±200 坐标单位的连续软死区，1000 单位为一个探头间距 |
| P / D | P=200/1000；D 使用每秒位置变化率，系数 20/1000，40 ms 滤波并限制贡献至 ±500 CPS |
| 编码器转向抑制 | 从请求差速中减去 `0.25 × (左反馈−右反馈)/2`；测得的半差速单位仍为 CPS |
| 速度反馈 | 按实际控制间隔计算 30 ms 低通，避免调度抖动改变滤波时间常数 |
| 前进目标 | 共用缓升：加速 2000、减速 4000 CPS/s |
| 转向目标 | 独立限变为 6000 CPS/s；不再对两个最终轮目标重复缓升；共同限幅保留转向符号 |
| 直线与弯道 | 默认仍为 1800 CPS；灰度位置绝对值每增加 1000，期望共同速度降低 300 CPS，再执行缓升/减速 |
| 轮速 PI | 保留原前馈、PI、积分抗饱和和 350‰ 输出上限 |
| 失效处理 | 非窄线时取消 D；重新见线首周期不计算跨丢线间隔的 D；丢线、堵转、迟到、超时和按键停车继续有效 |
| 遥测 | v5，53 列；LAP 运行约 100 ms/帧，停止与其他页 500 ms/帧；新增位置、变化率、差速请求/反馈、共同目标和控制间隔 |

原理参考为 WPILib 官方[PID 的微分项](https://docs.wpilib.org/en/latest/docs/software/advanced-controls/introduction/introduction-to-pid.html)与[差速运动学](https://docs.wpilib.org/en/stable/docs/software/kinematics-and-odometry/differential-drive-kinematics.html)。本项目参数按未标定的编码器/探头坐标表达，并非移植 WPILib 或套用其调参值。

## 离线验证

源码基线是 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加现有循线工作区。修改前 36 份代码/测试/工具/镜像文件及原工作区 diff 保存在 `.scratch/lap-damping-baseline-20261117/`，未覆盖用户已有修改。当前源码、构建散列与工具版本见[source-manifest.json](../../Firmware/validation/2026-11-17/lap-damping/source-manifest.json)。

工具为 CMake 4.3.1、ARM GCC 14.3.1、主机 GCC 15.2.0、Python 3.12.14。从仓库根目录运行：

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
.scratch/firmware-tools/Scripts/python.exe -m unittest discover -s Firmware/tests -p 'test_*.py'
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
```

- [C 测试 6/6 通过](../../Firmware/validation/2026-11-17/lap-damping/ctest.txt)：原有方向、停车与恢复测试继续通过；增加起步差速、编码器抑制、回中撤回、丢线重捕获及目标/PWM 边界检查。
- [Python 15/15 通过](../../Firmware/validation/2026-11-17/lap-damping/python-tests.txt)：覆盖 v1～v5 兼容、分片解析、字段及原 OTA 工具。
- [Debug](../../Firmware/validation/2026-11-17/lap-damping/build-debug.txt)为 24,620 B、CRC `D900AC7A`；[Release](../../Firmware/validation/2026-11-17/lap-damping/build-release.txt)为 21,284 B、CRC `6CAC09B8`。编译告警视为错误。

`test_lap_dynamics.c` 使用实际 C 控制器、离散探头掩码及整数编码器，模拟 36 组直线/半径 500 mm 弯道、轮响应 50～150 ms、前视距离 80～160 mm、固定/抖动调度组合。假设轮距 210 mm、7 计数/mm、10 mm 探头间距和不等的两轮增益，均不是实车标定。
同一模型下，[旧版](../../Firmware/validation/2026-11-17/lap-damping/dynamics-before.txt)有 3 组丢线，新版[36 组均完成模型运行](../../Firmware/validation/2026-11-17/lap-damping/dynamics-after.txt)。仍运行案例的末 4 秒横摆峰峰值最大值由 8.806 mm 变为 5.155 mm；这些数字仅属模型，不能当作实车改善比例。
测试分别统计稳态偏置与摆动：二值中心掩码无法分辨模型内 ±5 mm 的位置，因此验证要求在一个探头区间内收敛，而不虚构亚探头分辨率。

开发中首次噪声跳变测试未通过，已减少 D 增益并增加 D 滤波；模型最初以距中心 RMS 3 mm 为标准，混淆了不可观测的量化偏置与摆动，改为分别记录偏置/横摆，并按一个探头区间检查。以上是测试设计修订，未放宽任何实车停车保护。

## 无线更新与实车范围

[无线上传](../../Firmware/validation/2026-11-17/lap-damping/wireless-flash.txt)成功，耗时 102.6 s，14 次重试；暂存校验、安装及运行身份检查通过。[断开重连查询](../../Firmware/validation/2026-11-17/lap-damping/after-info.txt)返回 `C5APP,1,D900AC7A,24620`。
更新后 LAP 为 READY，PWM 为零，未远程启动。用户按 RUN 复测后明确反馈“摆动明显减轻”。[实际记录](../../Firmware/validation/2026-11-17/lap-damping/track-damped-1.csv)共 663 帧，其中 570 帧为运行，持续 62.170 s、91150 计数后以 `STOPPED` 结束；PWM/使能均归零，未触发丢线/轮故障保护，但也未触发 `DONE`。
[统计](../../Firmware/validation/2026-11-17/lap-damping/track-damped-1-metrics.json)显示：稳定运行采样控制间隔 16～20 ms、中位数 17 ms；UART 错误、收发队列丢弃和轮故障计数为零。无线序号有 2 帧缺口，解析器拒绝 2 行；不能写成全链路零丢帧。
[停车后查询](../../Firmware/validation/2026-11-17/lap-damping/after-run-query.txt)仍为同一 CRC，`C5PERF,1,22,20,3` 表示主循环/显示/遥测最大耗时分别为 22/20/3 ms。

用户随后补充：速度偏慢，到 A 不停车，在 A 判断线上有时拒绝起步。本版的横线识别和速度尚不满足这些要求；后续处理见[起停标记与速度修正](2026-11-17-lap-marker.md)。摆动改善为用户目视反馈，未测量横摆幅度或停车误差。

复测操作见[循线一圈](../how-to/line-following-lap.md)，新增诊断字段见[遥测 v5](../reference/firmware-telemetry.md)。
