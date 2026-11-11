# A 题项目文档

整理日期：2026-11-11。
用途、当前状态与工程入口从[项目首页](../README.md)开始。
接口和采购以参考表为入口，检查结果按日期保留。

| 类别 | 文档 |
| --- | --- |
| 原始赛题 | [2026 校赛题目](2026校赛题目（小车·无人机·嵌入式AI）.pdf)、[AC 器材清单](AC题清单.doc) |
| 总体方案 | [A 题大纲](explanation/a-system-outline.md) |
| 当前 PCB 问题 | [2026-11-10 Motor PCB 复查](validation/2026-11-10-motor-pcb-review.md) |
| Motor 采购 | [完整 BOM、精简 RC、降本候选及使用限制](reference/motor-bom.md) |
| 机械设计 | [总装与预览](../Mechanical/README.md)、[建模大纲](explanation/mechanical-outline.md)、[参数](../Mechanical/parameters.yaml) |
| 外接模块 | [模块清单](reference/external-modules.md)、[板端针序与保护边界](reference/m2-interfaces.md) |
| 电机接口 | [MG513 六针口](reference/motor-interface.md) |
| GPIO | [Motor 核心板信号分配](reference/motor-core-pins.md)、[资源计划](reference/resource-plan.md) |
| 待完成硬件 | [实现状态和未验证项](reference/pending-hardware-changes.md) |
| PCB 元器件模型 | [模型来源及精确性限制](validation/2026-11-09-3d-models.md)、[M3 安装孔](validation/2026-11-09-m3-mounting-holes.md) |
| 调试 | [分阶段上电与验收](how-to/bring-up.md) |
| 验证记录 | [按日期查看全部记录](validation/README.md) |
| 器件历史依据 | [选型与采购快照](reference/parts-selection.md) |
| 同学参考资料 | [总索引](reference/classmate-materials.md)、[电赛案例](26电赛/README.md)、[步进代码片段](step_motor/README.md) |
| 控制方案参考 | [平衡球历史代码解读](explanation/classmate-balance-reference.md) |
| 摄像头资料 | [模块与原始资料](摄像头与视觉板/README.md)、[摄像头参数](reference/cameras.md) |
| 灰度资料 | [实物、MSPM0L1306 辅助板及原始源码](感为八路灰度资料/README.md) |
| 附带网页归档 | [独立网页示例](archive/README.md) |

旧项目 `D:/dev/qrs/stm32/Coral-C5` 仅作为历史资料来源。
厂家原文和旧项目记录按原样保留，其中部分外部资源未随本仓库收录；不能将这些资料的旧检查结果当作当前实现状态。
模块精简已实施，其[原始需求](reference/module-power-simplification.md)只作追溯。
文档整理采用 `mthines/agent-skills` 的 `docs update`，来源及本机支持树状态见[Skills 记录](reference/skills.md)。
