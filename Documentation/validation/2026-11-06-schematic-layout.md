# Motor M2.1 原理图整理记录

本页记录 M2.1 整理与主页补线的历史结果。随后按用户要求修改器件、电源、连接器与 0603，当前结果见 [M2.3 验证](2026-11-06-m23-power-connectors.md)。

2026-11-06，按用户要求应用 `schematic-design` skill，整理当前 Motor 的九页原理图：一张 A3 总览和八张 A4 功能页。工具为 KiCad CLI 10.0.6、Python 3.12.14、PyMuPDF 2.1。源码基于 `4cf36a2ce5a0d5bb6b469a9a82320fcfbcc38c1f` 加用户和本轮工作区修改。文档按固定版本 `mthines/agent-skills` 的 `docs update` 规则同步，只处理本轮状态变化。

## 整理内容

| 图页 | 本轮处理 |
| --- | --- |
| 总览 | 调整为 A3，将 TFT/四键、灰度/双摄、蓝牙/步进与 Core 的 31 路信号逐路实线连接；左右电机信号完整连接，MOTOR_RUN 实际分支至两路；入口电源至 Core、入口 VM 至外设电源实线连接 |
| 原入口电源、核心接口、左右电机 | 保留原器件坐标与电气连接，统一分区线宽与图签版本；核心接口的新增 GPIO 标签改为层次端口 |
| 外设电源 | 五伏、三伏三按两个模块分区；VM 端口直接落到供电线路，去除独立悬空的别名线段 |
| TFT 与按键 | 屏幕、按键分区；CS/RES 上拉改用实线接入对应信号；去耦并入电源线；四键共用清楚的上拉电源干线 |
| 灰度与双摄 | 灰度为一个模块，两路相机各为一个模块；接口与相邻去耦用实际电源、回地导线连接 |
| 蓝牙与步进 | 蓝牙电源选择用实线接至接口；STEP/DIR/nEN 偏置移近接口并实线连接，UART 和驱动电源成组排放 |

每页至多三个相关功能模块；框图总览例外。独立说明采用中文、默认字体、100mil（2.54mm）。31 路新增信号在 Core 和外设页各保留一个原标签 UUID，改为层次端口，不再通过全局标签绕过主页。上拉、下拉电阻附近原先重复的同名标签由实际导线替代。公共电源、地按 skill 允许使用同名网络标签。
TFT 两个 2mm 安装孔保留，30mm 孔距仍为机械图边距推算约束，定位前须复核实物。

## 电气与资源检查

| 项目 | 实际结果 |
| --- | --- |
| 整理前基线 ERC | 0 错误、0 警告 |
| 整理后原生 ERC | **0 错误、0 警告** |
| 全部原生网表对比 | **134 个网络的完整引脚分组一致**，包括未用触点，无增加、合并或断开 |
| 元件身份 | **176 个符号实例 UUID 全部保留**，位号、单元、引脚 UUID、属性值、封装和实例路径一致 |
| 符号缓存、项目与库表 | 九页缓存符号不变，`.kicad_pro`、本地符号库和库表与基线逐字节一致 |
| `tools/verify_schematic.py` | **1454 项通过，0 失败**，含 98 触点和新增引脚合同 |
| 主页可见连接 | 107 个层次引脚均有实际导线；31 路新增信号与左右电机共 45 对信号端点通过连续导线连接，独立于全局标签检查 |
| 九页自动版面检查 | 导线/文字交叠 0，文字相互交叠 0 |
| 人工图面检查 | 已逐页查看实际导出的 PDF 渲染 |
| 元件资源 | 116 个位号含两个机械孔；113 个位号绑定可解析模型；J1 准确卧式 XT60PW-M 模型仍缺，两个孔无实体模型 |
| PCB、DRC、实物和固件 | 本轮未读取或修改 PCB，未执行 DRC、实物或固件验证 |

本轮保留所有电气参数、接口定义和保护配置。未重新选型或修改生产 BOM。已有模型精度和实物边界沿用[此前 M2 记录](2026-11-06-m2-schematic.md)。

[当时完整验证](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-zdt-ttl/verification.json)、[当时整理对比](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-zdt-ttl/layout_verification.json)、[当时版面检查](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-zdt-ttl/visual_check.json)已冻结在后续 ZDT 修改前基线中。
修改前原理图、项目、库表、PDF、网表和 ERC 保存在 [layout 基线](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-layout/)。

第一轮自动版面检查发现屏幕电源端口与左侧坐标字母 A 重叠，已移动端口并重新验证；原始 `iteration1-visual.json` 保留在基线目录，最终结果为零交叠。另人工调整倒置 R54 的字段位置，避开符号本体。

用户指出主页模块缺少可见连线后，保存[补线前状态](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-06-layout/before-overview-wiring/)，将三个新增模块改为真实层次信号连接，并补全右电机连线。该轮首次 ERC 发现 Core 三伏三线尾未终止；将标签移到线尾后恢复 0 违规。首次扩充验证时，两页标签 UUID 断言未考虑重复偏置标签已由实际导线替代，已改为核对每个信号唯一端口保留的原标签 UUID，完整网表对比仍严格保留。没有添加 ERC 排除或隐藏真实电气失败。

## 执行与后续编辑

在 KiCad 10.0.6 下导出 Core/Motor 的 `kicadxml` 原生网表、运行 `sch erc --format json --severity-all --exit-code-violations`、导出原生 PDF，然后执行：

```powershell
python -B <Motor>/tools/verify_schematic.py
python -B <Motor>/tools/check_visual.py
```

`verify_m2.py` 保留电气合同检查，并加入 `verify_layout_edit.py` 的全网表、元件身份和主页实际导线连续性对比。纸张检查明确限定总览 A3、八张功能页 A4；原层次引脚 UUID 保留、父子页端口完全对应。旧的“电机页逐字节相同”断言改为“电路对象保持一致”，允许图签版本和图形样式整理；原检查脚本已存入本轮基线。

本轮只从保存的当前文件做增量坐标、导线和说明编辑，没有运行旧电路生成器。`tidy_schematics.py` 是本轮历史整理记录，默认拒绝执行；显式执行也会核对当前文件哈希，避免覆盖之后的手工修改。后续应直接使用 KiCad 编辑。

可编辑入口为[主原理图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch)，预览为[九页 PDF](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf)。
