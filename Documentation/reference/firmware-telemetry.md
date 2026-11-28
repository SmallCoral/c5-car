# Coral-C5 蓝牙遥测

USART1 向 HC-08 发送 ASCII CSV，以 CRLF 结尾。LAP 运行时约每 100 ms 一帧，其余状态每 500 ms 一帧。
普通比赛与调试页帧头为 `C5,6`，共 56 列。第三、四题和发挥一的准备、运行和结果页采用下文的 v7。[monitor.py](../../Firmware/tools/monitor.py)接收 BLE 分片、校验并保存。
v1 为前 27 列，v2 为前 30 列，v3 为前 37 列，v4 为前 47 列，v5 为前 53 列。接收工具兼容七个版本，旧版缺少字段填 0 并保留版本号；不能把旧版补零的控制诊断当作测量值。

| 字段 | 含义 |
| --- | --- |
| `tag`, `version` | 固定 `C5`、`6` |
| `ms`, `seq` | MCU 启动毫秒和发送序号，均为 uint32，可回绕 |
| `task` | 0 MONITOR、1 LEFT、2 RIGHT、3 BOTH、4 STEPPER、5 ALL（两轮与步进连续联动）、6 GRAY、7 LAP（单圈循线）、8 BALANCE（停车稳球调试模式） |
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
| `lap_armed` | 第二、四题和发挥一驶离起点、连续看到纵线 100 ms 后置 1，开始检测 A 标记。第三题按里程停车，此位为 0。设备调试 LAP 页另要求运行满 2 s、行程达到 30000 计数 |
| `lap_position` | 转向使用的低通灰度位置，1000 单位/探头间距；短暂失效时保留最后位置，需结合灰度状态 |
| `lap_position_rate` | 滤波位置变化率，位置单位/秒；非窄线、重新见线的首个周期及停止时清零 |
| `lap_steering` | 实际施加的半轮速差目标，CPS；正值左目标更快，负值右目标更快，停止清零 |
| `lap_differential` | `(lap_left_cps - lap_right_cps) / 2`，CPS；编码器转向反馈，未标定为 rad/s，停止后保留最后样本 |
| `lap_base_target` | 两轮共同速度目标，CPS。4600 档加速斜率为 4300 CPS/s，其余档为 3200 CPS/s，各档减速斜率为 8000 CPS/s。外轮触及目标上限时进一步下调共同目标，停车清零 |
| `lap_control_ms` | 最近一次 LAP 控制调用的实际时间间隔；由 MCU 时钟测量，停止后保留 |
| `lap_departing` | 1 表示起步阶段低速直行。比赛第二题在等待首次识别纵向窄线时显示 FIND LINE，设备调试 LAP 页驶离起点标记时显示 LEAVE A |
| `lap_marker_hits` | 启用终点检测后标记候选出现的次数。比赛整圈接受 4～5 路，调试 LAP 页在终点窗口内也接受三路候选。未持续到停车时长的候选也计数，停止后保留 |
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

`C5:BALL?\n` 返回 `C5BALL,1,frames,rejected,sequence,flags,age,x,state,permille,updates,shown,shown_permille,uart_errors,rx_drops`。位置 0、500、1000 分别对应实物左端、中点、右端。`state` 的 0 为离线，1 为未见球，2 为可信直接观测，3 为坐标待确认。仅 2 有实时位置。`shown` 和 `shown_permille` 是 BALANCE 或 LAP 最近完成绘制的状态和位置。固件 `78F0CB6C` 起，`shown=4` 表示最多 2 秒前的可信位置，屏上标为 `LAST SEEN` 并画空心黄圈。`updates=0` 表示尚未完成水管绘制。板上 `EB453865` 的 640 像素标定中，`shown_permille` 使用实测五点显示标尺，`L、-5、O、+5、R` 分别为 0、263、500、762、1000。LCD 可从三次连续且位置相邻的直接观测重新取得静止球位，因此 `state=3` 时也可能出现 `shown=2`。控制环使用 `state` 和实物 O 对应的偏差，640 像素图像按 2:1 换算到原控制尺度。末两项来自相机 UART5，运动轨迹的锁定状态见 `C5:ROLL?`。
LAP 的持续运行由独立 90 s 期限限制，步进离线不阻止单独轮循线，轮驱动硬件保护继续生效。
安装方向、黑白校准见[灰度联调](../how-to/grayscale-check.md)，首圈操作见[循线一圈](../how-to/line-following-lap.md)。

## 比赛菜单

`C5:MENU` 停止运动并回到比赛主页。`C5:MENU?` 只读，返回 `C5MENU,1,page,item,notice,leg,running,wheels,elapsed_ms`。page 的 0～5 依次为设备调试页、比赛主页、题目详情、运行、结果、调试列表。item 的 1～6 对应四道基本题和两道发挥题。在第一题运行页，leg 按路线依次为 3（确认水平）、4（向 O）、0（向右 5 cm）、1（折返左 5 cm）、2（保持左点）。详情页确认启动时开始计时。兼容查询 `C5:ROLL?` 中的 boot_pending 为 0。[阶段枚举](../../Firmware/include/contest.h)

## 第三、四题与发挥一的行驶稳球

从 `855500CB` 起，可在起跑后或停车后只读查询 `C5:RANGE?`。返回 `C5RANGE,1,start_ms,target_mm,samples,missing,min_error_mm,max_error_mm,outside,min_at_ms,max_at_ms,max_gap_ms,last_at_ms,crc32`。主控按序号更新的相机包统计，仅计车轮循线阶段。可信球位进入样本数和有符号极值，绝对偏差严格大于 20 mm 才计一次超差。`missing` 计收到但无可信球位的包，断流由有效观测间隔反映。各时刻相对起跑，`max_gap_ms` 包含起跑至首样本，终点未覆盖时段用圈时减 `last_at_ms`。零样本时极值无效。查询行末 CRC32 的计算与 v7 相同，下次起跑重置统计。

从 C 应用 `36AD515A` 起，第三、四题同样支持 `C5:RANGE?` 和下述 v7，固定目标为 0 mm。发挥一可选目标。三题的准备、原地稳球、循线和结果页每 100 ms 发送一帧 `C5,7`。前 56 列沿用 v6。新增的 13 列按下表顺序排列。每帧同时记录两轮目标和实际速度，可用 `lap_counts` 对齐一圈中的路段。停车后的实际轮速取 `left_cps`、`right_cps`，这些字段在静止和结果页继续采样。

| 新增字段 | 含义 |
| --- | --- |
| `offset_phase`, `phase_ms` | 1 选点、2 原地稳球、3 循线稳球、4 结果。选点为 0 ms；开始稳球与起跑分别重置计时，结果页保留结束时长。 |
| `target_mm` | O 为零，右正左负。 |
| `ball_status`, `camera_seq`, `camera_rx_ms`, `camera_age_ms`, `ball_mm` | 主控判定 0 离线、1 漏检、2 可信实测、3 不确定。相机包序号、该包的主控接收时刻、包内年龄和标定球位。采集程序仅在状态 2 时写入球位和偏差。 |
| `wheel_sample_ms`, `shaft_goal_cd`, `shaft_actual_cd`, `event_notice` | 编码器差分采样的主控毫秒时间、稳球目标轴角、实际轴角（相对标定水平，0.01°）、菜单提示码。目标轴角在运行页有效；从 `24978E6C` 起，正常完圈后仍在闭环的结果页也上报目标轴角。 |
| `frame_crc32` | 从行首到提示码的 ASCII 字节计算 CRC32，线上发送 8 位十六进制，采集程序验证后存为十进制。 |

CSV 另存 `mean_accel_cps2` 与 `ball_error_mm`。前者用相邻两帧的实际平均轮速差除以轮速采样时间差，单位为编码器计数/秒²。后者为实测球位减目标，正数表示偏右。缺失观测时偏差留空，遥测序号不连续时加速度留空。以主控时钟分析，相机每个遥测周期只记录最新一包，采样间的峰值可能更大。[实车抽样与逐帧统计](../validation/README.md)

## 步进状态

| 值 | 名称 | 行为 |
| --- | --- | --- |
| 0 | BOOT | 等待驱动启动 |
| 1 | IDLE | 已完成停止/失能，轮询反馈 |
| 2 | ENABLE | 等待使能应答 |
| 3 | SPEED | 等带转速和限位终点的 FD 定位命令应答 |
| 4 | MOVING | 执行有限行程，轮询状态、速度和位置 |
| 5 | STOP | 等待立即停止应答 |
| 6 | DISABLE | 等待失能应答 |
| 7 | OFFLINE | 下次仅尝试停止/失能恢复，不自动重发运动 |
| 8 | POSITION | BALANCE 使能后，用 FD 发送绝对轴位，并用 43、3A 轮询轴位和驱动状态 |
| 9 | BRAKE | 停止轴运动，保留驱动使能 |
| 10 | PARKED | 继续读编码器和驱动标志，电机抱持连杆 |
| 11 | LATCH | 正在使能抱持并读取参考轴位 |
| 12 | BAL_SPEED | 同学控制环请求转速，非零转速使用限位 FD，轮询 43 轴位、3A 状态，故障发 FE 抱停 |

`C5:BAL?` 返回 `C5BAL,1,ms,state,active,leveled,map,ball_state,ball_permille,velocity,target_cd,shaft_cd,zero_raw,position_raw,pulses_per_rev,phase,error,flags,wheels,centered_ms`。F407 稳球时，`target_cd` 为管角目标折算的电机轴角，`shaft_cd` 是参考轴位以来的实测轴角，单位均为 0.01°。`leveled` 表示有软件参考轴位。球速的单位是显示管长千分比/秒，用于稳态显示。状态 0～7 为待启动、参考就绪、正向往返、返回、等球、调节、稳定、驱动错误。状态 8 和 12 用于手动试抬，11 用于退回起点。

`C5:BAL` 停车并进入该页。在 BALANCE 调试页，`C5:HOLD` 和面板 SW2 从当前位置开始滚动寻球。启动等候相机有数据包和 15 ms 内的编码器反馈。`C5:MAP+` 为默认安装方向，`C5:MAP-` 翻转稳球纠偏及首次倾斜方向。任意键或 `!` 取消本次启动并抱停。车轮保持 `wheels=0`。

`C5:ROLL?` 返回 `C5ROLL,1,ms,reset_flags,boot_pending,state,leg,origin_raw,lower_raw,upper_raw,lock_frames,lock_dx,lock_px_s,lock_ms`。13 表示正在滚动寻球，14 表示到达本次搜索端点，等待球开始滚动，15 表示相机断流期间原位保持。相机恢复后继续判断钢球轨迹。锁定时间从本次寻球开始计，坐标差和速度按实物左负右正。总计时 `C5:TIME?` 还包括启动抱持。

`C5:JOG?` 返回 `C5JOG,1,ms,state,steps,origin_raw,target_raw,actual_raw,limited,drive_flags,drive_error`。点动状态依次为 0 待机、1 等待抱持与新轴位、2 定位、3 抱持、4 故障。每一级为电机轴 1.8°，左键为负，右键为正。`limited=1` 表示最近一次按键的目标超出行程。物理水平和两端均由 0x43 同包的多圈位置和单圈绝对角恢复。[实物水平及上下限标定](../../Firmware/calibration/camera640-and-shaft.json)

`C5:LIMIT?` 返回 `C5LIMIT,1,valid,level_raw,highest_raw,lowest_raw,actual_raw`。后三个标定位置与实测位置使用本次驱动上电的多圈计数。物理轴角为 `(actual_raw-level_raw)*360/65536`。仅 valid=1 时接受运动请求。`C5:BAL?` 的 zero_raw 是控制参考，active 和 leveled 都为零时使用 LIMIT 查询物理轴角。

`C5:CAL` 进入手动标定页。`C5:CAL?` 返回 `C5CAL,1,ms,cal,camera_ms,frames,seq,flags,age_ms,x,position,speed,encoder,encoder_ms,phase,drive_flags,drive_error,wheels`。MaixCAM2 V2 的 x 范围为 0～639，position 单位 0.01 cm，speed 单位 0.1 cm/s，两者均有符号。标定程序对 CRC 正确且序号更新的有效实测帧取样，排除预测、异常候选和过期帧。三个机构姿态使用现场确认位置时的驱动编码器，65536 计数为电机轴一圈。

`C5:LOCK` 从 BALANCE 待机页在现有位置使能步进并抱持。水平标定使用 `C5:LEVEL` 或长按 SW3。软件重启后可通过 `C5:LOCK` 恢复抱持。

`C5:F407?` 返回 `C5F407,1,ms,ball_state,raw_x,offset_px,target_beam_cd,estimated_beam_cd,requested_rpm,sent_rpm,speed_loop`。有效偏差按实物左负右正。两个管角的单位为 0.01°。转速按原代码的符号，正为驱动 CCW，负为 CW。最后一项为 1 时速度环运行。主控在 24 ms 应答超时、40 ms 无新编码器反馈或收到损坏数据时先发 FE 停车并保持使能，`flags & 1` 可核对使能。完全失去串口后，主控无法确认 FE 到达驱动。

无直接球位达到 120 ms 时，转速降为零，界面显示 WAIT BALL。3 s 内取得可信新观测会恢复闭环，重新初始化外环差分。连续 3 s 无可信观测时，自动回到本次上电的行程边界内重新寻球，总计时继续。重新取得球位后从零累计连续居中时长。

维护命令 `C5:LEVEL` 记录参考，`C5:TRY` 发约 4° 的有限相对试步，`C5:TILT` 做定角往返，`C5:REST` 以不超过 20 rpm 回到首次记录的起点。未起动时可用 `C5:UP-` 或 `C5:UP+` 选试步方向。`C5:LIFT?` 回复 `C5LIFT,1,cw_sign,origin_valid,origin_raw,lower_raw,upper_raw`。相对试步丢应答后读回轴位。已承托时可发 `C5:FREE` 释放并清除参考。抱持期间升级请求返回 `C5OTA,BEAM HELD`。

兼容查询 `C5:LOOP?` 返回 `C5LOOP,1,ms,demand_cd,shaft_cd,goal_cd,requested_rpm,sent_rpm,motor_uart_errors,motor_rx_drops,motor_timeouts`。速度环运行时 `demand_cd` 和 `goal_cd` 都显示折算的轴角，速度字段为绝对值，带符号速度以 `C5:F407?` 为准。末三项分别是步进串口接收错误、丢字节与命令超时的累计次数。

带驱动串口诊断的固件可查询 `C5:SLINK?`，回复为 `C5SLINK,1,ms,framing,noise,overrun,parity,last_bad_phase,last_bad_command`。四项计数来自 USART6 的帧格式、噪声、接收溢出和校验标志。同一字节可以触发多项标志。末两项记录最近一次接收出错或丢字节时的驱动状态和当时等待应答的命令，命令值为十进制，0 表示当时没有待应答命令。

`C5:TIME?` 返回 `C5TIME,1,operation,counting,elapsed_ms,tracking_ms,steady_ms`。操作编号 1 为稳球，2 为试抬，3 为定角往返，4 为退回。所有毫秒数都从主控接受本次指令时起算，包含初始转轴行程。后两项分别记录首次进入闭环调节和首次通过持续一秒的稳态判定。稳态要求换算后的控制坐标在标定中点 ±1 控制像素、滤波球速不超过显示管长的 6%/s。值为 0 表示本次尚未达到该状态。停车后保留本次结果。

本地错误码包括 `E0` 应答超时、`E1` TX 队列拒绝、`E3` 运动期间 UART 接收损坏、`E4` 超出轴位限制、`E5` 顺时针试步未取得可信的编码器位移。
驱动非 `02` 应答原样保存；`E2` 也用于堵转标志。
上电时“已失能所以拒绝停止”的错误，在收到成功失能应答后清除；运动期间错误保持到下次手动启动。
清除堵转保护、参数存储、回零、X 固件与 Modbus 不属于当前实现。
