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
#define TEST_DURATION_MS		3000U
#define MAIN_STALL_MS		100U
#define TELEMETRY_MS		500U
#define KEY_DEBOUNCE_MS		25U
/* Confirmed against the user's photo: keys above, LCD pins below. */
#define LCD_MADCTL		0x00U

#endif
