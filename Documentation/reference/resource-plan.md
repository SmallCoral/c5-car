# Motor M2 当前资源与集成范围

2026-11-08，M2.8P2。保留荔枝派4A、MaixCAM2识别、Action 4图传、ZDT_XS第二代TTL套件及感为八路灰度。十页原理图、98触点合同不变，13路轮控制加26路外设，共39路GPIO。逐针定义和AF只维护在[当前分配](motor-core-pins.md)。

| 资源 | 当前分工 |
| --- | --- |
| TIM5 / TIM2 | 左右编码器，独立计数器 |
| TIM4 CH4 / CH1 | 左右轮PWM，共享频率 |
| TIM3 CH4 / SPI3 | TFT背光与显示 |
| USART1 | HC-08，固定三伏三 |
| UART5 | MaixCAM2模块侧UART4，三伏三直连 |
| USART6 | ZDT TTL，J23:7/8，VM与UART直连 |
| UART4 | 荔枝派J18:11/12 UART1，保留1.8V/3.3V电平转换 |
| ADC2 / ADC1 | 左右轮电流采样，通道分别为IN13/IN8 |
| 灰度 | 八路数字输入，无内部上下拉；J21 GND/五伏，CLK/DAT 不接 |
| 四键 | 低有效，10k 上拉，固件消抖 |

PD12仍空闲，已无STEP_PWR_EN、独立步进控电或U15；PD15改为模式键，PB6改为右轮PWM。PA9 USB检测、PA11/PA12 USB、PA13/PA14 SWD以及Core专用脚保留。摆杆角度探头尚未画入当前原理图，不能把预想的AS5600/I²C写成已集成功能。

双路 AP64500 标称 5.088V：A 轨 4.2A 预算供荔枝派/Core/灰度/TFT蓝牙，B 轨 4.0A 预算供 Maix/Action 4。U7 LDO 与 Core 三伏三分开，D3 隔离不变。12V/12.5A 适配器共用，输入连续预算约 11.09A，启动与热仍需实测。

[详细接线](m2-interfaces.md)、[原理图功能复核](../validation/2026-11-08-pinplan-rework.md)。
当前142个实体、184网络；原理图ERC0/0、956项及独立320项功能核对是此前原理图阶段记录。
11 月 10 日 PCB 复查已确认有效电气焊盘网络匹配、未连接项为零，安装孔一致性和 DRC 仍未通过，见[PCB 记录](../validation/2026-11-10-motor-pcb-review.md)。
固件、机械重建及实物验收未完成。
