# 2026-11-17 循迹调参与 A 点起停交付检查

本次整理 C/LL 灰度循迹、转向抑制、速度调整、A 点起步及重复 RUN 停车修复；同步操作、遥测 v6、项目索引和[交接](../../.agent/main/handoff.md)。不改变最后实测的固件行为，不新增烧录或电机动作。

## 范围与版本

基线为 Git `5c09ee0d09d186b52a6b1959b378c783622240dd`。本轮源码、测试、工具和选定记录以包含本文的提交为准；实测时尚未提交，因此历史 manifest 的基线加工作区说明保留原样。

- 收录 `Firmware` 的灰度/LAP、板层左右映射、调度、显示切片、UI、遥测接收及测试改动。
- 保留 11 月 17 日各次验证记录，包括真实丢线、蓝牙连接失败、前版重复漏停，以及最新连续两次回 A 成功的 CSV、摘要和镜像清单。
- 当前入口统一为 LAP、默认 2200 CPS、档位 800/1800/2200；删除交接中相互矛盾的多组“当前版本”，旧状态从日期记录和 Git 历史查阅。
- Rust 与厂家参考代码不变。`Hardware/stm32c5a3/Hardware/CoralC5-Motor/05_Right_Motor.kicad_sch` 的本地手工改动未暂存，未包含在本次提交，文件保持原样。
- 不提交 `.scratch`、工具链、IDE 状态或生成的固件二进制。归档文本日志中一处 Ninja 输出行尾空格已去除，失败信息未改；原/新摘要见[源码与归档核对](../../Firmware/validation/2026-11-17/lap-handoff/source-check.json)。

## 独立源码验证

先暂存授权范围，使用 `git ls-files -z -- Firmware` 和 `git checkout-index --prefix=<空目录>/ -z --stdin` 导出 **816 个暂存文件**到 `.scratch/lap-delivery-20261117/`。未复制本机 build 目录；离线构建由此副本完成，最后再加入本次检查日志。

对照[最后实测清单](../../Firmware/validation/2026-11-17/lap-repeat/source-manifest.json)，36 个源码、测试及工具文件的工作区 SHA-256 全部一致；导出文件统一 LF/CRLF 后内容也全部一致，见 [source-check.json](../../Firmware/validation/2026-11-17/lap-handoff/source-check.json)。

工具：CMake 4.3.1、Ninja 1.13.2、ARM GCC 14.3.1、主机 GCC 15.2.0、Python 3.12.14。本机主机 GCC 不在默认 PATH，验证进程使用 CLion MinGW 的绝对路径并仅在子进程 PATH 中补齐其 DLL 目录，未修改全局环境。

下列命令中的 `<导出目录>` 为上述干净副本，`<主机gcc>` 与 `<python>` 指向本机已安装工具。实际参数、退出码保存在 [host](../../Firmware/validation/2026-11-17/lap-handoff/commands-host.json)、[debug](../../Firmware/validation/2026-11-17/lap-handoff/commands-debug.json)、[release](../../Firmware/validation/2026-11-17/lap-handoff/commands-release.json)。

```powershell
cmake -S <导出目录>/Firmware -B <导出目录>/Firmware/build/host -G Ninja -DHOST_TESTS=ON -DCMAKE_C_COMPILER=<主机gcc>
cmake --build <导出目录>/Firmware/build/host
ctest --test-dir <导出目录>/Firmware/build/host --output-on-failure
<python> -m unittest discover -s <导出目录>/Firmware/tests -p 'test_*.py'
cmake --preset debug -S <导出目录>/Firmware -DPython3_EXECUTABLE=<python>
cmake --build <导出目录>/Firmware/build/debug
cmake --preset release -S <导出目录>/Firmware -DPython3_EXECUTABLE=<python>
cmake --build <导出目录>/Firmware/build/release
```

| 检查 | 结果与证据 |
| --- | --- |
| CTest | [6/6 通过](../../Firmware/validation/2026-11-17/lap-handoff/ctest.txt)：任务逻辑、灰度、LAP 控制、36 组控制模型、UI 边界/切片、OTA 恢复 |
| Python | [16/16 通过](../../Firmware/validation/2026-11-17/lap-handoff/python-tests.txt)：接收/协议/更新工具；测试内模拟中断和重试输出不代表连接了实车 |
| ARM Debug | [构建通过](../../Firmware/validation/2026-11-17/lap-handoff/build-debug.txt)，应用 25,132 B、CRC `29D123B8` |
| ARM Release | [构建通过](../../Firmware/validation/2026-11-17/lap-handoff/build-release.txt)，应用 21,724 B、CRC `04A4FE7E` |
| 产物身份 | [Debug](../../Firmware/validation/2026-11-17/lap-handoff/artifacts-debug.json)、[Release](../../Firmware/validation/2026-11-17/lap-handoff/artifacts-release.json) 的 BIN/OTA 长度、CRC 与 SHA-256 均逐项匹配最后实测清单，二进制内容一致 |
| 提交范围、路径与格式 | [暂存检查](../../Firmware/validation/2026-11-17/lap-handoff/staged-check.json)记录 Markdown 本地链接、格式、疑似密钥与构建产物检查；Git 不包含硬件或 Rust 改动 |

## 实测结论与限制

最新两次 RUN 为 24.333 s / 43059 计数、24.262 s / 42898 计数，均 DONE 并由用户确认回 A 停车；完整证据见[重复起停实测](2026-11-17-lap-repeat.md)。本次离线整理没有再次读取板上身份，不能写成新的实车试验。

A 部分覆盖窗口基于当前车辆/赛道约 43000 计数一圈；停车位置误差、不同电量与更大样本重复性仍待量测。无线记录存在拒绝帧和缺口，模型结果不能替代实物精度。Rust、视觉、钢球稳控和最新硬件 ERC/DRC 不在本次验证范围。

交接包含目标、当前状态、后续步骤、关键文件、注意事项、构建验证六部分。Windows 本机无 `pbcopy`，未复制剪贴板，仓库内交接文件为正式入口。
