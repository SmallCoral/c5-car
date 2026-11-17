# 2026-11-17 A 点起停标记与速度修正

接续[摆动抑制](2026-11-17-lap-damping.md)。用户确认摆动明显减轻，但仍有三项问题：速度偏慢、回到 A 不停车、从 A 判断线上起步时可能显示灰度异常而拒绝运动。

## 实物输入与原因

在用户手动放置并缓慢推过 A 线时，[被动记录](../../Firmware/validation/2026-11-17/lap-damping/a-marker-pattern.csv)采到黑色掩码 `0xFF` 持续约 3 s，入口有 `0x3D` 分段覆盖，离开过程有 `0x3C` 四路、`0x38` 三路，最终回到 `0x18`/`0x10` 普通线。前次起步失败时也记录到 `0x0D`（三路、两段）。
因此 A 线既会全覆盖，也有分段/部分覆盖的边缘。旧启动入口只接受单段或至少六路连续覆盖，会拒绝上述分段状态。
第一次普通监测连接在 1 帧后断开，原始[失败日志](../../Firmware/validation/2026-11-17/lap-damping/a-marker-observe.txt)保留；重连被动观察完成上述采样，未遥控启动。

旧版实测 `C5PERF,1,22,20,3`：LCD 单次工作可达 20 ms。灰度虽然配置 2 ms 采样，实际依赖主循环，三次连续滤波可能跨越约 34 ms；再叠加标记保持时间，存在快速经过横线时漏检的风险。旧 10 Hz 遥测没有逐控制周期记录，不能证明漏检只由显示阻塞造成。

## 修改

- **起步驶离：** 允许已稳定的 A 宽线或“中央线加侧标记”的三路/两段输入起步。显示 `LEAVE A`，不把横线变化的重心当成转弯，使用编码器差速抑制保持直行；共同目标最高 800 CPS。窄线稳定 100 ms 后进入正常循线；最长 1.5 s 或 1000 计数，届时仍未离开则停车。
- **返回识别：** 离开起点后，至少 2 s 且平均行程达到 30000 计数才允许停车。此后四路及以上、至多两段的标记保持 10 ms 即 `DONE`。60000 计数仍未识别 A 则以 `NO A MARK` 保护停车，不自动跑下一圈。
- **行程边界：** 赛题路线约 6.142 m；上一轮两次候选 A 附近事件约为 42791/85499 计数，间隔约 42708。该对应关系是结合赛道与输入变化的推断，未独立标定轮径/每转计数，也不代表事件位置已经测量。30000～60000 是当前场地的宽窗口；换赛道必须重测。
- **速度：** 档位改为 800/1800/2200 CPS，默认 2200。每一探头间距偏差的减速由 300 改为 200 CPS；轮目标上限 2800 CPS、LAP PWM 上限 40%。原前进加减速、转向抑制、抗积分饱和和故障停车保留。目标增幅不等同于已测得的车速增幅。
- **显示调度：** 每次绘制/发送两行；整段文字不与当前切片相交时跳过字形遍历。保留画面和帧状态快照，为灰度与控制提供更短的主循环间隔。
- **诊断：** 遥测 v6，增加驶离阶段、停车允许后的标记候选次数及最大覆盖数。运行约 10 Hz，停止约 2 Hz；接收器兼容 v1～v6。

保留白场拒绝启动、运行丢线/异常线、编码器反向/停转、控制迟到、驱动故障、90 s 上限以及任意键/BLE 停止。允许 A 起步不代表允许在任意无黑线区域盲走。

## 测试与镜像

修改前的 `D900AC7A` 源码/测试/工具/OTA 备份于 `.scratch/lap-marker-baseline-20261117/`。当前源码仍为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加工作区，未提交；散列与工具版本见[source-manifest.json](../../Firmware/validation/2026-11-17/lap-marker/source-manifest.json)。
使用 CMake 4.3.1、ARM GCC 14.3.1、主机 GCC 15.2.0、Python 3.12.14，在仓库根目录执行：

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
.scratch/firmware-tools/Scripts/python.exe -m unittest discover -s Firmware/tests -p 'test_*.py'
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
```

- [C 测试 6/6 通过](../../Firmware/validation/2026-11-17/lap-marker/ctest.txt)。新增实际分段起点输入、驶离时间/行程上限、提前横线不停车、30 ms 部分覆盖标记停车、漏标记行程停止，以及全部页面两行切片与整帧像素一致性检查。
- [Python 16/16 通过](../../Firmware/validation/2026-11-17/lap-marker/python-tests.txt)，包括 v6 字段、旧版兼容，以及实测后补充的异常拼接帧拒绝测试。
- [36 组模型通过](../../Firmware/validation/2026-11-17/lap-marker/dynamics.txt)；模型假设和实车边界沿用[前一阶段说明](2026-11-17-lap-damping.md)，不作为赛道成绩。
- [Debug](../../Firmware/validation/2026-11-17/lap-marker/build-debug.txt)：24,944 B、CRC `75753E4B`；[Release](../../Firmware/validation/2026-11-17/lap-marker/build-release.txt)：21,588 B、CRC `24BD4BBF`。
- [无线更新成功](../../Firmware/validation/2026-11-17/lap-marker/wireless-flash.txt)，103.7 s、14 次重试；应用身份检查通过，电机保持停止。
- [重连身份与性能查询](../../Firmware/validation/2026-11-17/lap-marker/after-info-perf.txt)：`C5APP,1,75753E4B,24944`、`C5PERF,1,6,4,3`。该观测窗口中主循环/显示最大耗时为 6/4 ms，前一版分别为 22/20 ms。

## 赛道验证

用户确认“能起步，回 A 自动停车”。[第一轮](../../Firmware/validation/2026-11-17/lap-marker/track-marker-1.csv)记录到：从五路黑色 `0x3E` 起步，先进入 LEAVE A；运行 22.951 s、平均行程 43208 计数后 `DONE`，标记次数 1、最大覆盖 4；PWM、目标和使能均归零。稳定运行记录的控制间隔为 10 ms。
起步前曾出现两路分段 `0x28` 导致 ALIGN LINE，随后在上述五路标记上成功启动；不能表述为所有 A 边缘形态均已解决。

原 CSV 收到 541 条记录，传输期间解析器拒绝 4 行；事后发现另有一行列数正确但字段拼接错位，出现 `lap_armed=1313`、`lap_departing=97`。已保留[异常原行](../../Firmware/validation/2026-11-17/lap-marker/track-marker-1-excluded.json)，并在接收器增加布尔字段范围校验。剔除后有效 540 帧、运行 222 帧、驶离 6 帧，有效序号缺口合计 6；不把无线日志说成完整无损。见[统计](../../Firmware/validation/2026-11-17/lap-marker/track-marker-1-metrics.json)。该接收器修正不改变已烧录的固件，源码身份详见下一阶段快照。

**重复测试未通过：** 用户随后反馈再次 RUN 不正常。[重连读取](../../Firmware/validation/2026-11-17/lap-marker/restart-failure-retry.csv)确认车辆运行了 31.542 s、60008 计数，最终 `NO A MARK`；`lap_marker_hits=0`、`lap_marker_peak=3`，轮驱动无故障。这不是“RUN 没执行”，而是同一四路门限在下一轮漏识别了部分覆盖的 A。第一次读取时 BLE 短暂断连，原始[失败日志](../../Firmware/validation/2026-11-17/lap-marker/restart-failure.txt)保留。

因此 `75753E4B` 只验证了单次成功，不能作为重复可靠完成版。后续修正与连续两轮验证见[重复起停记录](2026-11-17-lap-repeat.md)。停车误差 ≤3 cm 仍须现场量测，不能由 `DONE` 状态证明。

操作见[循线一圈](../how-to/line-following-lap.md)，字段见[遥测 v6](../reference/firmware-telemetry.md)。
