# Coral-C5 当前交接

更新日期：2026-11-11。
仓库为 `SmallCoral/c5-car`，分支为 `main`；本轮起点为 `0b9b2a66126a25e0a6bbb31a1b0634d705a871a3`。
交付提交和远端状态以 `git log -1`、`git status -sb` 为准。

## 当前状态

- 校赛 A 题采用 Core + Motor。网络 Expansion 是历史方案，当前仓库没有其工程文件。
- Motor 电路为 M2.8P2，142 个原理图实体、184 个网络、39 路 GPIO；PCB 有 146 个封装，其中 140 个电气元件、6 个孔。
- 2026-11-10 KiCad 10.0.6 原生 DRC：17 条错误、6 条警告、0 项未连接，另有 6 条原理图一致性警告。
- 新导出网表与 PCB 的有效电气焊盘网络无错配；H1 重复位号和机械孔关联仍使同步检查失败。不要宣称完全通过。
- 当前 PCB SHA256 为 `9de443e35a98fb3a4f64617e2b5f9bcd7b38bafe53b79f5d6056c7db906da830`，11 月 11 日整理时核对未变。
- 固件仍为初始化后空循环；机械总装为历史框架；未完成整车实物验证。

## 从这里继续

| 内容 | 文件 |
| --- | --- |
| 主要入口 | [根 README](../../README.md)、[Motor README](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/README.md) |
| 最新 PCB 检查 | [复查说明](../../Documentation/validation/2026-11-10-motor-pcb-review.md)、[原始 DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/drc-refill.json) |
| 采购表 | [Motor BOM 说明](../../Documentation/reference/motor-bom.md) |
| 接口 | [接线](../../Documentation/reference/m2-interfaces.md)、[GPIO](../../Documentation/reference/motor-core-pins.md)、[电机六针](../../Documentation/reference/motor-interface.md) |
| 本轮整理 | [整理记录](../../Documentation/validation/2026-11-11-project-cleanup.md) |
| 原交接 | [整理前版本](archive/2026-11-11-before-cleanup-handoff.md) |

## 未完成工作

1. 依据连接器原厂图纸处理 J14 的 0.05mm 孔到铜间距；将 V5_SYS 的 0.1mm 微孔改成适合 1.6mm 板厚的普通通孔。
2. 检查 U8/U16 的大电流细线、U1/U6 的开关回路，以及 U1/U6/U2/U4 的局部散热地铜和过孔。
3. 按实际器件尺寸检查 16 处 Courtyard 重叠，处理安装孔重复位号、封装定义及丝印，再重新跑 DRC。
4. 三份 BOM 中只有完整表覆盖 140 个位号。精简表省略 42 个阻容；U10、MOS 和降本替换仍是候选，不能直接写成已实现器件。
5. 完成电源启动、持续负载、温升、负载阶跃、模块供电、编码器和整车验证；按当前分配接入固件。

本轮授权为项目整理和推送，未执行上述硬件修改。
继续修改前读取根 AGENTS.md 和新的用户指令；保留实时设计，不重跑旧生成器或用历史基线覆盖设计。

## 本轮提交内容

归档三份采购工作簿和 11 月 10 日原始检查报告，更新项目、文档、硬件与固件入口，标明历史方案和未落实替代件。
根目录 `core.png` 原样移到 `Images/core.png`；独立网页原样移到 `Documentation/archive/web-demos/pelican-bike.html`。
本地工作缓存、Office 锁文件、浏览器状态和代理记忆继续排除在版本控制外。
硬件源文件、符号封装模型、固件源码和机械设计保持原内容。
