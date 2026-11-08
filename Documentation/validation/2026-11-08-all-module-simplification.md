# 2026-11-08 外设模块精简实施与复核

本轮按用户确认的范围实施：所有外设接口删除重复的模块级通断、隔离和防反灌外围；12V总输入、主电源保护、两路AP64500、Type-C必要CC/源端功能、电机驱动本身及荔枝派电平转换保留。修改前基线见 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-08-all-module-simplification-before/`。

## 本地续接：原理图清理与最小同步核对（2026-11-08）

按用户最新范围仅清理原理图，不做PCB布局、布线或其他板级优化。采用用户指定 `SmallCoral/schematic-design-skill` 的原理图规则；原理图阶段和随后独立的PCB同步核对分开处理。

- 当前143个实体器件的简化在续接前已存在，不重复删除或扩展功能。本次保存新基线 `docs/baseline-2026-11-08-schematic-only-resume/`，定点清理03、08、09、10页的无功能残线、孤立标签/虚拟电源符号、重复标签，并删除主图对应的废弃步进三伏三接口枝线；修正Maix/步进的过时说明。保留实体元件位号、UUID、属性与位置。
- KiCad CLI 10.0.6 重新导出 `docs/schematic-only-resume/netlist-verified.xml` 和 `erc-verified.json`。143实体、184网络；清理前后逐引脚网络名和网络成员分组完全相同。ERC从0错误61警告降到0错误3警告，悬空线端点和GND局部/全局同名告警已消除；剩余U10/U11/U12库差异告警保留，不强刷缓存符号。
- PCB143/143元件按UUID唯一匹配，元件缺失、多余、引脚缺焊盘及焊盘网络失配均0。当前PCB已经符合新网表，因此本次没有写入PCB；176段走线及3过孔、布局和全部板文件字节不变。PCB SHA256仍为 `082e1684afc962443f22c9c8033ed88bec1da71a601a17d3e1ee27632f0896de`。
- 仅用KiCad只读连通性接口检查实际铜连接，仍有303项未布连接，分布于81个网络；它们不是同步漏网/错网，保持不动。完整网络与焊盘组见 `docs/schematic-only-resume/connectivity-result.json`。未运行DRC，未做实物、固件或机械验证，也未提交/推送。

下面为续接前实施的历史结果；旧ERC/PDF/DRC不代替上述本次检查。

## 已实施

- MaixCAM2：保留 U10 SGM2531C、R72 限流设定、输入/输出去耦；删除 U14/Q5 理想二极管、U13/Q4 串口隔离、J27、独立分压和对应外围。ENUV 接 V5_CAM，OVP 接 GND，SS 悬空采用默认斜率；J19:5/6 分别直连 VIDEO_RX/VIDEO_TX。
- 步进：删除 U15、Q6～Q9、D5、F2、R81～R90、C80 和 STEP_PWR_EN。J23:1 接 VM，2 接 GND，7/8 直连 USART1，3～6 NC；J14:B30/PD12 释放，GPIO 由40路降为39路。
- 荔枝派：保留 U8 Type-C 源端、U11/U12 1.8V/3.3V 电平转换；删除 Q3、R74/R75 感知通断，U11/U12 的 OE 直接拉低常使能，J18:2 保持 NC。
- PCB：删除上述 37 个器件封装，保留其余封装 UUID、坐标、角度、176 段既有走线和3个过孔；按新原生网表更新保留焊盘网络和受影响铜线网络名。

## 独立功能复核

使用 KiCad CLI 10.0.6 导出 `docs/simplification-netlist-test.xml`，143 个原理图实体、184 个网络。独立合同检查 `docs/simplification-connectivity-audit.json` 通过：Maix 输入/ENUV、限流输出、UART；步进 VM/UART/NC；荔枝派电平转换与常使能；删除位号均不在网表；PCB 143/143 封装按 UUID 唯一匹配，预计新增0，焊盘网络不匹配0。

原生 PCB DRC `docs/simplification-pcb-drc.json`：原理图一致性0，18条既有违规，303项未连接；仍无板框、地铜和完整布线，不能制板。违规与未连接是布线/机械边界问题，不能由同步通过替代。

原生 ERC `docs/simplification-erc.json`：21错误、106警告。错误主要来自删除后残留的层次页线段、标签和新 NC 标记清理不足；3个符号库差异警告沿用 U10/U11/U12 缓存符号现状。需在 KiCad 中清理残线并重新运行 ERC，不能把本轮网表功能检查通过当作 ERC 通过。

原理图 PDF 已导出为 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/simplification-schematic.pdf`；本机 KiCad Python 未提供 PyMuPDF，未运行旧 PDF 交叠脚本。固件、机械、实物电源、UART、温升和整车功能仍未验证。
