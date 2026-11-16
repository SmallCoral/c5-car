# 2026-11-16 项目整理与提交前检查

目标为整理当前项目并推送 `SmallCoral/c5-car` 的 `main`。整理起点为 `64c5210ef6b4a552aac22b4bfe8a288ffd95149f`；本次交付版本以 Git 提交记录为准。

## 整理范围

- 更新根目录、硬件、固件和文档入口，补齐无线升级及交接说明，增加[目录归属](../reference/repository-layout.md)。
- 纳入已有 LL 固件、蓝牙无线升级、主机测试与板上验证记录；移除已被替换的 MX2 工程，旧版本仍保留在 Git 历史。
- 原样纳入用户保存的 Motor PCB 和 Transfer 文件。Transfer 用于修正原电源输入正负极画反的问题；仓库快照尚未连线、布线或验证修正效果，详见[工程说明](../../Hardware/stm32c5a3/Hardware/Transfer/README.md)。
- KiCad 自动历史、本地 `.kicad_prl` 和两份 Python 字节码从索引移除，19 个文件仍留本机；IDE 配置、USB 诊断、Flash 备份和缓存不纳入本次提交。
- 修复旧 MX2 许可证路径，将原包的 Apache-2.0 许可证原样迁入 `Firmware/vendor/CMSIS`。当前依赖源码保留原版本，不在构建时下载。
- 使用项目约定的 `docs update`，不改原始厂商资料，不重写历史验证结果，不运行硬件生成器。

## 检查记录

使用 Git 2.45.1.windows.1、CMake 4.3.1、Ninja 1.13.2、Arm GCC 14.3.1、MinGW GCC 15.2.0 和 Python 3.12.14。具体计数、输出哈希与保留的失败项见[机器记录](2026-11-16-project-cleanup.json)。

| 检查 | 结果 |
| --- | --- |
| 硬件与机械保留 | 整理前基线中的 1339 个非 Markdown 文件逐个 SHA256 相同；含用户的 Motor PCB，未替换为历史版本 |
| 独立源码构建 | 从 Git 暂存区导出 `Firmware` 到新目录，仅额外指定已安装的 Python 和编译器；Debug、Release 应用及引导程序均成功构建 |
| 产物一致性 | Debug 应用 18,976 B、CRC `4084B512`；Release 应用 16,696 B、CRC `D7E57D4B`。两配置的 BIN、OTA 与首次安装 HEX 共 8 个文件均与此前工作目录产物逐字节一致 |
| 主机测试 | CTest 3/3，Python unittest 12/12 通过；[主机日志](2026-11-16-project-cleanup/build-host.txt) |
| 文档链接 | 对暂存文件检查 Markdown 本地文件/目录目标；当前项目文档无缺失目标，外部网站可用性与页内锚点不在本次范围 |
| 凭据特征检查 | 对暂存文本扫描常见私钥、GitHub/API token、AWS key 和含凭据 URL，未发现匹配；不能据此宣称已覆盖所有敏感信息类型 |
| 本机状态 | 19 个停止跟踪的本机文件仍在磁盘；待提交索引不含被 `.gitignore` 排除的文件 |
| Git 空白检查 | 自有源码与文档通过；保留原始设备日志的 970 项空白提示，详情见下文 |

构建输出：[Debug](2026-11-16-project-cleanup/build-debug.txt)、[Release](2026-11-16-project-cleanup/build-release.txt)。固件本轮只有说明与许可证整理，没有修改运行逻辑。日常烧录仍按[无线升级步骤](../how-to/wireless-update.md)执行。

## 保留的问题和验证边界

完整 `git diff --cached --check` 返回 2：48 份 `Firmware/validation` 原始工具输出共 970 项行尾空白或末尾空行提示。没有重写这些原始证据；排除该日志目录后的差异检查返回 0。

历史归档文档保留 171 个无法在当前相对目录解析的链接，厂商包保留 4 个缺失图片引用；机器记录列出具体文件和目标。它们属于旧基线快照、历史交接或随包说明，未修改原文，也不属于当前操作入口。检查脚本首次运行遇到 Windows 默认 GBK 读取 UTF-8 基线失败，显式指定 UTF-8 后完成核查。

本轮不烧录或启动电机，不改电路、PCB 或机械模型。11 月 10 日 Motor DRC 是历史快照，不能代表用户此后保存的 PCB；本轮不重跑 ERC/DRC，也不把实物联调结果当作设计发布验证。

## 主要命令

`git checkout-index` 仅将暂存的 `Firmware` 文件导出到全新目录；以下命令在该目录执行，Python 与主机 GCC 指向本机已安装版本：

```powershell
cmake --preset debug -DPython3_EXECUTABLE=<python.exe>
cmake --build --preset debug
cmake --preset release -DPython3_EXECUTABLE=<python.exe>
cmake --build --preset release
cmake -S . -B build/host -G Ninja -DHOST_TESTS=ON -DCMAKE_C_COMPILER=<gcc.exe>
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
```

仓库检查和推送：

```powershell
git diff --cached --check
git diff --cached --check -- . ':(exclude)Firmware/validation/**'
git ls-files -ci --exclude-standard
git fetch origin main
git push origin main
git rev-parse HEAD
git ls-remote origin refs/heads/main
```

推送后以本地 `HEAD` 与远端 `main` 相同为完成依据；实际提交与推送结果以 Git 记录为准。
