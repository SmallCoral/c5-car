#include <string.h>
#include "config.h"
#include "logic.h"
#include "serial.h"
#include "stepper.h"

static bool send_command(struct stepper *step, const uint8_t *data,
			 size_t length, uint32_t now)
{
	if (!serial_write(SERIAL_STEP, data, length)) {
		step->error = 0xe1;
		return false;
	}
	step->pending = data[1];
	step->sent_at = now;
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
	step->phase = STEP_STOP;
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

bool stepper_online(const struct stepper *step, uint32_t now)
{
	return step->seen && !elapsed(now, step->last_reply, 1000);
}

bool stepper_running(const struct stepper *step)
{
	return step->phase == STEP_ENABLE || step->phase == STEP_SPEED ||
	       step->phase == STEP_MOVING;
}

bool stepper_start(struct stepper *step, bool reverse, uint16_t rpm, uint32_t now)
{
	uint8_t frame[6];
	size_t length;

	if (step->phase != STEP_IDLE || !stepper_online(step, now) ||
	    (step->flags & 4U) || !rpm || rpm > 3U * STEP_RPM_STEP)
		return false;
	step->reverse = reverse;
	step->target_rpm = rpm;
	step->rpm_valid = false;
	step->error = 0;
	length = emm_enable(frame, STEP_ADDRESS, true);
	if (!send_command(step, frame, length, now))
		return false;
	step->phase = STEP_ENABLE;
	return true;
}

static void handle_reply(struct stepper *step, const struct emm_reply *reply,
			 uint32_t now)
{
	uint8_t frame[8];
	size_t length;

	step->replies++;
	/* Only the outstanding command proves that this exchange succeeded. */
	if (!step->pending || reply->command != step->pending)
		return;
	step->pending = 0;
	step->seen = true;
	step->last_reply = now;
	if (reply->command == 0x3a) {
		step->flags = reply->status;
		if (stepper_running(step) && (step->flags & 4U)) {
			step->error = 0xe2;
			stepper_stop(step, now);
		}
		/* Position-reached is not a completion condition in velocity mode. */
		return;
	}
	if (reply->command == 0x35) {
		step->rpm = reply->value;
		step->rpm_valid = true;
		return;
	}
	if (reply->command == 0x36) {
		step->position = reply->value;
		step->position_valid = true;
		return;
	}
	if (reply->status != 0x02) {
		step->error = reply->status ? reply->status : 0xee;
		if (step->phase == STEP_STOP) {
			send_disable(step, now);
		} else if (step->phase == STEP_DISABLE) {
			step->phase = STEP_OFFLINE;
			step->sent_at = now;
		} else {
			stepper_stop(step, now);
		}
		return;
	}
	switch (step->phase) {
	case STEP_ENABLE:
		length = emm_velocity(frame, STEP_ADDRESS, step->reverse,
				      step->target_rpm, STEP_ACCEL);
		step->phase = STEP_SPEED;
		if (!send_command(step, frame, length, now))
			stepper_stop(step, now);
		break;
	case STEP_SPEED:
		step->phase = STEP_MOVING;
		step->last_poll = now;
		step->poll_index = 0;
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
	step->pending = 0;
	step->timeouts++;
	step->error = 0xe0;
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

void stepper_poll(struct stepper *step, uint32_t now)
{
	struct serial_stats stats = serial_stats(SERIAL_STEP);
	struct emm_reply reply;
	uint8_t byte;

	if (stats.errors + stats.rx_drops != step->rx_errors) {
		step->rx_errors = stats.errors + stats.rx_drops;
		step->parser.used = 0;
		/* A gap may turn a partial frame into a different valid-looking one. */
		while (serial_read(SERIAL_STEP, &byte))
			;
		if (stepper_running(step)) {
			step->error = 0xe3;
			stepper_stop(step, now);
		}
	}
	while (serial_read(SERIAL_STEP, &byte)) {
		if (emm_feed(&step->parser, STEP_ADDRESS, byte, now, &reply))
			handle_reply(step, &reply, now);
	}
	if (step->pending && elapsed(now, step->sent_at, STEP_REPLY_TIMEOUT_MS))
		reply_timeout(step, now);
	if ((step->phase == STEP_BOOT && now >= STEP_BOOT_MS) ||
	    (step->phase == STEP_OFFLINE && elapsed(now, step->sent_at, 1000)))
		stepper_stop(step, now);
	if (step->pending || (step->phase != STEP_IDLE &&
			      step->phase != STEP_MOVING) ||
	    !elapsed(now, step->last_poll, STEP_POLL_MS))
		return;
	{
		static const uint8_t queries[] = { 0x3a, 0x35, 0x36 };
		uint8_t frame[] = { STEP_ADDRESS, queries[step->poll_index], 0x6b };

		step->poll_index = (step->poll_index + 1U) % sizeof(queries);
		step->last_poll = now;
		if (!send_command(step, frame, sizeof(frame), now))
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
	if (step->phase == STEP_STOP || step->phase == STEP_DISABLE)
		return "STOPPING";
	return "READY";
}
