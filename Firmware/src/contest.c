#include <stdlib.h>
#include "contest.h"

static const enum task_id tools[] = {
	TASK_BALANCE, TASK_LAP, TASK_GRAY, TASK_MONITOR, TASK_LEFT, TASK_RIGHT, TASK_BOTH, TASK_STEPPER, TASK_ALL
};

enum task_id contest_tool(unsigned int index)
{
	return tools[index % (sizeof(tools) / sizeof(tools[0]))];
}

enum menu_action contest_key(struct contest_menu *m, enum key_id key)
{
	const unsigned int count = sizeof(tools) / sizeof(tools[0]);
	switch (m->page) {
	case MENU_HOME:
		if (key == KEY_MODE) {
			m->notice = NOTICE_NONE;
			m->page = MENU_DETAIL;
		} else if (key == KEY_START) {
			return MENU_NONE;
		} else {
			m->item = (m->item + (key == KEY_OK ? EVENT_COUNT - 1 : 1)) % EVENT_COUNT;
		}
		break;
	case MENU_DETAIL:
		if (key == KEY_START)
			m->page = MENU_HOME;
		else if (key == KEY_MODE)
			return m->item == EVENT_POSITION && m->notice == NOTICE_TRAVEL ?
			       MENU_RELEVEL : MENU_START_EVENT;
		else if (m->item == EVENT_OFFSET) {
			int next = m->target_cm + (key == KEY_OK ? 1 : -1);
			if (next >= -5 && next <= 5)
				m->target_cm = next;
		}
		break;
	case MENU_RUN:
		return MENU_STOP_EVENT;
	case MENU_RESULT:
		if (key == KEY_START || key == KEY_MODE) {
			m->page = MENU_DETAIL;
			m->notice = NOTICE_NONE;
		}
		break;
	case MENU_TOOLS:
		if (key == KEY_MODE) {
			m->page = MENU_DIAGNOSTIC;
			return MENU_OPEN_TOOL;
		}
		if (key == KEY_START)
			m->page = MENU_HOME;
		else
			m->tool = (m->tool + (key == KEY_OK ? count - 1 : 1)) % count;
		break;
	case MENU_DIAGNOSTIC:
		break;
	}
	return MENU_NONE;
}

int contest_ball_mm(int raw_x)
{
	int dx = RULER_X_O - raw_x;
	int span = dx >= 0 ? RULER_X_O - RULER_X_PLUS_5 : RULER_X_MINUS_5 - RULER_X_O;
	return (dx * 50 + (dx >= 0 ? span / 2 : -span / 2)) / span;
}

void position_begin(struct position_run *run, int x, uint16_t seq, uint32_t now)
{
	*run = (struct position_run){ .leg = LEVEL_SHAFT, .start = now, .sample_at = now,
				      .mm = x >= 0 ? contest_ball_mm(x) : 0, .seq = seq,
				      .have_sample = x >= 0 };
}

void position_check_level(struct position_run *r, bool at_level, uint32_t encoder_at)
{
	if (r->leg != LEVEL_SHAFT)
		return;
	if (!at_level) {
		r->steady = false;
	} else if (!r->steady) {
		r->steady = true;
		r->steady_at = encoder_at;
	} else if (encoder_at - r->steady_at >= 75) {
		r->leg = MOVE_CENTER;
		r->steady = false;
	}
}

static bool position_settled(struct position_run *r, int error, uint32_t at, unsigned int unseen_ms)
{
	_Static_assert(POSITION_BANDS <= 32, "Dwell bands must fit in the active mask");
	unsigned int dwell = r->leg == MOVE_CENTER ? POSITION_CENTER_DWELL_MS :
			     r->leg == MOVE_RIGHT ? POSITION_RIGHT_DWELL_MS : POSITION_LEFT_DWELL_MS;
	bool slow = abs(r->velocity) <= 12;
	/* Every valid 4 mm interval fits one of these overlapping bands. Leaving
	 * one band does not discard time already spent in an adjacent band. */
	for (int i = 0; i < POSITION_BANDS; i++) {
		int low = i - POSITION_TOLERANCE_MM;
		uint32_t bit = 1U << i;
		if (error < low || error > low + POSITION_SPAN_MM) {
			r->active_bands &= ~bit;
			continue;
		}
		if ((r->active_bands & bit) && unseen_ms) {
			unsigned int missed = r->band_unseen_ms[i] + unseen_ms;
			if (missed > 140)
				r->active_bands &= ~bit;
			else
				r->band_unseen_ms[i] = missed;
		}
		if (slow) {
			if (!(r->active_bands & bit)) {
				r->active_bands |= bit;
				r->band_since[i] = at;
				r->band_unseen_ms[i] = 0;
			} else if (at - r->band_since[i] >= dwell + r->band_unseen_ms[i]) {
				r->steady_at = r->band_since[i];
				return true;
			}
		}
	}
	return false;
}

/* The shaft angles that held a slow ball varied along this tube: roughly
 * -8 to -11 degrees at -50 mm, -3 to -5 at O, and near zero at +50 mm.
 * Use the conservative end of those measurements and let trim correct it. */
static int position_bias_cd(int mm)
{
	if (mm <= -50)
		return -800;
	if (mm >= 50)
		return 0;
	return -300 + mm * (mm < 0 ? 10 : 6);
}

/* Resume from two fresh measured packets. During travel the ball can move
 * appreciably between them. A completed hold only reacquires its left mark. */
static bool reacquire_position(struct position_run *r, enum ball_status view, int x, uint16_t seq, uint32_t at)
{
	int mm = contest_ball_mm(x);
	bool holding = r->leg == HOLD_LEFT;
	if (view != BALL_TRACKED || (holding && abs(mm + 50) > 2 * POSITION_TOLERANCE_MM)) {
		r->reacquire_seen = false;
		return false;
	}
	if (r->reacquire_seen && seq == r->reacquire_seq)
		return false;
	uint32_t dt = at - r->reacquire_at;
	int dx = mm - r->reacquire_mm;
	if (r->reacquire_seen && (uint16_t)(seq - r->reacquire_seq) == 1 && dt >= 12 && dt <= 85 &&
	    abs(dx) <= (holding ? 3 : 3 + (int)dt / 3)) {
		r->seq = r->reacquire_seq;
		r->sample_at = r->reacquire_at;
		r->mm = r->reacquire_mm;
		r->velocity = abs(dx) <= 1 ? 0 : dx * 1000 / (int)dt;
		r->have_sample = true;
		r->reacquire_seen = false;
		return true;
	}
	r->reacquire_seen = true;
	r->reacquire_seq = seq;
	r->reacquire_at = at;
	r->reacquire_mm = mm;
	return false;
}

enum contest_notice position_update(struct position_run *r, enum ball_status view, int x, uint16_t seq,
				    uint32_t sample_at, uint32_t now)
{
	uint32_t gap = now - r->sample_at;
	bool preparing = r->leg == LEVEL_SHAFT;
	bool quiet_left = (r->leg == MOVE_LEFT || r->leg == HOLD_LEFT) && r->have_sample &&
		abs(r->mm + 50) <= POSITION_TOLERANCE_MM && abs(r->velocity) <= 12;
	if (!preparing && gap >= 120) {
		/* A quiet ball at the left mark needs its last holding angle;
		 * commanding geometric level during a camera gap rolls it away. */
		if (!quiet_left) {
			r->goal_cd = 0;
			r->trim_milli_cd = 0;
		}
		r->active_bands = 0;
		r->still_ms = 0;
	}
	if (!preparing && (r->waiting_vision || gap >= (quiet_left && r->leg == HOLD_LEFT ? 900U : 500U))) {
		if (view == BALL_OFFLINE)
			return NOTICE_VISION;
		r->waiting_vision = true;
		if (!reacquire_position(r, view, x, seq, sample_at))
			return NOTICE_NONE;
		r->waiting_vision = false;
	}
	if (view != BALL_TRACKED || seq == r->seq)
		return NOTICE_NONE;
	uint32_t dt = sample_at - r->sample_at;
	if (!dt || dt > gap)
		return NOTICE_NONE;
	int mm = contest_ball_mm(x);
	int movement = mm - r->mm;
	uint16_t advance = (uint16_t)(seq - r->seq);
	/* Bridge one missing frame only between nearby direct observations.
	 * The entire interval between them is excluded from the dwell. */
	if (advance > 2 || dt > 85 || (advance == 2 && abs(mm - r->mm) > 3))
		r->active_bands = 0;
	if (dt < 120 && r->have_sample) {
		int v = movement * 1000 / (int)dt;
		r->velocity += (v - r->velocity) * (int)dt / (100 + (int)dt);
	} else {
		r->velocity = 0;
		if (!preparing)
			r->active_bands = 0;
	}
	r->mm = mm;
	r->seq = seq;
	r->sample_at = sample_at;
	r->have_sample = true;
	if (r->leg == LEVEL_SHAFT)
		return NOTICE_NONE;
	int target = r->leg == MOVE_CENTER ? 0 : r->leg == MOVE_RIGHT ? 50 : -50;
	int error = target - mm;
	if (r->leg != HOLD_LEFT && position_settled(r, error, sample_at, advance == 2 ? dt : 0)) {
		if (r->leg == MOVE_LEFT) {
			r->finished_ms = sample_at - r->start;
			r->leg = HOLD_LEFT;
		} else {
			r->leg = r->leg == MOVE_CENTER ? MOVE_RIGHT : MOVE_LEFT;
			error = (r->leg == MOVE_RIGHT ? 50 : -50) - mm;
			r->trim_milli_cd = 0;
		}
		r->active_bands = 0;
		r->still_ms = 0;
	}
	/* Positive shaft travel accelerates the ball toward the right. */
	bool holding = r->leg == HOLD_LEFT;
	int pd_cd = holding ? 30 * error - 22 * r->velocity : 35 * error - 18 * r->velocity;
	int base_cd = position_bias_cd(mm) + pd_cd;
	/* A stationary ball more than 3 cm from its target can need extra tilt
	 * to start rolling. Count observed still time and drop the boost on motion. */
	if (holding || abs(error) <= 30 || abs(r->velocity) > 8 || abs(movement) > 2 ||
	    advance > 2 || dt > 85) {
		r->still_ms = 0;
	} else if (advance == 1) {
		r->still_ms += dt;
		if (r->still_ms > 2150)
			r->still_ms = 2150;
	}
	if (r->still_ms > 350) {
		int boost_cd = (int)(r->still_ms - 350) / 2;
		base_cd += error > 0 ? boost_cd : -boost_cd;
	}
	/* At rest a small position error can persist until the ball breaks
	 * free. Limit the stored correction so it cannot launch the ball. */
	int trim_limit = holding ? 250000 : 900000;
	int demand_cd = base_cd + r->trim_milli_cd / 1000;
	bool pushing_limit = (demand_cd <= -1600 && error < 0) || (demand_cd >= 1600 && error > 0);
	if (!pushing_limit && dt < 120 && abs(error) <= 30 &&
	    abs(error) > 3 && abs(r->velocity) <= 25) {
		r->trim_milli_cd += (holding ? 12 : 42) * error * (int)dt;
	}
	if (r->trim_milli_cd < -trim_limit)
		r->trim_milli_cd = -trim_limit;
	if (r->trim_milli_cd > trim_limit)
		r->trim_milli_cd = trim_limit;
	int cd = base_cd + r->trim_milli_cd / 1000;
	r->goal_cd = cd < -1600 ? -1600 : cd > 1600 ? 1600 : cd;
	return NOTICE_NONE;
}
