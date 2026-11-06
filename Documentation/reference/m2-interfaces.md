# M2 接线与核心板引脚

2026-11-06 原理图更新至 M2.4，蓝牙固定三伏三、删除 J16；PCB、固件和实物仍待完成。[九页 PDF](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/CoralC5-Motor_Schematic.pdf)、[原生工程](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/CoralC5-Motor.kicad_pro)、[最新验证](../validation/2026-11-06-m24-fixed-bluetooth.md)、[电流预算](../validation/2026-11-06-m23-power-connectors.md)。

下表按扩展板焊盘编号从 1 开始。所有 UART 的 MCU_TX 接模块 RX，MCU_RX 接模块 TX，逻辑为三伏三；这是本板线束定义，不宣称不同外设插座物理顺序相同。
J15/J17 为 2.54mm 排母。J18/J19/J20/J21/J22/J23 为普通 JST XH 直插护套座，采用原厂 2.50mm 尺寸，不是 XHB 带卡扣款；国产商品所称“XH254”仍须核对实物间距。
J10 为 2.54mm 两针跳帽排针；J16 蓝牙电压选择器已删除。J24 保留 5.08mm 电源接线端子，J1 保留 XT60。

| 位号 | 从 1 脚开始的定义 |
| --- | --- |
| J1 | BAT+、GND；XT60PW-M，输入经 F1，实际壳体正负与配对方向仍须核对 |
| J10 左右轮硬件使能 | 两针排针；1 来自 R6/主控使能请求，2 接 MOTOR_RUN；用跳帽短接允许主控使能左右轮 |
| J15 HC-08 | 从照片 3V3 端起：3V3_EXT、GND、模块 RXD（接 BT_TX）、模块 TXD（接 BT_RX）；无五伏选择和跳帽 |
| J17 TFT | GND、3V3_EXT、SCL、SDA、RES、DC、CS、BL；严格按屏幕资料顺序 |
| J18 MaixCAM | 5V_EXT、GND、MAIX_TX、MAIX_RX |
| J19 图传处理模块 | 5V_EXT、GND、VIDEO_TX、VIDEO_RX；不接裸 OV2640 |
| J20 灰度并行 | 5V_EXT、GND、CH1、CH2、CH3、CH4、CH5、CH6、CH7、CH8 |
| J21 灰度同步串行 | 5V_EXT、GND、CLK、DAT；与 J20 择一接线 |
| J22 步进脉冲 | Core 3V3_REF、GND、STEP、DIR、nEN、GND；PUL/DIR 默认下拉，nEN 默认上拉禁用 |
| J23 ZDT TTL 8P | VM_STEP、GND、NC、NC、NC、NC、STEP_UART_TX、STEP_UART_RX；只接 1/2/7/8 四根线 |
| J24 步进驱动备用电源 | VM_STEP、GND；5.08mm 接线端子，同样经过 F2 分路保险 |
| SW2～SW5 | 模式、下一项、确认、启动；低有效 |

J10 短接后仍由 PE8 的 MOTOR_EN_REQ 控制左右轮使能，不是插帽即转动。
拔下后 R7 将 MOTOR_RUN 拉低，U2/U4 的 nSLEEP 为低，左右轮驱动休眠；它不控制 ZDT 步进驱动，也不是整车总电源开关。

HC-08 的[用户实物照片](../蓝牙/HC08_实物针序_2026-11-06.png)中，引脚向上、白色丝印面可见时，左到右为 3V3、GND、RXD、TXD。
J15 符号采用模块侧 RXD/TXD 标注；BT_TX/BT_RX 网络名仍按主控方向命名，因此第 3 脚接主控 PA2/USART2_TX，第 4 脚接 PA3/USART2_RX。
照片顺序不规定未来 PCB 的安装旋转，排母定位时以 3V3 对应的 1 脚确认朝向。

用户确认购买 ZDT_XS 第二代闭环电机及驱动一体套件，通信为 TTL 版。
J23 按手册的八位主接口顺序预留：1→V+、2→Gnd、3→Com、4→En、5→Stp、6→Dir、7→R/A/H、8→T/B/L。
其中 3～6 在扩展板明确标 NC；7 是主控 TX 接驱动 RX，8 是主控 RX 接驱动 TX。J23 没有 Core 三伏三供电脚。
TTL 模式将驱动的 P_Serial 配置为 UART_FUN；驱动板本身给线圈供电，主控不接 A+/A−/B+/B−。
J22 六针保留为脉冲备用，三伏三仅作电平参考；PLC 版 COM 不接，光耦版 COM 接三伏三并按共阳方式配置。
若使用 nEN 上拉实现失能，必须把驱动 En 设为 L；出厂默认 Hold 不受 En 引脚控制。

## TFT 的两个安装孔

H1/H2 已作为机械孔符号加入原理图，绑定 2.0mm 非金属化孔封装，不接地、不计入电气 BOM。
按原厂第 5 页四孔图选同一短边的两个孔。板宽为 35mm，图中两个方向的孔心边距细标均为 2.5000mm，短边孔距据此计算为 **35−2×2.5＝30mm**，实物复核后再定位。图中的 33mm 是屏幕本体宽度，不能当孔距。可查看[整体放大图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/TFT_dimension_detail.png)与[孔心边距细节](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/sources/TFT_hole_offset_detail.png)。使用能通过实物 2.0mm 孔的紧固件，并核对柱高、排针、屏幕背面器件避让。两个符号在原理图上画多远没有机械含义。

## 新增 31 路信号

| 信号 | GPIO | J14 触点 | 功能 | 电气端点 |
| --- | --- | --- | --- | --- |
| BT_TX | PA2 | A2 | USART2_TX / AF7 | J15:3 |
| BT_RX | PA3 | B2 | USART2_RX / AF7 | J15:4 |
| MAIX_TX | PD8 | B26 | USART3_TX / AF7 | J18:3 |
| MAIX_RX | PD9 | B27 | USART3_RX / AF7 | J18:4 |
| VIDEO_TX | PC6 | B34 | USART6_TX / AF7 | J19:3 |
| VIDEO_RX | PC7 | B35 | USART6_RX / AF7 | J19:4 |
| STEP_UART_TX | PB6 | A30 | USART1_TX / AF7 | J23:7 |
| STEP_UART_RX | PB7 | A29 | USART1_RX / AF7 | J23:8 |
| TFT_SCK | PA5 | B4 | SPI1_SCK / AF5 | J17:3 |
| TFT_MOSI | PA7 | B6 | SPI1_MOSI / AF5 | J17:4 |
| TFT_RES | PB0 | B9 | GPIO | J17:5 |
| TFT_DC | PB1 | B10 | GPIO | J17:6 |
| TFT_CS | PA4 | B3 | GPIO | J17:7 |
| TFT_BL | PB4 | A32 | TIM3_CH1 / AF2 | J17:8 |
| GRAY_CH1 | PE10 | B15 | GPIO | J20:3 |
| GRAY_CH2 | PE11 | B16 | GPIO | J20:4 |
| GRAY_CH3 | PE12 | B17 | GPIO | J20:5 |
| GRAY_CH4 | PE13 | B18 | GPIO | J20:6 |
| GRAY_CH5 | PE14 | B19 | GPIO | J20:7 |
| GRAY_CH6 | PE15 | B20 | GPIO | J20:8 |
| GRAY_CH7 | PD10 | B28 | GPIO | J20:9 |
| GRAY_CH8 | PD11 | B29 | GPIO | J20:10 |
| GRAY_CLK | PD12 | B30 | GPIO | J21:3 |
| GRAY_DAT | PD13 | B31 | GPIO | J21:4 |
| STEP_PUL | PE9 | B14 | TIM1_CH1 / AF1 | J22:3 |
| STEP_DIR | PD14 | B32 | GPIO | J22:4 |
| STEP_nEN | PD15 | B33 | GPIO | J22:5 |
| KEY_MODE | PC4 | B7 | GPIO | SW2:1 |
| KEY_NEXT | PC5 | B8 | GPIO | SW3:1 |
| KEY_OK | PB2 | B11 | GPIO | SW4:1 |
| KEY_START | PE7 | B12 | GPIO | SW5:1 |

MaixCAM 此轮使用 USART3，蓝牙 USART2、图传 USART6、步进 UART 为 USART1；不能直接沿用同学 F407 代码的引脚和串口配置。PB6/PB7 不再保留 I²C1。TIM1 用于步进脉冲、TIM3 用于屏幕背光；原 TIM2/TIM5 编码器和 TIM4 轮 PWM 保留。

灰度 CH1～CH8 与 DAT 配置输入、无内部上下拉；CLK 配置输出。厂家给的串行是 CLK/DAT 同步 GPIO，不是 UART 或 I²C。白色 1、黑色 0，物理左右顺序需按安装方向确认。

## 98 触点审计

[逐触点原生网表审计 CSV](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_motor_98_contact_audit.csv)同时列 Core 的 MCU 物理脚和 Motor 实际网络。
映射以共享合同的 gpio/net/mcu_pin 为准。A13、A14、B45、B46 是空脚，其他未用触点保留 X；原理图已使用的 44 路功能 GPIO 没有被 X 截断。PA9 的 USB 检测电路和既有 USB、SWD、Core 用户灯/键/启动脚均避开。

外设五伏总预算 1.5A，三伏三预算 150mA；Core 五伏支路预算 0.75A。
F1 已改为 12A 延时保险，F2 为 4A 延时步进分路保险；它们不是精确限流器，不能据此把 XH 当作 4A 或 12A 接头。
整车保守预算为 12V 时约 8.10A、10V 时约 8.32A，包含两路轮电机各 2A、步进输入预留 3A、低压电源折算；这是预算而非实测，详见验证记录。

灰度五伏接整套辅助板的五伏入口，不接 MCU 的三伏三脚。辅助板开源原理图中的 U3 为 HT7533-1，板内逻辑为三伏三。Core 仅 USB 供电而外设断电时，固件须先将这些外设接口保持高阻，避免从信号脚反向供电。
