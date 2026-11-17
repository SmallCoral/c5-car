# 2026-11-17 灰度采集与无线更新

源码基线：`5c09ee0d09d186b52a6b1959b378c783622240dd` 加本轮工作区改动。
完整源码 SHA-256 和镜像身份保存在 [source-manifest.json](../../Firmware/validation/2026-11-17/grayscale/source-manifest.json)。
修改前代码与旧 OTA 包保存在本机 `.scratch/grayscale-baseline-20261117/`。未修改硬件工程、Option Bytes 或独立 Rust 工程。

## 实现与工具

C/LL 应用新增灰度模块、只读 GRAY 页、黑白目标切换、每路三次一致滤波、单段通道坐标和无目标/全覆盖/多段识别。
主循环最短采样间隔 2 ms；调度可能增加间隔，本轮没有测量实际采样周期或滤波延迟。
遥测升级为 v3，Python 接收工具兼容 v1/v2。Rust 保持原 v2，未迁移本轮功能、未烧录。

工具版本：CMake 4.3.1、Ninja 1.13.2、Arm GCC 14.3.1、MinGW GCC 15.2.0、Python 3.12.14、Bleak 3.1.0。

## 离线验证

命令均从项目根目录执行：

```powershell
cmake -S Firmware -B Firmware/build/host -G Ninja -DHOST_TESTS=ON '-DCMAKE_C_COMPILER=C:/Program Files/JetBrains/CLion 2026.2.4.1/bin/mingw/bin/gcc.exe'
cmake --build Firmware/build/host
ctest --test-dir Firmware/build/host --output-on-failure
.scratch/firmware-tools/Scripts/python.exe -m unittest discover -s Firmware/tests -p 'test_*.py' -v
cmake --preset debug -S Firmware
cmake --build Firmware/build/debug
cmake --preset release -S Firmware
cmake --build Firmware/build/release
Firmware/build/host/render_ui.exe Firmware/build/host/gray.ppm gray
```

| 检查 | 结果与证据 |
| --- | --- |
| 主机 C 测试 | [4/4 通过](../../Firmware/validation/2026-11-17/grayscale/ctest.txt)：原任务/步进、灰度、画面边界、升级恢复 |
| 灰度算法 | 两种目标极性各遍历 256 种输入；检查逐路抖动隔离、短脉冲拒绝、初始化、时间回绕、无效偏差清零、GRAY 按键不启动电机 |
| Python 测试 | [14/14 通过](../../Firmware/validation/2026-11-17/grayscale/python-tests.txt)：v1/v2/v3、BLE 分片与错误长度、升级工具恢复 |
| Debug | [构建通过](../../Firmware/validation/2026-11-17/grayscale/build-debug.txt)，应用 20,324 B，CRC `B63CDAED` |
| Release | [构建通过](../../Firmware/validation/2026-11-17/grayscale/build-release.txt)，应用 17,808 B，CRC `EED9E897`，未烧录 |
| 灰度页面 | [主机渲染预览](../../Firmware/validation/2026-11-17/grayscale/gray-preview.png)已目视检查；不是实物屏幕照片 |

首次 C 测试有 1 项失败：[原始记录](../../Firmware/validation/2026-11-17/grayscale/ctest-initial.txt)。原任务循环断言仍期望 ALL→MONITOR；同步为新增页面后的 ALL→GRAY→MONITOR 后复测通过。没有关闭或删除原测试。

## HC-08 实际更新

执行：

```powershell
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
./Firmware/flash-wireless.ps1
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/wireless_flash.py --info
```

地址 `F8:2E:0C:F3:68:37`，USART1 保持 57600 8N1；此次仅上传应用。
更新前[查询](../../Firmware/validation/2026-11-17/grayscale/before-info.txt)为 18,976 B、CRC `4084B512`。
[真实上传](../../Firmware/validation/2026-11-17/grayscale/wireless-flash.txt)耗时 68.6 秒，数据包重试 7 次，暂存校验与安装成功，工具确认运行 20,324 B、CRC `B63CDAED`。
断开后再次连接[查询](../../Firmware/validation/2026-11-17/grayscale/after-info.txt)，返回 `C5APP,1,B63CDAED,20324`，与构建镜像一致。
本轮未改升级协议和 Flash 操作；没有重复此前的断电/中断恢复实验。

## 板上观察

执行：

```powershell
.scratch/firmware-tools/Scripts/python.exe Firmware/tools/monitor.py --ble F8:2E:0C:F3:68:37 --gray --seconds 15 --out Firmware/validation/2026-11-17/grayscale/ble-gray.csv
```

`--gray` 通过停止命令切入 GRAY，不启动轮电机或步进。
收到 [31 帧 v3 遥测](../../Firmware/validation/2026-11-17/grayscale/ble-gray.csv)，[汇总](../../Firmware/validation/2026-11-17/grayscale/ble-summary.json)确认无序号缺口、无无效帧。
全部为 `task=6`、`running=0`、轮 PWM/使能请求 0、步进目标 0；`lcd_ok=1`、`fault_mask=0`、`wheel_trip=0`，串口错误及 RX/TX 丢弃均为 0。

八路原始值均为 `FF`，滤波黑色掩码 0、目标数 0、`NO LINE`，偏差无效。用户确认当时朝向白底，结果与厂家定义白 1 一致。
步进当时 `step_online=0`、`step_error=E0`（应答超时）；本轮未启动或复测步进，不能把灰度采集通过记为整车通过。

随后使用同一命令记录 50 秒，将 `--seconds` 改为 `50`、`--out` 改为 `Firmware/validation/2026-11-17/grayscale/ble-sweep.csv`。
收到 [100 帧](../../Firmware/validation/2026-11-17/grayscale/ble-sweep.csv)，无无效帧或序号缺口；出现 6 种原始掩码，88 帧 NO LINE、11 帧 LINE、1 帧 MULTI，有效通道偏差为 -3500、+500、+1000、+2000。
[汇总](../../Firmware/validation/2026-11-17/grayscale/ble-sweep-summary.json)逐帧检查滤波掩码对应的目标数、段数、状态和偏差，全部符合合同，所有帧均保持停止。
这证明输入状态和板端处理输出会变化；用户表示暂时不方便执行规定的黑纸操作，采集时的具体遮挡条件没有确认，因此不将此记录当作黑白校准或八路逐一验收通过。

## 待验证

- 用户当前不方便执行黑纸逐路扫描，受控黑白切换、八路逐一对应与屏幕实物观察尚未完成。
- 辅助板是否完成阈值校准、实际左右顺序、最终安装高度与光照下的稳定性仍需确认，按[灰度联调步骤](../how-to/grayscale-check.md)继续。
- 该模块只提供观察与通道坐标，尚未接入轮速闭环、差速循线或交叉路口决策。
- 原理图 ERC、PCB DRC、真实采样时序、电气波形、本轮电机运动和 Rust 实物均未测试。

本轮使用 `kernel-guided-coding` 开发固件、`docs update` 同步操作与协议说明；来源和读取方式见[技能记录](../reference/skills.md)。
