#include <stdio.h>
#include <string.h>
#include "stm32c5xx.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "logic.h"
#include "ota.h"
#include "serial.h"
#include "stepper.h"

static struct controls controls = {
	.task = TASK_ALL, .level = WHEEL_DEFAULT_LEVEL
};
static struct debounce keys[KEY_COUNT];
static struct stepper step;
static struct ui_state ui;
static uint32_t previous_encoder[2];
static uint32_t sampled_at;
static uint32_t telemetry_at;
static uint32_t sequence;
static bool lcd_ok;
static bool wheels_started;
static bool upgrade_requested;
static uint32_t upgrade_at;
static char remote_line[24];
static size_t remote_used;

#ifdef CORAL_DEBUG_KEYS
/* SWD mailbox: write 0xc5000001 + key_id to simulate one debounced press.
 * The main loop consumes it once; physical keys take priority. Release builds
 * have no mailbox, and neither build starts a motion automatically at boot.
 */
volatile uint32_t debug_key_request;
/* Last stop: 1 key, 2 BLE, 3 nFAULT, 4 deadline, 5 wheel trip, 6 stepper.
 * High byte holds the fault mask or wheel_trip at the stopping decision.
 */
volatile uint32_t debug_stop_reason;
#define TRACE_STOP(reason) (debug_stop_reason = (reason))
#else
#define TRACE_STOP(reason) ((void)0)
#endif

static void stop_test(uint32_t now)
{
	board_wheels_stop();
	if (stepper_running(&step))
		stepper_stop(&step, now);
	controls.running = false;
	wheels_started = false;
}

static bool start_wheels(void)
{
	int duty = controls_duty(&controls);

	wheels_started = board_wheels_start(
		controls.task == TASK_RIGHT ? 0 : duty,
		controls.task == TASK_LEFT ? 0 : duty,
		controls.task == TASK_ALL);
	return wheels_started;
}

static void handle_remote(uint8_t byte, uint32_t now)
{
	if (byte == '!') {
		if (controls.running)
			TRACE_STOP(2);
		stop_test(now);
		remote_used = 0;
		return;
	}
	if (byte == '\r')
		return;
	if (byte != '\n') {
		if (remote_used < sizeof(remote_line) - 1)
			remote_line[remote_used++] = (char)byte;
		else
			remote_used = 0;
		return;
	}
	remote_line[remote_used] = '\0';
	remote_used = 0;
	if (!strcmp(remote_line, "C5:OTA") && !upgrade_requested) {
		stop_test(now);
		stepper_stop(&step, now);
		upgrade_requested = true;
		upgrade_at = now;
		ui.updating = true;
		ui.update_status = "PREPARING";
		serial_write(SERIAL_BT, (const uint8_t *)"C5OTA,REBOOT\r\n", 14);
	} else if (!strcmp(remote_line, "C5:INFO")) {
		const struct ota_header *header = (const struct ota_header *)OTA_APP_META;
		char line[64];
		int length;

		if (!ota_header_valid(header))
			return;
		length = snprintf(line, sizeof(line), "C5APP,1,%08lX,%lu\r\n",
				  (unsigned long)header->crc, (unsigned long)header->length);
		if (length > 0 && (size_t)length < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)length);
	}
}

static void upgrade_poll(uint32_t now)
{
	volatile uint32_t *request = (volatile uint32_t *)OTA_REQUEST_ADDR;

	if (!upgrade_requested || now - upgrade_at < 1200 ||
	    !serial_tx_idle(SERIAL_BT) || !serial_tx_idle(SERIAL_STEP))
		return;
	request[0] = OTA_REQUEST_MAGIC;
	request[1] = ~OTA_REQUEST_MAGIC;
	__DSB();
	NVIC_SystemReset();
}

static void handle_keys(uint32_t now)
{
	uint8_t raw = board_keys();
	unsigned int index;
	int event = -1;
	bool was_running = controls.running;

	for (index = 0; index < KEY_COUNT; index++) {
		if (key_update(&keys[index], raw & (1U << index), now))
			event = (int)index;
	}
#ifdef CORAL_DEBUG_KEYS
	{
		uint32_t request = debug_key_request;

		debug_key_request = 0;
		if (event < 0 && !raw && request >= 0xc5000001U &&
		    request < 0xc5000001U + KEY_COUNT)
			event = (int)(request - 0xc5000001U);
	}
#endif
	if (event < 0)
		return;
	controls_key(&controls, (enum key_id)event, now);
	if (was_running) {
		TRACE_STOP(1);
		stop_test(now);
		return;
	}
	if (!controls.running)
		return;
	TRACE_STOP(0);
	if (task_uses_stepper(controls.task)) {
		if (controls.task == TASK_ALL && board_faults()) {
			TRACE_STOP(3U | (uint32_t)board_faults() << 8);
			stop_test(now);
			return;
		}
		if (!stepper_start(&step, controls.reverse, controls_rpm(&controls), now))
			stop_test(now);
	} else if (task_uses_wheels(controls.task) && !start_wheels()) {
		stop_test(now);
	}
}

static void check_motors(uint32_t now)
{
	if (!controls.running)
		return;
	if (task_uses_stepper(controls.task) && !stepper_running(&step)) {
		TRACE_STOP(6);
		stop_test(now);
		return;
	}
	/* ALL waits for the Emm velocity ACK before starting the wheels.
	 * A wheel trip ends the run; it must never trigger an automatic restart.
	 */
	if (controls.task == TASK_ALL && !wheels_started &&
	    step.phase == STEP_MOVING && !start_wheels()) {
		TRACE_STOP(5U | (uint32_t)board_wheels_trip() << 8);
		stop_test(now);
		return;
	}
	if (wheels_started && !board_wheels_active()) {
		TRACE_STOP(5U | (uint32_t)board_wheels_trip() << 8);
		stop_test(now);
	}
}

static void sample_inputs(uint32_t now)
{
	uint32_t interval = now - sampled_at;
	unsigned int side;

	if (interval < 100)
		return;
	for (side = 0; side < 2; side++) {
		uint32_t count = board_encoder(side);
		int32_t delta = encoder_delta(count, previous_encoder[side]);

		ui.encoder[side] = encoder_delta(count, 0);
		ui.counts_per_second[side] = (int32_t)((int64_t)delta * 1000 / interval);
		previous_encoder[side] = count;
	}
	sampled_at = now;
}

static void update_ui(uint32_t now)
{
	int duty = controls_duty(&controls);

	ui.task = controls.task;
	ui.running = controls.running;
	ui.reverse = controls.reverse;
	ui.duty = (unsigned int)(duty < 0 ? -duty : duty);
	ui.remaining_ms = controls.running && !task_continuous(controls.task) ?
		TEST_DURATION_MS - (now - controls.started_at) : 0;
	ui.gray = board_gray();
	ui.phases = board_encoder_pins();
	ui.faults = board_faults();
	ui.wheel_faults = board_wheels_faults();
	ui.wheel_requested = board_wheels_active();
	ui.wheel_enable_fault = board_wheels_trip() == WHEEL_ENABLE_LOW;
	ui.step_online = stepper_online(&step, now);
	ui.step_status = stepper_name(&step, now);
	ui.step_error = step.error;
	ui.step_target_rpm = controls_rpm(&controls);
	ui.step_rpm = step.rpm;
	ui.step_rpm_valid = step.rpm_valid && ui.step_online;
}

static void send_telemetry(uint32_t now)
{
	struct serial_stats bt = serial_stats(SERIAL_BT);
	struct serial_stats motor = serial_stats(SERIAL_STEP);
	char line[320];
	int length;

	if (!elapsed(now, telemetry_at, TELEMETRY_MS))
		return;
	telemetry_at = now;
	/* Versioned CSV; J10 is intentionally not represented as a measured bit. */
	length = snprintf(line, sizeof(line),
		"C5,2,%lu,%lu,%u,%u,%d,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%ld,%u,%lu,%lu,%lu,%u,%u,%lu,%u,%d,%ld,%u\r\n",
		(unsigned long)now, (unsigned long)sequence++,
		(unsigned int)controls.task, controls.running,
		controls.running && task_uses_wheels(controls.task) ? controls_duty(&controls) : 0,
		(long)ui.encoder[0], (long)ui.encoder[1],
		(long)ui.counts_per_second[0], (long)ui.counts_per_second[1],
		ui.gray, ui.phases, ui.faults, ui.wheel_requested, ui.step_online,
		(unsigned int)step.phase, step.flags, (long)step.position, step.error,
		(unsigned long)(bt.errors + motor.errors),
		(unsigned long)(bt.rx_drops + motor.rx_drops),
		(unsigned long)(bt.tx_drops + motor.tx_drops), lcd_ok,
		(unsigned int)board_wheels_trip(), (unsigned long)step.timeouts,
		step.position_valid,
		stepper_running(&step) ? (step.reverse ? -(int)step.target_rpm :
							(int)step.target_rpm) : 0,
		(long)step.rpm, ui.step_rpm_valid);
	ui.bt_tx = length > 0 && (size_t)length < sizeof(line) &&
		   serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)length);
}

int main(void)
{
	if (!board_init() || !serial_init())
		board_panic();
	stepper_init(&step);
	lcd_ok = display_init();
	/* A held key during power-up must be released before it can start a test. */
	for (unsigned int index = 0; index < KEY_COUNT; index++) {
		keys[index].stable = !!(board_keys() & (1U << index));
		keys[index].candidate = keys[index].stable;
		keys[index].changed_at = board_millis();
	}
	for (;;) {
		uint32_t now = board_millis();
		uint8_t byte;
		uint8_t faults;
		bool was_running;
		bool fault_stop;

		board_heartbeat();
		stepper_poll(&step, now);
		if (!upgrade_requested)
			handle_keys(now);
		while (serial_read(SERIAL_BT, &byte)) {
			handle_remote(byte, now);
		}
		board_wheels_poll();
		faults = board_faults();
		was_running = controls.running;
		/* Wheel faults are checked by the board after its PWM-off wake delay. */
		fault_stop = controls.task == TASK_STEPPER && faults != 0;
		controls_tick(&controls, now, fault_stop);
		if (was_running && !controls.running)
			TRACE_STOP(fault_stop ? 3U | (uint32_t)faults << 8 : 4U);
		check_motors(now);
		if (!controls.running)
			stop_test(now);
		sample_inputs(now);
		update_ui(now);
		if (!upgrade_requested)
			send_telemetry(now);
		upgrade_poll(now);
		if (lcd_ok)
			lcd_ok = display_poll(&ui, now);
		board_led((now / (controls.running ? 125U : 500U)) & 1U);
	}
}
