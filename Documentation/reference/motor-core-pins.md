# Motor 核心板信号分配

分配日期：2026-11-08，M2.8P2，共39路 GPIO。
下表保留当时重新分配20路 GPIO 的结果，Core 的98触点合同不变。
2026-11-10 复查已确认当前 Motor PCB 的有效焊盘网络与原理图匹配，未连接项为零；剩余孔间距、散热和功率布线问题见[PCB 复查](../validation/2026-11-10-motor-pcb-review.md)。

机器表为 [13路轮控制](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_pin_assignment.csv)和[26路外设](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/m2_pin_assignment.csv)。固定触点依据[共享接口表](../../Hardware/stm32c5a3/Hardware/Shared/interface.csv)；逐路功能证据见[39路审计表](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/functional-pinout.csv)，本轮范围和命令见[复核记录](../validation/2026-11-08-pinplan-rework.md)。

## 当前分配

TX/RX 从 Core 方向命名，MCU TX 接模块 RX。J14 的 A/B 触点编号按实际 Core 合同读取；`standard`栏的标准名称不能代替实际`gpio`栏。LQFP100 管脚号直接核对 DS15137 Table13，复用核对 Table14。

| 信号 | GPIO | MCU管脚 | J14触点 | 配置 | 电气端点 |
| --- | --- | --- | --- | --- | --- |
| L_ENC_A | PA0 | 23 | A4 | TIM5_CH1 / AF2 | U3:6 |
| L_ENC_B | PA1 | 24 | A3 | TIM5_CH2 / AF2 | U3:4 |
| R_ENC_A | PB3 | 89 | A33 | TIM2_CH2 / AF1 | U5:6 |
| R_ENC_B | PA15 | 77 | A45 | TIM2_CH1 / AF1 | U5:4 |
| L_PWM | PC2 | 17 | A9 | TIM4_CH4 / AF2 | U2:1 |
| R_PWM | PB6 | 92 | A30 | TIM4_CH1 / AF2 | U4:1 |
| L_DIR | PC3 | 18 | A8 | GPIO | U2:2 |
| R_DIR | PC11 | 79 | A43 | GPIO | U4:2 |
| L_nFAULT | PH4 | 19 | A7 | EXTI4 | U2:4 |
| R_nFAULT | PC10 | 78 | A44 | EXTI10 | U4:4 |
| L_ISENSE | PH5 | 22 | A5 | ADC2_IN13 | R15:2 |
| R_ISENSE | PC0 | 15 | A11 | ADC1_IN8 | R35:2 |
| MOTOR_EN_REQ | PA3 | 26 | B2 | GPIO | R6:1 |
| BT_TX | PA6 | 31 | B5 | USART1_TX / AF7 | J15:3 |
| BT_RX | PA7 | 32 | B6 | USART1_RX / AF7 | J15:4 |
| VIDEO_TX | PB13 | 52 | B23 | UART5_TX / AF14 | J19:6 |
| VIDEO_RX | PB12 | 51 | B22 | UART5_RX / AF14 | J19:5 |
| STEP_UART_TX | PC6 | 63 | B34 | USART6_TX / AF7 | J23:7 |
| STEP_UART_RX | PC7 | 64 | B35 | USART6_RX / AF7 | J23:8 |
| LICHEE_TX | PB9 | 96 | A26 | UART4_TX / AF8 | U12:2 |
| LICHEE_RX | PB8 | 95 | A27 | UART4_RX / AF8 | U11:4 |
| TFT_SCK | PB1 | 36 | B10 | SPI3_SCK / AF4 | J17:3 |
| TFT_MOSI | PB2 | 37 | B11 | SPI3_MOSI / AF7 | J17:4 |
| TFT_CS | PC4 | 33 | B7 | GPIO | J17:7 |
| TFT_RES | PB0 | 35 | B9 | GPIO | J17:5 |
| TFT_DC | PC5 | 34 | B8 | GPIO | J17:6 |
| TFT_BL | PA4 | 29 | B3 | TIM3_CH4 / AF2 | J17:8 |
| KEY_MODE | PD15 | 62 | B33 | GPIO | SW2:1 |
| KEY_NEXT | PA8 | 67 | B38 | GPIO | SW3:1 |
| KEY_OK | PA10 | 69 | B40 | GPIO | SW4:1 |
| KEY_START | PH3 | 73 | B44 | GPIO | SW5:1 |
| GRAY_CH1 | PE7 | 38 | B12 | GPIO | J20:1 |
| GRAY_CH2 | PE8 | 39 | B13 | GPIO | J20:2 |
| GRAY_CH3 | PE9 | 40 | B14 | GPIO | J20:3 |
| GRAY_CH4 | PE10 | 41 | B15 | GPIO | J20:4 |
| GRAY_CH5 | PE11 | 42 | B16 | GPIO | J20:5 |
| GRAY_CH6 | PE12 | 43 | B17 | GPIO | J20:6 |
| GRAY_CH7 | PE13 | 44 | B18 | GPIO | J20:7 |
| GRAY_CH8 | PE14 | 45 | B19 | GPIO | J20:8 |

## 飞线与顺序

当前 PCB 正面 A 排在右、B 排在左。右侧轮驱动、编码器与荔枝派仍接 A 排；蓝牙、TFT、灰度、Maix、步进及按钮仍接 B 排。蓝牙使用 B5/B6，Maix 使用 B22/B23，荔枝派使用 A26/A27；收发线集中到相邻触点。四键依实物左右顺序对应 PD15、PA8、PA10、PH3。

保持所有端点位置，比较39条“J14焊盘→模块端点”的直线，交叉由39处降到14处；水平加竖直距离和由1944.202mm到1961.202mm，增加17mm（约0.87%）。取舍是减少交叉并集中信号，未据此宣称铜线更短。此模型不包含障碍、层、走线宽度、回流和电源铜线，不是完成的布线或DRC。见[前后比较图](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/pinplan-routing-comparison.png)。

## 固件与电气约束

- 左编码器 PA0/PA1 用 TIM5_CH1/CH2；右编码器 PB3/PA15 用 TIM2_CH2/CH1。右编码器 A 相在CH2、B相在CH1，方向须按实车标定；PB3不同时启用SWO，双线SWD保留。
- 左右轮 PWM 为 PC2/TIM4_CH4 与 PB6/TIM4_CH1，共享频率。背光 PA4/TIM3_CH4；两路编码器、轮PWM、背光共四个独立计数器。
- 蓝牙 PA6/PA7 为 USART1 AF7；Maix PB13/PB12 为 UART5 AF14；步进 PC6/PC7 为 USART6 AF7；荔枝派 PB9/PB8 为 UART4 AF8。四组串口互不占用同一实例。
- Maix 模块侧六针仍是自身 UART4，Core 侧改为 UART5；荔枝派 Linux 端仍是 UART1，Core 侧改为 UART4。两端串口编号不同不构成接线错误。
- TFT 的 PB1/SPI3_SCK 是 AF4，PB2/SPI3_MOSI 是 AF7，均经 ST 原厂表核对；同一个SPI外设的不同管脚可以使用不同AF编号。CS为PC4，RES为PB0，DC为PC5。
- 左采样 PH5/ADC2_IN13，右采样 PC0/ADC1_IN8，分别配置ADC2与ADC1，不能按旧F4模拟通道编号填写。
- 左故障 PH4/EXTI4，右故障 PC10/EXTI10，互不冲突。四键低有效、轮询消抖；灰度PE7～PE14使用无内部上下拉输入。
- MOTOR_EN_REQ改PA3；上电、复位及故障时保持低。PA9、PA11/PA12、PA13/PA14、PH2、PE3、PD2仍保留Core原用途。

这些外设的初始化和协议尚未写入固件。
网络匹配不能代替铜线载流、实物或整车验证；封装、线序、电源和电平边界见[M2接口](m2-interfaces.md)。
