# 2026-11-17 首圈速度与摆动调整

基线为已无线安装的前进方向版：24,204 B、CRC `26023F22`。代码与旧 OTA 备份在本机 `.scratch/lap-tuning-baseline-20261117/`。
源码仍为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加本轮工作区；源码和构建身份见[source-manifest.json](../../Firmware/validation/2026-11-17/lap-tuning/source-manifest.json)。

## 现场反馈与边界

用户确认：车辆已沿赛道跑起来，但速度太慢，行驶时左右摆动较大，在 C、D 之间停止，没有完成一圈。
停车后未读取屏幕提示即断电；电脑未取得运行阶段遥测，所以本轮不能把停车原因归为某项保护。前次[首圈记录](2026-11-17-lap.md)保留此失败。

## 修改

| 项目 | 原值 | 新值与目的 |
| --- | --- | --- |
| 上电默认轮速目标 | 800 CPS | 1800 CPS，目标提高 2.25 倍，保留 800/1300 档 |
| 灰度位置输入 | 直接使用二值通道坐标 | 40 ms 一阶低通，再对平滑位置计算微分，减少相邻探头跳变 |
| 灰度 PD | P=320、D=80 | P=220、D=140；减少比例转向，并使微分作用于平滑后的变化 |
| 差速修正变化 | 直接采用当次 PD 输出 | 限制为 2400 CPS/s，避免左右转向突变 |
| 保护 | 原有丢线/编码器/迟到/90 s 等 | 保留，未为消除中途停车而关闭或放宽 |

CPS 尚未换算为 m/s；目标提高不等同于实际车速必定提高同一倍数。
如果该次停止与灰度丢线或 90 s 期限有关，改善速度和转向可能有帮助；具体原因仍需保留供电并读出下一次停止状态验证。

## 构建与测试

沿用前次工具版本，在项目根目录执行：

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
```

- [C 测试 5/5 通过](../../Firmware/validation/2026-11-17/lap-tuning/ctest.txt)。新增二值边缘跳变平滑、差速修正变化上限、持续大偏差转向能力及默认 1800 CPS 检查。
- [Debug 构建通过](../../Firmware/validation/2026-11-17/lap-tuning/build-debug.txt)：24,244 B，CRC `05E37A81`。
- [Release 构建通过](../../Firmware/validation/2026-11-17/lap-tuning/build-release.txt)：21,040 B，CRC `B4A73379`。
- 原一圈状态机、丢线/堵转/反向/超时、时间回绕及切片 UI 检查继续通过。

## 烧录与复测状态

首次更新时用户确认板子尚未重新上电；[首次日志](../../Firmware/validation/2026-11-17/lap-tuning/wireless-flash.txt)记录三次未找到 HC-08，上传失败。该次失败记录保留。
用户随后确认已上电，重新执行 `./Firmware/flash-wireless.ps1`，[真实上传成功](../../Firmware/validation/2026-11-17/lap-tuning/wireless-flash-powered.txt)：从 CRC `26023F22` 更新到 24,244 B、CRC `05E37A81`，耗时 92.7 s、12 次重试，暂存校验、安装与运行身份确认通过。
上传完成后重新连接[查询](../../Firmware/validation/2026-11-17/lap-tuning/after-info.txt)，返回 `C5APP,1,05E37A81,24244`。当前板上为调参版，默认 1800 CPS，升级后保持停止。
新参数是否解决摆动、能否完整一圈，以及一圈时间和停车误差，尚需实车复测。操作见[循线一圈](../how-to/line-following-lap.md)。

后续用户反馈偏离更明显，确认实际接线 J6 为左轮、J4 为右轮，与此前固件侧序相反。已统一修正驱动和反馈映射，当前运行镜像与复测状态以[映射修正记录](2026-11-17-lap-side-map.md)为准。
