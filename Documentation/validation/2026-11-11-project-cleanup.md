# 2026-11-11 项目整理与提交前检查

本轮目标为整理当前项目并推送 GitHub。
起点提交为 `0b9b2a66126a25e0a6bbb31a1b0634d705a871a3`，分支为 `main`。
远端为 `https://github.com/SmallCoral/c5-car.git`；实际交付提交以 Git 记录为准。

## 文档归属与文件整理

采用本机已经下载的 `mthines/agent-skills` 的 `docs` skill，模式为 `update`。
支持树固定在 `2b8d75d325c4816c4c1f5f3e982f682442b79c5d`，具体读取与规则来源见[Skills 记录](../reference/skills.md)。

| 范围 | 结果 |
| --- | --- |
| 根 README | 保留用途、当前状态和入口，移除“PCB 尚未同步”等过期结论 |
| 项目/硬件/固件入口 | 明确 Core + Motor；网络 Expansion 工程不在当前仓库，旧资料仅作历史参考 |
| 当前接口与调试文档 | 统一串口编号、直供步进、Maix 简化保护与已删除 J27 的说明 |
| 验证记录 | 增加按日期索引、11 月 10 日 PCB 复查说明和原始 JSON/钻孔统计 |
| 采购 | 三份 XLSX 原样归入 Motor `docs/bom`，说明完整表、采购子集和未落实替代件 |
| 图片/网页 | `core.png` 原样移至 `Images/core.png`；独立网页原样移至 `Documentation/archive/web-demos/pelican-bike.html` |
| 交接 | 更新当前交接，整理前版本保留在 `.agent/main/archive`；旧提示词标记为历史 |
| 本地状态 | `.scratch`、代理本地记忆、Office 锁文件、KiCad 本地状态和浏览器缓存排除在提交外 |

已有厂家资料、历史基线与设计资源保持各自目录，未重写 Git 历史。
所有待推送的有效文件均纳入本轮提交，不上传本地运行时与缓存。

## 校验

使用 Git 2.45.1.windows.1、KiCad 配套 Python 3.11.5 执行检查；具体结果见[机器记录](2026-11-11-project-cleanup.json)。

| 项目 | 结果与边界 |
| --- | --- |
| 设计保留 | 对整理前记录的硬件、固件与机械文件逐个核对 SHA256；仅 README 文档允许变化，设计源码、PCB、模型及固件原内容保留 |
| BOM 文件 | 三个工作簿 ZIP/XML 可读取，移动前后 SHA256 相同 |
| 完整 BOM | 53 行采购物料覆盖 140 个电气位号，无多余/重复位号，逐行数量与位号数相同 |
| 两份精简表 | 各 39 行、98 个元件；省略的 42 个阻容仍在设计中，缺项如实列入机器记录 |
| PCB 检查 | 源板指纹与 11 月 10 日报告一致；保留 17 条 DRC 错误、6 条警告及 6 条一致性警告，没有重写成通过 |
| 本地文档链接 | 项目入口与 Documentation 中的 Markdown 本地目标逐项检查；数量和失败项见机器记录 |
| 资源补齐 | 原 Sipeed 外设原文引用的 27 张缺失图片从相同官方 Wiki 路径补齐；原文字节不变，下载来源和 SHA256 见[清单](../摄像头与视觉板/assets/peripheral/sources.json) |
| 历史跨项目引用 | 三份电机旧记录中的 6 个缺失外部路径保留为文字说明，避免伪装成本仓库可打开文件 |
| Git 提交内容 | 执行差异空白检查、待提交文件检查与常见凭据特征扫描，随后提交并核对远端 main |

本轮没有重新查询现货或价格、重新选型、改电路、改 PCB、编译或烧录固件、重建机械模型，也没有进行实物验证。
XLSX 中的库存日期仍是 2026-11-09，未验证替代件仍为候选；详见[采购说明](../reference/motor-bom.md)。

提交前使用的主要命令：

```powershell
git fetch origin
git status --short --branch
git diff --check
git diff --cached --check
git diff --cached --stat
```

Git 推送完成后，以本地 `HEAD` 与 `git ls-remote origin refs/heads/main` 返回值一致作为远端校验。
