# Skills 安装与使用记录

## 2026-11-17 循线调参与交接整理

沿用完整支持树的 `docs update`，清理旧速度、默认页面、镜像号和实现状态，同步遥测 v6、操作入口与验证索引。采用本机 `handoff` 更新 `.agent/main/handoff.md`，集中记录最新镜像、连续两次回 A 的证据、实车映射及后续验证边界；历史失败保留在各次记录中。
本轮只整理、离线复核和提交，不新增烧录或电机动作；不修改 Rust 或手工硬件文件，也不安装技能或运行自动 PR 流程。

## 2026-11-17 灰度循线一圈

继续采用 `kernel-guided-coding` 的通用工程原则与嵌入式适配；按实际赛道目标查阅同学 26H、21E、22C 归档，没有执行旧工程或改写原资料。
采用 `pdf` 只读提取并渲染校赛 A 题原件，确认 A 横线和顺时针要求；使用本地 bundled PDF 依赖。
文档沿用完整支持树中的 `docs update`，同步按键、灰度方向、v4 遥测、首圈操作、真实失败和交接。未安装技能、运行自动 PR 或修改硬件设计。

## 2026-11-17 灰度固件与联调说明

固件采用 `kernel-guided-coding` 的通用工程原则与嵌入式 C 适配，沿用现有 LL/CMSIS 工程与 Tab 风格。
文档采用 `docs update`，从 `.scratch/docs-support/skills/authoring/docs/` 完整支持树读取入口及 routing、placement、writing、drift、CLAUDE.md 规则；更新灰度操作、遥测、索引及验证记录。
未安装或更新技能，未运行自动 PR 流程；未修改原理图、PCB 或独立 Rust 工程。

## 2026-11-16 Rust 与交接整理

采用 `docs update` 模式，范围为 Rust 开发入口、项目索引、硬件快照状态与验证记录。
沿用 `.scratch/docs-support/skills/authoring/docs/` 完整支持树，读取 routing、placement、writing、drift 和 CLAUDE.md 规则。
采用本机 `handoff` 技能更新 `.agent/main/handoff.md`，覆盖 C/Rust 现状、构建、剩余工作和硬件边界。
没有安装或升级技能，也没有运行自动 PR 流程。

## 2026-11-16 固件与项目整理

文档继续采用 `docs update`，从本机 `.scratch/docs-support/skills/authoring/docs/` 的完整支持树读取入口及 routing、placement、writing、drift、CLAUDE.md 规则。目录仅记录当前环境，不作为克隆项目后的必需依赖。

本轮整理当前 LL 固件、无线升级、硬件入口和交接；保留原始资料与历史失败结果。固件开发采用 `kernel-guided-coding` 的通用工程原则与嵌入式 C 适配。没有全局安装/更新技能，也没有运行自动 PR 流程。

## 2026-11-11 项目整理

本轮从项目本地缓存的完整支持树读取 `skills/authoring/docs/SKILL.md`，使用 `update` 模式。
缓存版本仍为下述 `2b8d75d325c4816c4c1f5f3e982f682442b79c5d`，读取 routing、placement、writing、drift 和 CLAUDE.md 规则；没有全局安装或升级技能。
缓存位于被忽略的 `.scratch/m2/docs-support/`，不随 Git 提交。
此路径只描述本轮环境；历史 Shan_ 路径不应当作其他机器的可执行命令。

核对日期：2026-11-03。
来源为[mthines/agent-skills](https://github.com/mthines/agent-skills)。
本机完整支持树固定在提交 `2b8d75d325c4816c4c1f5f3e982f682442b79c5d`。

## 完整性

完整支持树为 `C:/Users/Shan_/.codex/skill-sources/mthines-agent-skills`。
其中 `skills/` 递归发现49份 `SKILL.md`，包括工作流下的嵌套入口。
开始核查时，扁平安装目录已有48份，缺少 `docs`。
使用Codex的 `skill-installer` 从同一固定提交补装 `skills/authoring/docs`。
补装后49份入口均存在；按统一换行符比较，文本全部与固定提交一致。
没有覆盖其余48个入口，没有运行上游一键同步脚本。
新增 `docs` 在后续对话可被发现，本轮已直接读取并使用其规则。

## 本轮采用

| Skill | 用途 |
| --- | --- |
| docs | 按现有目录做update，整理README、参考表、方案与验证入口 |
| skill-installer | 补齐缺少的docs入口并核对49份文本 |
| powershell-safe-invocation | Windows只读检查、路径及原生命令调用 |
| lcsc | 零件采购编号、销售来源和供货核对 |
| pdf | 阅读原赛题、重新导出并渲染电机原理图PDF |
| 项目保存的schematic-design规范 | 原理图与必要资源编辑、保留现有PCB，不重跑生成器 |

UI、视频录制、遥测、GitHub PR和自动发布没有对应任务，本轮不运行这些流程。
没有把未运行的工作流记为通过。
固定版本不是对“永久最新”的声明，后续升级另行记录。

## 2026-11-05 本机读取情况

以上安装路径是原机器记录，当前 `sc` 用户环境没有对应的 `docs` 安装或完整支持树。
本轮从[相同固定提交](https://github.com/mthines/agent-skills/tree/2b8d75d325c4816c4c1f5f3e982f682442b79c5d)取得完整支持树，在临时工作区读取 `skills/authoring/docs/SKILL.md` 及所需规则。
采用 `update` 模式，范围为新增同学参考资料及其索引。
没有全局安装、升级技能或生成新的代理配置。

## 2026-11-07 M2.8 续接

当前 Shan_ 环境从原完整支持树读取 `skills/authoring/docs/SKILL.md` 及其 routing、placement、writing、drift 规则，采用 update 模式。未重装或升级技能；本轮补齐当前入口、接口、采购、验证及交接，历史报告继续保留。PowerShell、lcsc 与 PDF skill 按实际任务使用，没有触发全套自动 PR 流程或 computer-use。
