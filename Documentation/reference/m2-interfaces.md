# M2 接线与核心板引脚

2026-11-07 更新至 M2.7。MaixCAM2 作识别，Action 4 作图传且仅供电；荔枝派电路继续删除。扩展板统一一路主降压，加屏幕/蓝牙共用 LDO。PCB 元器件已核对并补齐同步，布线与固件未完成。

[九页 PDF](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf)、[原生主原理图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_sch)、[本轮验证](../validation/2026-11-07-m27-unified-power.md)。

下表从扩展板 1 脚开始。UART 信号名按 MCU 方向命名，MCU_TX 接模块 RX；核心侧为三伏三。J15/J17 是 2.54mm 排母，J20/J21/J23 使用普通 JST XH 护套座的 2.50mm 尺寸，不是 XHB 锁扣款。商品所称“XH254”仍需核对实物间距。J10/J27 保持 2.54mm 跳帽排针。J19 单独采用与 MaixCAM2 同型号 MX1.25 六针卧贴座。

| 位号 | 从 1 脚开始的定义 |
| --- | --- |
| J1 电池 | BAT+、GND；XT60PW-M，经 F1 和整车总开关 |
| J10 左右轮硬件使能 | 1 主控使能请求经 R6，2 MOTOR_RUN；跳帽短接后允许 PE8 控制左右轮 |
| J15 HC-08 | 3V3_EXT、GND、模块 RXD、模块 TXD；固定三伏三，无五伏跳线 |
| J17 TFT | GND、3V3_EXT、SCL、SDA、RES、DC、CS、BL |
| J19 MaixCAM2 识别 | GND、保护后五伏、NC、NC、模块 TX、模块 RX；与模块六针同序，线束须逐针测通 |
| J20 灰度并行 | CH1、CH2、CH3、CH4、CH5、CH6、CH7、CH8 |
| J21 灰度电源 | GND、5V_EXT；与 J20 同时连接 |
| J23 ZDT TTL 8P | 开关后 VM_STEP、GND、NC、NC、NC、NC、模块 RX、模块 TX；仅接 1/2/7/8 |
| J27 识别相机供电使能 | 短接使能 U10；普通带电 Type-C 调试前拔下，内置电池仍可能让模块运行 |
| J28 Action 4 | Type-C 母座，VBUS 五伏、GND、CC1/CC2；3A 通告，只供电，无 MCU 数据 |
| SW2～SW5 | 模式、下一项、确认、启动；低有效，6×6×4.3mm 四脚 SMD 通用封装，实物跨距与触点分组待核对 |

J18、J25、J26 随荔枝派删除；U8/U9、U11/U12、L3、Q3 和对应外围也已删除。J22 脉冲备用和 J24 重复电源口删除。未重用这些位号，也没有保留荔枝派 Type-C 输出。

## 步进电源与通信

J23 按 ZDT_X42S 手册主接口顺序预留：1→V+、2→Gnd、3→Com、4→En、5→Stp、6→Dir、7→R/A/H、8→T/B/L。用户购买 TTL 版，因此 7 为驱动 RX、8 为驱动 TX，3～6 在扩展板保持不连接。没有 Core 三伏三供电脚，主控不接电机线圈 A+/A−/B+/B−。驱动菜单使用对应的 UART_FUN 通信配置；具体菜单及波特率以套件固件为准。

供电路径为 VM → F2（4A 延时）→ Q6/Q7 背靠背 P-MOS → J23:1。**PD12 / J14:B30 / STEP_PWR_EN 为高有效**：低电平或悬空时关闭，高电平时经 R83、Q8、R82 拉低 MOS 栅极而供电。R84 将 Q8 基极下拉，R81 拉栅极到共源点，D5 限制栅源电压。GPIO 控制信号不承载电机电流。

U15 为 SN74LVC2G125 双三态缓冲，Q9 从实际 VM_STEP 分压取样，断电时禁用串口输出，防止 MCU TX 持续给断电驱动反向供电。PB6/USART1_TX → U15 → J23:7；J23:8 → U15 → PB7/USART1_RX。

固件先保持 PD12 低及串口安全状态，供电稳定、驱动启动完成后再发送命令；关电前停止运动和通信。PB7 可使用内部上拉维持缓冲禁用时的空闲高电平。电源关断没有主动放电，负载电容仍会有残压；不能当作经认证的急停。启动浪涌、关断期间再生能量和温升待实测。

## 识别相机保护与六针接口

U10 改为 **SGM2531CXPS8G/TR，SOIC-8，1.27mm 脚距**，保留欠压、过压、限流和软启动。底部裸露焊盘接地，必须焊接；外露引脚更方便手工焊接，底部焊接仍需合适工艺。R68/R69 为 33.6k/15k，欠压典型 4.504V；R70/R71 为 42.2k/15k，过压典型 5.301V。R72=95.3k 的限流公式典型值 0.99859A，C73=12nF 的五伏软启动典型约 11.7ms。

SGM2531C 本身不能阻止 OUT 经体二极管向 IN 反灌，因此加 **U14 LTC4412 + Q5 P-MOS 理想二极管**：Q5 漏极朝 U10 输出、源极朝摄像头，U14 的 VIN/SENSE 分别检测两侧。此组合保留上游防反灌，正常压降随电流和器件温度变化。SGM 手册的 ±5% 限流精度仅对应 1A/25℃，不是全温恰好一安的上界；识别叠加充电及启动必须验证。

MaixCAM2 官方 U56 是 ZX-MX1.25-6PWT 六针卧贴座，1～6 为 GND、5V、I2C6_SCL、I2C6_SDA、UART4_TX、UART4_RX。本板 J19 同序连接 1/2/5/6，3/4 不接。连接器厂家额定仅 1A，线束须按实物确认，不得把主降压 8A 额定当成六针承载能力。其六针、Type-C 和扩展口五伏在模块内相通，本板防反灌不能隔开模块内部并联电源。普通带 VBUS 的 USB 调试前拔 J27；内置电池仍可能供电。

模块内部 U52 为 1.5A 自恢复保险，本板六针座额定 1A 比其更低；提高限流不提高任一器件的额定能力。过压门限有基准、电阻和漏电容差，属于切断保护，不是精准五伏钳位。详见本轮验证中的计算与限制。

## Action 4 与 TFT、蓝牙、灰度

Action 4 只接 J28 Type-C 电源，TPS25810 集成接入检测、3A 通告与限流；固定五伏，不支持 PD/PPS 快充。C81 输入储能、C82 输入旁路、C83 输出电容按原厂源端要求保留。
四键删除 C57～C60 硬件滤波，保留 10k 上拉，固件需约 20ms 消抖。

H1/H2 保留两个 D2.0 非金属化孔、不接地、不计入电气 BOM。30mm 孔距来自 35mm 板宽减两侧 2.5mm 孔心边距，仍待实际屏幕复核。见 [孔心图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/TFT_hole_offset_detail.png)。

HC-08 按用户照片从 3V3 端起为 3V3、GND、RXD、TXD。J15:3 接 PA2/USART2_TX，J15:4 接 PA3/USART2_RX；排母安装旋转仍由 PCB 按 1 脚确认。灰度 CH1～CH8 使用输入、无内部上下拉，KEY/Err/CLK/DAT 不接；白色 1、黑色 0，左右顺序按安装方向校准。

J10 插帽后仍由 PE8 控制左右轮；拔帽由 R7 将 MOTOR_RUN 拉低，使 U2/U4 休眠。J10 不控制 ZDT 步进和整车总电源。

VIDEO_TX/VIDEO_RX 仍沿用旧网名，现在用于 MaixCAM2 识别坐标串口。Action 4 不占 GPIO。

## 新增接口的 25 路信号

| 信号 | GPIO | J14 触点 | 功能 | 电气端点 |
| --- | --- | --- | --- | --- |
| BT_TX | PA2 | A2 | USART2_TX / AF7 | J15:3 |
| BT_RX | PA3 | B2 | USART2_RX / AF7 | J15:4 |
| VIDEO_TX | PC6 | B34 | USART6_TX / AF7 | U13:2 |
| VIDEO_RX | PC7 | B35 | USART6_RX / AF7 | U13:3 |
| STEP_UART_TX | PB6 | A30 | USART1_TX / AF7 | U15:2 |
| STEP_UART_RX | PB7 | A29 | USART1_RX / AF7 | U15:3 |
| TFT_SCK | PA5 | B4 | SPI1_SCK / AF5 | J17:3 |
| TFT_MOSI | PA7 | B6 | SPI1_MOSI / AF5 | J17:4 |
| TFT_RES | PB0 | B9 | GPIO | J17:5 |
| TFT_DC | PB1 | B10 | GPIO | J17:6 |
| TFT_CS | PA4 | B3 | GPIO | J17:7 |
| TFT_BL | PB4 | A32 | TIM3_CH1 / AF2 | J17:8 |
| GRAY_CH1 | PE10 | B15 | GPIO | J20:1 |
| GRAY_CH2 | PE11 | B16 | GPIO | J20:2 |
| GRAY_CH3 | PE12 | B17 | GPIO | J20:3 |
| GRAY_CH4 | PE13 | B18 | GPIO | J20:4 |
| GRAY_CH5 | PE14 | B19 | GPIO | J20:5 |
| GRAY_CH6 | PE15 | B20 | GPIO | J20:6 |
| GRAY_CH7 | PD10 | B28 | GPIO | J20:7 |
| GRAY_CH8 | PD11 | B29 | GPIO | J20:8 |
| KEY_MODE | PC4 | B7 | GPIO | SW2:1 |
| KEY_NEXT | PC5 | B8 | GPIO | SW3:1 |
| KEY_OK | PB2 | B11 | GPIO | SW4:1 |
| KEY_START | PE7 | B12 | GPIO | SW5:1 |
| STEP_PWR_EN | PD12 | B30 | GPIO | R83:1 |

合计保留 13 路原轮控制，加上述 25 路为 **38 路 GPIO**。PD8/B26、PD9/B27、PE9/B14、PD14/B32、PD15/B33 释放；PD13/B31 继续空闲。PB6/PB7 用于 USART1，不是 I²C1。PA9、PA11/PA12、PA13/PA14 以及既有 Core 专用脚保留。

[98 触点审计](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_motor_98_contact_audit.csv)核对 Core MCU 物理脚与 Motor 实际网络。A13/A14/B45/B46 为原合同空脚；已用信号未被 X 截断。

## 核心板与总电源

Motor U1 TPS56837 提供标称 4.992V，D3 前分给相机、灰度及 U7；D3 后经 J14:A48/B48 送 Core，Core U2 AP63203 产生 3.3V 并接 A47/B47。Core USB 经自己的 D2 接系统五伏，两侧二极管防反灌。Core 对外 3.3V 预算只有 100mA，因此 TFT/蓝牙 150mA 继续由 U7 供电，两个 3.3V 不并联。

五伏总预算 5.2A，含 Core 0.75A、Maix 1A、Action 4 3A、灰度 0.3A、LDO 0.15A。按效率 85%、轮/步进电池端 7A，10V/12V/12.6V 输入预算约 10.0539A/9.5449A/9.4238A。F1 15A 快断、F2 4A 延时保留。此为设计预算，六针相机支路能否边识别边充电、Type-C 是否能覆盖录像负载及总降压的板级温升均须实测。
