#ifndef CORAL_CONFIG_H
#define CORAL_CONFIG_H

#define BOARD_CLOCK_HZ		48000000U
/* Measured on the fitted HC-08, not the module manual's factory default. */
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
/* User verified the fitted harness: J6 = physical left, J4 = physical right. */
#define WHEEL_SWAP_SIDES		1
#define TEST_DURATION_MS		3000U
#define MAIN_STALL_MS		100U
#define TELEMETRY_MS		500U
#define KEY_DEBOUNCE_MS		25U
#define GRAY_SAMPLE_MS		2U
#define GRAY_FILTER_SAMPLES	3U
/* Fitted cable: PE14 is physical left, PE7 is physical right. */
#define GRAY_REVERSE_CHANNELS	1
/* LAP speeds are encoder counts/s (wheel scale uncalibrated). */
#define LAP_CONTROL_MS		10U
#define LAP_CONTROL_MAX_MS	50U
#define LAP_TIMEOUT_MS		90000U
#define LAP_LINE_LOST_MS		150U
#define LAP_AMBIGUOUS_MS		250U
#define LAP_MARKER_CHANNELS	4U
#define LAP_MARKER_HOLD_MS	10U
#define LAP_DEPART_HOLD_MS	100U
#define LAP_DEPART_TIMEOUT_MS	1500U
#define LAP_DEPART_MAX_COUNTS	1000U
#define LAP_ARM_MS		2000U
/* Provisional window for the 6.142 m course (~43k counts/lap), not mm. */
#define LAP_ARM_COUNTS		30000U
#define LAP_MAX_COUNTS		60000U
/* First successful measured lap: 43208 counts. Slow down before its A zone. */
#define LAP_APPROACH_COUNTS	39000U
#define LAP_APPROACH_CPS		1100
#define LAP_PARTIAL_MIN_COUNTS	41000U
#define LAP_PARTIAL_MAX_COUNTS	47000U
#define LAP_PARTIAL_CHANNELS	3U
#define LAP_PARTIAL_HOLD_MS	30U
/* Two clockwise R=0.5 m arcs. Counts are referenced to a detected A start. */
#define LAP_HALF_COURSE_COUNTS	21500
#define LAP_BEND_BRAKE_COUNTS	9800
#define LAP_BEND_ENTRY_COUNTS	10500
#define LAP_BEND_END_COUNTS	22300
#define LAP_BEND_BLEND_COUNTS	1000
#define LAP_COURSE_BEND_CPS	2200
#define LAP_COURSE_BEND_PERMILLE	200
/* Both measured bends need more braking distance on the high gear. */
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
/* Position uses 1000 units/probe; rate uses position units/second. */
#define LAP_CENTER_POSITION	500	/* CH4 and CH5 */
#define LAP_LINE_KP		220
#define LAP_OUTER_KP		850	/* additional gain beyond CH4/5 */
#define LAP_LINE_KD		48
#define LAP_BEND_KI		700	/* permille per probe interval per second */
#define LAP_RACE_BEND_KI		775	/* unmarked start, gray-guided turns */
#define LAP_BEND_MAX_PERMILLE	420	/* half wheel-speed difference / mean */
#define LAP_TRIM_DECAY_MS	120
#define LAP_TURN_LAG_CPS		270
#define LAP_POSITION_DEADBAND	150
#define LAP_POSITION_FILTER_MS	15U
#define LAP_RATE_FILTER_MS	35U
#define LAP_CENTER_RATE_CPS	130
#define LAP_RATE_LIMIT_CPS	650
#define LAP_ENCODER_FILTER_MS	30U
/* Feedback is half the forward wheel-speed difference, not an IMU yaw rate. */
#define LAP_TURN_DAMPING		800
#define LAP_CORNER_SLOWDOWN	750	/* CPS per probe beyond the middle pair */
#define LAP_BRAKE_LOOKAHEAD_MS	60
#define LAP_PWM_RESERVE		20	/* permille of PWM reserved for steering */
#define LAP_CPS_PER_PERMILLE	7	/* wheel feedforward slope */
#define LAP_STEER_SLEW_CPS_S	6000U
#define LAP_EDGE_SLEW_CPS_S	14000U
#define LAP_STEER_MAX_CPS		1400
#define LAP_TELEMETRY_MS		100U
/* Physical sides after WHEEL_SWAP_SIDES: forward left J6+, right J4-. */
#define LAP_LEFT_DRIVE_SIGN	1
#define LAP_RIGHT_DRIVE_SIGN	-1
/* Forward wheel motion produces positive counts on both fitted encoders. */
#define LAP_LEFT_ENCODER_SIGN	1
#define LAP_RIGHT_ENCODER_SIGN	1
/* Confirmed against the user's photo: keys above, LCD pins below. */
#define LCD_MADCTL		0x00U

#endif
