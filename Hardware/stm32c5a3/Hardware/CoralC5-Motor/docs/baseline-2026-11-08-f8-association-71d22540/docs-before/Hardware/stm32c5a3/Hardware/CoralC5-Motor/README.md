# CoralC5-Motor

2026-11-07，M2.8。[主原理图](CoralC5-Motor.kicad_sch)、[十页 PDF](docs/CoralC5-Motor_Schematic.pdf)。主页 A2，电源/灰度识别/蓝牙步进/荔枝派四页 A3，其余五页 A4。

保留荔枝派 4A、MaixCAM2 和 Action 4。整车输入为 12V/12.5A 适配器，经原总开关与保护产生 VM。U1/U6 两路 AP64500SP-13 标称 5.088V、500kHz：A 轨 V5_EXT 预算 4.2A，供荔枝派、Core、灰度和 TFT/蓝牙 LDO；B 轨 V5_CAM 预算 4.0A，供 MaixCAM2 与 Action 4。5A 是单颗芯片额定，尚未证明板级能力。

荔枝派 J25 与 Action 4 J28 均为 USB4125-GF-A 六触点供电 Type-C，分别由 U8/U16 TPS25810 检测接入、通告 3A 并供固定五伏，无 PD/PPS 或 USB 数据。150µF 储能留在源开关 IN，输出各 10µF。荔枝派 J18 是原板针序 2×6：1 脚不供电，2 脚仅检测其三伏三，11/12 为 UART1 RX/TX；U11/U12 分别处理 1.8→3.3V 与 3.3V 路径，经 USART3/PD8/PD9 通信。应用分工和协议未实现。

Core 从 D3 后 A48/B48 取电，Core AP63203 产生三伏三；USB 隔离保留。TFT/HC-08 由独立 U7 AP2112K 三伏三供电，不与 Core 三伏三并联。Maix J19 保留六针同序、U10 保护、U14/Q5 防反灌及 J27。其连接器仅额定 1A，识别叠加充电必须实测；普通带 VBUS 的 USB 调试前拔 J27，模块电池仍可能供电。

J23 保留普通 XH8P，1/2 供电、7/8 TTL，3～6 不接。PD12/B30 高有效控电、默认断电，U15 断电隔离串口。J4/J6 六针轮电机、J10 停机、八路灰度、TFT 双孔及四个贴片键保留。固件未修改。

| 检查 | 本轮结果 |
| --- | --- |
| 原生 ERC | 0 错误、0 警告，KiCad 10.0.8 |
| 原理图、98 触点和资源 | 3786 项通过；205 个网络等于独立合同；40 路 GPIO |
| 十页 PDF | 逐页渲染与人工查看；导线/文字、文字/文字交叠各 0 |
| 资源 | 180 个实体位号，含两孔；177 个绑定模型，12 个为通用外形，J1 准确模型缺失 |
| PCB 同步 | 3815 项通过，原生一致性 0；新增 33 个器件待布局；原 151 段走线、3 过孔保留 |
| PCB DRC | 40 条违规、390 项未连接；没有完成 Edge.Cuts 外形、布线和散热设计 |
| 用料 | 178 个电气位号草表；新增/修订范围 22 种 SKU 有国内现货快照；旧料全表尚未重新审计 |
| 固件与实物 | 未修改固件，未进行实物电源、UART、稳球与整车验证 |

补偿高频电容 C63/C89 已由 39pF 改为 470pF，保留 15.8k+10nF 外部补偿，依据聚合物电容 ESR 和原厂公式估算，仍须测环路与负载阶跃。12V 输入连续预算约 11.09A；F1 15A 按 75% 降额仅余约 0.16A，启动与温升未验证。C81/C92 的 16SVP150M 为 NRND，L1/L2 仅有少量库存快照，不能据此发布生产 BOM。

见 [接线表](../../../../Documentation/reference/m2-interfaces.md)、[本轮验证](../../../../Documentation/validation/2026-11-07-m28-resume.md)、[用料草表](docs/BOM_M2_SCHEMATIC_DRAFT.csv)、[采购快照](docs/procurement_m28.json)、[交接](../../../../.agent/main/handoff.md)。`docs/BOM.csv` 同样是草表入口，不是生产清单。历史 M2～M2.7 报告保留。

禁止重跑旧生成器覆盖设计。更新网表/ERC 后运行 `tools/verify_schematic.py`；更新 PDF 后运行 `tools/check_visual.py`；PCB 原生 DRC 后运行 `tools/verify_pcb_sync.py`。`tools/update_m28_metadata.py` 只更新清单与状态，不编辑电路。
