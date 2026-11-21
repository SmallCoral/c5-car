#include <stdlib.h>
#include "contest.h"

static const enum task_id tools[] = {
	TASK_BALANCE, TASK_LAP, TASK_GRAY, TASK_MONITOR, TASK_LEFT, TASK_RIGHT, TASK_BOTH, TASK_STEPPER, TASK_ALL
};

enum task_id contest_tool(unsigned int index)
{
	return tools[index % (sizeof(tools) / sizeof(tools[0]))];
}

static void select_target(struct contest_menu *m, enum key_id key)
{
	int next = m->target_mm + (key == KEY_OK ? -1 : 1);
	if (next >= contest_ball_mm(RULER_X_LEFT) && next <= contest_ball_mm(RULER_X_RIGHT))
		m->target_mm = next;
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
		else if (m->item == EVENT_LAP && key == KEY_NEXT)
			return MENU_LAP_SPEED;
		else if (m->item == EVENT_OFFSET)
			select_target(m, key);
		break;
	case MENU_RUN:
		if (m->item == EVENT_OFFSET) {
			if (key == KEY_OK || key == KEY_NEXT) {
				if (!m->offset_driving)
					select_target(m, key);
				return MENU_NONE;
			}
			if (!m->offset_driving && key == KEY_MODE)
				return MENU_START_LAP;
		}
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

enum contest_notice contest_lap_notice(enum lap_status status, uint32_t elapsed_ms, uint32_t limit_ms)
{
	switch (status) {
	case LAP_RUNNING:
		return elapsed_ms >= limit_ms ? NOTICE_TIMEOUT : NOTICE_NONE;
	case LAP_DONE:
		return elapsed_ms <= limit_ms ? NOTICE_FINISHED : NOTICE_TIMEOUT;
	case LAP_TIMEOUT:
		return NOTICE_TIMEOUT;
	case LAP_CANCELLED:
		return NOTICE_STOPPED;
	case LAP_DRIVE:
	case LAP_ENCODER:
		return NOTICE_DRIVE;
	default:
		return NOTICE_LINE;
	}
}

/* 图像 x 向右侧挡杆减小。以 O 为零，按两侧各自的 50 mm 标尺分段换算。 */
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

void position_begin_target(struct position_run *run, int target_mm, int x, uint16_t seq, uint32_t now)
{
	position_begin(run, x, seq, now);
	run->fixed_target = true;
	run->target_mm = target_mm;
}

void position_retarget(struct position_run *r, int target_mm)
{
	if (r->target_mm == target_mm)
		return;
	r->target_mm = target_mm;
	if (r->leg != LEVEL_SHAFT)
		r->leg = MOVE_TARGET;
	r->active_bands = 0;
	r->still_ms = 0;
	r->trim_milli_cd = 0;
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
		r->leg = r->fixed_target ? MOVE_TARGET : MOVE_CENTER;
		r->steady = false;
	}
}

static bool position_settled(struct position_run *r, int error, uint32_t at, unsigned int unseen_ms)
{
	_Static_assert(POSITION_BANDS <= 32, "Dwell bands must fit in the active mask");
	unsigned int dwell = r->leg == MOVE_CENTER ? POSITION_CENTER_DWELL_MS :
			     r->leg == MOVE_RIGHT ? POSITION_RIGHT_DWELL_MS : POSITION_LEFT_DWELL_MS;
	bool slow = abs(r->velocity) <= 12;
	/* 停稳需要球位一直落在某个 4 mm 窄带内。各窄带分别计时，离开一个窄带时，相邻窄带内已经积累的时间仍有效。 */
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

/* 钢球静止时所需轴角随位置变化。左 50 mm 处实测约 −8°～−11°，O 点约 −3°～−5°，右 50 mm 处接近 0°。先按保守值给定，再由积分微调。 */
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
	int target = r->fixed_target ? r->target_mm :
		     r->leg == MOVE_CENTER ? 0 : r->leg == MOVE_RIGHT ? 50 : -50;
	bool quiet = (r->leg == MOVE_LEFT || r->leg == HOLD_LEFT || r->fixed_target) &&
		r->have_sample && abs(r->mm - target) <= POSITION_TOLERANCE_MM && abs(r->velocity) <= 12;
	if (!preparing && gap >= 120) {
		/* 球已低速靠近目标时，漏帧期间保留上次抱持角度。此时回到几何水平可能让球重新滚动。 */
		if (!quiet) {
			r->goal_cd = 0;
			r->trim_milli_cd = 0;
		}
		r->active_bands = 0;
		r->still_ms = 0;
	}
	if (!preparing && (r->waiting_vision || gap >= (quiet && r->leg == HOLD_LEFT ? 900U : 500U))) {
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
	/* 只允许在相邻两次直接观测位置接近时跨过一帧漏检。无观测的整段时间从停稳计时中扣除。 */
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
	int error = target - mm;
	if (r->leg == HOLD_TARGET && abs(error) > POSITION_TOLERANCE_MM)
		r->leg = MOVE_TARGET;
	if (r->leg != HOLD_LEFT && r->leg != HOLD_TARGET &&
	    position_settled(r, error, sample_at, advance == 2 ? dt : 0)) {
		if (r->leg == MOVE_TARGET) {
			r->leg = HOLD_TARGET;
		} else if (r->leg == MOVE_LEFT) {
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
	/* 实车轴角增大时球向右加速。位置误差给出纠偏量，球速项用于提前制动。角度单位为 0.01°。 */
	bool holding = r->leg == HOLD_LEFT || r->leg == HOLD_TARGET;
	int pd_cd = holding ? 30 * error - 22 * r->velocity : 35 * error - 18 * r->velocity;
	/* 车辆行驶时加大位置与速度修正，在球越过目标前开始制动。 */
	if (r->driving)
		pd_cd += 10 * error - 2 * r->velocity;
	int base_cd = position_bias_cd(mm) + pd_cd;
	/* 球在目标 3 cm 外静止时，按直接观测的持续时间逐渐增加倾角。球开始滚动便清除这项补偿。 */
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
	/* 球与管壁的静摩擦会留下小偏差，低速时用积分补偿。限制累积角度，避免脱离静摩擦后突然加速。 */
	/* 车在行驶时进一步收紧积分限幅，避免上一路段积累的倾角带入下一段。 */
	int trim_limit = r->driving ? 200000 : holding ? 250000 : 900000;
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
