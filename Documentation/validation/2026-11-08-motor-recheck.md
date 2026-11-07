# 2026-11-08 Motor当前原理图与PCB复查

本轮为只读检查，随后用户要求先交接、上传GitHub。尚未实施供电/接口简化，没有按讨论删除任何元件。源码父提交`d79ffc78ad5093452b1b1b852f7de347f0cc084b`，结果包含当前未提交硬件修改。KiCad CLI10.0.8、Python3.11.5。

当前PCB SHA256：`e90e3653baefca012a6a8e8c4b6e853c7d2f1770460ffc8d91d0b7b150d3ee80`。输入快照和全部源文件哈希见 [inputs.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/inputs.json)。检查后重新计算哈希，与检查前一致。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 电气网表 | 205网络分区等于手写M2.8合同，无多接/少接 | [合同对比](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/net-intent.json)、[原生网表](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/netlist.xml) |
| F8匹配 | 原理图180、PCB180；唯一对应，预计新增/替换0，值/焊盘网络不匹配0 | [对应检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/associations.json) |
| ERC | 0错误、3警告：U10/U11/U12缓存符号与本地库差异 | [原生ERC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/erc.json) |
| PCB一致性 | 0 | [常规DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/pcb-drc.json) |
| 常规DRC | 18违规：17错误、1警告；另379未连接 | [常规DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/pcb-drc.json) |
| 全走线错误补查 | 同为18违规、379未连接，未多发现短路/铜间距违规 | [全走线DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/pcb-drc-all-tracks.json) |
| 当前铜线 | 176段、3过孔、无铜区、无Edge.Cuts边框 | [图层预览](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/recheck-2026-11-08-004941-e90e3653/pcb-overview.png) |

## 仍有的问题

1. J14接地固定焊盘51与NPTH孔的实际孔边距0.05mm，小于当前0.25mm规则；需对照准确厂家机械图处理，不以降低全板规则掩盖。
2. 缺少Edge.Cuts外形，尚未形成可制造板框。
3. 15处Courtyard装配边界重叠，集中在左右电机周边，例如R16/C16、R16/R18、U2/C10、U4/C30以及J6周边。需要按实际外形和装配余量调整，不把边界重叠等同短路。
4. 左电机L_nFAULT过孔在(241.25,85.85)仅连接单层或悬空，需补齐目标连接。
5. 379项未连接包括逻辑、电源和地；当前仍没有GND走线或地铜区，不能接电当成成品板。
6. U10/U11/U12符号缓存与库的引脚编号、名称、电气类型一致，几何位置等属性不同；当前205网络未改变。直接从旧库刷新可能移动引脚导致断线，整理时应保留用户实际图形及连接，而非强刷旧库。

本次未重新执行旧3786项完整M2.8保留约束/模型检查，也未完成新的98触点逐项报告；旧报告不能算这轮通过。Core原生网表已导出供接手使用。旧PDF未重新导出，旧0交叠结果仅属于当时PDF。

## 已运行命令

在Motor工程目录，输出目录记为`docs/recheck-2026-11-08-004941-e90e3653`：

```powershell
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch export netlist --format kicadxml --output docs/recheck-2026-11-08-004941-e90e3653/netlist.xml CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' sch erc --format json --severity-all --output docs/recheck-2026-11-08-004941-e90e3653/erc.json CoralC5-Motor.kicad_sch
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb drc --format json --schematic-parity --severity-all --refill-zones --output docs/recheck-2026-11-08-004941-e90e3653/pcb-drc.json CoralC5-Motor.kicad_pcb
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb drc --format json --schematic-parity --severity-all --all-track-errors --refill-zones --output docs/recheck-2026-11-08-004941-e90e3653/pcb-drc-all-tracks.json CoralC5-Motor.kicad_pcb
```

F8对应检查调用`tools/verify_pcb_associations.py`的只读`audit()`，没有执行GUI F8。当前有用户新增的走线，不得用F8修复前151段走线基线限制最新布局；旧151/3或229违规均不是本轮最新结果。DRC忽略类别沿用当前工程配置，未增加排除、未降低规则。

下一轮已确认需求见 [供电与接口简化](../reference/module-power-simplification.md)，现阶段已停止硬件修改。
