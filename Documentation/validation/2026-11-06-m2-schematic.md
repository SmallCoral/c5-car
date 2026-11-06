# Motor M2 原理图修改与验证

日期为 2026-11-06，工具为 KiCad CLI 10.0.6、Python 3.12.14 和 PyMuPDF。源版本为 `4cf36a2ce5a0d5bb6b469a9a82320fcfbcc38c1f` 加当前工作区修改。
本次仅修改原理图、原理图库资源和对应文档，没有打开、读取或修改 PCB，也未修改 Core、Expansion、固件或机械总装。

## 实际修改

- J1 从 DC005 换成卧式板载 XT60PW-M，原输入正极经 F1，负极接地。这里替换的是电池输入；J24 才是新增步进驱动的受保护电源输出。
- 增加 HC-08 四针口与供电选择、TFT 八针口、四个低有效按键及 RC、八路灰度并行口与可选 CLK/DAT 口、两路相机模块电源和串口、外置步进驱动的 STEP/DIR/nEN、UART 和 VM 电源接口。
- TFT 加 H1/H2 两个独立机械孔，2.0 mm 非金属化钻孔，按模块同一短边标注中心距 30 mm。原理图上的符号位置不代表 PCB 孔位；后者由用户定位。
- 增加 U6 AP63205 五伏外设降压和 U7 AP2112K 三伏三外设电源。Core 的电源、左右轮功率与六针线序保留。
- 保留原有器件位号、UUID 和器件坐标。左右电机两页与基线逐字节相同。重排总览为九页 A4 层次图；新增信号用全局网络名，电源及原有轮控制用真实层次端口。

接口及全部 31 路新增 GPIO 见 [M2 接线表](../reference/m2-interfaces.md)。

## 98 触点与 X 标记

同时导出 Core 与 Motor 原生网表，对照共享 `interface.csv` 核查。Core J13 与 Motor J14 的同编号触点对应一致；有 MCU 管脚的触点均在 Core 原生网表中接到合同指定的 MCU 物理脚。
不能把合同中的历史 `standard` 列当成当前 GPIO；应使用 `gpio`、`net`、`mcu_pin`。

A13、A14、B45、B46 是真正的空脚。其他剩余 X 为这轮没有使用的触点；新分配的引脚已去除无连接标记并接入网络。当前 44 路功能 GPIO 为原电机 13 路加新增 31 路，另有两路 SWD 和八个电源/地触点；44 个触点保持未用。
避开 PA9 的 Core USB 检测电路，以及 USB、SWD、启动脚、Core 用户灯和用户键。

## 检查结果

| 检查 | 实际结果 |
| --- | --- |
| 修改前本轮原生 ERC | 0 错误、7 警告，主要为 J9 手工删除后留下的孤立标签 |
| 当前原生 ERC | **0 错误、0 警告**，未新增排除项 |
| `tools/verify_schematic.py` | **1228 项通过，0 失败** |
| 九页自动版面检查 | 导线/文字交叠 0，文字相互交叠 0 |
| 人工图面复查 | 已查看九页实际 PDF 渲染，检查标题、引脚、说明和安装孔 |
| 元件资源 | 116 个位号含两个机械孔；113 个绑定可解析模型 |
| 准确模型缺项 | J1 卧式 XT60PW-M 模型缺失，未用立式模型代替；H1/H2 为钻孔，无实体模型 |
| PCB、原理图/PCB 一致性、DRC | 本轮未执行 |
| 实物通信、温升、电流和装配 | 未验证 |

[98 触点逐项审计](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_motor_98_contact_audit.csv)、[验证 JSON](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)、[ERC](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json)、[版面报告](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json) 均保留原始结果和文件哈希。

修改前文件、原生网表、ERC、旧检查脚本和旧失败报告保存在 [本轮基线](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-m2/)。旧报告日期与本轮新导出的基线 ERC 不同，不能将旧“8 警告”当作本轮基线结果。
J9 没有恢复；废弃 AUX 标签改为实际分配的接口，未使用的 NRST 在 Motor 侧明确 NC。

## 执行命令

在项目根目录使用 KiCad CLI 与 Python 执行下列命令；输出均位于 Motor 的 `docs/`。示例省略安装路径，实际使用上述版本。

```powershell
kicad-cli sch export netlist --format kicadxml --output <Motor>/docs/core_netlist.xml <Core>/CoralC5-Core.kicad_sch
kicad-cli sch export netlist --format kicadxml --output <Motor>/docs/netlist.xml <Motor>/CoralC5-Motor.kicad_sch
kicad-cli sch erc --format json --severity-all --exit-code-violations --output <Motor>/docs/erc.json <Motor>/CoralC5-Motor.kicad_sch
kicad-cli sch export pdf --output <Motor>/docs/CoralC5-Motor_Schematic.pdf <Motor>/CoralC5-Motor.kicad_sch
python <Motor>/tools/verify_schematic.py
python <Motor>/tools/check_visual.py
```

`verify_schematic.py` 对 M2 调用 `verify_m2.py`，用真实修改前网表逐网核对既有电路，并验证新增接口、AF、98 触点、资源和 ERC。旧脚本对已经删除的 J9 的精确断言保留在基线中，没有伪装为本轮通过。
`extend_m2_schematic.py` 为本轮历史增量迁移，不是日常编辑入口；默认拒绝执行，显式重放也要求全部已保存原理图和资源哈希匹配，防止覆盖后续手工修改。

## 电气依据与边界

- [AP63205 DS41326 Rev3](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/AP63205_DS41326.pdf) 第 2 页 Figure 1、引脚说明及第 13～14 页外围要求：4.7 µH、10 µF 输入、两颗 22 µF 输出、100 nF 自举；固定五伏型号 FB 直接取输出。U6 EN 接受保护 VM，随总开关启停。
- [Diodes AP2112 DS39724 Rev2](https://www.diodes.com/datasheet/download/AP2112.pdf) 第 1～3 页：SOT25 引脚 1 VIN、2 GND、3 EN、4 NC、5 VOUT，输入输出均按推荐 1 µF X7R。芯片 600 mA 额定不是本板连续承诺；本设计三伏三预算 150 mA，对应约 0.255 W LDO 损耗。
- [中景园 ZJY180S0800TG01 Rev A](../中景园ZJY180S0800TG01技术资料/01-规格书与控制芯片手册/ZJY180S0800TG01.pdf) 第 5～8 页：35×56 mm，四孔图中选同一短边两孔，孔径 2.0 mm；按细小的 2.5000 mm 孔心边距计算间距 35−2×2.5＝30 mm，实物复核后定位。33 mm 是屏幕本体宽度，已更正本轮中间过程的误读。[放大证据](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/TFT_hole_offset_detail.png)保留该边距标注。供电不接五伏；背光已带 S8050 与电阻。
- [AMASS 原厂目录](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/AMASS_XT60PW.pdf) 第 15 页与[产品页](https://www.china-amass.com/biao/289.html)确认 XT60PW 为卧式板接系列；绑定 KiCad 对应卧式封装，针距 7.2 mm。采购与制板前仍须核对实际配对方向及壳体正负号。
- 灰度依据用户提供的厂家说明、辅助板源码与[原厂资料](../感为八路灰度资料/README.md)。并行输出和 DAT 用无上下拉输入，CLK/DAT 不是 UART，也没有加入 I²C 上拉。
- 只解析辅助板 EPRJ 的 SCH 记录，确认实际使用 U3 HT7533-1 和板内 3V3 逻辑；五伏应接整套组件五伏入口。未读取其中 PCB 记录。Core 仅 USB 供电、外设未上电时，固件须将相关接口保持高阻，避免信号脚反向供电。

外设五伏总预算 1.5 A，芯片额定 2 A；两相机具体版本、启动峰值和负载合计需要实测。三伏三外设预算 150 mA，Core 外接预算仍为 100 mA。蓝牙电源选择默认装 1—2 跳帽为三伏三，确认载板可用五伏后才可改 2—3。没有跳帽则该接口无电。

原 F1 四安保护保留。换 XT60 不提高整板额定；新增步进与摄像头后的上游总电流、保险丝浪涌和散热尚待确认。步进电机不直接接这些 GPIO，光耦型驱动板需匹配电平及有效极性。
本轮导出的 [M2 原理图用料草表](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/BOM_M2_SCHEMATIC_DRAFT.csv) 只对应已绘制电路，不是生产发布 BOM；旧 `BOM.csv` 为历史 M1 文件。
