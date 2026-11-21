#include <limits.h>
#include <string.h>
#include "config.h"
#include "logic.h"
#include "serial.h"
#include "stepper.h"

static size_t encode_goal(const struct stepper *step, int32_t target, uint16_t rpm, uint8_t *frame);

static bool within_travel(const struct stepper *step, int32_t raw)
{
	return step->travel_valid && raw >= step->shaft_level + SHAFT_HIGH_OFFSET &&
	       raw <= step->shaft_level + SHAFT_LOW_OFFSET;
}

static bool send_command(struct stepper *step, const uint8_t *data,
			 size_t length, uint32_t now)
{
	if (!serial_write(SERIAL_STEP, data, length)) {
		step->error = 0xe1;
		return false;
	}
	step->pending = data[1];
	step->sent_at = now;
	step->retried = false;
	return true;
}

static void send_disable(struct stepper *step, uint32_t now)
{
	uint8_t frame[6];
	size_t length = emm_enable(frame, STEP_ADDRESS, false);

	step->phase = STEP_DISABLE;
	if (!send_command(step, frame, length, now)) {
		step->phase = STEP_OFFLINE;
		step->sent_at = now;
	}
}

void stepper_stop(struct stepper *step, uint32_t now)
{
	uint8_t frame[5];
	size_t length = emm_stop(frame, STEP_ADDRESS);

	step->cleanup_only = step->phase == STEP_BOOT || step->phase == STEP_OFFLINE;
	step->boot_parked = false;
	step->phase = STEP_STOP;
	step->direction_probe = DIR_NONE;
	step->hold = false;
	/* Override pending reads/enables so a late ACK cannot trigger a move. */
	step->pending = 0;
	if (!send_command(step, frame, length, now))
		send_disable(step, now);
}

void stepper_init(struct stepper *step)
{
	memset(step, 0, sizeof(*step));
	step->phase = STEP_BOOT;
}

void stepper_brake(struct stepper *step, uint32_t now)
{
	uint8_t frame[5];
	size_t length;

	if (!step->hold) {
		stepper_stop(step, now);
		return;
	}
	/* FE stops the shaft. Keep the drive energized to support the linkage. */
	step->phase = STEP_BRAKE;
	step->direction_probe = DIR_NONE;
	step->pending = 0;
	step->sent_at = now;
	length = emm_stop(frame, STEP_ADDRESS);
	send_command(step, frame, length, now);
}

bool stepper_online(const struct stepper *step, uint32_t now)
{
	return step->seen && !elapsed(now, step->last_reply, 1000);
}

void stepper_resume_hold(struct stepper *step, uint32_t now)
{
	/* FE stops any residual motion while preserving the drive's enable
	 * state. The subsequent 3A reply tells us whether it was energized. */
	step->cleanup_only = true;
	step->hold = true;
	stepper_brake(step, now);
}

bool stepper_running(const struct stepper *step)
{
	return step->phase == STEP_ENABLE || step->phase == STEP_SPEED ||
	       step->phase == STEP_MOVING || step->phase == STEP_POSITION || step->phase == STEP_BAL_SPEED;
}

void stepper_request_config(struct stepper *step, uint32_t now)
{
	const uint8_t cmd[] = { STEP_ADDRESS, 0x42, 0x6c, 0x6b };

	if ((step->phase == STEP_IDLE || (step->phase == STEP_PARKED && step->hold)) &&
	    !step->pending && !step->pulses_per_rev &&
	    elapsed(now, step->config_at, 1000)) {
		step->config_at = now;
		send_command(step, cmd, sizeof(cmd), now);
	}
}

static bool start(struct stepper *step, bool reverse, uint16_t rpm, bool hold,
		  uint32_t now)
{
	uint8_t frame[6];
	size_t length;

	bool available = step->phase == STEP_IDLE ||
			 (step->phase == STEP_PARKED && step->boot_parked && (step->flags & 1U));
	if (!available || !stepper_online(step, now) ||
	    (step->flags & 0x0cU) || !rpm || rpm > 3U * STEP_RPM_STEP)
		return false;
	length = emm_enable(frame, STEP_ADDRESS, true);
	if (!send_command(step, frame, length, now))
		return false;
	step->boot_parked = false;
	step->reverse = reverse;
	step->hold = hold;
	step->target_rpm = rpm;
	step->rpm_valid = false;
	step->error = 0;
	step->phase = STEP_ENABLE;
	return true;
}

bool stepper_start(struct stepper *step, bool reverse, uint16_t rpm, uint32_t now)
{
	if (!within_travel(step, step->position) || !step->pulses_per_rev ||
	    elapsed(now, step->position_at, 750))
		return false;
	step->lower = step->shaft_level + SHAFT_HIGH_OFFSET;
	step->upper = step->shaft_level + SHAFT_LOW_OFFSET;
	return start(step, reverse, rpm, false, now);
}

bool stepper_latch(struct stepper *step, uint32_t now)
{
	if (!step->pulses_per_rev || !stepper_online(step, now) || (step->flags & 4U))
		return false;
	if (step->phase == STEP_PARKED && step->hold && (step->flags & 1U)) {
		step->boot_parked = false;
		step->last_poll = now - 130;
		step->error = 0;
		return true;
	}
	if (!start(step, false, 15, true, now))
		return false;
	/* Enable at the present shaft position without issuing a motion target. */
	step->phase = STEP_LATCH;
	return true;
}

bool stepper_hold(struct stepper *step, int32_t lower, int32_t upper, uint32_t now)
{
	if (!within_travel(step, step->position))
		return false;
	if (lower < step->shaft_level + SHAFT_HIGH_OFFSET)
		lower = step->shaft_level + SHAFT_HIGH_OFFSET;
	if (upper > step->shaft_level + SHAFT_LOW_OFFSET)
		upper = step->shaft_level + SHAFT_LOW_OFFSET;
	if (step->phase != STEP_PARKED || !step->hold || !(step->flags & 1U) ||
	    (step->flags & 4U) || !step->pulses_per_rev || !step->position_valid ||
	    elapsed(now, step->position_at, 750) || lower > step->position ||
	    upper < step->position || lower >= upper || !stepper_online(step, now))
		return false;
	step->lower = lower;
	step->upper = upper;
	step->boot_parked = false;
	step->target_rpm = 15;
	step->goal = step->position;
	step->goal_sent = false;
	step->phase = STEP_POSITION;
	step->last_poll = now - 40;
	step->flags_at = now;
	step->error = 0;
	return true;
}

bool stepper_speed_loop(struct stepper *step, int32_t lower, int32_t upper, uint32_t now)
{
	if (step->pending || elapsed(now, step->position_at, 15) ||
	    !stepper_hold(step, lower, upper, now))
		return false;
	step->phase = STEP_BAL_SPEED;
	step->reverse = true;
	step->target_rpm = step->sent_rpm = 0;
	step->speed_due = true;
	return true;
}

void stepper_balance_rpm(struct stepper *step, int rpm)
{
	if (step->phase != STEP_BAL_SPEED)
		return;
	step->reverse = rpm >= 0; /* F407 Contron_X_Speed: positive is direction 1 (CCW). */
	step->target_rpm = (uint16_t)(rpm < -50 || rpm > 50 ? 0 : rpm < 0 ? -rpm : rpm);
	step->speed_due = true;
}

bool stepper_measure_direction(struct stepper *step, int32_t origin, bool ccw, uint32_t now)
{
	if (origin < INT32_MIN + 65536 || origin > INT32_MAX - 65536 ||
	    (int64_t)step->position - origin < -120 || (int64_t)step->position - origin > 120 ||
	    !within_travel(step, step->position - 1040) || !within_travel(step, step->position + 1040) ||
	    !stepper_hold(step, origin - 1040, origin + 1040, now))
		return false;
	step->direction_origin = step->position;
	step->direction_probe = DIR_ALIGN;
	step->probing_ccw = ccw;
	step->direction_started = now;
	step->target_rpm = 2;
	return true;
}

static void handle_reply(struct stepper *step, const struct emm_reply *reply,
			 uint32_t now)
{
	uint8_t frame[13];
	size_t length;

	step->replies++;
	/* Only the outstanding command proves that this exchange succeeded. */
	if (!step->pending || reply->command != step->pending ||
	    (reply->command == 0xfd && reply->status == 0x9f))
		return;
	step->pending = 0;
	step->seen = true;
	step->last_reply = now;
	if (reply->command == 0x42) {
		step->pulses_per_rev = (uint32_t)reply->value;
		return;
	}
	if (reply->command == 0x3a) {
		step->flags = reply->status;
		if (step->phase == STEP_PARKED && !(step->flags & 1U)) {
			if (step->cleanup_only && !(step->flags & 0x0cU))
				step->error = 0;
			step->cleanup_only = false;
			step->boot_parked = false;
			step->hold = false;
			step->phase = STEP_IDLE;
			return;
		}
		if (step->phase == STEP_PARKED)
			step->cleanup_only = false;
		if ((stepper_running(step) && (step->flags & 4U)) ||
		    ((step->phase == STEP_POSITION || step->phase == STEP_BAL_SPEED) && !(step->flags & 1U))) {
			step->error = 0xe2;
			stepper_brake(step, now);
		}
		/* Position-reached is not a completion condition in velocity mode. */
		return;
	}
	if (reply->command == 0x35) {
		step->rpm = reply->value;
		step->rpm_valid = true;
		return;
	}
	if (reply->command == 0x43) {
		/* Both fields are from one Emm snapshot. A drive restart can change the
		 * multi-turn origin while the absolute shaft orientation stays the same. */
		int offset = (uint16_t)(reply->absolute - SHAFT_LEVEL_ABS);
		if (offset >= 32768)
			offset -= 65536;
		int64_t level = (int64_t)reply->value - offset;
		bool active = stepper_running(step);
		bool rebased = step->travel_valid &&
			       (level < (int64_t)step->shaft_level - 4 || level > (int64_t)step->shaft_level + 4);
		step->travel_valid = level > INT32_MIN + 65536 && level < INT32_MAX - 65536 &&
				     offset >= SHAFT_HIGH_OFFSET && offset <= SHAFT_LOW_OFFSET;
		if (step->travel_valid)
			step->shaft_level = (int32_t)level;
		step->position = reply->value;
		step->position_at = now;
		step->position_valid = true;
		step->flags = reply->status;
		if (active && (rebased || !step->travel_valid || (step->flags & 0x0dU) != 1U)) {
			step->error = 0xe4;
			stepper_brake(step, now);
			return;
		}
		if (step->direction_probe != DIR_NONE) {
			int64_t delta = (int64_t)step->position - step->direction_origin;
			if (delta < -1040 || delta > 1040) {
				step->error = 0xe4;
				stepper_brake(step, now);
			} else if (step->direction_probe == DIR_MEASURE &&
				   elapsed(now, step->direction_at, 430) && (delta < -520 || delta > 520)) {
				step->cw_sign = (delta > 0) != step->probing_ccw ? 1 : -1;
				stepper_brake(step, now);
			}
		}
		return;
	}
	if (reply->status != 0x02) {
		step->error = reply->status ? reply->status : 0xee;
		if (step->phase == STEP_STOP) {
			send_disable(step, now);
		} else if (step->phase == STEP_DISABLE) {
			step->phase = STEP_OFFLINE;
			step->sent_at = now;
		} else if (step->phase == STEP_BRAKE) {
			/* No automatic enable on a drive fault. Verify flags while parked. */
			step->phase = STEP_PARKED;
			step->flags_at = now - 220;
		} else {
			stepper_brake(step, now);
		}
		return;
	}
	switch (step->phase) {
	case STEP_POSITION:
		if (reply->command == 0xfd && step->direction_probe == DIR_ALIGN) {
			step->direction_probe = DIR_SETTLE;
			step->direction_at = now;
		}
		break;
	case STEP_LATCH:
		step->phase = STEP_PARKED;
		step->last_poll = now - 130;
		step->flags_at = now - 220;
		break;
	case STEP_ENABLE:
		length = encode_goal(step, step->reverse ? step->lower : step->upper, step->target_rpm, frame);
		step->phase = STEP_SPEED;
		if (!length || !send_command(step, frame, length, now))
			stepper_stop(step, now);
		break;
	case STEP_SPEED:
		step->phase = STEP_MOVING;
		step->last_poll = now;
		step->poll_index = 0;
		break;
	case STEP_BRAKE:
		step->phase = STEP_PARKED;
		step->flags_at = now - 220;
		break;
	case STEP_STOP:
		send_disable(step, now);
		break;
	case STEP_DISABLE:
		step->phase = STEP_IDLE;
		step->flags &= (uint8_t)~1U;
		/* A successful disable establishes the startup/reconnect safe state,
		 * even if the preceding stop was rejected while already disabled.
		 * Errors during an actual movement remain visible until the next start.
		 */
		if (step->cleanup_only)
			step->error = 0;
		break;
	default:
		break;
	}
}

static void reply_timeout(struct stepper *step, uint32_t now)
{
	uint8_t pending = step->pending;
	step->pending = 0;
	step->timeouts++;
	step->error = 0xe0;
	step->travel_valid = false;
	if (pending == 0x42 && step->phase == STEP_IDLE)
		return;
	if (step->hold) {
		stepper_brake(step, now);
		return;
	}
	step->pulses_per_rev = 0;
	step->cw_sign = 0;
	step->position_valid = false;
	if (step->phase == STEP_STOP) {
		/* Still try disable when the stop reply is missing. */
		send_disable(step, now);
	} else if (step->phase == STEP_DISABLE || step->phase == STEP_IDLE) {
		step->phase = STEP_OFFLINE;
		step->sent_at = now;
	} else {
		stepper_stop(step, now);
	}
}

static size_t encode_goal(const struct stepper *step, int32_t target, uint16_t rpm, uint8_t *frame)
{
	if (!within_travel(step, target) || !step->pulses_per_rev ||
	    target < step->lower || target > step->upper)
		return 0;
	/* The drive rounds to a whole microstep. Keep the transmitted endpoint inside
	 * both encoder limits even when level falls between adjacent microsteps. */
	int64_t margin = (32768U + step->pulses_per_rev - 1U) / step->pulses_per_rev;
	int64_t lo = (int64_t)step->lower + margin, hi = (int64_t)step->upper - margin;
	int32_t safe_lo = step->shaft_level + SHAFT_HIGH_OFFSET + (int32_t)margin + 24;
	int32_t safe_hi = step->shaft_level + SHAFT_LOW_OFFSET - (int32_t)margin - 24;
	if (lo < safe_lo)
		lo = safe_lo;
	if (hi > safe_hi)
		hi = safe_hi;

	if (lo > hi)
		return 0;
	if (target < lo)
		target = (int32_t)lo;
	if (target > hi)
		target = (int32_t)hi;
	return emm_position(frame, STEP_ADDRESS, target, step->pulses_per_rev, rpm);
}

static void position_poll(struct stepper *step, uint32_t now)
{
	uint8_t frame[13] = { STEP_ADDRESS, 0x43, 0x7a, 0x6b };
	size_t size;
	bool parked = step->phase == STEP_PARKED;

	if (!elapsed(now, step->last_reply, 6))
		return;
	if (step->direction_probe == DIR_SETTLE && elapsed(now, step->direction_at, 80) &&
	    now - step->position_at < now - step->direction_at &&
	    (int64_t)step->position - step->direction_origin >= -80 &&
	    (int64_t)step->position - step->direction_origin <= 80) {
		/* The last accepted absolute target is the present shaft position.
		 * Both Emm generations accept explicit CW/CCW in relative mode 00. */
		size = emm_position(frame, STEP_ADDRESS, step->probing_ccw ? -728 : 728,
				    step->pulses_per_rev, 2);
		frame[10] = 0;
		step->direction_probe = DIR_MEASURE;
		step->direction_at = now;
	} else if (elapsed(now, step->flags_at, parked ? 220 : 150)) {
		step->flags_at = now;
		frame[1] = 0x3a;
		frame[2] = 0x6b;
		size = 3;
	} else if (elapsed(now, step->last_poll, parked ? 130 : 24)) {
		step->last_poll = now;
		size = 4;
	} else if (!parked && (!step->goal_sent || step->goal != step->sent_goal ||
				step->target_rpm != step->sent_rpm) && elapsed(now, step->goal_at, 28)) {
		size = encode_goal(step, step->goal, step->target_rpm, frame);
		if (!size) {
			step->error = 0xe4;
			stepper_brake(step, now);
			return;
		}
		step->sent_goal = step->goal;
		step->sent_rpm = step->target_rpm;
		step->goal_sent = true;
		step->goal_at = now;
	} else {
		return;
	}
	if (!size || !send_command(step, frame, size, now))
		stepper_brake(step, now);
}

static void retry_pending_command(struct stepper *step, uint32_t now)
{
	uint8_t frame[13] = { STEP_ADDRESS, step->pending, 0x6b };
	size_t n;

	switch (step->pending) {
	case 0x35: case 0x36: case 0x3a: n = 3; break;
	case 0x42:
		frame[2] = 0x6c;
		frame[3] = 0x6b;
		n = 4;
		break;
	case 0x43:
		frame[2] = 0x7a;
		frame[3] = 0x6b;
		n = 4;
		break;
	case 0xf3: n = emm_enable(frame, STEP_ADDRESS, true); break;
	case 0xfe: n = emm_stop(frame, STEP_ADDRESS); break;
	case 0xfd:
		n = encode_goal(step, step->sent_goal, step->sent_rpm, frame);
		break;
	default: return;
	}
	if (n && send_command(step, frame, n, now))
		step->retried = true;
}

static void speed_loop_poll(struct stepper *step, uint32_t now)
{
	uint32_t age = now - step->position_at;
	/* Continuous F6 needs live feedback even while waiting for its ACK. */
	if (age >= 40 || (step->pending && elapsed(now, step->sent_at, 24))) {
		step->error = 0xe0;
		step->timeouts++;
		stepper_brake(step, now);
		return;
	}
	if ((int64_t)step->position < (int64_t)step->lower - 273 ||
	    (int64_t)step->position > (int64_t)step->upper + 273) {
		step->error = 0xe4;
		stepper_brake(step, now);
		return;
	}
	if (step->pending || !elapsed(now, step->last_reply, 1))
		return;
	uint8_t frame[13] = { STEP_ADDRESS, 0x43, 0x7a, 0x6b };
	size_t size = 4;
	/* Reserve enough stroke to stop on the next encoder/velocity cycle. */
	int64_t remain = step->reverse ? (int64_t)step->position - step->lower :
					 (int64_t)step->upper - step->position;
	int64_t runout = (int64_t)step->target_rpm * 65536 * (age + 25) / 60000;
	uint16_t rpm = remain <= runout ? 0 : step->target_rpm;
	bool stop_now = !rpm && step->sent_rpm;
	if (stop_now || (step->speed_due && !rpm)) {
		size = emm_velocity(frame, STEP_ADDRESS, step->reverse, 0, 0);
	} else if (elapsed(now, step->last_poll, 10)) {
		step->last_poll = now;
	} else if (elapsed(now, step->flags_at, 120)) {
		frame[1] = 0x3a;
		frame[2] = 0x6b;
		size = 3;
		step->flags_at = now;
	} else if (step->speed_due) {
		/* A lost link must not let the last speed request run past a stop. */
		int32_t goal = step->reverse ? step->lower : step->upper;
		if (step->sent_rpm == rpm && step->sent_goal == goal) {
			step->speed_due = false;
			return;
		}
		size = encode_goal(step, goal, rpm, frame);
		step->sent_goal = goal;
	} else {
		return;
	}
	if (frame[1] == 0xf6 || frame[1] == 0xfd) {
		step->speed_due = false;
		step->sent_rpm = rpm;
		step->sent_reverse = step->reverse;
	}
	if (!size || !send_command(step, frame, size, now))
		stepper_brake(step, now);
}

void stepper_poll(struct stepper *step, uint32_t now)
{
	struct serial_stats stats = serial_stats(SERIAL_STEP);
	struct emm_reply reply;
	uint8_t byte;

	if (stats.errors + stats.rx_drops != step->rx_errors) {
		step->rx_errors = stats.errors + stats.rx_drops;
		step->last_bad_phase = (uint8_t)step->phase;
		step->last_bad_command = step->pending;
		step->parser.used = 0;
		/* A gap may turn a partial frame into a different valid-looking one. */
		while (serial_read(SERIAL_STEP, &byte))
			;
		if (stepper_running(step) || step->phase == STEP_LATCH) {
			step->error = 0xe3;
			stepper_brake(step, now);
		}
	}
	while (serial_read(SERIAL_STEP, &byte)) {
		if (emm_feed(&step->parser, STEP_ADDRESS, byte, now, &reply))
			handle_reply(step, &reply, now);
	}
	if (step->phase == STEP_BAL_SPEED) {
		speed_loop_poll(step, now);
		return;
	}
	/* A lost ACK must never replay a relative movement. Verify the encoder instead. */
	if (step->direction_probe == DIR_MEASURE && step->pending == 0xfd) {
		if (elapsed(now, step->sent_at, 65))
			step->pending = 0;
	} else if ((step->hold || step->phase == STEP_IDLE) && step->pending &&
		   !step->retried && elapsed(now, step->sent_at, 45)) {
		/* IDLE only polls the drive. Recover one damaged read before dropping
		 * its configuration and declaring it offline. */
		retry_pending_command(step, now);
	}
	if (step->direction_probe != DIR_NONE && elapsed(now, step->direction_started, 2200)) {
		step->error = 0xe5;
		stepper_brake(step, now);
	}
	if (step->pending && elapsed(now, step->sent_at, STEP_REPLY_TIMEOUT_MS))
		reply_timeout(step, now);
	if (step->phase == STEP_BOOT && now >= STEP_BOOT_MS) {
		/* Stop residual motion and read the enable flag. An energized
		 * drive may already be supporting the pipe at power-up. */
		stepper_resume_hold(step, now);
		step->boot_parked = true;
	}
	if (step->phase == STEP_OFFLINE && elapsed(now, step->sent_at, 1000))
		stepper_stop(step, now);
	if (step->phase == STEP_BRAKE && !step->pending && elapsed(now, step->sent_at, 50))
		stepper_brake(step, now);
	if (!step->pending && (step->phase == STEP_POSITION || step->phase == STEP_PARKED)) {
		position_poll(step, now);
		return;
	}
	if (step->pending || (step->phase != STEP_IDLE &&
			      step->phase != STEP_MOVING) ||
	    !elapsed(now, step->last_poll, STEP_POLL_MS))
		return;
	{
		static const uint8_t queries[] = { 0x3a, 0x35, 0x43 };
		uint8_t frame[] = { STEP_ADDRESS, queries[step->poll_index], 0x6b, 0x6b };
		size_t length = 3;
		if (frame[1] == 0x43) {
			frame[2] = 0x7a;
			length = 4;
		}

		step->poll_index = (step->poll_index + 1U) % sizeof(queries);
		step->last_poll = now;
		if (!send_command(step, frame, length, now))
			stepper_stop(step, now);
	}
}

const char *stepper_name(const struct stepper *step, uint32_t now)
{
	if (step->phase == STEP_BOOT)
		return "BOOT";
	if (!stepper_online(step, now) || step->phase == STEP_OFFLINE)
		return "NO LINK";
	if (step->error || (step->flags & 4U))
		return "ERROR";
	if (stepper_running(step))
		return "MOVING";
	if (step->phase == STEP_PARKED)
		return "HOLDING";
	if (step->phase == STEP_LATCH)
		return "LOCKING";
	if (step->phase == STEP_STOP || step->phase == STEP_DISABLE || step->phase == STEP_BRAKE)
		return "STOPPING";
	return "READY";
}
