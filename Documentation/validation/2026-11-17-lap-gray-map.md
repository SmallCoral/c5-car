# 2026-11-17 灰度反序与转向链路修正

电机侧序版 CRC `E9A3458B` 试跑后，用户反馈从 A 朝 B 起步即向左偏出。
本次[运行记录](../../Firmware/validation/2026-11-17/lap-side-map/track-side-map-1.csv)捕获 1849 ms 后 `LOST LINE` 停车：开始双轮目标相同，随后旧灰度坐标从 0 变为 −500、−1000，控制器提高右轮目标，最终丢线；`wheel_trip=0`，未报告驱动故障。

进一步保持车静止核对，用户明确确认：**实际最左探头点亮旧主控屏幕的 8**，同时车头确实沿 A→B 放置。由此确认八路线束相对 C5 输入也反序，原来的原理图通道顺序不能代表实车左右。

## 修正后的完整链路

`GRAY_REVERSE_CHANNELS=1` 将 PE7～PE14 的输入位序反转，保持白 1/黑 0。显示、滤波、坐标及循线统一使用反转后的物理顺序：最左为通道 1/PE14，最右为通道 8/PE7。

| 实际输入 | 排序后的含义 | 应有电机修正 |
| --- | --- | --- |
| 黑线在最左 PE14，电气输入 `0x7F` | 通道 1 黑，位置 −3500 | 右轮 J4 更快，向左靠线 |
| 黑线在最右 PE7，电气输入 `0xFE` | 通道 8 黑，位置 +3500 | 左轮 J6 更快，向右靠线 |
| 中间通道 4/5 同时黑 | 位置 0 | 双轮目标相同 |

前次电机侧序 `WHEEL_SWAP_SIDES=1` 保留。实车左 J6、右 J4，前进输出 J6 正/J4 负；输入、坐标、差速及实际输出现在统一按实车侧解释。
正确路线是 A→B 直线、B→C 半圆、C→D 直线、D→A 半圆；程序依据黑线和起点横线闭环控制，不在 A 预设左转。

## 构建与更新

源码基线为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd` 加工作区；修改前代码与旧 OTA 保存在 `.scratch/lap-gray-map-baseline-20261117/`。
源码与镜像散列见[source-manifest.json](../../Firmware/validation/2026-11-17/lap-gray-map/source-manifest.json)。工具版本沿用[首圈记录](2026-11-17-lap.md)。

从项目根目录执行：

```powershell
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
cmake --build Firmware/build/debug
cmake --build Firmware/build/release
./Firmware/flash-wireless.ps1
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
```

- [C 测试 5/5 通过](../../Firmware/validation/2026-11-17/lap-gray-map/ctest.txt)。新增实际 GPIO 模式→灰度排序→滤波坐标→PD/PI→J4/J6 输出的双向端到端测试，验证两端黑线均向正确方向修正。
- [Debug 构建](../../Firmware/validation/2026-11-17/lap-gray-map/build-debug.txt)：24,324 B，CRC `B1A813D4`；[Release](../../Firmware/validation/2026-11-17/lap-gray-map/build-release.txt)：21,100 B，CRC `4F1CA934`。
- [无线更新成功](../../Firmware/validation/2026-11-17/lap-gray-map/wireless-flash.txt)，101.1 s、14 次重试，安装后运行身份校验通过。
- 重新连接[查询](../../Firmware/validation/2026-11-17/lap-gray-map/after-info.txt)返回 `C5APP,1,B1A813D4,24324`。更新后仍保持停止。

## 实物验证

更新后先不启动电机，用户分别将黑线放到最左、最右探头下各停约 2 秒，确认主控彩屏已变为 **左侧亮 1、右侧亮 8**。这是本轮实际取得的主控端左右验证，已写入项目约定。
随后从 A 点朝 B 进行整圈复测；运动及停车结果仍需结合记录和现场反馈确认，不能由静态左右测试或编译通过替代。
1800 CPS、位置平滑及安全停车条件保留；没有修改原理图、PCB、接口 CSV 或独立 Rust 工程。
