# A 题项目文档

按用途阅读，详细针序只在对应接口表维护。

| 类别 | 文档 |
| --- | --- |
| 原始赛题 | [2026 校赛题目](2026校赛题目（小车·无人机·嵌入式AI）.pdf)、[AC 器材清单](AC题清单.doc) |
| 总体方案 | [A 题大纲](explanation/a-system-outline.md) |
| 同学历史资料 | [资料总索引](reference/classmate-materials.md)、[电赛案例](26电赛/README.md)、[步进控制片段](step_motor/README.md) |
| 历史控制方案解读 | [平衡球代码与当前方案的关系](explanation/classmate-balance-reference.md) |
| 机械设计 | [首版 3D 总装与预览](../Mechanical/README.md)、[建模大纲](explanation/mechanical-outline.md)、[参数文件](../Mechanical/parameters.yaml) |
| 器件参考 | [器件选型与采购](reference/parts-selection.md) |
| 双摄参考 | [荔枝派、MaixCAM2、Action 4 与旧机械参数](reference/cameras.md) |
| 外接模块清单 | [三模块、摆杆步进和底部灰度](reference/external-modules.md) |
| 感为八路灰度 | [实物、MSPM0L1306 辅助板、接口及原始资料](感为八路灰度资料/README.md) |
| 已提出的后续硬件变更 | [TFT、任务按钮和四脚 HC-08](reference/pending-hardware-changes.md)，本轮原理图已完成，PCB 与固件待落实 |
| 接口参考 | [电机六针](reference/motor-interface.md)、[当前GPIO分配](reference/motor-core-pins.md)、[M2 资源计划](reference/resource-plan.md) |
| 调试步骤 | [分阶段上电与验收](how-to/bring-up.md) |
| 验证状态 | [最新：M2.8 双路电源、荔枝派及 PCB 同步](validation/2026-11-07-m28-resume.md)、[历史：M2.7 统一电源](validation/2026-11-07-m27-unified-power.md)、[M2.5 视觉板供电、灰度与贴片按键](validation/2026-11-06-m25-vision-power.md)、[M2.3 电源、接口及 0603](validation/2026-11-06-m23-power-connectors.md)、[此前 M2.1 原理图整理](validation/2026-11-06-schematic-layout.md)、[此前 M2 接口](validation/2026-11-06-m2-schematic.md)、[此前布线与引脚调整](validation/2026-11-03-motor-pin-remap.md)、[删除J3/J5](validation/2026-11-03-remove-j3-j5.md)、[此前方案与六针修改](validation/2026-11-03.md) |
| 参考资料整理记录 | [2026-11-05 资料清点与完整性核对](validation/2026-11-05-classmate-materials.md) |
| 机械模型验证记录 | [2026-11-05 框架、参考几何与运动检查](validation/2026-11-05-mechanical-framework.md) |

旧项目 `D:/dev/qrs/stm32/Coral-C5` 仅作为历史资料来源。
本轮只修改当前 `c5-car`，不修改旧项目。
电机厂家的受限资料不复制入仓库；必要事实及来源摘要见电机接口文档。
新增同学资料作为历史参考保存；其平台、引脚、控制参数和既有验证结果不代表当前 C5 小车的实现状态。

## 文档整理方式

采用已安装支持树中的 `mthines/agent-skills` 的 `docs` skill，模式为 `update`。
入口是 `C:/Users/Shan_/.codex/skill-sources/mthines-agent-skills/skills/authoring/docs/SKILL.md`。
根 README 提供状态与入口，方案归 `explanation`，参数及接口归 `reference`，操作归 `how-to`，运行结果归 `validation`。
已按确认的仓库补齐并核对49份入口，详见[Skills记录](reference/skills.md)。
不复制旧项目的验证结论，不把计划器件列为已装板元件。
