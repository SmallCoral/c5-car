# M2.5 视觉板供电、灰度与贴片按键验证

2026-11-06；KiCad 10.0.6。源码基于提交 `31b3e7e7154e511c48ea62b1ed1c7e3de3b91dca`，以下包含尚未提交的工作区修改；原生文件 SHA-256 见 [current_design.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/current_design.json)。本轮只处理原理图与元件库、文档及原生导出，不读取或改写 PCB、固件或机械总装。

## 实际修改

| 范围 | 当前电路 |
| --- | --- |
| SW2～SW5 | 6×6×4.3mm 四脚 SMD 通用封装；原低有效、10k／100nF 回路不变。焊脚跨度、触点分组和无品牌电气额定仍待实测；E-Switch 仅为几何参考，不作为实物品牌 |
| 灰度 | J20 改 CH1～CH8 八针；J21 改 GND、五伏两针。KEY/Err/CLK/DAT 不接，PD12/PD13 释放；当前 29 路新增、42 路总 GPIO，98 触点合同不变 |
| 荔枝派 4A | 独立 U8 五伏三安降压，U9 保护，J25 功率输出，J26 使能跳帽；J18 仅串口和状态检测 |
| MaixCAM2 | 改为图传端。U6 升为 AP63300 三安降压，增加可调反馈；U10 保护，J27 使能跳帽，J19 四线转接到机身六针 |
| 串口 | 荔枝派 RX 已有板内 1/2 分压，U12 驱动三伏三；板端 TX 一伏八由 U11 低阈值缓冲升为三伏三。Maix 两向经 U13，缓冲供电均取 Core 三伏三；Q3/Q4 根据模块上电状态控制输出使能，R79/R80 提供主控 TX 复位空闲上拉 |
| 主路 | F1 改为 0448015.MR、15A／85V Very Fast；F2 保留 0454004.MR、4A 延时，SW1 和双 MOS 栅极结构保留 |

完整线束和机身插座针序见 [M2 接线表](../reference/m2-interfaces.md)。J25 是两线供电端子，不包含 USB-C CC 控制器，需使用适合三安、正确处理 CC 的供电线送入板端 Type-C 入口。未把普通 GPIO 排针当功率入口。

## 计算和原厂核对

两片 [AP63300](https://www.diodes.com/datasheet/download/AP63300_AP63301.pdf)按原厂五伏推荐的 158k／30.1k、6.8µH、47pF 和输出两颗 22µF 配置，名义输出 4.9993V。L2/L3 改为 [SRN8040TA-6R8M](https://www.bourns.com/docs/product-datasheets/srn8040ta.pdf)，Irms／Isat 典型为 5.1A／5.6A；这是元件额定条件，不是本板温升实测。三安满载能力仍依赖实际 PCB 散热、压降及负载动态。

[TPS25947 SLVSFC9C，2026-05](https://www.ti.com/lit/ds/symlink/tps25947.pdf)的第 5～6 页核对引脚，7.3 节核对保护，末尾 RPW0010A 图核对封装。U9/U10 是 TPS259470LRPWR，可调过压关断，不采用约 5.74V 的固定钳位档。OVLO 用 49.9k／15k、0.1% 电阻；计入门限、电阻与输入漏电后约 5.1056～5.3047V，典型 5.192V。UVLO 典型上升 4.496V；其迟滞下降门限更低，不能把上升门限当断电门限。R67=953Ω、R72=1.5kΩ，按原厂公式 3334／R 计算典型限流 3.498A／2.223A，不是精确限流值或持续负载许可。4.7nF dVdt 给出约 11.75ms 名义五伏上升时间。过流进入主动限流，热故障锁断；过压解除可恢复，ITIMER 开路使用最短响应。

[荔枝派底板原理图](../摄像头与视觉板/SIPEED_TH1520_EXPAND_LPI4A_51602_SCH.pdf) PDF 第 17 页核对 J2500/J2501、UART1 分压及一伏八 TX；[官方外设说明](https://wiki.sipeed.com/hardware/zh/lichee/th1520/lpi4a/6_peripheral.html)与图一致。[SN74LV1T125](https://www.ti.com/lit/ds/symlink/sn74lv1t125.pdf) 8.3.1 节明确输入只有负向钳位，支持一伏八输入升三伏三；输出没有 Ioff 保证，不允许将指定 UART_RX 接成外部驱动输出。[SN74LVC2G125](https://www.ti.com/lit/ds/symlink/sn74lvc2g125.pdf)用于 Maix 两向，具有部分掉电 Ioff。Q3/Q4 采用 [BSS138BK](https://assets.nexperia.com/documents/data-sheet/BSS138BK.pdf)，选型有 2.5V 栅压下的导通额定，非仅凭阈值电压判断。

MaixCAM2 [官方图](../摄像头与视觉板/SCH_MaixCam2_379C.pdf)第 7 页 U56 是 ZX-MX1.25-6PWT；第 9 页显示六针、Type-C、扩展口五伏经零欧姆相连，第 10 页是充电电路。J27 拔下只关闭扩展板的供电与反向通路，不能切断内部电池，也不能隔开模块内部两只插座。普通带 VBUS 的 USB 调试前拔 J27。图内 U52 为 1.5A PTC，外部预留的 2A 工作加充电预算不扩大它的持续允许电流；需实物验证温度和充电叠加负载。

按两轮输入合计 4A、步进输入 3A，Core 五伏 0.75A、原外设五伏合计 2.5A、荔枝派五伏 3A，假设各降压效率 85%：

`Ibat = 4 + 3 + 5 × (0.75 + 2.5 + 3) / (Vbat × 0.85)`。

| 电池电压 | 设计预算输入电流 |
| --- | --- |
| 10V | 10.6765A |
| 12V | 10.0637A |
| 12.6V | 9.9178A |

预算没有把三伏三负载再加一次，也没有把步进相电流当电池电流。实际电机和 Linux 板工作电流未知。15A 主保险按厂商常用 75% 持续电流条件在 25°C 约为 11.25A，较预算仅余约 0.57A；高温降额、所有负载峰值与启动浪涌都需要验证。按每只 MOS 冷态 10mΩ 估算，两只总损耗约 2.28W，热态更高，不能由额定电流判断实板散热通过。448 为快断，不能沿用旧 454 的延时/I²t 结论；电池短路电流与保险丝分断能力必须匹配。

## 实际检查

| 检查 | 结果 |
| --- | --- |
| 原生网表 | 10 页、168 个网络，与保留基线加显式修订合同完全一致 |
| 原生 ERC | 0 错误、0 警告，无 ERC 排除 |
| 项目验证 | 1872 项通过，含核心 98 触点、全部旧电路保持、新增精确连接、原位号/UUID和库一致性 |
| PDF | 逐页渲染核查；导线文字交叠 0、文字交叠 0；新页电源框避开图签 |
| 封装库 | 48 个 SVG 原生导出成功，非 PCB DRC |
| 资源 | 164 个实体位号含两个机械孔，161 个有可解析模型；J1 缺准确卧式模型，11 个通用外形已明确声明 |
| 用料草表 | 162 个电气装配位号来自原生网表，增加精度字段；SW2～SW5 保留实物尺寸待核对提示 |

命令依次为 `kicad-cli sch export netlist --format kicadxml`、`kicad-cli sch erc --format json --severity-all --exit-code-violations`、`python -B -X utf8 tools/verify_schematic.py`、`kicad-cli sch export pdf` 和 `python -B -X utf8 tools/check_visual.py`；封装使用 `kicad-cli fp export svg`。都以当前 Motor 原理图／库为输入，未调用 PCB 命令。

首轮真实失败没有删掉：按键说明字段曾可见造成交叠；视觉电路首轮 ERC 为 1 错误、3 警告，后续完整网表比较检出了保护门限与使能支路的误连接，均已改正并重新导出。对应首轮 JSON 保存在各 `baseline-2026-11-06-*` 目录，当前结果见 [verification.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/verification.json)、[erc.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/erc.json)和 [visual_check.json](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/visual_check.json)。未以放宽排除规则获得通过。

## 实物与软件尚未验证

PCB 仍由用户绘制；本轮没有读取、修改、DRC 或检查 PCB 与新原理图的同步。荔枝派具体底板修订、Type-C 供电线 CC、电压和三安带载能力，Maix 工作加充电峰值、板内 PTC、USB 双源隔离，主路 MOS／保险丝／电池能力，视觉串口、识别、图传、机械安装与四键实物封装均需后续核对。当前输出是原理图草案及库资源，不是生产发布或整车运行验收。
