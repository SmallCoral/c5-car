# 2026-11-17 实车左右映射修正

用户反馈 1800 CPS 调参版偏离更明显，进一步确认：灰度探头位于驱动轮轴前方，**J6 接实车左轮，J4 接实车右轮**。
此前固件按原理图组名将 J4 当左轮、J6 当右轮，灰度差速落到了相反的实车侧。该映射与实际接线不符，是本次明确找到的转向方向问题。

## 修改

保持硬件网络和六针线序不变，新增 `WHEEL_SWAP_SIDES=1`，在板层统一将实车左右转换到原理图通道。

| 上层含义 | 当前实车通道 |
| --- | --- |
| 左轮命令/PWM | J6，PH=PC11、TIM4 CH1 |
| 右轮命令/PWM | J4，PH=PC3、TIM4 CH4 |
| 左编码器 | J6，TIM2，A/B 输入 PB3/PA15 |
| 右编码器 | J4，TIM5，A/B 输入 PA0/PA1 |
| 左 nFAULT | J6，PC10 |
| 右 nFAULT | J4，PH4 |

MAP A 的实车前进仍是 J6 正/J4 负；原来已确认的前进输出没有反转，只修正左右角色。两路前进编码器归一化为正。
因此正灰度偏差（黑线在右）要求左轮更快时，实际增加 J6 输出；负偏差反之。轮速反馈、页面、故障提示与命令使用同一侧。
1800 CPS、40 ms 位置平滑及现有保护保留。Rust 仍保留旧迁移版，未同步本轮映射；未烧录 Rust。

## 离线验证与无线更新

工具版本沿用[首圈记录](2026-11-17-lap.md)。源码基线为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加工作区；修改前固件保存在 `.scratch/lap-side-map-baseline-20261117/`，当前摘要见[source-manifest.json](../../Firmware/validation/2026-11-17/lap-side-map/source-manifest.json)。

从项目根目录执行：

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
```

- [C 测试 5/5 通过](../../Firmware/validation/2026-11-17/lap-side-map/ctest.txt)，新增按实际线束验证单轮输出、向右差速、编码器侧、故障位与 A/B 位序的测试；原闭环/停车/保护继续通过。
- [Debug](../../Firmware/validation/2026-11-17/lap-side-map/build-debug.txt)：24,280 B，CRC `E9A3458B`；[Release](../../Firmware/validation/2026-11-17/lap-side-map/build-release.txt)：21,064 B，CRC `3124CC3D`。
- [HC-08 真实更新](../../Firmware/validation/2026-11-17/lap-side-map/wireless-flash.txt)成功，107.2 s、16 次重试，运行身份校验通过。
- 重新连接[查询](../../Firmware/validation/2026-11-17/lap-side-map/after-info.txt)，返回 `C5APP,1,E9A3458B,24280`。当前板上是映射修正版，上电停止。

## 现场验证范围

前一调参版试跑仍被用户反馈偏离。首个观察窗口记录起步被 `ALIGN LINE` 拒绝，当时原始 `0x43`、滤波为多段；后来中间四路 `0x3C` 命中、位置 0，单段输入已有效。
这说明 RUN 已被处理，拒绝起步与当时的灰度状态有关。无效起步不会自动在输入恢复后重新启动，需松开再按 RUN。
前两段观察窗口之间存在缺口，后续无效重试也会重置本次 LAP 状态，所以未将这些日志写成完整的运动/停车原因证据。

修正左右后的整圈、时间、停车误差及偏摆改善仍需实车确认。原理图、PCB 和接口 CSV 未改，未将构建或上传成功当作赛道验收通过。

后续真实试跑捕获 1849 ms 后丢线，用户进一步确认最左探头在主控屏幕点亮 8；灰度线束还存在反序。已增加输入位序修正并取得主控左 1/右 8 的静态确认，当前版本和复测见[灰度反序记录](2026-11-17-lap-gray-map.md)。
