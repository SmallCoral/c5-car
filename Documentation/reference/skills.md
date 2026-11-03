# Skills 安装与使用记录

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
