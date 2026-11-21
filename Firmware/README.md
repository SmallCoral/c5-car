# STM32C5A3 比赛固件

实车使用 C/LL 合并应用。上电进入校赛首页，从左到右为 SW5 返回、SW4 上一项或向左、SW3 下一项或向右、SW2 进入或启动。每次确认后松开 SW2，再按才会触发下一步。第三题、第四题和发挥二还在菜单中预留。

## 第一题 · 停车移球

进入第一题，等屏幕显示 `LEVEL OK`。再按 SW2 开始计时，车轮保持静止，球依次移到 O、右 50 mm、左 50 mm。规定停留分别为 0.30、0.40、0.40 s。停留时球须在目标 ±10 mm 内，且同一停留段的位置跨度不超过 4 mm。8 s 后继续到左点，完成后维持闭环。预测帧不能单独确认到位。[停稳计算](src/contest.c) · [实车计时](../Documentation/validation/README.md)

## 第二题 · 循线停车

在 A 线对准车身参考点，车头朝顺时针。进入第二题，SW3 可换档，SW2 起跑。默认直道目标为 4600 编码器计数/秒，弯道上限 3100。双轮平均行程达到 41800 计数停车，最后 900 计数平滑降速。[场地操作与保护条件](../Documentation/how-to/line-following-lap.md)

## 发挥一 · 定点稳球

1. 在 A 点选中发挥一，按 SW2 进入选点页。SW4 向左、SW3 向右，短按 1 mm。按住 0.40 s 后每 50 ms 连调 1 mm。按当前挡杆标定可选 −106～+95 mm，赛题指定范围为 −50～+50 mm。
2. 再按 SW2 开始原地稳球。此时两个选点键仍有效，目标会随按键更新。长度条上绿色箭头表示目标，黄色实心点表示实测球位，空心点表示最近一次观测。[长度条预览](../Documentation/validation/data/ruler.png)
3. 球稳好、屏幕显示 `SW2 RUN` 后再按 SW2，锁定目标并起跑。巡航目标为 1400 计数/秒，最后 2500 计数降至 450 计数/秒，42000 计数停车。到结果页仍按原目标稳球。行驶中持续 0.50 s 缺少可信球位会停车。[轮速与球位实测](../Documentation/validation/README.md)

## 构建与回归

需要 CMake ≥3.22、Ninja、Arm GNU 工具链、主机 C 编译器和 Python ≥3.10。实车镜像用 Arm GNU 14.3.1 的 Debug 配置构建。以下命令从 `Firmware` 目录执行，Windows 可把 MinGW 的 `bin` 加入当前终端 PATH。

```powershell
cmake --preset debug
cmake --build build/debug --target competition_image
python -m unittest discover -s tests -p "test_*.py"
```

`competition_image` 生成 `build/debug/coral_c5.ota`，检查最终链接的比赛入口，并运行主机 CTest，含实车轨迹回放。构建 Release 使用 `cmake --preset release` 和 `cmake --build build/release --target firmware_package`。单独查看回归结果可运行 `ctest --test-dir build/host --output-on-failure`。

日常从仓库根目录运行 `./Firmware/flash-wireless.ps1`。步进正在托管水管时，先将水管独立托稳，再按操作说明进入蓝牙引导程序。应用区起始地址为 `0x08010000`。J-Link 更新用 `./Firmware/flash-jlink.ps1`。[升级步骤](../Documentation/how-to/wireless-update.md) · [镜像恢复与校验](releases/README.md)

## 读源码与调参

| 想看什么 | 从这里读 |
| --- | --- |
| 按键如何切题、选点和起跑 | [菜单状态机](src/contest.c) · [主循环与任务切换](src/main.c) |
| 原始像素如何变成球位，怎样判断停稳 | [相机收包与有效性](src/ball.c) · [坐标与稳球控制](src/contest.c) · [五点和轴角标定](calibration/camera640-and-shaft.json) |
| 灰度如何变成左右轮速，何时减速停车 | [灰度滤波](src/grayscale.c) · [循线与轮速闭环](src/lap.c) · [速度、方向与里程参数](include/config.h) |
| 步进怎样抱持、限位、处理应答 | [驱动状态机](src/stepper.c) · [Emm 报文](src/emm.c) |
| 接线、屏幕与故障排查 | [引脚与驱动](src/board.c) · [屏幕绘制](src/ui.c) · [实物联调](../Documentation/how-to/firmware-bringup.md) |
| 蓝牙收数与无线升级 | [遥测字段](../Documentation/reference/firmware-telemetry.md) · [电脑端工具](tools) · [引导程序](boot) |

首页同时按 SW4 和 SW3 进入设备调试。BALANCE 使用独立的回中控制，灰度调试页可逐路核对 CH1～CH8。[相机工程](26h/README.md) · [控制移植来源](../Documentation/explanation/classmate-balance-reference.md) · [厂商依赖与许可](vendor/README.md)
