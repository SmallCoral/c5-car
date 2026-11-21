#include <limits.h>
#include <stdlib.h>
#include "balance.h"
#include "jog.h"
#include "logic.h"

enum { JOG_PER_TURN = 200, JOG_LONG_MS = 700 };

enum jog_action jog_key_update(struct jog_keys *keys, uint8_t down, bool enabled, uint32_t now)
{
	const uint8_t left = 1U << KEY_OK, right = 1U << KEY_NEXT;
	uint8_t pressed = down & (left | right);
	if (!pressed) {
		enum jog_action tap = JOG_NONE;
		if (enabled && !down && !keys->consumed && keys->pressed)
			tap = keys->pressed == left ? JOG_LEFT : JOG_RIGHT;
		*keys = (struct jog_keys){0};
		return tap;
	}
	if (!enabled || down != pressed || pressed == (left | right) ||
	    (keys->pressed && keys->pressed != pressed))
		keys->consumed = true;
	if (!keys->pressed)
		keys->since = now;
	keys->pressed = pressed;
	if (keys->consumed || !elapsed(now, keys->since, JOG_LONG_MS))
		return JOG_NONE;
	keys->consumed = true;
	return pressed == left ? JOG_MAP : JOG_ZERO;
}

bool jog_busy(const struct shaft_jog *jog)
{
	return jog->state == JOG_LOCKING || jog->state == JOG_MOVING;
}

enum jog_action jog_cal_key(struct jog_keys *keys, uint8_t down, uint32_t now)
{
	const uint8_t up = 1U << KEY_OK, lower = 1U << KEY_NEXT;
	if (!down) {
		*keys = (struct jog_keys){0};
		return JOG_NONE;
	}
	if ((down != up && down != lower) || (keys->pressed && keys->pressed != down))
		keys->consumed = true;
	if (keys->consumed)
		return JOG_NONE;
	if (keys->pressed) {
		if (!elapsed(now, keys->since, keys->repeating ? 120 : 450))
			return JOG_NONE;
		keys->repeating = true;
	}
	keys->pressed = down;
	keys->since = now;
	return down == up ? JOG_LEFT : JOG_RIGHT;
}

static int64_t nearest(int64_t value, int32_t divisor)
{
	return (value + (value < 0 ? -divisor / 2 : divisor / 2)) / divisor;
}

/* Anchor the button grid to an actual drive microstep. Each click adds exactly
 * 1/200 turn in pulse space; retries resend the same absolute FD target. */
static bool set_target(struct shaft_jog *jog, uint32_t ppr, int steps)
{
	int64_t pulses = nearest((int64_t)jog->origin * ppr, 65536) +
			 (int64_t)steps * (ppr / JOG_PER_TURN);
	int64_t raw = nearest(pulses * 65536, (int32_t)ppr);
	int32_t margin = (32768U + ppr - 1U) / ppr + 24;
	if (raw < (int64_t)jog->lower + margin || raw > (int64_t)jog->upper - margin)
		return false;
	jog->target = (int32_t)raw;
	return true;
}

bool jog_press(struct shaft_jog *jog, struct stepper *step, int direction, uint32_t now)
{
	if ((direction != -1 && direction != 1) || !step->pulses_per_rev ||
	    step->pulses_per_rev % JOG_PER_TURN || !stepper_online(step, now) ||
	    (step->flags & 0x0cU) || step->error)
		return false;
	if (!jog_busy(jog)) {
		if (step->phase != STEP_PARKED || !step->hold)
			jog->referenced = false;
		if (!jog->referenced)
			jog->steps = 0;
		jog->held_at = jog->target;
		jog->held_steps = jog->steps;
	}
	int next = jog->steps + direction;
	jog->limited = abs(next) > JOG_PER_TURN ||
		       (jog->referenced && !set_target(jog, step->pulses_per_rev, next));
	if (jog->limited)
		return false;
	if (!jog_busy(jog)) {
		if (!stepper_latch(step, now))
			return false;
		jog->state = JOG_LOCKING;
		jog->since = now;
	} else if (jog->state == JOG_MOVING) {
		step->goal = jog->target;
		jog->since = now;
	}
	jog->steps = next;
	return true;
}

static void jog_fault(struct shaft_jog *jog, struct stepper *step, uint32_t now)
{
	jog->state = JOG_FAILED;
	jog->referenced = false;
	if (stepper_running(step) || step->phase == STEP_LATCH)
		stepper_brake(step, now);
}

void jog_poll(struct shaft_jog *jog, struct stepper *step, uint32_t now)
{
	if (!jog_busy(jog))
		return;
	if (step->error || (step->flags & 0x0cU) || !stepper_online(step, now) ||
	    elapsed(now, jog->since, 1500)) {
		jog_fault(jog, step, now);
		return;
	}
	if (jog->state == JOG_LOCKING) {
		if (step->phase == STEP_LATCH)
			return;
		if (step->phase != STEP_PARKED || !step->hold) {
			jog_fault(jog, step, now);
			return;
		}
		if (!(step->flags & 1U) || !step->position_valid ||
		    now - step->position_at >= now - jog->since)
			return;
		if (jog->referenced && llabs((int64_t)step->position - jog->held_at) > 40) {
			/* The linkage was displaced between presses. Apply only the new
			 * clicks to the fresh shaft position and keep the travel limits. */
			jog->steps -= jog->held_steps;
			jog->referenced = false;
		}
		if (!jog->referenced) {
			if (!step->travel_valid) {
				jog_fault(jog, step, now);
				return;
			}
			jog->lower = step->shaft_level + SHAFT_HIGH_OFFSET;
			jog->upper = step->shaft_level + SHAFT_LOW_OFFSET;
			jog->origin = jog->target = step->position;
			jog->referenced = true;
		}
		if (!set_target(jog, step->pulses_per_rev, jog->steps)) {
			jog->limited = true;
			jog->steps = 0;
			jog->referenced = false;
			jog->state = JOG_HELD;
			return;
		}
		if (!stepper_hold(step, jog->lower, jog->upper, now)) {
			jog_fault(jog, step, now);
			return;
		}
		step->goal = jog->target;
		jog->state = JOG_MOVING;
		jog->since = now;
		return;
	}
	if (step->phase != STEP_POSITION || elapsed(now, step->position_at, 240) ||
	    (int64_t)step->position < (int64_t)jog->lower - 32 ||
	    (int64_t)step->position > (int64_t)jog->upper + 32) {
		jog_fault(jog, step, now);
		return;
	}
	/* Wait for fresh position and reached-bit replies for this particular FD.
	 * Keep the completed target energized when returning to parked polling. */
	if (step->goal_sent && step->sent_goal == jog->target && !step->pending &&
	    now - step->position_at < now - step->goal_at && now - step->flags_at < now - step->goal_at &&
	    (step->flags & 3U) == 3U && llabs((int64_t)step->position - jog->target) <= 24) {
		step->phase = STEP_PARKED;
		jog->state = JOG_HELD;
	}
}
