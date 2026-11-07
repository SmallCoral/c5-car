# Motor M2 当前资源与集成范围

2026-11-07 原理图为 M2.7。MaixCAM2 负责识别，Action 4 只从 Type-C 取电作图传，荔枝派电路继续删除。步进采用已购 ZDT_XS 第二代 TTL 一体套件，只保留 J23 8P XH，新增 MCU 控制的默认断电电源。

九页包括 A3 总览、电源、灰度与识别、蓝牙与步进，以及五张 A4 功能页。13 路轮控制分配保留，加 25 路外设信号共 38 路 GPIO；98 触点合同不变。

| 资源 | 当前分工 |
| --- | --- |
| TIM5 / TIM2 | 左右编码器 |
| TIM4 CH4 / CH3 | 左右轮 PWM |
| TIM3 CH1 | TFT 背光 |
| SPI1 | TFT SCK/MOSI，另有 CS/DC/RES |
| USART2 | HC-08 |
| USART6 | MaixCAM2 识别 UART4，经 U13 隔离 |
| USART1 | ZDT TTL，经 U15 隔离，J23:7/8 |
| PD12 / B30 | STEP_PWR_EN，高有效、硬件默认断电 |
| 灰度 | 八路数字输入；J21 GND/五伏，CLK/DAT 不接 |
| 四键 | 低有效，10k 上拉，固件消抖；C57～C60 删除 |

USART3 的 PD8/PD9、原脉冲接口 PE9/PD14/PD15 释放；PD13 继续空闲。PB6/PB7 已用于 USART1，不作 I²C1。PA9 的 USB 检测、PA11/PA12 USB、PA13/PA14 SWD 和既有 Core 专用脚保留。

U1 TPS56837 为统一五伏主降压，5.2A 总负载预算、芯片等级 8A；U6/L2 删除。U7 LDO 供 TFT 与蓝牙。Core 经 D3 后 A48/B48 供电，本板 AP63203 产生三伏三。Maix 六针座限一安预算，Action 4 端口五伏三安通告；10V 电池端预算约 10.054A，仍需实测。

[当前接线](m2-interfaces.md)、[验证与限制](../validation/2026-11-07-m27-unified-power.md)。PCB、固件与实物验证分别完成，不能由原理图通过推定其完成。
