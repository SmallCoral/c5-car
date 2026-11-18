# Coral-C5 遥测 v6

USART1 向 HC-08 发送 ASCII CSV，以 CRLF 结尾。LAP 运行时约每 100 ms 一帧，其余状态每 500 ms 一帧。
帧头为 `C5,6`，共 56 列；[monitor.py](../../Firmware/tools/monitor.py)接收 BLE 分片、校验并保存。
v1 为前 27 列，v2 为前 30 列，v3 为前 37 列，v4 为前 47 列，v5 为前 53 列。接收工具兼容六个版本，旧版缺少字段填 0 并保留版本号；不能把旧版补零的控制诊断当作测量值。
独立 Rust 迁移版仍保留 v2，未同步本轮 C 版灰度、循线和实车左右映射；观察 C 版使用上述 Python 工具。

| 字段 | 含义 |
| --- | --- |
| `tag`, `version` | 固定 `C5`、`6` |
| `ms`, `seq` | MCU 启动毫秒和发送序号，均为 uint32，可回绕 |
| `task` | 0 MONITOR；1 LEFT；2 RIGHT；3 BOTH；4 STEPPER；5 ALL（两轮与步进连续联动）；6 GRAY（只观察灰度）；7 LAP（默认页面，单圈循线） |
| `running` | 应用当前是否在执行任务，不等同于实测机械运动 |
| `duty_permille` | 普通轮测试的 PWM 请求，带方向，单位千分之一；步进、LAP 为 0，LAP 的两轮输出见专用字段 |
| `left_count`, `right_count` | 当前实车左 J6/TIM2、右 J4/TIM5 的原始 x4 正交计数，以带符号 32 位显示；旧记录映射不同，需结合当时源码 |
| `left_cps`, `right_cps` | 约 100 ms 差分所得 count/s，未换算成轮速 |
| `gray_raw` | 未滤波的八路数字输入，已按实车左右排序；当前线束反序，bit0 为最左 PE14、bit7 为最右 PE7。白 1、黑 0；原理图的 PE7/GRAY_CH1 名称不等于实车最左 |
| `encoder_ab` | bit0 实车左 A/PB3、bit1 左 B/PA15、bit2 右 A/PA0、bit3 右 B/PA1 |
| `fault_mask` | bit0 实车左 J6/PC10、bit1 右 J4/PH4；1 表示 nFAULT 为低 |
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
| `gray_black` | 逐通道滤波后黑色掩码，bit0 为 CH1；1 表示检测到黑色 |
| `gray_line` | 当前目标掩码，默认等于 `gray_black`，白线模式时取其八位反码 |
| `gray_count`, `gray_groups` | 目标通道数 0～8、连续目标段数 0～4 |
| `gray_position` | 仅 `gray_status=2` 时有效，排序后的通道坐标平均值，物理左 CH1=-3500、右 CH8=+3500，间距单位 1000；无效时为 0，不保留旧偏差，不代表毫米。旧日志尚未反序，需结合当时源码解释 |
| `gray_status` | 0 WAIT（滤波未就绪）；1 NO LINE（无目标）；2 LINE（单段且非全覆盖，偏差有效）；3 ALL（八路全覆盖）；4 MULTI（多段，偏差无效） |
| `gray_white` | 0 检测黑线（默认），1 检测白线，由 GRAY 页 B/W 键切换 |
| `lap_status` | 0 READY；1 FOLLOW；2 DONE；3 STOPPED；4 LOST LINE；5 BAD LINE；6 ENC ERROR；7 TIMEOUT；8 DRIVE ERR；9 ALIGN LINE；10 LOOP LATE；11 DIR CHECK（Debug 架空检查）；12 NO A MARK（行程上限内未识别 A） |
| `lap_left_pwm`, `lap_right_pwm` | LAP 左右轮 PWM 幅值。4600 CPS 档上限为 600‰，其余档与 3300 稳定版上限均为 450‰。11 月 17 日版为 400‰。电气极性取决于 MAP，停止时清零 |
| `lap_left_target`, `lap_right_target` | 共同前进目标加/减差速后的左右轮目标，编码器计数/秒；不是 RPM 或 m/s |
| `lap_left_cps`, `lap_right_cps` | 根据当前 MAP 归一化为前进为正、低通滤波后的双轮反馈，计数/秒；原始符号仍保留在 `left_cps/right_cps` |
| `lap_ms`, `lap_counts` | 最近一次 LAP 的运行毫秒、两轮正向编码器行程平均值，停止后保留；行程未换算为毫米 |
| `lap_armed` | 已稳定离开起点、运行至少 2 s、平均行程至少 30000 计数，允许将后续横线识别为终点 |
| `lap_position` | 转向使用的低通灰度位置，1000 单位/探头间距；短暂失效时保留最后位置，需结合灰度状态 |
| `lap_position_rate` | 滤波位置变化率，位置单位/秒；非窄线、重新见线的首个周期及停止时清零 |
| `lap_steering` | 实际施加的半轮速差目标，CPS；正值左目标更快，负值右目标更快，停止清零 |
| `lap_differential` | `(lap_left_cps - lap_right_cps) / 2`，CPS；编码器转向反馈，未标定为 rad/s，停止后保留最后样本 |
| `lap_base_target` | 两轮共同速度目标，CPS。4600 档加速斜率为 4300 CPS/s，其余档为 3200 CPS/s，各档减速斜率为 8000 CPS/s。外轮触及目标上限时进一步下调共同目标，停车清零 |
| `lap_control_ms` | 最近一次 LAP 控制调用的实际时间间隔；由 MCU 时钟测量，停止后保留 |
| `lap_departing` | 1 为从起点标记受限直行驶离，屏幕显示 LEAVE A；停止时为 0 |
| `lap_marker_hits` | `lap_armed=1` 后标记候选的上升沿次数，包含预计终点窗口内的三路候选；未满足保持时间的候选也计数，停止后保留 |
| `lap_marker_peak` | `lap_armed=1` 后灰度目标覆盖通道数的最大值，停止后保留，用于判断是否漏采 A |

电脑 CSV 在前面增加 `received_utc`。BT 图标只是 MCU 队列写入结果，不能替代电脑 CSV 证明无线接收。
计数器与输入电平是瞬时采样；静止时 count/s=0 不表示编码器失效。
`wheel_trip=4` 会撤销使能并保持两路 PWM 为零；错误保持到下一次启动请求。PA3 位于 J10 前端，不能据此判断跳帽或驱动端 MOTOR_RUN 的实际状态。
`fault_mask` 为瞬时电平，可能采到初始 5 ms 的 PWM 关闭唤醒窗口；轮保护在该窗口结束后检查，并以 `wheel_trip=1` 锁存。屏幕也保留导致停止的左右故障位。
ALL 中 `duty_permille` 与 `step_target_rpm` 可同时非零，分别表示两轮与步进请求；等待步进速度应答期间轮使能仍为 0。

灰度由主循环以不短于 2 ms 的间隔采样，每路连续三次相同才更新稳定值；循环调度会使实际间隔增大。
原始值保留在 `gray_raw`，模拟量与归一化由辅助板内部处理，C5 八针并行接口不能读取 ADC 数值。
命令 `C5:GRAY\n` 先停止任务再进入 GRAY 页，`monitor.py --gray` 使用此入口；`!` 仍为立即停止命令。
命令 `C5:LAP\n` 先停止再进入 LAP 页，`monitor.py --lap` 使用此入口，不通过蓝牙发起运动；实物 RUN 启动。LAP 固定检测黑线，不继承 GRAY 页的白线目标。
Debug 另有 `C5:CHECK\n`，只在停止时执行一次固定 15% 输出、1.5 s 后停止的架空检查，独立于黑线；Release 不提供此命令。
`C5:PERF\n` 返回独立文本 `C5PERF,1,<max_loop_ms>,<max_display_ms>,<max_telemetry_ms>`；它不是 `C5,6` 的 56 列遥测帧，诊断接收器单独处理。
LAP 的持续运行由独立 90 s 期限限制，步进离线不阻止单独轮循线，轮驱动硬件保护继续生效。
安装方向、黑白校准见[灰度联调](../how-to/grayscale-check.md)，首圈操作见[循线一圈](../how-to/line-following-lap.md)。

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
