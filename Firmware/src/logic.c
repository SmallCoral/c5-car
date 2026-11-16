#include <limits.h>
#include "config.h"
#include "logic.h"

bool elapsed(uint32_t now, uint32_t then, uint32_t interval)
{
	return now - then >= interval;
}

bool key_update(struct debounce *key, bool down, uint32_t now)
{
	if (down != key->candidate) {
		key->candidate = down;
		key->changed_at = now;
	}
	if (key->stable == down ||
	    !elapsed(now, key->changed_at, KEY_DEBOUNCE_MS))
		return false;
	key->stable = down;
	return down;
}

int32_t encoder_delta(uint32_t current, uint32_t previous)
{
	uint32_t delta = current - previous;

	if (delta <= INT32_MAX)
		return (int32_t)delta;
	/* Avoid implementation-defined unsigned-to-signed wrap conversion. */
	return -1 - (int32_t)(UINT32_MAX - delta);
}

void controls_key(struct controls *ctl, enum key_id key, uint32_t now)
{
	/* Any key first stops a running test; changing direction requires a stop. */
	if (ctl->running) {
		ctl->running = false;
		return;
	}
	switch (key) {
	case KEY_MODE:
		ctl->task = (ctl->task + 1) % TASK_COUNT;
		break;
	case KEY_NEXT:
		ctl->level = (ctl->level + 1) %
			(ctl->task == TASK_STEPPER ? 3U : WHEEL_LEVEL_COUNT);
		break;
	case KEY_OK:
		ctl->reverse = !ctl->reverse;
		break;
	case KEY_START:
		if (ctl->task != TASK_MONITOR) {
			ctl->running = true;
			ctl->started_at = now;
		}
		break;
	default:
		break;
	}
}

void controls_tick(struct controls *ctl, uint32_t now, bool fault)
{
	if (fault || (!task_continuous(ctl->task) &&
		      elapsed(now, ctl->started_at, TEST_DURATION_MS)))
		ctl->running = false;
}

int controls_duty(const struct controls *ctl)
{
	static const int duties[WHEEL_LEVEL_COUNT] = {
		100, 200, 300, WHEEL_MAX_PERMILLE
	};
	int duty = duties[ctl->level % WHEEL_LEVEL_COUNT];

	return ctl->reverse ? -duty : duty;
}

uint16_t controls_rpm(const struct controls *ctl)
{
	return (uint16_t)((ctl->level % 3U + 1U) * STEP_RPM_STEP);
}

bool task_uses_wheels(enum task_id task)
{
	return task == TASK_LEFT || task == TASK_RIGHT ||
	       task == TASK_BOTH || task == TASK_ALL;
}

bool task_uses_stepper(enum task_id task)
{
	return task == TASK_STEPPER || task == TASK_ALL;
}

bool task_continuous(enum task_id task)
{
	return task == TASK_STEPPER || task == TASK_ALL;
}

const char *task_name(enum task_id task)
{
	static const char *const names[] = {
		"MONITOR", "LEFT", "RIGHT", "BOTH", "STEPPER", "ALL"
	};

	return (unsigned int)task < TASK_COUNT ? names[task] : "INVALID";
}
