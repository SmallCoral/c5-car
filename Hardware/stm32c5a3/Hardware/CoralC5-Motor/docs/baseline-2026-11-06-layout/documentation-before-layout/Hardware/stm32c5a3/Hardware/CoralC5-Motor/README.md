# CoralC5-Motor

2026-11-06 更新至 M2 原理图。使用 KiCad 10 打开[工程](CoralC5-Motor.kicad_pro)或[主原理图](CoralC5-Motor.kicad_sch)，预览见[九页 A4 电路 PDF](docs/CoralC5-Motor_Schematic.pdf)。本轮仅处理原理图和必要资源，PCB 由用户绘制。

J1 已换为卧式 XT60PW-M。新增四脚 HC-08、TFT、四键、感为八路灰度加 MSPM0L1306 辅助板、两路相机处理模块以及外置步进驱动接口；U6/U7 为独立五伏和三伏三外设电源。TFT 的 H1/H2 为 2mm 非金属化孔，按同一短边中心距 30mm 标注，PCB 孔位尚未定位。

既有 13 路轮控制、J4/J6 六针线序、J8 调试及 J10 停机回路保留。Core 和 98 触点合同未修改；共新增 31 路 GPIO，完整分配见 [M2 接线表](../../../../Documentation/reference/m2-interfaces.md)和[机器表](docs/m2_pin_assignment.csv)。J9 沿用用户此前删除的状态，旧 AUX 标签已整理。

| 结果 | 状态 |
| --- | --- |
| 原生 ERC | 0 错误、0 警告 |
| 原理图合同检查 | 1228 项通过 |
| 九页自动版面检查 | 导线/文字交叠 0，文字相互交叠 0 |
| 封装与模型 | 116 位号含两个钻孔；113 位号绑定可解析模型；J1 准确卧式模型缺失 |
| PCB、DRC、原理图/PCB 一致性、实物验证 | 本轮未执行 |

[本轮记录](../../../../Documentation/validation/2026-11-06-m2-schematic.md)保留基线、版本、命令、原厂依据、资源缺项及检查边界。原有左右电机页与基线逐字节相同；其他既有器件的位号、UUID 和坐标保留。

新增五伏外设预算 1.5A、三伏三外设预算 150mA；Core 外接三伏三预算仍为 100mA。原 F1 四安保护保留，换 XT60 不提高整板持续电流能力。HC-08 的 J16 默认装 1—2 跳帽，外置步进控制须与驱动板电平和有效极性匹配。两个相机接口均接处理模块，不能直接接裸 OV2640。

[M2 用料草表](docs/BOM_M2_SCHEMATIC_DRAFT.csv)仅对应当前原理图，不是生产发布 BOM；旧 `docs/BOM.csv` 为历史 M1 清单。原厂与封装依据见 [sources](docs/sources/README.md)。固件没有实现本轮新增功能。

手工编辑后不要重跑历史生成器。更新原生网表和 ERC 后运行 `tools/verify_schematic.py`；导出 PDF 后运行 `tools/check_visual.py`。`extend_m2_schematic.py` 仅记录本轮迁移，默认拒绝执行，并校验保存的文件哈希，防止覆盖后续手工修改。
