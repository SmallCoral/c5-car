# Motor M2 当前资源与集成范围

2026-11-07，M2.8。保留荔枝派 4A、MaixCAM2 识别、Action 4 图传，ZDT_XS 第二代 TTL 一体套件与感为八路灰度。十页原理图：主页 A2，电源/灰度识别/蓝牙步进/荔枝派 A3，其余 A4。98 触点合同不变，13 路轮控制加 27 路外设，共 40 路 GPIO。

| 资源 | 当前分工 |
| --- | --- |
| TIM5 / TIM2 | 左右编码器，独立计数器 |
| TIM4 CH4 / CH3 | 左右轮 PWM，共享频率 |
| TIM3 CH1 / SPI1 | TFT 背光与显示 |
| USART2 | HC-08，固定三伏三 |
| USART6 | MaixCAM2 UART4，经 U13 隔离 |
| USART1 | ZDT TTL，经 U15 隔离，J23:7/8 |
| USART3 AF7 | PD8/B26 TX、PD9/B27 RX，荔枝派 J18:11/12 UART1 |
| PD12 / B30 | STEP_PWR_EN，高有效、硬件默认断电 |
| 灰度 | 八路数字输入，无内部上下拉；J21 GND/五伏，CLK/DAT 不接 |
| 四键 | 低有效，10k 上拉，固件消抖 |

PE9、PD13/PD14/PD15 仍空闲。PB6/PB7 已用于 USART1，不作 I²C1。PA9 USB 检测、PA11/PA12 USB、PA13/PA14 SWD 以及 Core 专用脚保留。摆杆角度探头尚未画入当前原理图，不能把预想的 AS5600/I²C 写成已集成功能。

双路 AP64500 标称 5.088V：A 轨 4.2A 预算供荔枝派/Core/灰度/TFT蓝牙，B 轨 4.0A 预算供 Maix/Action 4。U7 LDO 与 Core 三伏三分开，D3 隔离不变。12V/12.5A 适配器共用，输入连续预算约 11.09A，启动与热仍需实测。

[详细接线](m2-interfaces.md)、[本轮验证](../validation/2026-11-07-m28-resume.md)。PCB 已同步 180 个封装，新增 33 个待布局，原生一致性 0；仍有 40 条 DRC 与 390 项未连接。固件、机械重建及实物验收未完成。
