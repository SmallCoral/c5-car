# Coral-C5

Coral-C5 包含核心板和两种扩展板。STM32C5A3VGT6、时钟、电源、USB 和几个常用按键留在 CoralC5-Core，网口、无线、SD 卡和四线 SWD 放到 CoralC5-Expansion。新增 CoralC5-Motor 用于双电机机器人，配有十二伏DC-005输入、拨动总开关、五伏降压、双路DRV8874和编码器接口。核心板通过98触点金手指配套使用其中一种扩展板。

当前硬件版本为 B1。原理图已拆成两套工程，Core 已基本完成布线，2026 年 10 月 24 日复查为 0 个 DRC 错误、0 个警告、0 个未连接项。Expansion 尚未布线。两板的原理图与 PCB 网络已重新校准。固件仍保留之前的生成工程，应用代码与端口配置还没有按 B1 完整接入。

| 要做什么 | 从这里打开 |
| --- | --- |
| 继续画核心板 | [CoralC5-Core](Hardware/CoralC5-Core/CoralC5-Core.kicad_pro) |
| 继续画扩展板 | [CoralC5-Expansion](Hardware/CoralC5-Expansion/CoralC5-Expansion.kicad_pro) |
| 查看双电机扩展板原理图 | [CoralC5-Motor](Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro)、[五页A4电路PDF](Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf) |
| 查机器人接口与电池估算 | [电机板说明](Hardware/CoralC5-Motor/README.md)、[2800mAh电池评估](Hardware/CoralC5-Motor/docs/battery_assessment.md) |
| 查金手指和兼容边界 | [双板接口说明](Hardware/Shared/interface.md)、[98 触点表](Hardware/Shared/interface.csv) |
| 查扩展排针 | [J8、J9 针脚表](Hardware/CoralC5-Expansion/docs/header_pinout.csv) |
| 看电路 | [Core 原理图](Hardware/CoralC5-Core/docs/CoralC5-Core_Schematic.pdf)、[Expansion 原理图](Hardware/CoralC5-Expansion/docs/CoralC5-Expansion_Schematic.pdf) |
| 接着写固件 | [固件说明](Firmware/README.md) |

Core 沿用 Streamline 的 70.00×51.45mm 公共板形和 PCIe x8 形状的金手指，参照 H562 公开照片整理摆放。MCU 沿用 −45° 方向放在板中上部，复位按钮位于主控左上，BOOT0 与 USER 位于顶边，降压电源和 USB-C 在右侧。三个按键已补上用途丝印，左侧放了珊瑚 Logo 和 Designed by SmallCoral 署名，见 [Core 当前预览](Hardware/CoralC5-Core/docs/placement_top.png)。C5-EDGE-3 电气针序已经自定义，MCU 下半信号接正面 B 排，上半信号接背面 A 排，扩展板插槽同步配对，不能与旧电气针序混用。这里传的是 GPIO 和电源，不能插电脑 PCIe 插槽。Expansion 的 100×86mm 板框是本项目暂定尺寸，顶部放核心板插槽，左右各一组 2×24 排针，下方放 RJ45 和 microSD，右侧放外置天线无线模块。

板上保留 LAN8720A 百兆网口、ESP32-C3-MINI-1U 的 2.4GHz Wi-Fi 和 Bluetooth LE，以及走 SPI3 的 microSD。无线天线座使用 I-PEX MHF III，选尾线时要按这个规格核对。两组排针引出 84 路 GPIO，PC14、PC15 直接连接金手指和排针。PH0、PH1 只接 Core 的 24MHz 晶振，金手指 A13、A14 及 J9 的 33、34 脚不连接。

四线 SWD 在 Expansion，1 至 4 脚依次为 **3V3／VTref、SWDIO、SWCLK、GND**，插座为 JST XH 2.50mm。Core 的四个调试测试点已移除，Expansion 保留 NRST 和 VREF_MCU 测试点。五个按键统一改成常见的两脚 3×6×2.5 贴片款，参考 SD-1107 图纸，实际本体为 6.1×3.6mm、高 2.5mm，购买或焊接前核对实物脚距。LED 按现有物料改成黄绿色电源灯和红色用户灯，保持 0603 封装与 1kΩ 限流。

整机从 Expansion 的 J1 输入稳压 5V、2A。Core 的 U2 和 Expansion 的 U6 分别降压，两个 3.3V 输出各带自己的负载。金手指上的 3V3 来自 Core，给调试参考和少量扩展负载使用，外接负载预算合计 100mA。USB 供电跳帽 JP1 已移除，USB 5V 经 D2 接入系统电源，仍仅供低负载调试；插拔核心板前先断电。

Core 已取消 32.768kHz 晶体 Y2、C19、C20 和 R45 至 R48，PC14、PC15 直接引出，PH0、PH1 留给 HSE。USB 的 R3、R4 按用户最新要求保留，均使用 0R，并保留原有 0603 位置与布线，原理图和 PCB 同步。

2026 年 10 月 23 日完成两板原理图与 PCB 全检。两板 ERC 均为 0 错误、0 警告，一致性问题均为 0，782 个编号焊盘与网表一致，连通性及资源检查通过 2019 项。J2 相邻地焊盘圆角由 0.15mm 增至 0.30mm，孔到铜约为 0.233mm，四条孔距错误已消除。10 月 24 日复查用户提交 88247a1，B49 接地正常，三条重叠线段均已移除，Core 的 DRC 和未连接项全部清零，见 [处理记录](Hardware/CoralC5-Core/docs/pcb_review.md)。Expansion 尚未布线，有 314 个未连接项和 J14 孔距错误。C7 标注的 6.3V 耐压仍低于 C5A3 对 VCAP 电容至少 10V 的要求，Expansion 多组去耦电容也需要靠近电源脚。原厂依据和检查边界见 [完整检查记录](Hardware/Shared/full_review_2026-10-23.md)。

硬件目录保留 Core、Expansion、新增的 Motor 原理图工程和 Shared。各工程带着实际使用的本地封装与模型，共同针脚表、检查工具和厂家资料放在 Shared。固件配置、源码与厂商支持包保留在 Firmware。复制工程时带上其本地符号、封装、Models及库表。第三方来源见 [资源说明](THIRD_PARTY.md)。

原单板及清理前的完整快照已备份到项目同级的 Coral-C5-archive 目录，归档时核对过原 PCB 哈希。旧单板、一次性生成脚本、历史缓存和临时预览已从当前项目移除，后续直接用 KiCad 修改两套 B1 工程。
