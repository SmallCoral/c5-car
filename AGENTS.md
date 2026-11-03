# Coral-C5 工作约定

当前目标是校赛A题；总体方案与当前状态从根README进入。

## Skills

- 按实际任务使用 `mthines/agent-skills`，不为运行全部流程制造无关任务。
- 文档整理采用 `docs`；安装版本和支持树见 `Documentation/reference/skills.md`。
- 扁平安装的相对支持路径失效时，从完整支持树读取原文件，不猜测替代规则。

## 硬件修改

- 用户已手工调整原理图和PCB，不直接重跑生成器覆盖设计。
- 先保存本轮基线，再编辑；保留既有位号、UUID和布局约束。
- Motor六针线序以 `Documentation/reference/motor-interface.md` 为准。
- 98触点合同以 `Hardware/stm32c5a3/Hardware/Shared/interface.csv` 为准。
- 原理图变更后导出原生网表、运行ERC与 `tools/verify_schematic.py`，保留真实失败。
- 原理图资源检查、PCB DRC和实物验证分别报告，不能相互替代。
- 新选型未画入原理图、未验证前，不写入已实现功能或生产BOM。
- 不改旧项目 `D:/dev/qrs/stm32/Coral-C5`，它仅供本项目查阅历史资料。

## 文档归属

- 根README只保留用途、现状和入口。
- 比赛与跨板方案放 `Documentation`，机械参数放 `Mechanical`。
- 硬件本地符号、封装、模型随原KiCad工程保留。
- 验证记录注明日期、工具版本、源码版本、命令、失败与未验证项。
- 价格、库存、计算、芯片额定和实测分别表述，保留原始来源及币种。
