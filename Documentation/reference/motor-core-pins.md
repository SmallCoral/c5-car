# Motor 核心板信号分配

更新日期：2026-11-08，M2.8P。当前为 39 路 GPIO，按本轮实际 PCB 焊盘位置重新分配其中 25 路。Core 的 98 触点针序和原理图保持原样；Motor PCB 未同步本次变化。

当前机器表为 [13 路轮控制](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_pin_assignment.csv)和 [26 路外设](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m2_pin_assignment.csv)；触点到 MCU 的固定关系来自[共享接口表](../../Hardware/stm32c5a3/Hardware/Shared/interface.csv)。[本轮检查记录](../validation/2026-11-08-layout-pinplan.md)列出验证命令和限制。

## 当前分配

TX、RX 均从 Core 方向命名。MCU TX 接模块 RX，MCU RX 接模块 TX。

| 信号 | GPIO | J14 触点 | 配置 | 电气端点 |
| --- | --- | --- | --- | --- |
| L_ENC_A | PA0 | A4 | TIM5_CH1 / AF2 | U3:6 |
| L_ENC_B | PA1 | A3 | TIM5_CH2 / AF2 | U3:4 |
| R_ENC_A | PB3 | A33 | TIM2_CH2 / AF1 | U5:6 |
| R_ENC_B | PA15 | A45 | TIM2_CH1 / AF1 | U5:4 |
| L_PWM | PC2 | A9 | TIM4_CH4 / AF2 | U2:1 |
| R_PWM | PB8 | A27 | TIM4_CH3 / AF2 | U4:1 |
| L_DIR | PC3 | A8 | GPIO | U2:2 |
| R_DIR | PC10 | A44 | GPIO | U4:2 |
| L_nFAULT | PH4 | A7 | EXTI4 | U2:4 |
| R_nFAULT | PC11 | A43 | EXTI11 | U4:4 |
| L_ISENSE | PH5 | A5 | ADC2_IN13 | R15:2 |
| R_ISENSE | PC0 | A11 | ADC1_IN8 | R35:2 |
| MOTOR_EN_REQ | PC5 | B8 | GPIO | R6:1 |
| BT_TX | PB0 | B9 | USART2_TX / AF7 | J15:3 |
| BT_RX | PA3 | B2 | USART2_RX / AF7 | J15:4 |
| VIDEO_TX | PB10 | B21 | USART3_TX / AF7 | J19:6 |
| VIDEO_RX | PC4 | B7 | USART3_RX / AF7 | J19:5 |
| STEP_UART_TX | PC6 | B34 | USART6_TX / AF7 | J23:7 |
| STEP_UART_RX | PC7 | B35 | USART6_RX / AF7 | J23:8 |
| LICHEE_TX | PB6 | A30 | USART1_TX / AF7 | U12:2 |
| LICHEE_RX | PB7 | A29 | USART1_RX / AF7 | U11:4 |
| TFT_SCK | PA5 | B4 | SPI1_SCK / AF5 | J17:3 |
| TFT_MOSI | PA7 | B6 | SPI1_MOSI / AF5 | J17:4 |
| TFT_CS | PA4 | B3 | GPIO | J17:7 |
| TFT_RES | PB1 | B10 | GPIO | J17:5 |
| TFT_DC | PB2 | B11 | GPIO | J17:6 |
| TFT_BL | PA6 | B5 | TIM3_CH1 / AF2 | J17:8 |
| KEY_MODE | PH3 | B44 | GPIO | SW2:1 |
| KEY_NEXT | PA10 | B40 | GPIO | SW3:1 |
| KEY_OK | PA8 | B38 | GPIO | SW4:1 |
| KEY_START | PC9 | B37 | GPIO | SW5:1 |
| GRAY_CH1 | PE7 | B12 | GPIO | J20:1 |
| GRAY_CH2 | PE8 | B13 | GPIO | J20:2 |
| GRAY_CH3 | PE9 | B14 | GPIO | J20:3 |
| GRAY_CH4 | PE10 | B15 | GPIO | J20:4 |
| GRAY_CH5 | PE11 | B16 | GPIO | J20:5 |
| GRAY_CH6 | PE12 | B17 | GPIO | J20:6 |
| GRAY_CH7 | PE13 | B18 | GPIO | J20:7 |
| GRAY_CH8 | PE14 | B19 | GPIO | J20:8 |

## 与当前摆放的关系

PCB 正面 A 排在右、B 排在左。左右轮驱动和编码器在右侧，使用 A 排；蓝牙、TFT、灰度、MaixCAM2 和下侧按键使用 B 排。荔枝派位于右上，其 MCU 串口改到 A29/A30；步进位于左下，改到 B34/B35。右轮方向和故障信号移到靠下的 A44/A43。

六路跨侧信号已移到对应一侧。按 39 个 J14 焊盘到各信号端点的水平加竖直距离求和，估算由 2280.8mm 降至 1980.6mm，减少约 13.2%。这是方位比较，未计算障碍、换层或铜线长度。右轮采样仍在 A11，因为该脚具有所需 ADC 功能，不能为接近模块改到普通 GPIO。

## 固件约束

- 左编码器 TIM5，右编码器 TIM2；右 A 相接 CH2、B 相接 CH1，计数方向须按实车标定。PB3 不同时启用 SWO，双线 SWD 保留。
- 左右轮 PWM 使用 TIM4 的 CH4/CH3，共享频率；背光为 PA6/TIM3_CH1。四类计数器独立。
- 蓝牙 USART2、MaixCAM2 USART3、步进 USART6、荔枝派 USART1，均为 AF7。TFT 为 SPI1，AF5。
- 灰度 CH1～CH8 为 PE7～PE14，输入无内部上下拉，按安装方向校准左右与黑白。
- 四个按钮依次为 PH3、PA10、PA8、PC9，低有效；固件消抖。MOTOR_EN_REQ 为 PC5，上电、复位和故障时保持低。
- PA9 的 USB VBUS 检测、PA11/PA12 的 USB、PA13/PA14 的 SWD，以及 PH2、PE3、PD2 的 Core 原用途不占用。

复用和模拟通道依据本地原厂 [DS15137 Rev3](../../Hardware/stm32c5a3/Hardware/Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf) Table13、Table14。当前固件仍未实现这些外设配置；原理图通过不代表固件可运行。外接连接器针序见 [M2 接口](m2-interfaces.md)。
