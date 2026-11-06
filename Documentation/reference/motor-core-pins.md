# Motor核心板信号分配

更新日期：2026-11-03，按当前Motor布局重新分配。
本次只修改Motor使用哪些GPIO，不改Core的98触点针序，也不改Core或网络Expansion的PCB。
完整机器表为[core_pin_assignment.csv](../../Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/core_pin_assignment.csv)。
GPIO与触点关系仍由[公共接口表](../../Hardware/stm32c5a3/Hardware/Shared/interface.csv)维护。

## 当前分配

| 信号 | MCU引脚 | J14触点 | 外设配置 | 原引脚 |
| --- | --- | --- | --- | --- |
| L_ENC_A | PA0 | A4 | TIM5_CH1，AF2 | PA15/A45 |
| L_ENC_B | PA1 | A3 | TIM5_CH2，AF2 | PB3/A33 |
| R_ENC_A | PB3 | A33 | TIM2_CH2，AF1 | PC6/B34 |
| R_ENC_B | PA15 | A45 | TIM2_CH1，AF1 | PC7/B35 |
| L_PWM | PC2 | A9 | TIM4_CH4，AF2 | PD12/B30 |
| R_PWM | PB8 | A27 | TIM4_CH3，AF2 | PD13/B31 |
| L_DIR | PC3 | A8 | 推挽GPIO | PD10/B28 |
| R_DIR | PD7 | A34 | 推挽GPIO | PD11/B29 |
| L_nFAULT | PH4 | A7 | 低有效输入，可用EXTI4 | PE4/A21 |
| R_nFAULT | PD6 | A35 | 低有效输入，可用EXTI6 | PE5/A20 |
| L_ISENSE | PH5 | A5 | ADC2_IN13，模拟输入 | PA0/A4 |
| R_ISENSE | PC0 | A11 | ADC1_IN8，模拟输入 | PA1/A3 |
| MOTOR_EN_REQ | PE8 | B13 | 默认低的推挽GPIO，经R6/J10使能 | PE2/A23 |

SWDIO仍为PA13/B43，SWCLK仍为PA14/A46。
2026-11-06 的 M2 新接口已分配：PA2/PA3 为 HC-08 的 USART2，PB6/PB7 为步进的 USART1，不再保留 I²C1。完整新增引脚与接线见 [M2 接口](m2-interfaces.md)。USB 的 PA11/PA12 保留。
Core的PE3用户灯、PD2用户键、PH2启动脚不参与分配。
左右六针电机插座的线序、编码器滤波和电机功率电路均不变。

## 按当前布局布线

当前PCB正面视图中，J14的A排位于右侧，B排位于左侧。
上方模块是L（J4、U2、U3），下方模块是R（J6、U4、U5）。

1. 先将U3.6的L_ENC_A接A4，将U3.4的L_ENC_B接A3；两根线都落在插槽上端附近。
2. 将L_ISENSE接A5，L_nFAULT接A7，L_DIR接A8，L_PWM接A9，按上方模块成组引入。
3. 下方模块的R_PWM接A27，R_DIR接A34，R_nFAULT接A35。
4. U5.6的R_ENC_A接A33，U5.4的R_ENC_B接A45；必要时用底层绕开驱动及功率线。
5. R_ISENSE仍需引向具备ADC功能的上端A11，沿安静区域布线，避开OUT1/OUT2和开关电源SW节点。
6. PE8/B13位于R6、J10附近，MOTOR_EN_REQ从插槽左排引出。

编码器A/B是普通数字信号，不要求按高速差分线配对或等长。
重点是连续参考地、避免和电机PWM功率线长距离贴近平行，以及保留现有RC和施密特整形。
本次没有代替手工布出这13路信号，也没有移动任何器件或改变已完成的铜线。

## 外设与固件注意事项

左编码器改用TIM5，右编码器改用TIM2，两个计数器独立。
TIM4仅负责左右PWM，CH4与CH3共享频率，不能与编码器复用同一个计数器。
M2 已将 TIM3_CH1/PB4 分配给 TFT 背光，TIM1_CH1/PE9 分配给步进脉冲。

右编码器的物理A相接TIM2的CH2，物理B相接CH1。
这是为方便布线选择的相位顺序，编码器接口仍使用TI1/TI2配对；固件需按车轮前进方向标定计数符号。
PB3用于编码器时不同时启用SWO，双线SWD调试保持可用。

左电流采样现在使用ADC2，右电流采样使用ADC1，需要分别初始化和标定。
旧PA0/PA1的ADC配置不能继续用于电流采样。
MOTOR_EN_REQ上电、复位及故障时保持低；nFAULT输入采用板上的外部上拉。
现有固件仍为空循环，本次未生成或实现这些外设代码。

## 依据与长度估计

复用依据为本地[DS15137 Rev3](../../Hardware/stm32c5a3/Hardware/Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf)的Table13、Table14及§3.26。
TIM5的PA0/PA1为AF2，TIM2的PB3/PA15为AF1，TIM4的PC2/PB8为AF2。
PH5支持ADC2_IN13，PC0支持ADC1_IN8。
验证脚本直接从原厂文本和公共触点表交叉检查，包含定时器占用冲突检查。

按当前焊盘位置估算，四根编码器线的水平加竖直距离合计由约212.8mm降至114.4mm，减少约46%。
该数字未计器件绕障、换层和实际走线，不是成品线长或信号质量测量。
实际检查与剩余问题见[本次记录](../validation/2026-11-03-motor-pin-remap.md)。
