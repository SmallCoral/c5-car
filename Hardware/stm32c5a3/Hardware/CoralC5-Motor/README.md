# CoralC5-Motor

## 当前状态

整理于 2026-11-21。
当前电路为 M2.8P2，用户已完成 Core/Motor 焊接，轮电机、编码器、Emm 步进、彩屏按键和蓝牙已进入实物联调；使用方法见[固件入口](../../../../Firmware/README.md)。

11 月 21 日在当前设计副本上导出原生网表，Motor ERC 无报项。PCB 原生 DRC 有 17 条错误、6 条警告，另有 6 条原理图一致性警告，未连接项为 0。资源检查 957 项中 4 项失败。原工程保留手工布局。[核验方法与边界](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Documentation/validation/2026-11-21-handoff.md) · [本次原始结果](docs/handoff-2026-11-21/snapshot.json) · [11 月 10 日历史复查](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Documentation/validation/2026-11-10-motor-pcb-review.md)

| 内容 | 入口 |
| --- | --- |
| 编辑工程 | [KiCad 工程](CoralC5-Motor.kicad_pro)、[原理图](CoralC5-Motor.kicad_sch)、[PCB](CoralC5-Motor.kicad_pcb) |
| 查看图纸 | [十页原理图 PDF](docs/CoralC5-Motor_Schematic.pdf) |
| 采购 | [完整 BOM、精简 RC 和降本候选](docs/bom/README.md) |
| 电气定义 | [外接接口](../../../../Documentation/reference/m2-interfaces.md)、[GPIO 分配](../../../../Documentation/reference/motor-core-pins.md) |
| 原始 PCB 检查 | [DRC](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/drc-refill.json)、[同步核对](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/verify-pcb-sync.json)、[摘要](https://github.com/SmallCoral/c5-car/blob/1b936e16e70845d13a9f0e41dbcd20ecfca4ad00/Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pcb-review-2026-11-10/review-summary.json) |
| 历史检查 | [验证索引](../../../../Documentation/validation/README.md) |

三份 XLSX 均为采购快照，不自动更新原理图或 PCB 中的料号。
精简表省略的阻容仍属于设计，降本替换仍须核对引脚、封装及外围。

## 修改与检查

继续在本目录的原生工程上编辑，保留本地库、位号、UUID 和手工布局。修改原理图后导出网表，运行 KiCad ERC 和 `tools/verify_schematic.py`。修改 PCB 后单独运行原生 DRC 及 `tools/verify_pcb_sync.py`。安装孔、模型精度与采购料号各有独立依据。[模型来源](Models/README.md) · [接线与保护边界](../../../../Documentation/reference/m2-interfaces.md)
