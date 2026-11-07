# F8 重复新增元件：关联路径修复

检查日期：2026-11-08。基于main/d79ffc78ad5093452b1b1b852f7de347f0cc084b及用户当前未提交修改；工具KiCad CLI10.0.8、Python3.11.5。未使用CUA，未操作GUI的F8按钮，未布局或布线。

## 原因

当前文件在修复前已有180个原理图元件、180个PCB封装，位号无重复。左右轮页面的48个封装缺少`path`原理图关联字段，包含U2/U3/U4/U5、J4/J6、R10～R19/R30～R39与C10～C20/C30～C40。数量和位号相同，仍不能证明F8能匹配。

[KiCad10官方文档](https://docs.kicad.org/10.0/en/pcbnew/pcbnew.html#re-linking-symbols-and-footprints)说明默认按唯一标识关联；未匹配的符号会新增封装。原厂[BOARD_NETLIST_UPDATER源码](https://github.com/KiCad/kicad-source-mirror/blob/10.0/pcbnew/netlist_reader/board_netlist_updater.cpp)的匹配逻辑为组件sheetpath加每个符号UUID，与footprint.path比较。按同样规则，修复前预计新增48个。无法仅从现有文件确定最初哪次保存或更新丢失了关联，不推测来源。

上一轮原生DRC一致性0及当时同步检查通过，没有实际验证GUI F8再次更新的行为；不能据此保证后续文件关联未丢失。旧报告保持为历史结果，本次增加独立的默认UUID匹配预检。

## 修复及保留

先保存 [最新基线](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-08-f8-association-71d22540/baseline.json)，原PCB SHA256为`71d22540c4928208ef3f44f82209ea4e4e32a91de5d3e22b4190d9be07dee2e8`。逐个补回48个缺失path，只插入关联字段，不移除或改写其它既有PCB字节；未从旧M2.8 PCB恢复布局。180个封装及焊盘UUID、当前坐标/角度、图形模型、151段走线、3个过孔和其它板级节点逐项与本次基线核对相同。输出PCB SHA256为`ba073cc304ebe87265242cb90751b266800e45346027e4b35721cdc8af43f8e7`。

检查期间外部编辑器再次保存了原理图及部分.history。工具记录这些变化，不回滚、不当作本次PCB修复；最终从当前原理图重新原生导出，验证导出前后源文件哈希稳定。旧M2.8“33件在板外”的布局状态已过时，不作为本次保留依据。

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 原理图/PCB元件数 | 180/180 | 180/180 |
| 缺少path | 48 | 0 |
| 按F8默认UUID规则预计新增 | 48 | 0 |
| UUID歧义/重复位号 | 0/0 | 0/0 |
| 封装类型预计替换 | 0 | 0 |
| 值和焊盘网络不匹配 | 修复前仅已关联132件检查 | 全部180件均0 |
| 原生原理图一致性 | 未作为关联依据 | 0 |
| 当前PCB DRC | 未复用旧会话数量 | 229违规、390未连接，真实失败 |

原始证据：[修复前报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-08-f8-association-71d22540/f8-before.json)、[48项修复映射](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/f8_association_repair.json)、[最终匹配与保留检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/f8_association_verification.json)、[最新原生网表](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/f8_current_netlist.xml)、[原生DRC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-drc-2026-11-08-f8.json)。DRC主要为丝印/铜皮、丝印交叠、庭院及外形等；关联修复不等于布线或生产检查通过。

## 在KiCad中继续

修复针对磁盘文件。若编辑器还开着，重新加载本项目PCB后再F8；如有未保存的布局，先另存副本，避免内存旧文件覆盖修复。已补齐关联，通常保持“按位号重新关联”未勾选，使用默认UUID匹配。F8仍可能提示字段刷新，字段更新不同于新增元件。尚未在GUI内复测，实际预览以重新加载后的更新清单为准。

## 重复检查

在Motor目录执行：

```powershell
& 'D:/KiCad/10.0/bin/kicad-cli.exe' pcb drc --format json --schematic-parity --refill-zones --output docs/pcb-drc-2026-11-08-f8.json CoralC5-Motor.kicad_pcb
& 'D:/KiCad/10.0/bin/python.exe' -X utf8 -B tools/verify_pcb_sync.py
```

入口现调用`verify_pcb_associations.py`：默认先导出当前原理图，再按所有符号单元UUID匹配，检查缺失/歧义/重复、封装类型、值和焊盘网络，并针对本次PCB基线验证只改关联字段。历史复现可显式传`--board`与`--netlist`。旧M2.8元数据生成器已加保护，避免用旧PCB报告覆盖当前状态；旧原理图/资源/采购报告保留，不冒充本次重新完整验证。
