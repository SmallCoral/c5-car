# CoralC5-Motor

2026-11-06 更新到 M2.4。
用 KiCad 10 打开[工程](CoralC5-Motor.kicad_pro)或[主原理图](CoralC5-Motor.kicad_sch)，预览见[九页电路 PDF](docs/CoralC5-Motor_Schematic.pdf)。
总览为 A3，八张功能页为 A4，主页模块信号已有实际层次连线。

TFT 与 HC-08 使用排母，其余模块线束采用普通 XH；J10 恢复两针跳帽排针，蓝牙固定接 3V3_EXT，删除 J16 电压选择跳帽和蓝牙五伏分支。
XT60 和备用电源端子保留。[最新验证](../../../../Documentation/validation/2026-11-06-m24-fixed-bluetooth.md)记录本次变更。
ZDT_XS 第二代 TTL 一体步进使用 J23 八针，仅接 1/2/7/8；完整针序只在 [M2 接线表](../../../../Documentation/reference/m2-interfaces.md)维护。
67 个英制 0603 实例统一为 0.85mm 内侧间隙、0.975×0.95mm 焊盘，按电阻、电容和 LED 保留模型及极性。

SW1 为 SS12D10G5，控制两只 DMP3007LK3-13 的栅极，R1/R2 为 100k/4.7k。
主保险 F1 为 12A 延时，步进分路 F2 为 4A 延时；保险丝不是精确限流器，XH 负载仍受其 3A 额定条件限制。
整车设计预算为 12V 时约 8.10A、10V 时约 8.32A，计算假设、原厂依据和未验证项见[本轮记录](../../../../Documentation/validation/2026-11-06-m23-power-connectors.md)。

| 结果 | 状态 |
| --- | --- |
| 原生 ERC | 0 错误、0 警告 |
| 原理图、接口与资源合同 | 1571 项通过 |
| 九页自动版面检查 | 导线/文字交叠 0，文字相互交叠 0 |
| 原生封装库导出 | SVG 导出成功；不是 PCB DRC |
| 资源 | 116 个实体位号含两个孔，113 个位号有可解析模型；J1 准确卧式模型仍缺；SW1/F1/F2 使用通用尺寸包络 |
| PCB、固件和实物 | 本轮未修改或验证 |

Core 和 98 触点合同、原 13 路轮控制、J4/J6 六针线序、J8 调试及 J10 停机功能保留。
新增 31 路 GPIO 的机器表见 [m2_pin_assignment.csv](docs/m2_pin_assignment.csv)。
五伏外设预算 1.5A、三伏三外设预算 150mA、Core 外接三伏三预算 100mA；提高总保险不会扩大这些稳压支路。
TFT 的 H1/H2 是两个 2mm 孔，30mm 孔距为资料推算约束，仍须复核实物。

[M2 用料草表](docs/BOM_M2_SCHEMATIC_DRAFT.csv)已按当前原理图更新，不是生产发布 BOM。
旧 `docs/BOM.csv` 和初始生成清单保留为历史资料，不能替代当前原生原理图。
后续使用 KiCad 手工编辑；先保存基线，再导出原生网表/ERC，运行 `tools/verify_schematic.py`，导出 PDF 后运行 `tools/check_visual.py`。
