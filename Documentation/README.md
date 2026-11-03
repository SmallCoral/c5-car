# A 题项目文档

按用途阅读，详细针序只在对应接口表维护。

| 类别 | 文档 |
| --- | --- |
| 原始赛题 | [2026 校赛题目](2026校赛题目（小车·无人机·嵌入式AI）.pdf)、[AC 器材清单](AC题清单.doc) |
| 总体方案 | [A 题大纲](explanation/a-system-outline.md) |
| 机械设计 | [3D 建模大纲](explanation/mechanical-outline.md)、[参数文件](../Mechanical/parameters.yaml) |
| 器件参考 | [器件选型与采购](reference/parts-selection.md) |
| 接口参考 | [电机六针](reference/motor-interface.md)、[当前GPIO分配](reference/motor-core-pins.md)、[M2 资源计划](reference/resource-plan.md) |
| 调试步骤 | [分阶段上电与验收](how-to/bring-up.md) |
| 验证状态 | [最新：布线与引脚调整](validation/2026-11-03-motor-pin-remap.md)、[删除J3/J5](validation/2026-11-03-remove-j3-j5.md)、[此前方案与六针修改](validation/2026-11-03.md) |

旧项目 `D:/dev/qrs/stm32/Coral-C5` 仅作为历史资料来源。
本轮只修改当前 `c5-car`，不修改旧项目。
电机厂家的受限资料不复制入仓库；必要事实及来源摘要见电机接口文档。

## 文档整理方式

采用已安装支持树中的 `mthines/agent-skills` 的 `docs` skill，模式为 `update`。
入口是 `C:/Users/Shan_/.codex/skill-sources/mthines-agent-skills/skills/authoring/docs/SKILL.md`。
根 README 提供状态与入口，方案归 `explanation`，参数及接口归 `reference`，操作归 `how-to`，运行结果归 `validation`。
已按确认的仓库补齐并核对49份入口，详见[Skills记录](reference/skills.md)。
不复制旧项目的验证结论，不把计划器件列为已装板元件。
