/* 实车调参入口。波特率属于各串口，轮速用编码器计数/秒，行程用计数；两题各自的终点和载球速度在这里设定。 */

#ifndef CORAL_CONFIG_H
#define CORAL_CONFIG_H

#define BOARD_CLOCK_HZ		48000000U
/* 当前实车 HC-08 的串口实测速率。 */
#define BT_BAUD			57600U
#define STEP_BAUD		115200U
#define CAMERA_BAUD		115200U
#define STEP_ADDRESS		1U
#define STEP_RPM_STEP		30U
#define STEP_ACCEL		10U
#define STEP_REPLY_TIMEOUT_MS	200U
#define STEP_POLL_MS		200U
#define STEP_BOOT_MS		1000U
#define WHEEL_PWM_HZ		20000U
#define WHEEL_WAKE_MS		5U
#define WHEEL_MAX_PERMILLE	450
#define WHEEL_LEVEL_COUNT	4U
#define WHEEL_DEFAULT_LEVEL	3U
/* 实车线束已核对，J6 接左轮，J4 接右轮。驱动、编码器和故障位一起换边。 */
#define WHEEL_SWAP_SIDES		1
#define TEST_DURATION_MS		3000U
#define MAIN_STALL_MS		100U
#define TELEMETRY_MS		500U
#define KEY_DEBOUNCE_MS		25U
#define GRAY_SAMPLE_MS		2U
#define GRAY_FILTER_SAMPLES	3U
/* 线束反序，PE14 对应实物最左探头，PE7 对应最右。转换后 CH1 到 CH8 从左向右。 */
#define GRAY_REVERSE_CHANNELS	1
/* 循线轮速用编码器计数/秒表示，轮径和每转计数尚未标定。 */
#define LAP_CONTROL_MS		10U
#define LAP_CONTROL_MAX_MS	50U
#define LAP_TIMEOUT_MS		90000U
#define LAP_LINE_LOST_MS		150U
#define LAP_AMBIGUOUS_MS		250U
#define LAP_MARKER_CHANNELS	4U
/* A 标记边缘能见 3 路，比赛按现场选定的 4～5 路停车。起点须退到普通纵线才允许判停。 */
#define LAP_DEPART_LINE_MAX 2U
#define LAP_RACE_MARKER_MIN 4U
#define LAP_RACE_MARKER_MAX 5U
#define LAP_MARKER_HOLD_MS	10U
#define LAP_DEPART_HOLD_MS	100U
#define LAP_DEPART_TIMEOUT_MS	1500U
#define LAP_DEPART_MAX_COUNTS	1000U
#define LAP_ARM_MS		2000U
/* 按 6.142 m 赛道暂定的计数窗口，整圈约 4.3 万编码器计数。 */
#define LAP_ARM_COUNTS		30000U
#define LAP_MAX_COUNTS		60000U
/* 第二题按四次经过 A 线的里程中位数安排减速，压到标记时停车。 */
#define LAP_DISTANCE_COUNTS	41800U
/* 载球整圈用历史回 A 里程安排减速，越过预计终点后保持收尾速度找标记。 */
#define LAP_OFFSET_DISTANCE_COUNTS	42000U
/* A→B 只有 1.5 m 直线。按载球整圈 6142 mm 的实测计数折算，B 点实车校正可单独改此门槛。 */
#define LAP_STRAIGHT_DISTANCE_COUNTS ((LAP_OFFSET_DISTANCE_COUNTS * 1500U + 3071U) / 6142U)
#define LAP_FINISH_BLEND_COUNTS	900U
#define LAP_FINISH_CPS		1100
/* 横线识别调试在接近终点时降速，以便多次采到 A 线。 */
#define LAP_APPROACH_COUNTS	39000U
#define LAP_APPROACH_CPS		1100
#define LAP_PARTIAL_MIN_COUNTS	41000U
#define LAP_PARTIAL_MAX_COUNTS	47000U
#define LAP_PARTIAL_CHANNELS	3U
#define LAP_PARTIAL_HOLD_MS	30U
/* 顺时针赛道含两段半径 0.5 m 的圆弧，里程从 A 点起算。 */
#define LAP_HALF_COURSE_COUNTS	21500
#define LAP_BEND_BRAKE_COUNTS	9800
#define LAP_BEND_ENTRY_COUNTS	10500
#define LAP_BEND_END_COUNTS	22300
#define LAP_BEND_BLEND_COUNTS	1000
#define LAP_COURSE_BEND_CPS	2200
#define LAP_COURSE_BEND_PERMILLE	200
/* 实测两段弯在高速档均需更长的提前制动距离。 */
#define LAP_RACE_BRAKE_COUNTS	9300
#define LAP_RACE_BEND_CPS	3100
#define LAP_COURSE_TRIM_PERMILLE	70
#define LAP_COURSE_TURN_PERMILLE	380
#define LAP_COURSE_TRIM_KI	110
#define LAP_COURSE_OUTER_KP	150
#define LAP_STALL_GRACE_MS	1200U
#define LAP_STALL_MS		500U
#define LAP_MAX_PERMILLE		WHEEL_MAX_PERMILLE
#define LAP_SPEED_SLOW_CPS	800
#define LAP_SPEED_NORMAL_CPS	1800
#define LAP_SPEED_TRACK_CPS	2200
#define LAP_SPEED_FAST_CPS	3300
#define LAP_SPEED_RACE_CPS	4600
#define LAP_OFFSET_LEVEL	1U /* 发挥一复用普通档的转向控制 */
#define LAP_OFFSET_ACCEL_CPS_S	800
#define LAP_OFFSET_CRUISE_CPS	1400
#define LAP_OFFSET_GAP_DECEL_CPS_S	1800
#define LAP_OFFSET_FINISH_COUNTS	2500U
#define LAP_OFFSET_FINISH_CPS	450
#define LAP_FAST_LEVEL		3U
#define LAP_RACE_LEVEL		4U
#define LAP_SPEED_LEVELS		(LAP_RACE_LEVEL + 1U)
#define LAP_DEFAULT_LEVEL	LAP_RACE_LEVEL
#define LAP_CHECK_MS		1500U
#define LAP_CHECK_PERMILLE	150
#define LAP_TARGET_MIN_CPS	200
#define LAP_TARGET_MAX_CPS	3500
#define LAP_RACE_WHEEL_MAX_CPS	4800
#define LAP_RACE_MAX_PERMILLE	600
#define LAP_ACCEL_CPS_S		3200
#define LAP_RACE_ACCEL_CPS_S	4300
#define LAP_DECEL_CPS_S		8000
/* 相邻灰度探头相差 1000 个位置单位，位置变化率按每秒计算。 */
#define LAP_CENTER_POSITION	500	/* 中间两个探头 CH4 和 CH5。 */
#define LAP_LINE_KP		220
#define LAP_OUTER_KP		850	/* 偏出 CH4、CH5 后增加的纠偏增益。 */
#define LAP_LINE_KD		48
#define LAP_BEND_KI		700	/* 每偏一个探头间距时，每秒增加的千分比。 */
#define LAP_RACE_BEND_KI		775	/* 无 A 点参考时由灰度引导转弯。 */
#define LAP_BEND_MAX_PERMILLE	420	/* 两轮速度差的一半除以平均轮速。 */
#define LAP_TRIM_DECAY_MS	120
#define LAP_TURN_LAG_CPS		270
#define LAP_POSITION_DEADBAND	150
#define LAP_POSITION_FILTER_MS	15U
#define LAP_RATE_FILTER_MS	35U
#define LAP_CENTER_RATE_CPS	130
#define LAP_RATE_LIMIT_CPS	650
#define LAP_ENCODER_FILTER_MS	30U
/* 反馈取两轮前向速度差的一半，单位仍为编码器计数/秒。 */
#define LAP_TURN_DAMPING		800
#define LAP_CORNER_SLOWDOWN	750	/* 每偏离中间探头一个间距所扣的计数/秒。 */
#define LAP_BRAKE_LOOKAHEAD_MS	60
#define LAP_PWM_RESERVE		20	/* 为转向预留的 PWM 千分比。 */
#define LAP_CPS_PER_PERMILLE	7	/* 从 PWM 估算轮速的前馈斜率。 */
#define LAP_STEER_SLEW_CPS_S	6000U
#define LAP_EDGE_SLEW_CPS_S	14000U
#define LAP_STEER_MAX_CPS		1400
#define LAP_TELEMETRY_MS		100U
/* 轮组映射后，实车前进对应左轮 J6 正、右轮 J4 负。 */
#define LAP_LEFT_DRIVE_SIGN	1
#define LAP_RIGHT_DRIVE_SIGN	-1
/* 两只编码器都已换算成前进为正。 */
#define LAP_LEFT_ENCODER_SIGN	1
#define LAP_RIGHT_ENCODER_SIGN	1
/* 按实物照片确定屏幕方向，按键在上，LCD 引脚在下。 */
#define LCD_MADCTL		0x00U

#endif
