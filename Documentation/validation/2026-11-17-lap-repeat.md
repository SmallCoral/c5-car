# 2026-11-17 重复 RUN 与部分覆盖 A 标记

前版 `75753E4B` 一次 22.951 s 回 A 成功，随后重跑却漏标记。保留完整[成功与失败](2026-11-17-lap-marker.md)；不能凭一次成功宣称重复可靠。

## 定位与修改

失败后的板上记录为 `NO A MARK`，31.542 s、60008 计数，已允许终点检测，但候选次数为 0、最大黑色覆盖只有 3 路。四路门限未满足，直到行程保护停车；双轮无驱动故障。实际标记覆盖会随循线位置变化，单一四路门限不够。

本次保留默认 2200 CPS、前进/转向分开控制及编码器抑制，增加：

| 阶段 | 行为 |
| --- | --- |
| 起点边缘 | `0x28` 等含中部探头的两路分段也允许进入受限 LEAVE A；无中部线索的双外侧点 `0x81` 和全白仍拒绝 |
| 平均行程 ≥39000 | 显示 A NEAR，共同速度目标最高 1100 CPS，保留原减速斜率；增加经过 A 标记的采样时间 |
| 41000～47000 计数 | 除原四路标记外，三路及以上、至多两段，持续 30 ms 可识别为部分覆盖的 A 并 DONE |
| 该区间之外 | 三路普通线不作为终点；四路候选仍受原离开起点、30000 计数及时间条件限制 |
| 60000 计数仍没标记 | NO A MARK 保护停车；里程本身不会产生 DONE |

窗口依据第一轮实测 43208 计数及此前约 42708 计数的候选重复间隔，只适用于当前从 A 点出发的约 6.142 m 赛道。它不是绝对定位，换起点、场地或轮胎后需重测；允许三路标记也可能把窗口内其他相似黑色区域误认，必须现场核对停车位置。

## 验证与版本

旧版代码/工具/镜像保存在 `.scratch/lap-repeat-baseline-20261117/`。源码仍为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加工作区；[源码与镜像摘要](../../Firmware/validation/2026-11-17/lap-repeat/source-manifest.json)记录实际散列。未改 Rust、原理图或 PCB。
工具沿用 CMake 4.3.1、ARM GCC 14.3.1、主机 GCC 15.2.0、Python 3.12.14。

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
```

- [C 测试 6/6 通过](../../Firmware/validation/2026-11-17/lap-repeat/ctest.txt)，含接近 A 的降速、窗口外三路不停车、窗口内单次跳变不停车、连续三路停车、重新 RUN 状态复位及原有保护测试。36 组控制模型及 UI 切片测试继续通过。
- Python 接收器及其 16 项测试本次未再改动，结果沿用[上一步](../../Firmware/validation/2026-11-17/lap-marker/python-tests.txt)。固件协议保持 v6。
- [Debug](../../Firmware/validation/2026-11-17/lap-repeat/build-debug.txt)：25,132 B、CRC `29D123B8`；[Release](../../Firmware/validation/2026-11-17/lap-repeat/build-release.txt)：21,724 B、CRC `04A4FE7E`。

[无线更新成功](../../Firmware/validation/2026-11-17/lap-repeat/wireless-flash.txt)，108.0 s、15 次重试；[重连查询](../../Firmware/validation/2026-11-17/lap-repeat/after-info-perf.txt)为 `C5APP,1,29D123B8,25132`，`C5PERF,1,6,4,3`。升级后保持停止。

用户从 A 启动，自动停稳后原地再次 RUN，明确确认“两次都在 A 自动停车”。[连续记录](../../Firmware/validation/2026-11-17/lap-repeat/track-repeat-1.csv)和[统计](../../Firmware/validation/2026-11-17/lap-repeat/track-repeat-1-metrics.json)如下：

| 轮次 | 运行时间 | 平均行程 | 允许停车后最大覆盖 | 结果 |
| --- | --- | --- | --- | --- |
| 第一次 RUN | 24.333 s | 43059 计数 | 3 路 | DONE，左右 PWM/目标/使能均为 0 |
| 停稳后再次 RUN | 24.262 s | 42898 计数 | 3 路 | DONE，左右 PWM/目标/使能均为 0 |

两轮均记录到受限驶离，稳定运行采样的控制间隔均为 10 ms，接近区共同目标不超过 1100 CPS。两次均由三路部分覆盖条件完成，验证了本次新增分支及再次启动后的状态复位。
共接收 632 条有效帧，单次连接，解析器拒绝 8 行、序号缺口 8；UART 错误、接收/发送队列丢弃及轮故障计数均为零。无线接收并非无损，最终状态由停车后的持续状态及现场反馈共同确认。
连续测试后补充查询时[未搜索到 HC-08](../../Firmware/validation/2026-11-17/lap-repeat/after-two-runs-query-failed.txt)，未取得新的身份回复；运行身份以更新后的成功重连查询为准，本轮源码散列最终核对未变化。

本轮结论限于当前车辆/赛道连续两次通过，未量测停车位置误差，不能据此宣称达到 ≤3 cm。此前 NO A MARK 停在其他位置时，先把车放回 A，不能直接把保护停车处当作起点。

操作见[循线一圈](../how-to/line-following-lap.md)。
