# Coral-C5 扩展板 M2.8 交接

生成日期：2026-11-07，来自 Codex 会话。
工作目录：`C:/Users/sc/Documents/Dev/Hardware/c5-car`。
分支：`main`；HEAD：`cc8dcdb9731c375813c00e5a12133c461657d29e`。
本文件是交接时点的工作记录，优先于仍停留在 M2.7 的状态说明。

## 目标与停止位置

上一项工作是恢复荔枝派 4A，保留 MaixCAM2 和 Action 4，修改扩展板原理图并同步 PCB 元器件及网络。
验收应包括原生网表、ERC、完整连接意图与 98 触点核对、资源核查、PDF 检查，以及保留手工设计的 PCB 同步核查。
PCB 同步不包含自动布局、重新布线或制板发布。

最新指令是“你先停下来，你先写一份交接文档与提示词”。
已停止硬件修改，当前只编写交接资料。
**交接文件本身不授权恢复执行；收到后续“继续”或续接提示词后再开展硬件工作。**

## 当前状态

| 项目 | 交接时点的事实 |
| --- | --- |
| 已画入的草图 | 两颗 AP64500SP-13、荔枝派 Type-C 电源口和 UART1 电平处理；保留 MaixCAM2、Action 4 |
| 实体位号 | 原生原理图盘点 180 个，含 H1/H2 两个机械孔；本轮新增 33 个，未删除旧实体位号 |
| 图纸 | 10 页；主页 A2，电源/灰度/Maix/蓝牙步进/荔枝派相关页为 A3，其余为 A4，准确列表见盘点文件 |
| 原生网表 | 已重新导出 `docs/netlist.xml`，205 个网络；这不是完整意图核查通过的声明 |
| 原生 ERC | 已验证，KiCad 10.0.6，2026-11-07 18:56:00 报告为 0 错误、0 警告 |
| PDF | 已重新导出 10 页，并生成预览；自动检查导线/文字、文字/文字交叠各 0 项 |
| 人工视觉检查 | 本轮重点查看了电源页和荔枝派页；尚未完成最终逐页人工复查 |
| 核心板 | 已重新导出 `docs/core_netlist.xml`，本轮未修改 Core 设计 |
| 完整原理图检查 | **未完成**；M2.8 检查器还未编写，旧入口仍调用 M2.7 合同 |
| 采购与 BOM | **未完成**；若干关键新料已查到国内现货，U11 和部分精密阻容等还未锁定合规采购记录 |
| PCB | **本轮没有执行 PCB 写操作，也没有完成同步**；只读盘点仍为 147 个封装、151 段走线、3 个过孔 |
| 实物 | 未测试供电能力、启动、环路稳定性、温升或 UART；未改固件 |
| Git | 工作区有大量本轮之前的修改及未跟踪文件；本轮没有提交、推送或关机 |

### 基线与文件变化

原修改前基线在 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/baseline-2026-11-07-m28/`。
其中 `baseline.json` 保存文件 SHA256，`live-netlist.xml` 是修改前原生网表。
不能用 HEAD 或 M2.7 报告代替这份用户实时设计基线。

**PCB 在会话期间发生了外部文件变化，来源未追溯，不要覆盖。**
基线 PCB SHA256 为 `c9a4b21155a301aa942548f59f8c11a15b036c25f57c034c2b2f391327b55826`。
交接盘点的实时 PCB SHA256 为 `d2b2c448cc9de39c734538639e64e3276378b2782743a73f583cd638f4b7897d`。
两者不同，但本轮没有运行修改 PCB 的脚本或 UI 操作。
恢复工作前先保存接手时的实时 PCB 新基线，并比对变化；用户手工改动优先保留。

本轮直接修改了 Motor 的主页、02 电源、03 核心板接口、06 模块电源、08 灰度/Maix 页，以及本地符号库。
新增 `10_Lichee_Interface.kicad_sch`、AP64500 本地封装、2×6 排针封装及模型。
04 左轮、05 右轮、07 TFT/按键、09 蓝牙/步进四页与 M2.8 基线逐字节相同。
Git 显示这些文件有改动时，可能来自更早工作，不能认作本轮修改。

### 已知过期记录

- `docs/current_design.json`、`docs/verification.json`、Motor README、旧 BOM 和跨板文档仍主要描述 M2.7。
- `docs/pcb_sync_verification.json`、`docs/pcb-drc-m27.json` 是历史结果。
  其中曾记录 18 条 DRC、305 项未连接；它们不代表当前 PCB，更不代表 M2.8 同步结果。
- `docs/m28_revision.json` 是初稿修改记录，尚未完成独立连接合同。
  其中 `paper["02_Power.kicad_sch"]` 仍为 A4，实际图纸已改为 A3；后续修正也未全部整理进该记录。
- `docs/m2_pin_assignment.csv` 尚未增加 LICHEE_TX/RX 两行。
  旧分配共 38 路 GPIO，本轮图纸拟增加为 40 路，须重新完整核查。
- `.scratch/m28/handoff-inventory.json` 是交接时的只读盘点，含文件哈希、页数、位号和 PCB 缺件清单。

## 下一步

以下步骤仅在明确恢复任务后执行。

1. 读取根 `AGENTS.md` 和本交接；确认实时设计相对交接盘点有无新增编辑。
   保存新的只读基线，特别是当前 PCB；保留旧 M2.8 基线。
2. 核对已经画入的 AP64500、TPS25810 和荔枝派 UART 电路是否满足原厂条件。
   补完下文列出的采购和电气未验证项，不把 ERC 当成电路设计证明。
3. 整理 `m28_revision.json`，明确每个旧网络的保留、拆分和新引脚意图。
   从基线原生网表建立预期连接集合，再与当前原生网表比较；不能把当前网表反抄成预期结果。
4. 新增 `tools/verify_m28.py`，在 `tools/verify_schematic.py` 中先分派 M2.8，再保留 M2.7/更早检查入口。
   覆盖全部网络分区、98 触点、40 路 GPIO、AF、保留位号/UUID、资源、孔位、说明字号与 ERC。
   更新 GPIO 表，处理真实失败，不删旧检查来凑通过。
5. 完成最终逐页 PDF 视觉检查、封装/引脚/3D 模型核查和采购记录。
   再更新草案 BOM、元数据和说明；不能使用“生产 BOM”或“已验证可用”的表述。
6. 以**最新实时 PCB**为输入同步元器件、封装、原理图关联路径和焊盘网络。
   新增 33 个器件放在板外待布局区；U1 更换封装，保留 U1 位号和器件 UUID。
   保留未受影响封装的位置/角度/UUID，以及用户走线、过孔、板框和约束。
   新的 5V 分路和 U1 换封装会要求后续重新布线，不能沿用旧铜线并宣称供电已完成。
7. 独立运行 PCB 同步核查和带原理图一致性检查的原生 DRC。
   报告真实违规、未连接项、保留对象及新增器件；没有收到授权时不替用户做布局布线。
8. 更新本文列出的旧状态说明，交付文件和检查结果。
   结束本地任务即可；本轮没有 GitHub 推送或关机授权。

## 关键文件

本节工程路径均相对仓库根目录。
Motor 工程前缀为 `Hardware/stm32c5a3/Hardware/CoralC5-Motor/`，下表以 `Motor/` 简写该前缀。

| 路径 | 用途或当前变化 |
| --- | --- |
| `AGENTS.md` | 用户手工设计保护、采购、验证和文档约定 |
| `Motor/CoralC5-Motor.kicad_sch` | 主页新增真实荔枝派图页、PD8/PD9 信号和两路电源层次连接 |
| `Motor/02_Power.kicad_sch` | 保留原输入/MOS/开关保护；下部改成两个 AP64500 电源模块 |
| `Motor/03_Core_Interface.kicad_sch` | 去掉 B26/B27 的 NC，接出 LICHEE_TX/RX；未改 J14 多单元符号本体 |
| `Motor/06_Module_Power.kicad_sch` | Action 4 改接 V5_CAM；TFT/BT LDO 继续接 V5_EXT |
| `Motor/08_Grayscale_Cameras.kicad_sch` | 灰度留在 V5_EXT，Maix 原保护支路改接 V5_CAM；保留原六针、保护及 UART |
| `Motor/10_Lichee_Interface.kicad_sch` | 新页：J25 Type-C 电源、J18 2×6、U11/U12 电平缓冲、Q3 掉电使能 |
| `Motor/Motor.kicad_sym` | 新增 AP64500、荔枝派双排针和两个缓冲器图形；旧定义意图上保留，尚需正式比较 |
| `Motor/Motor.pretty/Diodes_SO8EP_AP64500.kicad_mod` | DS41979 第 25 页推荐焊盘，本地模型为通用 SOIC 外形 |
| `Motor/Motor.pretty/Connector_PinHeader_2.54mm__PinHeader_2x06_P2.54mm_Vertical.kicad_mod` | J18 对应 2×6 直排针，配套模型已复制到 Models |
| `Motor/CoralC5-Motor.kicad_pcb` | 尚未同步 M2.8；以最新实时文件为准，不覆盖 |
| `Motor/docs/m28_revision.json` | 初稿变更、AP64500 逻辑分组和预算；非最终验证报告 |
| `Motor/docs/baseline-2026-11-07-m28/` | 修改前实时设计快照，含哈希和原生网表 |
| `Motor/docs/netlist.xml`、`core_netlist.xml` | 本轮最新 Motor/Core 原生网表 |
| `Motor/docs/erc.json`、`visual_check.json` | 本轮最终 ERC 与 PDF 自动交叠检查 |
| `Motor/docs/erc-m28-first.json` | 初次 ERC 的真实失败记录，不能删除冒充一次通过 |
| `Motor/docs/CoralC5-Motor_Schematic.pdf`、`previews/page-1.png` 至 `page-10.png` | 本轮最新导出与逐页预览 |
| `Motor/tools/verify_m27.py` | 可以复用原生网表分区、资源及 Core 合同检查方法，不可直接当 M2.8 合同 |
| `Motor/tools/verify_pcb_sync.py` | 仍硬编码 M2.7；M2.8 同步时需增量扩展或另建分派 |
| `Motor/tools/schematic_primitives.py`、`../Shared/design_tools/sexpr.py` | 局部编辑工具；不要运行旧整板生成器 |
| `Hardware/stm32c5a3/Hardware/Shared/interface.csv` | 98 触点合同，不因恢复荔枝派而擅改触点定义 |
| `Documentation/reference/motor-interface.md` | 两个行走电机六针线序依据 |
| `Documentation/摄像头与视觉板/SIPEED_TH1520_EXPAND_LPI4A_51602_SCH.pdf` | 荔枝派载板原理图，第 17 页 GPIO、第四页输入电源 |
| `.scratch/m28/edit.py` | 已执行的一次性局部编辑脚本，有防重复断言；不要重跑 |
| `.scratch/m28/correct.py`、`fix2.py` | 已执行的布线/版式修正脚本，也不能重复执行；以当前图纸为准 |
| `.scratch/m28/handoff_inventory.py`、`handoff-inventory.json` | 交接盘点脚本和读取得到的事实；不修改硬件 |
| `.scratch/m28/stock.py`、`map.py`、`research.py`、`lookup.py` | 公共采购页面研究辅助，部分查询失败；不得视为完整采购核查 |

## 决策、接线与注意事项

### 已确认的需求边界

- 三块模块均保留：MaixCAM2、Action 4、荔枝派 4A。
  当前保留的职责为 MaixCAM2 识别、Action 4 图传；恢复荔枝派后具体软件分工没有进一步确认，别自行宣布识别已迁移。
- 整车电源是 **12V / 12.5A 适配器**，此前 12V / 2.5A 是已纠正口误。
- 荔枝派由扩展板供电，使用 6 触点 Type-C 母座、C 转 C 供电线，另接双排串口排针。
- TFT、HC-08 使用排母；其他接口尽量普通 XH，不能换成带锁扣的 XHB。
  跳线保留普通排针/跳帽。HC-08 固定 3V3，不恢复 5V 或供电选择跳线。
- 四个按键为 6×6×4.3mm SMD 四脚，先用通用封装并标注实物焊脚待核对。
  TFT 保留 H1/H2 两个安装孔，孔距仍须实物确认。
- 步进是 ZDT_XS 第二代 TTL 一体闭环套件；J23 普通 XH 8P，1/2/7/8 使用、3～6 NC。
  PD12/B30 控电，硬件默认断电。不得恢复已经取消的 J24/J22 接口。
- 灰度为感为八路配 MSPM0L1306 辅助板：J20 CH1～CH8，J21 GND、+5V。
  KEY、Err、CLK、DAT 不接；读取 GPIO 不启用上下拉。

### 两路 5V 的当前草案

| 支路 | 芯片与网络 | 负载预算 |
| --- | --- | --- |
| A | U1 AP64500SP-13，V5_EXT | 荔枝派 3.0A + Core 0.75A + 灰度 0.30A + TFT/BT LDO 输入 0.15A = 4.2A |
| B | U6 AP64500SP-13，V5_CAM | Action 4 3.0A + Maix 1.0A = 4.0A |

每颗芯片额定 5A、输入 3.8～40V，8 个外露脚，脚距 1.27mm；另有必须接地焊接的 EP 散热焊盘。
相对原 U1 TPS56837 的细小底部焊盘，外部引脚更便于处理；**仍需要可靠焊接散热焊盘，不能承诺烙铁即可完成全部焊接**。
现有 SMAJ15A 约 24V 钳位不会直接超过新芯片 40V 工作上限，但浪涌与布局仍需核实。

反馈采用 53.6kΩ/10kΩ、0.1%，按 0.8V 基准标称为 **5.088V**。
RT=200kΩ，目标 500kHz；电感沿用 3.3µH/12A 的 744325330。
每路输入保留 10µF/50V、22µF/25V 和 100nF；输出保留 22µF/25V，并使用 USB 源端入口的 150µF 储能电容。
A 的聚合物电容为 J25 支路的 C92，B 为 J28 支路已有的 C81，两者都接在对应原始 5V 轨。

**AP64500 需要外部补偿，不能把 COMP 接地。**
当前每路为 COMP 串联 15.8kΩ + 10nF 到地，另有 39pF COMP 到地。
分别是 A 的 R67/C91/C63，B 的 R66/C90/C89。
这些值参考 DS41979 第 16～21 页公式，按聚合物加 MLCC 总有效电容约 180µF 做初步估算。
相对原厂 45µF 示例，交越频率被降低；10nF 是本轮使用的标准值，**还没有完成环路裕量、负载阶跃或真实有效电容核查**。
后续须记录假设、计算和可接受负载条件，不能复制原厂 2.7nF 后声称验证过。

整车预算按左右轮输入合计 4A、步进输入 3A、两路合计 8.2A、效率假设 85%：
`Iin = 4 + 3 + 5.088 × 8.2 / (12 × 0.85) ≈ 11.090A`，约 133.1W。
这是设计假设，不是实测峰值。
F1 15A 快断的 75% 连续降额为 11.25A，与预算很接近，不能把 150W 适配器或 5A 芯片额定值当成整板热验证。
启动、堵转、充电、保险分断、双 MOS 温升和线束压降均未实测。

Core 继续从 D3 后 V5_SYS，经 J14 A48/B48 供电；Core 自带 AP63203 产生 Core 3V3。
D3 方向为原始 A 轨到 Core，避免 Core USB 反供外设。
U7 AP2112K-3.3 继续从 A 轨给 TFT/BT，不能把两个 3V3 电源并联。
Core 3V3 对外合计预算仍为 100mA，新增缓冲器的电流应计入。

### 荔枝派 J18 / J25

J25 为与原 Action 4 相同的 USB4125-GF-A 6 触点 Type-C 母座。
U8 采用 TPS25810RVCR，提供固定 5V、3A 通告、接入检测和限流，不是 PD 升压输出。
本页 USB 电路是现有 U16/J28 电路的局部复制，未引入另一颗降压。
J28/U16 的 Action 4 电源电路继续保留，改从 B 轨取电。

荔枝派载板原图中的 J2500 在左、J2501 在右，本轮仅覆盖前六行。
以原图排列定义奇数脚左列、偶数脚右列；实物 1 脚朝向和线束仍须核对。

| J18 引脚 | 处理 |
| --- | --- |
| 1 | 对应荔枝派 5V，NC；只通过 J25 Type-C 供电 |
| 2 | 对应荔枝派 3V3，仅给 Q3 作上电状态检测，不接 Core 3V3 |
| 3、4 | GND |
| 5～10 | SPI/串口控制台位置，全部 NC |
| 11 | 原板 UART1_RX 的 3.3V 输入，通过 U12 接 MCU PD8 / USART3_TX |
| 12 | 原板 UART1_TX 的 1.8V 输出，通过 U11 升到 Core 3V3，接 PD9 / USART3_RX |

PD8 对应 J14:B26，PD9 对应 J14:B27；拟用 USART3、AF7，须对照本项目 C5A3 数据手册重新跑 AF 检查。
荔枝派 UART1 默认 `/dev/ttyS1`；UART0 是控制台，本轮不使用。
原板 UART1_RX 排针后有 2.2k/2.2k 分压，可接 3.3V；不要先转换成 1.8V 再送入该排针，否则再次分压会变成约 0.9V。

U12=SN74LVC1G125DBVR，3V3 供电，将 MCU TX 以 3.3V 缓冲到 J18:11，并具 Ioff。
U11=SN74LV1T125DBVR，3V3 供电，低阈值输入用于接收 J18:12 的 1.8V TX。
U11 数据手册说明输入无正向 VCC 钳位；其输出只连接同一 Core 电源域的 MCU 输入，不能额外对输出施加电压或凭空宣称其具 Ioff。
两芯片 1=nOE、2=A、3=GND、4=Y、5=VCC，各保留一个 100nF 去耦。
Q3 BSS138BK 用荔枝派 3V3 拉低共同 nOE；R74=10k 上拉，R75=100k 栅极下拉。
荔枝派断电时两路输出高阻；R73/R79=100k 将 Core 侧 UART 默认保持空闲高。

最初绘制的 J18 接地线穿过未使用行，随后已移到排针外侧，并移动接地符号。
控制 nOE 标签也曾悬空，已修正。
最终 ERC 为 0/0，但仍要用完整网表合同确认 1、5～10 的 NC 各自独立，3/4 只接地，2 不接 Core 3V3。

### 保留的 MaixCAM2 保护

J19 为 ZX-MX1.25-6PWT，同模块线序：1 地、2 保护后 5V、3/4 NC、5 模块 TX、6 模块 RX。
U10 SGM2531C、U14 LTC4412、Q5 反灌保护及 J27 跳帽继续保留；本轮仅切换其输入原始电源到 B 轨。
六针座额定 1A，R72=95.3k 使限流典型约 0.9986A；Maix 识别叠加电池充电可能触发限流或掉电，仍须实测。
Maix 内有小电池，普通供电 USB 调试前仍须拔 J27，电池也可能保持模块上电。

### U1 换封装的身份与网络映射

保留 U1 器件 UUID，但不能按原来针号继承网络。
需复核的旧 TPS56837 引脚到新 AP64500 引脚映射如下。

| 原引脚/功能 | 新引脚/功能 |
| --- | --- |
| 1 EN | 3 EN |
| 2 FB | 5 FB |
| 3 AGND | 7 GND |
| 6 SW | 8 SW |
| 7 BOOT | 1 BST |
| 8 VIN | 2 VIN |
| 9 PGND | 9 EP_GND |
| 10 MODE | 4 RT/CLK，配置功能已改变 |

原 4 PG、5 SS 移除；新 6 COMP 是新功能，不复用无关引脚身份。
旧 QFN VIN 为多个同号铜焊盘，新 SO-8EP 为不同几何，不能承诺全部旧焊盘 UUID 一一保留。
未更换封装的其他器件应保留全部旧焊盘 UUID。
AP64500 精确推荐焊盘为引脚中心 x=±2.4975mm，y=±1.905/±0.635mm，焊盘 1.505×0.802mm；EP=2.613×3.502mm。
本地封装用了分窗锡膏，模型是现有通用 SOIC 外形，不能标成原厂精确 3D。

### PCB 同步的边界

原理图新增而 PCB 尚缺少的 33 个实体位号如下。

```text
C74 C75 C84 C85 C86 C87 C88 C89 C90 C91 C92 C93 C94
J18 J25 L2 Q3
R61 R62 R63 R64 R65 R66 R67 R73 R74 R75 R79 R93
U6 U8 U11 U12
```

PCB 原有 147 个位号均仍在新原理图中；交接盘点没有发现多余位号。
U1 封装需从旧 QFN 改成本地 SO-8EP；其他旧器件的值、网络和原理图路径也要同步。
5V 原轨拆分为 A/B 后，原铜线不能全部自动认定属于新轨，尤其 Maix/Action4 的旧 V5_EXT 走线。
走线几何应保留并如实报告需要重布之处，不可为了 DRC 好看而删用户铜线。
KiCad 10 本项目 PCB 焊盘和走线的 `net` 字段使用网络名，不能直接套用老式整数网络编号编辑脚本。

### 原厂依据和采购状态

电气设计依据是原厂资料；商城页面只用来证明料号和供货。
下表为 2026-11-07 查询快照，数量不是预留库存。
本轮所有新增电路仍属草案，以下表格不是可直接下单的最终 BOM。

| 完整料号 | 立创编号 | 国内商品页 | 已记录库存 |
| --- | --- | --- | ---: |
| AP64500SP-13 | C2070920 | [商品页](https://item.szlcsc.com/2162503.html) | 1814 |
| SN74LVC1G125DBVR | C23654 | [商品页](https://item.szlcsc.com/24390.html) | 151067 |
| 744325330 | C5129929 | [商品页](https://item.szlcsc.com/5735814.html) | 15 |
| GRM32ER71E226KE15L | C21397 | [商品页](https://item.szlcsc.com/22114.html) | 52306 |
| 16SVP150M | C178346 | [商品页](https://item.szlcsc.com/189739.html) | 62 |
| ARG03BTC5362 | C2684559 | [商品页](https://item.szlcsc.com/2782693.html) | 3975 |
| 0603WAF1582T5E | C22880 | [商品页](https://item.szlcsc.com/23607.html) | 8260 |
| 0603WAF2003T5E | C25811 | [商品页](https://item.szlcsc.com/26554.html) | 1063703 |
| CC0603JRNPO9BN390 | C107049 | [商品页](https://item.szlcsc.com/108265.html) | 430856 |
| CC0603KRX7R9BB103 | C100042 | [商品页](https://item.szlcsc.com/101256.html) | 579841 |

多数快照原始 HTML 和 `C编号-stock.json` 在 `Motor/docs/sources/m28/`。
AP64500 的快照在 `.scratch/u1-alternatives/C2070920.json`；商品页与日期已记录，需归档进最终采购文件。
150µF 的 16SVP150M 为 NRND，虽有现货，也应说明长期供货限制。

**未完成项：U11 SN74LV1T125DBVR（C473338）的独立国内商品页和实时现货记录未闭环。**
[SN74LV1T34 的国内推荐列表](https://item.szlcsc.com/101238.html) 曾列出 TI SN74LV1T125DBVR 现货，但这不代替完整的直接商品页记录。
不要把国际站库存、TP 后缀替代品或同名其他封装当成本项目准确型号的采购证明。
10kΩ/0.1% 的反馈下阻及部分通用新增阻容/连接器也还未逐项绑定新采购 SKU。
RT0603BRD0753K6L、RT0603BRD0720KL 和 CC0603JRX7R9BB123 的本轮查询无可直接购买现货，不得当成已锁定料。
国内库存无法确认的器件只列候选；应补核或更换已核实替代品，再更新电路与草案 BOM。

主要原厂资料：

- [AP64500 原厂数据手册](https://www.diodes.com/datasheet/download/AP64500.pdf)，DS41979 Rev5-2，2024-12，26 页。
  已取得同一原厂 PDF 的立创镜像，保存在 `Motor/docs/sources/m28/AP64500_DS41979.pdf` 及文本。
  引脚见第 3 页，补偿见第 16～21 页，推荐焊盘见第 25 页；应按 PDF 实际页码再定位。
- [SN74LV1T125](https://www.ti.com/lit/ds/symlink/sn74lv1t125.pdf)，Rev D；[SN74LVC1G125](https://www.ti.com/lit/ds/symlink/sn74lvc1g125.pdf)，Rev U。
- [TPS25810](https://www.ti.com/lit/ds/symlink/tps25810.pdf)，相关本地资料在 `Motor/docs/sources/m27/` 与 `.scratch/m27/`。
- [荔枝派供电 FAQ](https://wiki.sipeed.com/hardware/zh/lichee/th1520/lpi4a/12_faq.html)；[外设与串口说明](https://wiki.sipeed.com/hardware/zh/lichee/th1520/lpi4a/6_peripheral.html)。

### 操作约束

- 不使用 `computer-use`、CUA 或其他 UI 自动化；用户明确要求直接编辑文件。
- 不重跑 `build_schematic.py`、`extend_m2_schematic.py` 等整板生成器。
  `.scratch/m28/edit.py` 已运行，有防重复保护；`correct.py`/`fix2.py` 会重复平移对象，也不能重跑。
- S-expression 新增语法标签要使用 `Atom`，不要把 `justify` 等标签写成带引号的字符串。
  本轮曾修复这类解析错误；每次改后都要用 KiCad 原生加载验证。
- 不执行 `git reset`、清理未跟踪文件、改 `.history` 或覆盖旧基线。
- 不修改旧项目 `D:/dev/qrs/stm32/Coral-C5`。
- 不把原理图资源、ERC、PCB 一致性、DRC、实物测试混为一个通过结果。
- 新型号未完成供货和电路核查前，只能标记草案，不能宣布已实现或填入生产 BOM。

## 如何运行和核查

以下命令供恢复任务后使用；编写交接时未再次执行硬件修改或 PCB 同步。
PowerShell 路径全部加引号，避免把命令文本当 shell 字符串再嵌套执行。

```powershell
Set-Location 'C:/Users/sc/Documents/Dev/Hardware/c5-car'
$motorDir = Join-Path (Get-Location) 'Hardware/stm32c5a3/Hardware/CoralC5-Motor'
$coreDir = Join-Path (Get-Location) 'Hardware/stm32c5a3/Hardware/CoralC5-Core'
$kicadCli = 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe'
$taskPython = 'C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'

& $kicadCli sch export netlist --format kicadxml -o "$motorDir/docs/netlist.xml" "$motorDir/CoralC5-Motor.kicad_sch"
& $kicadCli sch export netlist --format kicadxml -o "$motorDir/docs/core_netlist.xml" "$coreDir/CoralC5-Core.kicad_sch"
& $kicadCli sch erc --format json --severity-all --exit-code-violations -o "$motorDir/docs/erc.json" "$motorDir/CoralC5-Motor.kicad_sch"
& $kicadCli sch export pdf -o "$motorDir/docs/CoralC5-Motor_Schematic.pdf" "$motorDir/CoralC5-Motor.kicad_sch"

# 先扩展 M2.8 合同；目前此入口仍进入旧 M2.7，不能期待通过。
& $taskPython -B -X utf8 "$motorDir/tools/verify_schematic.py"

# pymupdf 已在本机临时目录中，使用时重新解析绝对路径。
$env:PYTHONPATH = (Resolve-Path '.scratch/m2/pdf-deps').Path
& $taskPython -B -X utf8 "$motorDir/tools/check_visual.py"

# 完成 PCB 同步后才生成 M2.8 的独立 DRC 报告。
& $kicadCli pcb drc --format json --severity-all --schematic-parity --exit-code-violations -o "$motorDir/docs/pcb-drc-m28.json" "$motorDir/CoralC5-Motor.kicad_pcb"
```

若需要 pcbnew，使用 `C:/Program Files/KiCad/10.0/bin/python.exe`。
本轮没有编写或执行 M2.8 的 PCB 同步程序。
交接盘点 PDF SHA256 为 `a2940d71657a652005aa5394a8796b22a15e8070f7d7d0de539e977061649bc8`。
原生 ERC 和 PDF 检查只证明对应报告当时的文件状态；恢复后有编辑必须重新导出和核查。
