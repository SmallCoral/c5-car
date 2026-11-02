# Coral-C5 双板接口

这份说明对应 B1 硬件和 C5-EDGE-3 自定义针序。2026 年 10 月 22 日删除 Core 的 R45 至 R48，PC14、PC15 改为直接引出，PH0、PH1 仅接 24MHz HSE。Core 使用 STM32C5A3VGT6，Expansion 接网口、无线和存储。旧单板与清理前的完整快照放在项目同级的 Coral-C5-archive，当前工程保留两套 B1 设计。

## 从 Streamline 沿用的部分

公共资源取自 [CurrentMakers 官方 KiCad 库](https://github.com/currentmakers/streamline_kicad_libraries/tree/27dc9a9413d3e3f1fc1cdf63371ded3b01293c0f)，固定在 `27dc9a9413d3e3f1fc1cdf63371ded3b01293c0f`。用于复核的板框和针位源文件、MIT 许可证及来源记录放在 [Streamline](Streamline) 目录中，实际使用的封装与模型随两块板分别保存。摆放参考了 [STM32H562 核心板](https://stm32world.com/wiki/Streamline_MCU_STM32H562) 的公开照片，接口同时对照 [Streamline 公共定义](https://stm32world.com/wiki/Streamline)。

| 项目 | 当前尺寸或约定 |
| --- | --- |
| Core 完整外框 | 70.00×51.45mm，直接沿用 SL_BOARD 的闭合轮廓 |
| Core 安装孔 | 两个 3.00mm NPTH，中心间距 64.00mm，距上边和侧边各 3.00mm |
| 板厚与层数 | 两板名义板厚均为 1.60mm；Core 为双层，Expansion 仍为四层，插槽配合公差待制板前确认 |
| 金手指 | A、B 各 49 个触点，1.00mm 间距，单触点 0.70×4.30mm |
| 接触面 | A 面在背面 B.Cu，B 面在正面 F.Cu |
| 防呆缺口 | 位于 11 与 12 号触点之间，宽 1.90mm，沿用原模板的圆顶缺口 |
| 母座 | XKB X1611WVS-98-9TV01，C21263085，采用官方符号映射的 3133 封装和随附 STEP |
| 母座附加焊盘 | 50、51 为固定金属焊盘，均接地，另有两个定位 NPTH |
| Expansion 外框 | 本项目暂定 100.00×86.00mm，四个 3.00mm 安装孔 |

Core 的 J13 封装自带公共板框和金手指，Expansion 的 J14 使用对应母座封装，已放进两套工程的本地库，便于继续复用。Expansion 外框没有冒充 Streamline 的统一底板尺寸。金手指镀层、倒角角度和深度、接触区阻焊以及具体连接器公差还需要在制板前按厂商图纸确认，这次没有生成 Gerber。

## 电气约定

完整表以 [interface.csv](interface.csv) 为准，两块板的原理图、PCB 和表格已经逐触点核对。C5-EDGE-3 按 MCU 当前 −45° 摆放的实际焊盘坐标排序，以器件正面俯视、金手指在下方为基准，各排沿同一视图从左到右排列。下半的 43 路 GPIO 接正面 B2–B44；上半的 41 路 GPIO、NRST 和 VREF_MCU 接背面 A2–A46，其中 A13、A14 空置。电源、地及 VCAP 不参加信号排序。

Streamline 只沿用机械板形与触点几何，信号针序已经自定义，必须配套使用同步更新后的 Expansion。interface.csv 的 standard 列保留历史规范用于对照，net 列才是当前实际网络，copper_layer 和 mcu_half 列标出铜面与 MCU 半区。背面 A 排的顺序仍按正面俯视坐标定义；翻转实物看背面时，视觉左右会反转。

| 触点 | 本项目连接 | 需要知道的区别 |
| --- | --- | --- |
| A1、B1、A49、B49 | GND | 公共地 |
| A47、B47 | Core 的 +3V3 | 输出参考与小负载电源，外接预算合计 100mA |
| A48、B48 | +5V_SYS | J1 经 D1 保护后送两块板，各自稳压 |
| A12 | NRST | Core 复位，Expansion 的 TP5 可接调试器复位线 |
| A28 | PH2／BOOT0 | C5A3 的 BOOT0 为 PH2，不能照搬 C551 的 PC15 |
| A6 | VREF_MCU | 当前经 R5 零欧接 Core 3.3V，只作参考，不从这里外部供电 |
| B45、B46 | 不连接 | 本次排序后空出的两个触点，原理图均明确标记 NC |
| A18、B44 | PH15、PH3 | 自定义触点 |
| A7、A5 | PH4、PH5 | 自定义触点 |
| A14、A13 | 不连接 | PH0、PH1 仅接 Core 的 HSE，不再引出 |
| A15、A16 | PC15、PC14 | LSE 晶体及引出零欧电阻已移除，直接连接 GPIO |

主控共有 86 路 GPIO，其中 PH0、PH1 仅用于 HSE，其余 84 路直接连接金手指与扩展排针。PC14、PC15 已取消 LSE 晶体和选装连接电阻，分别直连 A16、A15 和 J9 的 31、32 脚；J9 的 33、34 脚不连接。用户灯使用 PE3，用户按键使用 PD2。公共接口只传 GPIO 和电源，不能插电脑 PCIe 插槽。

Core 保留 24MHz HSE、VCAP 和 C5A3 的启动电路，32.768kHz LSE 晶体 Y2 及 C19/C20 已移除，没有把 C551 电路整段复制过来。复用功能依据本地 DS15137 Rev3 的表格解析，特别保留了 PC4 的 ETH_RXD0 为 AF12、PC5 的 ETH_RXD1 为 AF13 这两处差异。

Core 的当前摆放进一步参照 H562 实物照片，MCU 保持用户调整的 −45°，RESET 与 BOOT 竖放在左上，HSE 晶体位于主控左上，模拟滤波在左侧，LED、电源与 USB 在右侧。去耦电容按 C5A3 的实际电源脚分配，AP63203 的电感和输入输出电容保留自己的回路关系。公开的 [H562 原理图 SVG](https://stm32world.com/images/6/65/Sl_mcu_stm32h562.svg) 与 [C551 原理图 SVG](https://stm32world.com/images/0/03/Sl_mcu_stm32c551.svg) 可用于对照电路，目前未找到两款 MCU 板完整的可编辑 KiCad 源工程。

| 功能 | B1 主控端口 |
| --- | --- |
| RMII REF_CLK、MDIO、MDC | PA1、PA2、PC1 |
| RMII CRS_DV、RXD0、RXD1 | PA7、PC4、PC5 |
| RMII TXD0、TX_EN、TXD1 | PB12、PB13、PB15 |
| PHY 复位、中断 | PE0、PE1 |
| SPI3 SCK、MISO、MOSI | PC10、PC11、PC12 |
| SD 片选、卡检测 | PA15、PH4 |
| USART3 TX、RX | PD8、PD9 |
| 无线 EN、BOOT | PD3、PD4 |
| USB DM、DP、VBUS 检测 | PA11、PA12、PA9 |
| SWDIO、SWCLK | PA13、PA14 |
| 用户按键、红灯 | PD2、PE3 |

## 调试、操作和供电

四线 SWD 插座 J12 只放在 Expansion，使用 JST B4B-XH-A，实际间距 2.50mm，1 至 4 脚为 3V3／VTref、SWDIO、SWCLK、GND。当前 Core 已移除 TP1 至 TP4，单独调试需从金手指或对应网络接线；Expansion 的 TP5 为 NRST，TP6 为 VREF_MCU。

五个按钮统一为两脚常开贴片按键，参考 [SD-1107 厂家图纸](sources/SD-1107_drawing.pdf)。这是市场常标的 3×6×2.5 款，图纸实际本体为 6.10×3.60mm，总高 2.50mm，推荐焊盘为 1.50×1.00mm、中心距 7.90mm。用户现有按键尚未实物测量，焊接前要核对脚距及常开形式；只有标称外形一致还不足以证明互换。对应模型明确采用通用外形，不作为精确厂商模型。

Core 的 D3 为黄绿色电源灯，D4 为 PE3 驱动的红色用户灯，均使用 0603 英制封装和 1kΩ 电阻。按约 1.8 至 2.4V 的常见正向压降估算，电流约 0.9 至 1.5mA，实际亮度取决于手头 LED，未指定精确料号。RJ45 内置指示灯属于连接器自身，与这两颗分立 LED 分开。

主电源从 Expansion 的 J1 输入 5V、2A，D1 后为 +5V_SYS。Core 的 U2 负责本板 +3V3，Expansion 的 U6 负责 +3V3_EXP，网口、无线和 SD 都从后者供电。两路稳压输出没有并联。USB 的 JP1 已移除，USB_VBUS 直接连接 D2 阳极，D2 阴极接 +5V_SYS；USB 仅用于低负载调试，整机运行仍从 J1 供电。两板之间不支持带电插拔。

## 检查结果和下一步

两套工程一共 11 页原理图，Core 有 43 个电气封装，Expansion 有 75 个，另加四个纯机械安装孔。115 个采购位号都绑定了本地模型。金手指和 Expansion 的两个测试点属于 PCB 本身，没有采购模型。通用外形替代用于五个开关及 Expansion 的 U4、J3、Y3，精确装配间隙仍应看图纸。

Core 已取消 Y2、C19、C20 和 R45 至 R48，PC14、PC15 直接引出，PH0、PH1 只接 HSE。2026 年 10 月 23 日按用户最新要求保留 USB 的 R3、R4，并改为 0R，U3 的 6、4 脚分别经对应电阻连接 PA11、PA12。保留原来的 0603 位置和布线，PCB 中已无临时跨层线。本次只改两个 PCB 阻值字段，用户最新保存的其他走线、过孔、摆放和铜区不变。TP1 至 TP4 已移除，清单按实际文件同步。

Core 最新 ERC 为 0 错误、0 警告，原理图与 PCB 一致性问题为 0；Expansion 的电路和 PCB 未在本次修改。两板全部 782 个编号焊盘与网表一致，连通性及资源检查通过 2019 项，无失败项，覆盖 MCU 引脚、84 路引出 GPIO、逐触点连接、两组排针和资源路径，也核对 R3、R4 为 0R。检查脚本位于 [design_tools](design_tools)，完整结果保存在 [verification.json](verification.json)。这些检查不代表 DRC 已通过，实际问题仍保留在原生报告中。

目前 Core 已基本完成布线，Expansion 尚未布线。原生 DRC 仍保留以下实际问题，Core 详细位置见 [复查记录](../CoralC5-Core/docs/pcb_review.md)。

| 位置 | 剩余问题 | 后续处理 |
| --- | --- | --- |
| Core J2 | 两个定位 NPTH 触发四项到 GND 焊盘的间距错误，约 0.1944mm，小于当前 0.20mm | 按 USB-C 原厂推荐焊盘与制板厂工艺要求确认，必要时调整焊盘或更换连接器 |
| Expansion J14 | 51 号固定焊盘到定位 NPTH 仅 0.05mm，小于 0.25mm | 核对母座实物和原厂推荐焊盘，再修订固定焊盘，不能直接照此下单 |
| Core 地铜 | 主控地铜已接通，剩金手指 B49 一个未连接项 | 连接主地后重新填铜检查 |
| Core 悬空线端 | 剩两条 +3V3 和一条 PB15 的同网重叠短段 | 清理多余短段，保留下方完整长线；PB14、PD11 的旧警告已消失 |
| Expansion | 314 个未连接项 | 后续布线后再检查，不能把网络校准通过当成制板 DRC 通过 |

LAN8720 裸露焊盘采用带 0.20mm 热过孔的现成封装，工程最小通孔孔径设为 0.20mm；这和孔到铜间距是两条独立规则。[嘉立创公开工艺能力](https://jlcpcb.com/capabilities/pcb-capabilities/)允许这一孔径，但最终叠层、过孔处理和金手指工艺仍随实际制板订单确认。网口、USB、射频、电源温升和实际插合均未经过实板验证。

2026 年 10 月 23 日双板全检另发现 C7 的耐压标注不符合 VCAP 要求，以及 Expansion 去耦电容需要靠近电源脚，见 [完整检查记录](full_review_2026-10-23.md)。
