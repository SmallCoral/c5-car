# Coral-C5 遥测 v2

USART1 向 HC-08 每 500 ms 发送一条 ASCII CSV，以 CRLF 结尾。
帧头为 `C5,2`，共 30 列；[monitor.py](../../Firmware/tools/monitor.py)接收 BLE 分片、校验并保存。
v1 为前 27 列，接收工具仍支持 v1，三个速度字段填 0 并保留版本号。

| 字段 | 含义 |
| --- | --- |
| `tag`, `version` | 固定 `C5`、`2` |
| `ms`, `seq` | MCU 启动毫秒和发送序号，均为 uint32，可回绕 |
| `task` | 0 MONITOR；1 LEFT；2 RIGHT；3 BOTH；4 STEPPER；5 ALL（两轮与步进连续联动，默认页面） |
| `running` | 应用当前是否在执行任务，不等同于实测机械运动 |
| `duty_permille` | 本次轮 PWM 请求，带方向，单位千分之一；步进任务为 0 |
| `left_count`, `right_count` | TIM5/TIM2 原始 x4 正交计数，以带符号 32 位显示 |
| `left_cps`, `right_cps` | 约 100 ms 差分所得 count/s，未换算成轮速 |
| `gray_raw` | PE7–PE14，bit0 为通道 1；黑白意义尚未标定 |
| `encoder_ab` | bit0 左 A/PA0、bit1 左 B/PA1、bit2 右 A/PB3、bit3 右 B/PA15 |
| `fault_mask` | bit0 左驱动、bit1 右驱动；1 表示 nFAULT 为低 |
| `enable_request` | PA3 请求轮驱动使能；不是 MOTOR_RUN 回读，也不是 J10 检测 |
| `step_online` | 最近 1 秒内是否收到与待处理命令匹配的回复 |
| `step_phase` | 见下表 |
| `step_flags` | Emm `3A` 返回原始标志：bit0 使能、bit1 位置到达、bit2 堵转；速度模式不以位置到达为完成条件 |
| `step_position_raw` | Emm `36` 的带符号位置原始值，65536 对应电机轴一圈，未作机构角度标定 |
| `step_error` | 驱动拒绝码或本地错误码，十进制传输；见下表 |
| `uart_errors` | 两个 UART 的帧、奇偶、噪声、溢出错误事件总数 |
| `rx_drops`, `tx_drops` | 两串口接收丢字节次数、发送队列不足而拒绝整次写入次数 |
| `lcd_ok` | 初始化及截至当前的 SPI 传输是否成功，不是屏幕光学回读 |
| `wheel_trip` | 0 正常；1 nFAULT；2 单独轮测试三秒到期（ALL 不适用）；3 主循环超过 100 ms 未更新心跳；4 请求使能至少 5 ms 后 PA3 引脚仍为低 |
| `step_timeouts` | 200 ms 命令应答超时累计次数 |
| `step_position_valid` | 是否至少读到一次合法位置；离线后保留最后值，需结合 `step_online` |
| `step_target_rpm` | 已发起步进运动的目标转速，CW 正、CCW 负；停止流程为 0 |
| `step_rpm`, `step_rpm_valid` | `35` 返回的带符号 RPM，以及已读到且当前在线的标志 |

电脑 CSV 在前面增加 `received_utc`。BT 图标只是 MCU 队列写入结果，不能替代电脑 CSV 证明无线接收。
计数器与输入电平是瞬时采样；静止时 count/s=0 不表示编码器失效。
`wheel_trip=4` 会撤销使能并保持两路 PWM 为零；错误保持到下一次启动请求。PA3 位于 J10 前端，不能据此判断跳帽或驱动端 MOTOR_RUN 的实际状态。
`fault_mask` 为瞬时电平，可能采到初始 5 ms 的 PWM 关闭唤醒窗口；轮保护在该窗口结束后检查，并以 `wheel_trip=1` 锁存。屏幕也保留导致停止的左右故障位。
ALL 中 `duty_permille` 与 `step_target_rpm` 可同时非零，分别表示两轮与步进请求；等待步进速度应答期间轮使能仍为 0。

## 步进状态

| 值 | 名称 | 行为 |
| --- | --- | --- |
| 0 | BOOT | 等待驱动启动 |
| 1 | IDLE | 已完成停止/失能，轮询反馈 |
| 2 | ENABLE | 等待使能应答 |
| 3 | SPEED | 等待 F6 连续速度命令应答 |
| 4 | MOVING | 持续运动，轮询状态/速度/位置 |
| 5 | STOP | 等待立即停止应答 |
| 6 | DISABLE | 等待失能应答 |
| 7 | OFFLINE | 下次仅尝试停止/失能恢复，不自动重发运动 |

本地错误码：`E0` 应答超时、`E1` TX 队列拒绝、`E3` 运动期间 UART 接收损坏。
驱动非 `02` 应答原样保存；`E2` 也用于堵转标志。
上电时“已失能所以拒绝停止”的错误，在收到成功失能应答后清除；运动期间错误保持到下次手动启动。
清除堵转保护、参数存储、回零、X 固件与 Modbus 不属于当前实现。
