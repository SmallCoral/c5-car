#ifndef CORAL_CONFIG_H
#define CORAL_CONFIG_H

#define BOARD_CLOCK_HZ		48000000U
/* Measured on the fitted HC-08, not the module manual's factory default. */
#define BT_BAUD			57600U
#define STEP_BAUD		115200U
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
/* First-track tuning values, in encoder counts/s (wheel scale uncalibrated). */
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
#define LAP_STALL_GRACE_MS	1200U
#define LAP_STALL_MS		500U
#define LAP_MAX_PERMILLE		400
#define LAP_SPEED_SLOW_CPS	800
#define LAP_SPEED_NORMAL_CPS	1800
#define LAP_SPEED_FAST_CPS	2200
#define LAP_SPEED_LEVELS		3U
#define LAP_DEFAULT_LEVEL	2U
#define LAP_CHECK_MS		1500U
#define LAP_CHECK_PERMILLE	150
#define LAP_TARGET_MIN_CPS	200
#define LAP_TARGET_MAX_CPS	2800
#define LAP_ACCEL_CPS_S		2000
#define LAP_DECEL_CPS_S		4000
/* Position uses 1000 units/probe; rate uses position units/second. */
#define LAP_LINE_KP		200
#define LAP_LINE_KD		20
#define LAP_POSITION_DEADBAND	200
#define LAP_POSITION_FILTER_MS	20U
#define LAP_RATE_FILTER_MS	40U
#define LAP_RATE_LIMIT_CPS	500
#define LAP_ENCODER_FILTER_MS	30U
/* Feedback is half the forward wheel-speed difference, not an IMU yaw rate. */
#define LAP_TURN_DAMPING		250
#define LAP_CORNER_SLOWDOWN	200
#define LAP_STEER_SLEW_CPS_S	6000U
#define LAP_STEER_MAX_CPS		1100
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
