#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32c5xx.h"
#include "ball.h"
#include "balance.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "jog.h"
#include "logic.h"
#include "ota.h"
#include "serial.h"
#include "stepper.h"

static struct controls controls = {
	.task = TASK_BALANCE, .level = WHEEL_DEFAULT_LEVEL, .lap_level = LAP_DEFAULT_LEVEL
};
static struct debounce keys[KEY_COUNT];
static struct stepper step;
static struct shaft_jog jog;
static struct jog_keys trim_keys;
static struct ui_state ui;
static struct grayscale gray;
static struct lap lap;
static struct ball_receiver camera;
static struct balance beam; /* 实车轴角增大时，球滚向车身右侧。 */
static struct balance_timer beam_time;
static bool camera_ready;
static struct contest_menu menu = { .page = MENU_HOME };
static struct position_run position;
static struct position_run prepare_level;
static bool preparing_shaft;
static bool holding_finish;
/* 每个新的相机序号只统计一次，蓝牙发送抽样不影响整圈极值。 */
static struct offset_range {
	uint32_t start, samples, missing, outside, min_at, max_at, last_at, max_gap;
	int target_mm, min_mm, max_mm;
} offset_range;
static uint32_t event_elapsed;
static bool menu_release;
static uint32_t target_repeat_at;
static uint32_t reset_flags;
static bool level_pending, mark_level;
static bool trial_requested, f407_requested, have_lift_origin;
static bool lift_ccw = true;
static int32_t lift_origin;
static uint32_t level_requested_at;
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
static uint32_t last_loop_at;
static uint32_t max_loop_ms;
static uint32_t max_display_ms;
static uint32_t max_telemetry_ms;

#ifdef CORAL_DEBUG_KEYS
/* SWD mailbox: write 0xc5000001 + key_id to simulate one debounced press.
 * The main loop consumes it once; physical keys take priority. Release builds
 * have no mailbox. Power-up opens the event menu.
 */
volatile uint32_t debug_key_request;
/* Read over SWD while the CPU runs. Times share the SW2 acceptance epoch. */
struct position_sample {
	uint16_t ms, frame_seq, rx_age, camera_age;
	int16_t x, shaft_cd, goal_cd, velocity;
	uint8_t camera_flags, view, leg, phase, drive_flags, error, page, notice;
};
static volatile struct position_sample position_trace[1024];
static volatile unsigned int position_count;
static bool position_recording;

static void record_position(uint32_t now)
{
	if (!position_recording)
		return;
	uint32_t elapsed_ms = now - position.start;
	if (menu.item != EVENT_POSITION || elapsed_ms > 14000 || position_count >= sizeof(position_trace) / sizeof(position_trace[0])) {
		position_recording = false;
		return;
	}
	if (position_count) {
		const volatile struct position_sample *last = &position_trace[position_count - 1];
		if (last->frame_seq == camera.sequence && last->leg == position.leg &&
		    last->page == menu.page && last->notice == menu.notice && elapsed_ms - last->ms < 50)
			return;
	}
	uint32_t age = now - camera.last_ms;
	position_trace[position_count] = (struct position_sample){
		.ms = elapsed_ms, .frame_seq = camera.sequence, .rx_age = age > UINT16_MAX ? UINT16_MAX : age,
		.camera_age = camera.age, .x = (int16_t)camera.x,
		.shaft_cd = (step.position - step.shaft_level) * 36000 / 65536,
		.goal_cd = position.goal_cd, .velocity = position.velocity, .camera_flags = camera.flags,
		.view = ball_view_at(&camera, now).status, .leg = position.leg, .phase = step.phase,
		.drive_flags = step.flags, .error = step.error, .page = menu.page, .notice = menu.notice,
	};
	position_count++;
}
/* Last stop: 1 key, 2 BLE, 3 nFAULT, 4 deadline, 5 wheel trip, 6 stepper, 7 lap.
 * High byte holds the fault mask or wheel_trip at the stopping decision.
 */
volatile uint32_t debug_stop_reason;
#define TRACE_STOP(reason) (debug_stop_reason = (reason))
#else
#define TRACE_STOP(reason) ((void)0)
#endif

static void reset_jog(void)
{
	jog = (struct shaft_jog){ .lower = jog.lower, .upper = jog.upper, .calibrating = jog.calibrating };
}

static void stop_test(uint32_t now)
{
	preparing_shaft = false;
	holding_finish = false;
	if (lap.status == LAP_RUNNING || lap.status == LAP_CHECK)
		lap_stop(&lap, LAP_CANCELLED);
	board_wheels_stop();
	if (jog_busy(&jog))
		reset_jog();
	if (beam.active)
		balance_stop(&beam, false);
	if (trial_requested || f407_requested) {
		level_pending = trial_requested = f407_requested = false;
		if (!beam.active && beam.state == BAL_SEEK)
			balance_stop(&beam, false);
	}
	if (stepper_running(&step) || (step.phase == STEP_LATCH && !level_pending))
		stepper_brake(&step, now);
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

static void set_calibration(bool enable, uint32_t now)
{
	stop_test(now);
	level_pending = trial_requested = f407_requested = false;
	controls.task = TASK_BALANCE;
	jog = (struct shaft_jog){ .calibrating = enable };
	trim_keys = (struct jog_keys){ .consumed = true };
}

static void report_ball(uint32_t now)
{
	struct ball_view current = ball_view_at(&camera, now);
	struct ball_view shown;
	uint32_t updates = display_last_ball(&shown);
	struct serial_stats uart = serial_stats(SERIAL_CAMERA);
	char line[136];
	int length = snprintf(line, sizeof(line),
		"C5BALL,1,%lu,%lu,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%lu,%lu\r\n",
		(unsigned long)camera.frames, (unsigned long)camera.rejected,
		(unsigned int)camera.sequence, (unsigned int)camera.flags,
		(unsigned int)camera.age, (unsigned int)camera.x,
		(unsigned int)current.status, (unsigned int)current.permille,
		(unsigned long)updates, (unsigned int)shown.status,
		(unsigned int)shown.permille, (unsigned long)uart.errors,
		(unsigned long)uart.rx_drops);

	if (length > 0 && (size_t)length < sizeof(line))
		serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)length);
}

static bool latch_beam(bool mark_horizontal, uint32_t now)
{
	if (jog.calibrating || controls.task != TASK_BALANCE || controls.running || level_pending ||
	    board_wheels_active() || !stepper_latch(&step, now))
		return false;
	reset_jog();
	mark_level = mark_horizontal;
	if (mark_horizontal) {
		beam.leveled = false;
		beam.state = BAL_NEED_LEVEL;
	}
	level_pending = true;
	level_requested_at = now;
	return true;
}

static bool start_beam(bool probe, bool trial, uint32_t now)
{
	if (jog.calibrating || controls.task != TASK_BALANCE || level_pending || beam.active || board_wheels_active() ||
	    (!probe && (!camera.received || now - camera.last_ms >= BALL_TIMEOUT_MS)) ||
	    !stepper_online(&step, now))
		return false;
	board_wheels_stop();
	if (!probe) {
		if (trial && have_lift_origin &&
		    ((int64_t)step.position - lift_origin < -120 || (int64_t)step.position - lift_origin > 120))
			return false;
		if (!stepper_latch(&step, now))
			return false;
		if (trial) {
			step.cw_sign = 0;
			beam.leveled = false;
		}
		level_requested_at = controls.started_at = now;
		reset_jog();
		level_pending = controls.running = true;
		trial_requested = trial;
		f407_requested = !trial;
		beam.state = trial ? BAL_SEEK : BAL_WAIT;
		balance_time_start(&beam_time, trial ? 2 : 1, now);
		return true;
	}
	if (step.phase != STEP_PARKED || !step.hold ||
	    !(step.flags & 1U) || (step.flags & 4U) ||
	    !step.position_valid || now - step.position_at >= 240 || !step.pulses_per_rev ||
	    !balance_probe(&beam, step.position, now))
		return false;
	if (!stepper_hold(&step, beam.zero - BAL_LIMIT_RAW, beam.zero + BAL_LIMIT_RAW, now)) {
		balance_stop(&beam, true);
		return false;
	}
	controls.running = true;
	controls.started_at = now;
	balance_time_start(&beam_time, 3, now);
	return true;
}

static void report_balance(uint32_t now)
{
	struct ball_view ball = ball_view_at(&camera, now);
	char line[170];
	int n = snprintf(line, sizeof(line),
		"C5BAL,1,%lu,%u,%u,%u,%d,%u,%u,%d,%d,%d,%ld,%ld,%lu,%u,%u,%u,%u,%lu\r\n",
		(unsigned long)now, (unsigned)beam.state, beam.active, beam.leveled,
		beam.reverse ? -1 : 1, (unsigned)ball.status, (unsigned)ball.permille,
		beam.velocity, beam.command, balance_shaft(&beam, step.position),
		(long)beam.zero, (long)step.position, (unsigned long)step.pulses_per_rev,
		(unsigned)step.phase, (unsigned)step.error, (unsigned)step.flags,
		board_wheels_active(), (unsigned long)(beam.active && beam.centered ?
		now - beam.centered_at : 0));

	if (n > 0 && (size_t)n < sizeof(line))
		serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
}

static void poll_balance(uint32_t now)
{
	stepper_request_config(&step, now);
	if (controls.task != TASK_BALANCE) {
		level_pending = false;
		return;
	}
	if (jog_busy(&jog)) {
		jog_poll(&jog, &step, now);
		controls.running = jog_busy(&jog);
		return;
	}
	if (jog.calibrating)
		return;
	if (level_pending) {
		if (now - level_requested_at >= ((trial_requested || f407_requested) ? 10000U : 1000U) || step.error ||
		    step.phase == STEP_IDLE || step.phase == STEP_OFFLINE) {
			level_pending = false;
			balance_stop(&beam, true);
			if (trial_requested || f407_requested)
				stop_test(now);
		} else if (step.phase == STEP_PARKED && (step.flags & 1U) &&
			   !(step.flags & 4U) && step.position_valid &&
			   now - step.position_at < now - level_requested_at) {
			bool finding = trial_requested;
			if ((finding || f407_requested) && !step.travel_valid)
				return;
			if (f407_requested || (finding && !have_lift_origin)) {
				lift_origin = f407_requested ? step.shaft_level : step.position;
				if (f407_requested) {
					beam.manual_level = true;
					beam.level_hint = step.shaft_level;
				}
				have_lift_origin = true;
			}
			if (finding && !step.cw_sign) {
				if (ball_view_at(&camera, now).status != BALL_TRACKED)
					return;
				if (!stepper_measure_direction(&step, lift_origin, lift_ccw, now)) {
					balance_stop(&beam, true);
					stop_test(now);
				}
				return;
			}
			if (f407_requested) {
				struct ball_view ball = ball_view_at(&camera, now);
				if (now - step.position_at >= 15 || step.pending) {
					if (now - step.position_at >= 15)
						step.last_poll = now - 130;
					return;
				}
				if (!balance_roll_start(&beam, lift_origin, step.position, &ball, now) ||
				    !stepper_speed_loop(&step, beam.lower, beam.upper, now)) {
					balance_stop(&beam, true);
					stop_test(now);
				}
				f407_requested = level_pending = false;
				return;
			}
			level_pending = false;
			trial_requested = false;
			if (finding) {
				controls.running = false;
				beam.zero = lift_origin;
				beam.state = BAL_TESTED;
				return;
			}
			if (!mark_level) {
				jog.state = JOG_HELD;
			} else if (!balance_level(&beam, step.position)) {
				balance_stop(&beam, true);
				stop_test(now);
			}
		}
	}
	if (!stepper_online(&step, now) || step.phase == STEP_OFFLINE) {
		if (beam.leveled || beam.active) {
			balance_stop(&beam, true);
			stop_test(now);
		}
		return;
	}
	if (!beam.active) {
		if (beam.leveled && (!step.hold || (step.flags & 4U) ||
		    (jog.state != JOG_HELD && abs(balance_shaft(&beam, step.position)) >= BAL_TRIP_CD)))
			balance_stop(&beam, true);
		return;
	}
	if (!stepper_running(&step) || board_wheels_active() ||
	    (now - beam.started >= 220 && now - step.position_at >= 220)) {
		balance_stop(&beam, true);
	} else {
		struct ball_view ball = ball_view_at(&camera, now);
		balance_update(&beam, &ball, camera.sequence, step.position, now);
		if (beam.active && beam.f407) {
			step.lower = beam.lower;
			step.upper = beam.upper;
			if (beam.speed_due) {
				stepper_balance_rpm(&step, beam.native_rpm);
				beam.speed_due = false;
			}
		} else if (beam.active) {
			step.target_rpm = (uint16_t)beam.shaft_rpm;
			step.goal = balance_goal(&beam);
		}
	}
	if (!beam.active)
		stop_test(now);
}

static void finish_event(enum contest_notice notice, uint32_t now);

static void record_offset_sample(uint32_t now)
{
	uint32_t t = now - offset_range.start;
	if (ball_view_at(&camera, now).status != BALL_TRACKED) {
		offset_range.missing++;
		return;
	}
	int error = contest_ball_mm(camera.x) - offset_range.target_mm;
	if (!offset_range.samples || error < offset_range.min_mm) {
		offset_range.min_mm = error;
		offset_range.min_at = t;
	}
	if (!offset_range.samples || error > offset_range.max_mm) {
		offset_range.max_mm = error;
		offset_range.max_at = t;
	}
	if (t - offset_range.last_at > offset_range.max_gap)
		offset_range.max_gap = t - offset_range.last_at;
	offset_range.last_at = t;
	offset_range.samples++;
	if (abs(error) > 20)
		offset_range.outside++;
}

static void handle_remote(uint8_t byte, uint32_t now)
{
	if (byte == '!') {
		if (menu.page == MENU_RUN)
			finish_event(NOTICE_STOPPED, now);
		trim_keys.consumed = true;
		if (controls.running)
			TRACE_STOP(2);
		stop_test(now);
		level_pending = false;
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
	if (!strcmp(remote_line, "C5:RANGE?")) {
		char line[160];
		int n = snprintf(line, sizeof(line), "C5RANGE,1,%lu,%d,%lu,%lu,%d,%d,%lu,%lu,%lu,%lu,%lu",
				 (unsigned long)offset_range.start, offset_range.target_mm,
				 (unsigned long)offset_range.samples, (unsigned long)offset_range.missing,
				 offset_range.min_mm, offset_range.max_mm, (unsigned long)offset_range.outside,
				 (unsigned long)offset_range.min_at, (unsigned long)offset_range.max_at,
				 (unsigned long)offset_range.max_gap, (unsigned long)offset_range.last_at);
		if (n > 0 && (size_t)n + 12 <= sizeof(line)) {
			unsigned long crc = ota_crc((const uint8_t *)line, (size_t)n);
			snprintf(line + n, sizeof(line) - n, ",%08lX\r\n", crc);
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n + 11);
		}
		return;
	}
	if (!strcmp(remote_line, "C5:MENU?")) {
		char line[96];
		int n = snprintf(line, sizeof(line), "C5MENU,1,%u,%u,%u,%u,%u,%u,%lu\r\n",
				 (unsigned)menu.page, (unsigned)menu.item + 1, (unsigned)menu.notice,
				 (unsigned)position.leg, controls.running, board_wheels_active(),
				 (unsigned long)ui.event_ms);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
		return;
	}
	if (!strcmp(remote_line, "C5:MENU") && !upgrade_requested) {
		set_calibration(false, now);
		menu.page = MENU_HOME;
		menu.notice = NOTICE_NONE;
		return;
	}
	if (!strcmp(remote_line, "C5:OTA") && !upgrade_requested) {
		if (step.hold) {
			static const uint8_t held[] = "C5OTA,BEAM HELD\r\n";
			serial_write(SERIAL_BT, held, sizeof(held) - 1);
			return;
		}
		stop_test(now);
		stepper_stop(&step, now);
		upgrade_requested = true;
		upgrade_at = now;
		ui.updating = true;
		ui.update_status = "PREPARING";
		serial_write(SERIAL_BT, (const uint8_t *)"C5OTA,REBOOT\r\n", 14);
	} else if (!strcmp(remote_line, "C5:GRAY") && !upgrade_requested) {
		menu.page = MENU_DIAGNOSTIC;
		if (jog.calibrating)
			set_calibration(false, now);
		stop_test(now);
		controls.task = TASK_GRAY;
	} else if (!strcmp(remote_line, "C5:LAP") && !upgrade_requested) {
		menu.page = MENU_DIAGNOSTIC;
		if (jog.calibrating)
			set_calibration(false, now);
		stop_test(now);
		controls.task = TASK_LAP;
	} else if (!strcmp(remote_line, "C5:CAL") && !upgrade_requested) {
		menu.page = MENU_DIAGNOSTIC;
		if (!jog.calibrating)
			set_calibration(true, now);
	} else if (!strcmp(remote_line, "C5:BAL") && !upgrade_requested) {
		menu.page = MENU_DIAGNOSTIC;
		if (jog.calibrating)
			set_calibration(false, now);
		stop_test(now);
		controls.task = TASK_BALANCE;
	} else if (!strcmp(remote_line, "C5:BAL?")) {
		report_balance(now);
	} else if (!strcmp(remote_line, "C5:JOG?")) {
		char line[104];
		int n = snprintf(line, sizeof(line), "C5JOG,1,%lu,%u,%d,%ld,%ld,%ld,%u,%u,%u\r\n",
				 (unsigned long)now, (unsigned)jog.state, jog.steps, (long)jog.origin,
				 (long)jog.target, (long)step.position, jog.limited, step.flags, step.error);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:CAL?")) {
		char line[160];
		int n = snprintf(line, sizeof(line),
			"C5CAL,1,%lu,%u,%lu,%lu,%u,%u,%u,%d,%d,%d,%ld,%lu,%u,%u,%u,%u\r\n",
			(unsigned long)now, jog.calibrating, (unsigned long)camera.last_ms,
			(unsigned long)camera.frames, camera.sequence, camera.flags, camera.age,
			(int)(int16_t)camera.x, camera.position_centi_cm, camera.speed_deci_cm_s,
			(long)step.position, (unsigned long)step.position_at, (unsigned)step.phase,
			step.flags, step.error, board_wheels_active());
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:LIMIT?")) {
		char line[108];
		int n = snprintf(line, sizeof(line), "C5LIMIT,1,%u,%ld,%ld,%ld,%ld\r\n",
			step.travel_valid, (long)step.shaft_level, (long)(step.shaft_level + SHAFT_HIGH_OFFSET),
			(long)(step.shaft_level + SHAFT_LOW_OFFSET), (long)step.position);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:ROLL?")) {
		char line[146];
		int n = snprintf(line, sizeof(line), "C5ROLL,1,%lu,%lu,%u,%u,%u,%ld,%ld,%ld,%u,%d,%d,%lu\r\n",
			(unsigned long)now, (unsigned long)reset_flags, 0U,
			(unsigned int)beam.state, beam.legs, (long)beam.origin, (long)beam.lower,
			(long)beam.upper, beam.lock_frames, beam.lock_travel, beam.lock_rate,
			(unsigned long)(beam.lock_frames ? beam.lock_at - beam.started : 0));
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:F407?")) {
		struct ball_view ball = ball_view_at(&camera, now);
		int sent = step.phase == STEP_BAL_SPEED ? (step.sent_reverse ? step.sent_rpm : -(int)step.sent_rpm) : 0;
		char line[108];
		int n = snprintf(line, sizeof(line), "C5F407,1,%lu,%u,%u,%d,%d,%d,%d,%d,%d\r\n",
				 (unsigned long)now, (unsigned)ball.status, (unsigned)camera.x,
				 ball.offset_px, beam.target_beam_cd, beam.actual_beam_cd, beam.native_rpm,
				 sent, step.phase == STEP_BAL_SPEED);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:LOOP?")) {
		char line[100];
		struct serial_stats motor = serial_stats(SERIAL_STEP);
		int n = snprintf(line, sizeof(line), "C5LOOP,1,%lu,%d,%d,%d,%u,%u,%lu,%lu,%lu\r\n",
			(unsigned long)now, beam.demand, balance_shaft(&beam, step.position),
			beam.command, (unsigned)step.target_rpm, (unsigned)step.sent_rpm,
			(unsigned long)motor.errors, (unsigned long)motor.rx_drops, (unsigned long)step.timeouts);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:SLINK?")) {
		struct serial_stats motor = serial_stats(SERIAL_STEP);
		char line[108];
		int n = snprintf(line, sizeof(line), "C5SLINK,1,%lu,%lu,%lu,%lu,%lu,%u,%u\r\n",
				 (unsigned long)now, (unsigned long)motor.framing,
				 (unsigned long)motor.noise, (unsigned long)motor.overrun,
				 (unsigned long)motor.parity, (unsigned)step.last_bad_phase,
				 (unsigned)step.last_bad_command);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:TIME?")) {
		char line[100];
		int n = snprintf(line, sizeof(line), "C5TIME,1,%u,%u,%lu,%lu,%lu\r\n",
				 beam_time.operation, beam_time.counting, (unsigned long)beam_time.elapsed,
				 (unsigned long)beam_time.tracking, (unsigned long)beam_time.settled);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:REST") && menu.page == MENU_DIAGNOSTIC && !upgrade_requested && !controls.running &&
		   controls.task == TASK_BALANCE && !level_pending && !board_wheels_active() &&
		   have_lift_origin && step.phase == STEP_PARKED &&
		   step.position_valid && now - step.position_at < 240 &&
	   balance_park(&beam, lift_origin, step.position, now)) {
		int32_t low = step.position < lift_origin ? step.position : lift_origin;
		int32_t high = step.position > lift_origin ? step.position : lift_origin;
		if (stepper_hold(&step, low - 32, high + 32, now)) {
			controls.running = true;
			controls.started_at = now;
			step.target_rpm = 20;
			balance_time_start(&beam_time, 4, now);
		} else {
			balance_stop(&beam, true);
		}
	} else if (!strcmp(remote_line, "C5:LIFT?")) {
		char line[98];
		int n = snprintf(line, sizeof(line), "C5LIFT,1,%d,%u,%ld,%ld,%ld\r\n",
				 step.cw_sign, have_lift_origin, (long)lift_origin,
				 (long)step.lower, (long)step.upper);
		if (n > 0 && (size_t)n < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)n);
	} else if (!strcmp(remote_line, "C5:FREE") && !upgrade_requested &&
		   !controls.running && controls.task == TASK_BALANCE &&
		   step.phase == STEP_PARKED) {
		level_pending = false;
		beam.leveled = false;
		have_lift_origin = false;
		jog = (struct shaft_jog){0};
		step.cw_sign = 0;
		balance_stop(&beam, false);
		stepper_stop(&step, now);
	} else if (!strcmp(remote_line, "C5:RAISE") && menu.page == MENU_DIAGNOSTIC && controls.task == TASK_BALANCE &&
		   !upgrade_requested && !controls.running && !level_pending && !board_wheels_active()) {
		/* Operator identified the bearing-side stop. A static return from the
		 * other side must not choose the first direction or appear on the LCD. */
		ball_expect_right(&camera, now);
		beam.reverse = false;
		start_beam(false, false, now);
	} else if (menu.page == MENU_DIAGNOSTIC && controls.task == TASK_BALANCE && !upgrade_requested && !controls.running &&
		   !have_lift_origin && (!strcmp(remote_line, "C5:UP-") || !strcmp(remote_line, "C5:UP+"))) {
		lift_ccw = remote_line[5] == '-';
	} else if (menu.page == MENU_DIAGNOSTIC && controls.task == TASK_BALANCE && !upgrade_requested && !controls.running &&
		   (!strcmp(remote_line, "C5:LEVEL") || !strcmp(remote_line, "C5:LOCK"))) {
		latch_beam(!strcmp(remote_line, "C5:LEVEL"), now);
	} else if (menu.page == MENU_DIAGNOSTIC && controls.task == TASK_BALANCE && !upgrade_requested && !controls.running &&
		   (!strcmp(remote_line, "C5:MAP+") || !strcmp(remote_line, "C5:MAP-"))) {
		beam.reverse = remote_line[6] == '-';
	} else if (menu.page == MENU_DIAGNOSTIC && controls.task == TASK_BALANCE && !upgrade_requested && !controls.running &&
		   (!strcmp(remote_line, "C5:TILT") || !strcmp(remote_line, "C5:HOLD") ||
		    !strcmp(remote_line, "C5:TRY"))) {
		start_beam(!strcmp(remote_line, "C5:TILT"), !strcmp(remote_line, "C5:TRY"), now);
#ifdef CORAL_DEBUG_KEYS
	} else if (!strcmp(remote_line, "C5:CHECK") && !upgrade_requested && !controls.running) {
		menu.page = MENU_DIAGNOSTIC;
		int direction = controls.lap_reverse ? -1 : 1;

		stop_test(now);
		controls.task = TASK_LAP;
		lap_check_start(&lap, now, board_encoder(0), board_encoder(1), controls.lap_reverse);
		wheels_started = board_wheels_start(direction * LAP_LEFT_DRIVE_SIGN * LAP_CHECK_PERMILLE,
				direction * LAP_RIGHT_DRIVE_SIGN * LAP_CHECK_PERMILLE, false);
		controls.started_at = now;
		controls.running = wheels_started;
		if (!wheels_started)
			lap_stop(&lap, LAP_DRIVE);
#endif
	} else if (!strcmp(remote_line, "C5:BALL?")) {
		report_ball(now);
	} else if (!strcmp(remote_line, "C5:PERF")) {
		char line[80];
		int length = snprintf(line, sizeof(line), "C5PERF,1,%lu,%lu,%lu\r\n",
			(unsigned long)max_loop_ms, (unsigned long)max_display_ms,
			(unsigned long)max_telemetry_ms);

		if (length > 0 && (size_t)length < sizeof(line))
			serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)length);
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

static void start_lap(uint32_t now, bool competition)
{
	uint32_t left = board_encoder(0), right = board_encoder(1);
	bool started;
	int direction;

	if (competition) {
		bool carry_ball = menu.item == EVENT_OFFSET;
		unsigned int level = carry_ball ? LAP_OFFSET_LEVEL : controls.lap_level;
		uint32_t distance = carry_ball ? LAP_OFFSET_DISTANCE_COUNTS : LAP_DISTANCE_COUNTS;
		started = lap_start_distance(&lap, &gray.reading, now, left, right,
					     level, distance);
		lap.carry_ball = carry_ball;
	} else
		started = lap_start(&lap, &gray.reading, now, left, right,
				    controls.lap_level, controls.lap_reverse);
	if (!started) {
		stop_test(now);
		return;
	}
	direction = lap.reverse ? -1 : 1;
	wheels_started = board_wheels_start(direction * LAP_LEFT_DRIVE_SIGN,
					 direction * LAP_RIGHT_DRIVE_SIGN, true);
	if (!wheels_started) {
		lap_stop(&lap, LAP_DRIVE);
		stop_test(now);
	}
}

static void finish_event(enum contest_notice notice, uint32_t now)
{
	if (!holding_finish)
		event_elapsed = menu.item == EVENT_POSITION && position.leg == HOLD_LEFT ?
				position.finished_ms : now - controls.started_at;
	if (menu.item == EVENT_OFFSET && menu.page == MENU_RUN && notice == NOTICE_FINISHED) {
		/* 循线已切断轮驱动。车辆滑停期间及结果页继续按原目标稳球。 */
		holding_finish = true;
		controls.running = false;
	} else {
		stop_test(now);
	}
	menu.page = MENU_RESULT;
	menu.notice = notice;
}

static bool observe_start_ball(uint32_t now)
{
	if (!ball_v2_measured(&camera, now) || (ball_display_at(&camera, now).status != BALL_TRACKED &&
	    ball_view_at(&camera, now).status != BALL_TRACKED))
		return false;
	/* A stationary ball may have been placed anywhere between runs. */
	camera.have_anchor = true;
	camera.uncertain = false;
	camera.anchor_x = camera.x;
	camera.anchor_at = camera.last_ms;
	camera.anchor_speed_px_s = 0;
	camera.candidate_frames = 0;
	return true;
}

static enum contest_notice position_drive_notice(uint32_t now)
{
	if (!step.travel_valid && step.position_valid && now - step.position_at < 750 &&
	    stepper_online(&step, now) && !(step.flags & 0x0cU) && (!step.error || step.error == 0xe4))
		return NOTICE_TRAVEL;
	return NOTICE_DRIVE;
}

static void prepare_position(uint32_t now)
{
	menu.notice = NOTICE_NONE;
	position_begin(&prepare_level, -1, 0, now);
	preparing_shaft = true;
}

static void start_event(uint32_t now)
{
	menu.notice = NOTICE_NONE;
	event_elapsed = 0;
	if (menu.item != EVENT_LAP && menu.item != EVENT_POSITION && menu.item != EVENT_OFFSET) {
		menu.page = MENU_RESULT;
		menu.notice = NOTICE_PENDING;
		return;
	}
	if (menu.item == EVENT_POSITION || menu.item == EVENT_OFFSET) {
		if (!step.travel_valid || (!preparing_shaft && !stepper_latch(&step, now))) {
			menu.notice = position_drive_notice(now);
			return;
		}
		controls.task = TASK_BALANCE;
		preparing_shaft = false;
		int x = observe_start_ball(now) ? (int)camera.x : -1;
		if (menu.item == EVENT_OFFSET) {
			menu.offset_driving = false;
			position_begin_target(&position, menu.target_mm, x, camera.sequence, now);
		} else {
			position_begin(&position, x, camera.sequence, now);
#ifdef CORAL_DEBUG_KEYS
			position_count = 0;
			position_recording = true;
#endif
		}
	} else {
		controls.task = TASK_LAP;
	}
	controls.started_at = now;
	controls.running = true;
	menu.page = MENU_RUN;
#ifdef CORAL_DEBUG_KEYS
	if (menu.item == EVENT_POSITION)
		record_position(now);
#endif
	if (menu.item == EVENT_LAP)
		start_lap(now, true);
}

static bool offset_lap_running(void)
{
	return menu.page == MENU_RUN && menu.item == EVENT_OFFSET && menu.offset_driving;
}

static bool position_drive_ok(uint32_t now)
{
	return !step.error && step.travel_valid && stepper_online(&step, now) &&
		!(step.flags & 0x0cU) && (!board_wheels_active() || offset_lap_running());
}

static bool shaft_at_level(uint32_t now)
{
	return step.phase == STEP_POSITION && step.goal_sent && step.position_valid &&
		abs(step.sent_goal - step.shaft_level) <= 4 && now - step.position_at < 60 &&
		now - step.position_at < now - step.goal_at && (step.flags & 3U) == 3U &&
		abs(step.position - step.shaft_level) <= 90;
}

static bool command_position(const struct position_run *run, uint32_t now, uint32_t latch_timeout_ms)
{
	if (step.phase == STEP_PARKED && !step.pending && (step.flags & 1U) &&
	    now - step.position_at < now - run->start) {
		stepper_hold(&step, step.shaft_level + SHAFT_HIGH_OFFSET,
			     step.shaft_level + SHAFT_LOW_OFFSET, now);
	}
	if (step.phase == STEP_POSITION) {
		step.target_rpm = 30;
		step.goal = step.shaft_level + run->goal_cd * 65536 / 36000;
		return true;
	}
	return (step.phase == STEP_LATCH || step.phase == STEP_PARKED) &&
	       now - run->start <= latch_timeout_ms;
}

static void poll_prepare_level(uint32_t now)
{
	stepper_request_config(&step, now);
	if (step.phase == STEP_IDLE && !step.pending && position_drive_ok(now))
		stepper_latch(&step, now);
	if (step.phase == STEP_BRAKE || step.phase == STEP_BOOT || !step.pulses_per_rev ||
	    !step.travel_valid || step.phase == STEP_IDLE) {
		if (now - prepare_level.start < 1800 && (!step.error || step.cleanup_only))
			return;
	}
	if (!position_drive_ok(now) || !command_position(&prepare_level, now, 2400) ||
	    (prepare_level.leg == LEVEL_SHAFT && now - prepare_level.start >= 3000)) {
		stop_test(now);
		menu.notice = position_drive_notice(now);
		return;
	}
	position_check_level(&prepare_level, shaft_at_level(now), step.position_at);
}

static void poll_position_event(uint32_t now)
{
	position.driving = offset_lap_running();
	if (!position_drive_ok(now)) {
		finish_event(position_drive_notice(now), now);
		return;
	}
	if (menu.item == EVENT_OFFSET && !menu.offset_driving)
		position_retarget(&position, menu.target_mm);
	if (!position.have_sample)
		observe_start_ball(now);
	if (position.leg == LEVEL_SHAFT)
		position_check_level(&position, shaft_at_level(now), step.position_at);
	enum ball_status view = ball_view_at(&camera, now).status;
	enum contest_notice status = position_update(&position, view, camera.x, camera.sequence, camera.last_ms, now);
	if ((offset_lap_running() || holding_finish) && position.waiting_vision)
		status = NOTICE_VISION;
	if (menu.item == EVENT_OFFSET && position.leg == LEVEL_SHAFT && now - position.start >= 3000)
		status = NOTICE_DRIVE;
	if (status != NOTICE_NONE)
		finish_event(status, now);
	else if (!command_position(&position, now, 1000))
		finish_event(NOTICE_DRIVE, now);
}

static bool offset_ready(uint32_t now)
{
	return step.phase == STEP_POSITION && position.leg != LEVEL_SHAFT && position.have_sample &&
	       !position.waiting_vision && now - position.sample_at < 120 && position_drive_ok(now);
}

static void start_offset_lap(uint32_t now)
{
	if (!offset_ready(now))
		return;
	menu.offset_driving = true;
	offset_range = (struct offset_range){ .start = now, .target_mm = menu.target_mm };
	controls.task = TASK_LAP;
	controls.started_at = now;
	start_lap(now, true);
}

static void handle_keys(uint32_t now)
{
	uint8_t raw = board_keys();
	uint8_t down = 0;
	unsigned int index;
	int event = -1;
	bool was_running = controls.running;

	for (index = 0; index < KEY_COUNT; index++) {
		if (key_update(&keys[index], raw & (1U << index), now))
			event = (int)index;
		if (keys[index].stable)
			down |= (uint8_t)(1U << index);
	}
	bool simulated = false;
#ifdef CORAL_DEBUG_KEYS
	uint32_t request = debug_key_request;
	debug_key_request = 0;
	if (event < 0 && !raw && request >= 0xc5000001U && request < 0xc5000001U + KEY_COUNT) {
		event = (int)(request - 0xc5000001U);
		simulated = true;
	}
#endif
	if (menu_release) {
		if (menu.page == MENU_RUN && event >= 0 && event != KEY_MODE &&
		    (menu.item != EVENT_OFFSET || event == KEY_START)) {
			finish_event(NOTICE_STOPPED, now);
			if (event == KEY_START)
				menu.page = MENU_DETAIL;
		}
		if (!down && !raw)
			menu_release = false;
		return;
	}
	if (menu.page != MENU_DIAGNOSTIC) {
		if (menu.page == MENU_HOME && down == ((1U << KEY_OK) | (1U << KEY_NEXT))) {
			menu.page = MENU_TOOLS;
			menu_release = true;
			return;
		}
		bool offset = menu.item == EVENT_OFFSET;
		bool selecting = offset && (menu.page == MENU_DETAIL || (menu.page == MENU_RUN && !menu.offset_driving));
		if (!simulated && selecting && down == raw && (down == (1U << KEY_OK) || down == (1U << KEY_NEXT))) {
			enum key_id key = down == (1U << KEY_OK) ? KEY_OK : KEY_NEXT;
			if (event < 0 && now - keys[key].changed_at >= 400 && now - target_repeat_at >= 50)
				event = key;
			if (event == (int)key)
				target_repeat_at = now;
		}
		if (event < 0 || (!simulated && (menu.page != MENU_RUN || (offset && event != KEY_START)) &&
				  down != (1U << event)))
			return;
		enum menu_action selected = contest_key(&menu, (enum key_id)event);
		if (selected == MENU_START_EVENT)
			start_event(now);
		else if (selected == MENU_START_LAP)
			start_offset_lap(now);
		else if (selected == MENU_RELEVEL)
			prepare_position(now);
		else if (selected == MENU_LAP_SPEED)
			controls_key(&controls, KEY_NEXT, now);
		else if (selected == MENU_STOP_EVENT) {
			finish_event(menu.item == EVENT_POSITION && position.leg == HOLD_LEFT ?
				     NOTICE_FINISHED : NOTICE_STOPPED, now);
			if (event == KEY_START)
				menu.page = MENU_DETAIL;
		} else if (selected == MENU_OPEN_TOOL) {
			controls.task = contest_tool(menu.tool);
			reset_jog();
			trim_keys = (struct jog_keys){ .consumed = true };
		}
		/* 必须先松键才能从进入切到启动，或从停车切到返回，避免一次长按触发两步。 */
		if (!simulated && (event == KEY_MODE || event == KEY_START))
			menu_release = true;
		return;
	}
	if (event == KEY_START) { /* Physical SW5 backs out of diagnostics. */
		level_pending = false;
		stop_test(now);
		if (jog.calibrating)
			set_calibration(false, now);
		else if (!was_running)
			menu.page = MENU_TOOLS;
		menu_release = !simulated;
		return;
	}
	bool manual = controls.task == TASK_BALANCE && !level_pending &&
		      (!was_running || jog_busy(&jog));
	if (manual && !was_running && !jog.calibrating && event >= 0 &&
	    down == ((1U << KEY_OK) | (1U << KEY_NEXT))) {
		set_calibration(true, now);
		return;
	}
	if (jog.calibrating) {
		if (event == KEY_MODE) {
			stop_test(now);
			trim_keys.consumed = true;
			return;
		}
		enum jog_action move = jog_cal_key(&trim_keys, down, now);
		/* Do not queue targets ahead of the mechanism during a held press. */
		if (move != JOG_NONE && !jog_busy(&jog) && !board_wheels_active() &&
		    jog_press(&jog, &step, move == JOG_LEFT ? -1 : 1, now)) {
			controls.running = true;
			controls.started_at = now;
		}
		return;
	}
	enum jog_action action = jog_key_update(&trim_keys, down, manual, now);
	/* Defer the two middle keys until a tap or a long press is known. While
	 * auto-balancing their press edge remains the normal immediate stop. */
	if (manual && (event == KEY_OK || event == KEY_NEXT)) {
		if (simulated)
			action = event == KEY_OK ? JOG_LEFT : JOG_RIGHT;
		event = -1;
	}

	if (manual && event < 0 && action != JOG_NONE) {
		if (action == JOG_LEFT || action == JOG_RIGHT) {
			if (!board_wheels_active() && jog_press(&jog, &step, action == JOG_LEFT ? -1 : 1, now)) {
				controls.running = true;
				controls.started_at = now;
			}
		} else if (!was_running) {
			if (action == JOG_ZERO)
				latch_beam(true, now);
			else
				beam.reverse = !beam.reverse;
		}
		return;
	}
	if (event < 0)
		return;
	if (event == KEY_MODE)
		event = KEY_START; /* Physical SW2 confirms the selected diagnostic. */
	controls_key(&controls, (enum key_id)event, now);
	if (was_running) {
		TRACE_STOP(1);
		stop_test(now);
		return;
	}
	if (controls.task == TASK_BALANCE) {
		if (event == KEY_START)
			controls.running = start_beam(false, false, now);
		return;
	}
	if (!controls.running)
		return;
	TRACE_STOP(0);
	if (controls.task == TASK_LAP) {
		start_lap(now, false);
	} else if (task_uses_stepper(controls.task)) {
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
	if (controls.task != TASK_BALANCE && task_uses_stepper(controls.task) &&
	    !stepper_running(&step)) {
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
		if (lap.status == LAP_RUNNING || lap.status == LAP_CHECK)
			lap_stop(&lap, LAP_DRIVE);
		TRACE_STOP(5U | (uint32_t)board_wheels_trip() << 8);
		stop_test(now);
	}
}

static void follow_line(uint32_t now)
{
	int direction = lap.reverse ? -1 : 1;

	if (!controls.running || controls.task != TASK_LAP ||
	    !lap_update(&lap, &gray.reading, now, board_encoder(0), board_encoder(1)))
		return;
	if ((lap.status == LAP_RUNNING || lap.status == LAP_CHECK) &&
	    !board_lap_set_wheels(direction * LAP_LEFT_DRIVE_SIGN * lap.wheel[0].pwm,
				  direction * LAP_RIGHT_DRIVE_SIGN * lap.wheel[1].pwm))
		lap_stop(&lap, LAP_DRIVE);
	if (lap.status != LAP_RUNNING && lap.status != LAP_CHECK) {
		TRACE_STOP(7U | (uint32_t)lap.status << 8);
		if (offset_lap_running() && lap.status == LAP_DONE) {
			board_wheels_stop();
			wheels_started = false;
		} else {
			stop_test(now);
		}
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

static bool gray_white_selected(void)
{
	if (menu.item == EVENT_OFFSET && (menu.page == MENU_DETAIL || menu.page == MENU_RUN))
		return false;
	return controls.task != TASK_LAP && controls.gray_white;
}

static void update_ui(uint32_t now)
{
	int duty = controls_duty(&controls);

	ui.menu = menu;
	ui.leg = position.leg;
	ui.offset_ready = menu.item == EVENT_OFFSET && menu.page == MENU_RUN && offset_ready(now);
	ui.waiting_vision = position.waiting_vision;
	ui.level_ready = preparing_shaft && prepare_level.leg != LEVEL_SHAFT && shaft_at_level(now);
	ui.position_seen = menu.item == EVENT_OFFSET ? ball_view_at(&camera, now).status == BALL_TRACKED :
			   ball_v2_measured(&camera, now);
	ui.ball_mm = ui.position_seen ? contest_ball_mm(camera.x) : 0;
	ui.event_ms = menu.page == MENU_RUN ? (menu.item == EVENT_POSITION && position.leg == HOLD_LEFT ?
		      position.finished_ms : now - controls.started_at) : event_elapsed;
	ui.task = controls.task;
	ui.running = controls.running;
	ui.reverse = controls.reverse;
	ui.duty = (unsigned int)(duty < 0 ? -duty : duty);
	ui.remaining_ms = controls.running && !task_continuous(controls.task) ?
		TEST_DURATION_MS - (now - controls.started_at) : 0;
	ui.gray = gray.reading.raw;
	ui.grayscale = gray.reading;
	ui.ball = ball_display_at(&camera, now);
	ui.calibrating = jog.calibrating;
	ui.cal_camera_x = ball_v2_measured(&camera, now) ? (int)camera.x : -1;
	ui.cal_encoder = step.position;
	ui.shaft_referenced = step.travel_valid;
	ui.balance_status = level_pending ? "LOCKING" : beam.state != BAL_SEEK ?
			    balance_name(beam.state) : lift_ccw ? "LIFT CCW" : "LIFT CW";
	ui.balance_reverse = beam.reverse;
	ui.balance_shaft = step.travel_valid ? (step.position - step.shaft_level) * 36000 / 65536 : 0;
	ui.balance_target = beam.command;
	ui.balance_speed_mode = beam.f407 || (!beam.active && beam.state != BAL_TESTED);
	ui.balance_rpm = step.phase == STEP_BAL_SPEED ? (step.sent_reverse ? step.sent_rpm : -(int)step.sent_rpm) : 0;
	ui.balance_jog = jog.state != JOG_IDLE && !beam.active && !level_pending;
	if (ui.balance_jog) {
		ui.balance_status = jog.state == JOG_FAILED || step.error ? "CHECK DRIVE" :
				    jog.limited ? "TRAVEL LIMIT" :
				    jog.state == JOG_LOCKING ? "LOCKING" :
				    jog.state == JOG_MOVING ? "JOGGING" : "HOLDING";
		ui.balance_target = jog.referenced && step.travel_valid ?
			(int)(((int64_t)jog.target - step.shaft_level) * 36000 / 65536) : ui.balance_shaft;
	}
	ui.balance_time = beam_time.settled ? beam_time.settled : beam_time.elapsed;
	ui.balance_timed = beam_time.settled != 0;
	ui.gray_white = gray_white_selected();
	ui.lap_status = lap.status;
	ui.lap_reverse = controls.lap_reverse;
	ui.lap_speed = offset_lap_running() ? LAP_OFFSET_CRUISE_CPS : (unsigned int)lap_speed(controls.lap_level);
	ui.lap_armed = lap.armed;
	ui.lap_departing = lap.departing;
	ui.lap_approach = lap.armed && lap_progress(&lap) >= LAP_APPROACH_COUNTS;
	ui.lap_ms = lap.elapsed_ms;
	ui.lap_counts = lap_progress(&lap);
	for (unsigned int side = 0; side < 2; side++) {
		ui.lap_pwm[side] = lap.wheel[side].pwm;
		ui.lap_cps[side] = lap.wheel[side].cps;
	}
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
	char line[480];
	int length;
	bool offset = menu.item == EVENT_OFFSET &&
		      (menu.page == MENU_DETAIL || menu.page == MENU_RUN || menu.page == MENU_RESULT);
	uint32_t period = offset || (controls.task == TASK_LAP && controls.running) ?
			  LAP_TELEMETRY_MS : TELEMETRY_MS;

	if (!elapsed(now, telemetry_at, period))
		return;
	telemetry_at = now;
	/* Versioned CSV; J10 is intentionally not represented as a measured bit. */
	length = snprintf(line, sizeof(line),
		"C5,%u,%lu,%lu,%u,%u,%d,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%ld,%u,%lu,%lu,%lu,%u,%u,%lu,%u,%d,%ld,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d,%d,%d,%d,%ld,%ld,%lu,%lu,%u,%d,%d,%d,%d,%d,%lu,%u,%u,%u\r\n",
		offset ? 7U : 6U, (unsigned long)now, (unsigned long)sequence++,
		(unsigned int)controls.task, controls.running,
		controls.running && task_uses_wheels(controls.task) &&
		controls.task != TASK_LAP ? controls_duty(&controls) : 0,
		(long)ui.encoder[0], (long)ui.encoder[1],
		(long)ui.counts_per_second[0], (long)ui.counts_per_second[1],
		ui.gray, ui.phases, ui.faults, ui.wheel_requested, ui.step_online,
		(unsigned int)step.phase, step.flags, (long)step.position, step.error,
		(unsigned long)(bt.errors + motor.errors),
		(unsigned long)(bt.rx_drops + motor.rx_drops),
		(unsigned long)(bt.tx_drops + motor.tx_drops), lcd_ok,
		(unsigned int)board_wheels_trip(), (unsigned long)step.timeouts,
		step.position_valid,
		stepper_running(&step) && !step.hold ? (step.reverse ? -(int)step.target_rpm :
							(int)step.target_rpm) : 0,
		(long)step.rpm, ui.step_rpm_valid,
		ui.grayscale.black, ui.grayscale.line, ui.grayscale.count,
		ui.grayscale.groups, (int)ui.grayscale.position,
		(unsigned int)ui.grayscale.status, ui.gray_white,
		(unsigned int)lap.status, lap.wheel[0].pwm, lap.wheel[1].pwm,
		lap.wheel[0].target, lap.wheel[1].target,
		(long)lap.wheel[0].cps, (long)lap.wheel[1].cps,
		(unsigned long)lap.elapsed_ms, (unsigned long)lap_progress(&lap), lap.armed,
		lap.last_position, lap.position_rate, lap.steering, lap.differential,
		lap.base_target, (unsigned long)lap.control_ms, lap.departing,
		lap.marker_hits, lap.marker_peak);
	if (offset && length >= 2 && (size_t)length < sizeof(line)) {
		/* 用同一个主控时钟对齐轮速与相机观测，便于从蓝牙轨迹核对惯性偏移。 */
		size_t used = (size_t)length - 2; /* replace the v6 line ending */
		unsigned int phase = menu.page == MENU_DETAIL ? 1U : menu.page == MENU_RESULT ? 4U :
				     menu.offset_driving ? 3U : 2U;
		int n = snprintf(line + used, sizeof(line) - used,
				 ",%u,%lu,%d,%u,%u,%lu,%u,%d,%lu,%d,%d,%u",
				 phase, phase == 1 ? 0UL : (unsigned long)ui.event_ms, menu.target_mm,
				 (unsigned)ball_view_at(&camera, now).status,
				 camera.sequence, (unsigned long)camera.last_ms, camera.age, ui.ball_mm,
				 (unsigned long)sampled_at, (menu.page == MENU_RUN || holding_finish) ? position.goal_cd : 0,
				 ui.balance_shaft, (unsigned)menu.notice);
		length = -1;
		if (n > 0 && used + (size_t)n + 12 <= sizeof(line)) {
			used += (size_t)n;
			uint32_t crc = ota_crc((const uint8_t *)line, used);
			n = snprintf(line + used, sizeof(line) - used, ",%08lX\r\n", (unsigned long)crc);
			if (n == 11)
				length = (int)used + n;
		}
	}
	ui.bt_tx = length > 0 && (size_t)length < sizeof(line) &&
		   serial_write(SERIAL_BT, (const uint8_t *)line, (size_t)length);
}

int main(void)
{
	volatile uint32_t *handover = (volatile uint32_t *)JLINK_HOLD_ADDR;
	bool wired_hold = handover[0] == JLINK_HOLD_MAGIC && handover[1] == ~JLINK_HOLD_MAGIC;
	handover[0] = handover[1] = 0;
	reset_flags = RCC->RSR;
	RCC->RSR |= RCC_RSR_RMVF;
	if (!board_init() || !serial_init())
		board_panic();
	stepper_init(&step);
	if (wired_hold)
		stepper_resume_hold(&step, board_millis());
	lcd_ok = display_init();
	camera_ready = serial_camera_init();
	/* A held key during power-up must be released before it can start a test. */
	for (unsigned int index = 0; index < KEY_COUNT; index++) {
		keys[index].stable = !!(board_keys() & (1U << index));
		if (keys[index].stable)
			menu_release = true;
		keys[index].candidate = keys[index].stable;
		keys[index].changed_at = board_millis();
	}
	trim_keys.consumed = true; /* A key already held at power-up cannot issue a jog on release. */
	for (;;) {
		uint32_t now = board_millis();
		uint8_t byte;
		uint8_t faults;
		bool was_running;
		bool fault_stop;
		uint32_t work_at;
		uint32_t work_ms;

		board_heartbeat();
		if (last_loop_at && now - last_loop_at > max_loop_ms)
			max_loop_ms = now - last_loop_at;
		last_loop_at = now;
		gray_update(&gray, board_gray(), now, gray_white_selected());
		stepper_poll(&step, now);
		enum contest_page previous_page = menu.page;
		if (!upgrade_requested)
			handle_keys(now);
		if (previous_page != menu.page) {
			if (holding_finish)
				stop_test(now);
			if (menu.page == MENU_DETAIL && menu.item == EVENT_POSITION) {
				prepare_position(now);
			} else if (preparing_shaft) {
				stop_test(now);
			}
			if (menu.page == MENU_DETAIL && menu.item == EVENT_LAP) {
				/* Select black-line sensing before the confirming press. */
				controls.task = TASK_LAP;
				controls.lap_reverse = false;
				menu.notice = NOTICE_NONE;
				event_elapsed = 0;
			}
		}
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
		follow_line(now);
		/* Bound a noisy camera's share of each pass through the control loop. */
		if (camera_ready) {
			for (unsigned int i = 0; i < 64 && serial_read(SERIAL_CAMERA, &byte); i++) {
				uint16_t previous = camera.sequence;
				ball_feed(&camera, byte, now);
				if (camera.sequence != previous && lap.status == LAP_RUNNING && offset_lap_running())
					record_offset_sample(now);
			}
		}
		if ((menu.page == MENU_RUN && menu.item == EVENT_LAP) || offset_lap_running()) {
			uint32_t duration = now - controls.started_at;
			enum contest_notice notice = contest_lap_notice(lap.status, duration,
				menu.item == EVENT_LAP ? CONTEST_LAP_LIMIT_MS : LAP_TIMEOUT_MS);
			if (notice == NOTICE_TIMEOUT) {
				lap.elapsed_ms = duration;
				lap_stop(&lap, LAP_TIMEOUT);
			}
			if (notice != NOTICE_NONE)
				finish_event(notice, now);
		}
		if (holding_finish || (menu.page == MENU_RUN && (menu.item == EVENT_POSITION || menu.item == EVENT_OFFSET)))
			poll_position_event(now);
		else if (preparing_shaft)
			poll_prepare_level(now);
		else
			poll_balance(now);
#ifdef CORAL_DEBUG_KEYS
		record_position(now);
#endif
		if (!controls.running && !preparing_shaft && !holding_finish)
			stop_test(now);
		balance_time_update(&beam_time, &beam, controls.running, now);
		sample_inputs(now);
		update_ui(now);
		work_at = board_millis();
		if (!upgrade_requested)
			send_telemetry(now);
		work_ms = board_millis() - work_at;
		if (work_ms > max_telemetry_ms)
			max_telemetry_ms = work_ms;
		upgrade_poll(now);
		work_at = board_millis();
		if (lcd_ok)
			lcd_ok = display_poll(&ui, now);
		work_ms = board_millis() - work_at;
		if (work_ms > max_display_ms)
			max_display_ms = work_ms;
		board_led((now / (controls.running ? 125U : 500U)) & 1U);
	}
}
