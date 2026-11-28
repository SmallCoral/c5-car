/* 比赛菜单和钢球位置闭环。第一题按 O、右 50 mm、左 50 mm 顺序判稳；发挥一复用这套观测与轴角计算，目标改为按键选定的毫米位置。 */

#include <stdlib.h>
#include "contest.h"
#include "config.h"

static const enum task_id tools[] = {
	TASK_BALANCE, TASK_LAP, TASK_GRAY, TASK_MONITOR, TASK_LEFT, TASK_RIGHT, TASK_BOTH, TASK_STEPPER, TASK_ALL
};

/* 第二题按高速停车标定，载球整圈按低速停车标定；直线从 A 到 B。 */
uint32_t contest_distance(enum contest_item item)
{
	if (item == EVENT_STRAIGHT)
		return LAP_STRAIGHT_DISTANCE_COUNTS;
	return contest_carries_ball(item) ? LAP_OFFSET_DISTANCE_COUNTS : LAP_DISTANCE_COUNTS;
}

uint32_t contest_time_limit(enum contest_item item)
{
	switch (item) {
	case EVENT_LAP: return CONTEST_LAP_LIMIT_MS;
	case EVENT_STRAIGHT: return 12000U;
	case EVENT_FULL: return 40000U;
	default: return LAP_TIMEOUT_MS;
	}
}

/* 从调试菜单索引取得对应设备任务。 */
enum task_id contest_tool(unsigned int index)
{
	return tools[index % (sizeof(tools) / sizeof(tools[0]))];
}

/* 按 SW4 或 SW3 将毫米目标向左或向右移动一格，并检查两端标定范围。 */
static void select_target(struct contest_menu *m, enum key_id key)
{
	int next = m->target_mm + (key == KEY_OK ? -1 : 1);
	if (next >= contest_ball_mm(RULER_X_LEFT) && next <= contest_ball_mm(RULER_X_RIGHT))
		m->target_mm = next;
}

/* 按当前页面解释同一个按键，返回交给主循环执行的动作。发挥一起跑前允许改点，行驶时锁定目标。 */
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
		if (contest_carries_ball(m->item)) {
			if (key == KEY_OK || key == KEY_NEXT) {
				if (!m->offset_driving && m->item == EVENT_OFFSET)
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

/* 把循线结束原因和圈时换成比赛结果页提示。 */
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

/* 相机 x 越小，钢球越靠右。以 O 为零，两侧分别按实测的 50 mm 标尺换算，球位向右为正。 */
int contest_ball_mm(int raw_x)
{
	int dx = RULER_X_O - raw_x;
	int span = dx >= 0 ? RULER_X_O - RULER_X_PLUS_5 : RULER_X_MINUS_5 - RULER_X_O;
	return (dx * 50 + (dx >= 0 ? span / 2 : -span / 2)) / span;
}

/* 新一轮从水平确认开始，同时保存起始时间及首个相机序号。 */
void position_begin(struct position_run *run, int x, uint16_t seq, uint32_t now)
{
	*run = (struct position_run){ .leg = LEVEL_SHAFT, .start = now, .sample_at = now,
				      .mm = x >= 0 ? contest_ball_mm(x) : 0, .seq = seq,
				      .have_sample = x >= 0 };
}

/* 复用第一题的位置控制器，将三段目标切换为操作者选择的固定点。 */
void position_begin_target(struct position_run *run, int target_mm, int x, uint16_t seq, uint32_t now)
{
	position_begin(run, x, seq, now);
	run->fixed_target = true;
	run->target_mm = target_mm;
}

/* 改点、失测或起跑时撤销原地助跑，下一帧重新按操作者选定的位置闭环。 */
void position_cancel_runup(struct position_run *r)
{
	if (r->runup) {
		r->runup = false;
		r->crossing_fast = false;
		r->kick_direction = 0;
		r->goal_cd = 0;
		r->trim_milli_cd = 0;
		r->active_bands = 0;
		r->still_ms = 0;
	}
	r->blocked_ms = 0;
}

static int direction_to(int target, int mm)
{
	return (target > mm) - (target < mm);
}

enum position_leg position_control_leg(const struct position_run *r)
{
	if (r->runup)
		return MOVE_RIGHT;
	if (r->leg == HOLD_TARGET)
		return HOLD_LEFT;
	if (r->leg != MOVE_TARGET)
		return r->leg;
	return r->approach_sign > 0 ? MOVE_RIGHT : r->approach_sign < 0 ? MOVE_LEFT : MOVE_CENTER;
}

/* 从旧标记右侧向右换点时，沿用已抵消右漂的补偿。右端的平衡角比旧点偏正。 */
static void transfer_trim(struct position_run *r, int old_target, int next_target)
{
	if (next_target > old_target && r->mm > old_target && r->trim_milli_cd < -250000)
		r->trim_milli_cd = r->trim_milli_cd * 3 / 4;
	else
		r->trim_milli_cd = 0;
}

/* 同一次长按可能逐毫米改点，连续向同一方向延伸时只做一次积分交接。 */
void position_retarget(struct position_run *r, int target_mm)
{
	if (r->target_mm == target_mm)
		return;
	int direction = direction_to(target_mm, r->mm);
	bool extending = r->leg == MOVE_TARGET && r->have_sample && !r->waiting_vision &&
		direction && direction == r->approach_sign && direction == direction_to(target_mm, r->target_mm);
	if (!r->have_sample || r->waiting_vision)
		r->trim_milli_cd = 0;
	else if (!extending)
		transfer_trim(r, r->target_mm, target_mm);
	position_cancel_runup(r);
	r->runup_used = false;
	if (direction != r->approach_sign) {
		r->crossing_fast = false;
		r->kick_direction = 0;
	}
	r->target_mm = target_mm;
	r->approach_sign = direction;
	if (r->leg != LEVEL_SHAFT && r->leg != FIND_START_BALL)
		r->leg = MOVE_TARGET;
	r->active_bands = 0;
	r->still_ms = 0;
}

/* 按编码器反馈时间确认轴在水平位置连续稳定，再进入第一段移球。 */
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

/* 在目标附近维护多组重叠窄带，只累计有观测的低速停留时间。 */
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

/* 钢球静止所需的轴角随位置变化。左 50 mm 处实测约 −8°～−11°，O 点约 −3°～−5°，右 50 mm 处接近 0°。以保守值给定基础轴角，再由积分微调。 */
static int position_bias_cd(int mm)
{
	if (mm <= -50)
		return -800;
	if (mm >= 50)
		return 0;
	return -300 + mm * (mm < 0 ? 10 : 6);
}

/* 长时间失测后，核对两帧新实测的序号、间隔与位移以恢复跟踪。途中允许球继续滚动，已完成的左点保持只在左标记附近重新接管。 */
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

/* 右挡杆处球心有像素抖动，向管内滚出约 4 mm 才允许交给稳球闭环。 */
enum { RIGHT_STOP_INNER_X = RULER_X_RIGHT + 10 };

void position_start_lift(struct position_run *r, uint32_t now)
{
	r->leg = FIND_START_BALL;
	r->goal_cd = -POSITION_START_LIFT_CD;
	r->lift_confirmed = false;
	r->steady_at = now;
	r->have_sample = false;
	r->waiting_vision = true;
	r->reacquire_seen = false;
}

/* 实测从 O 静止向左慢抬至 −46° 仍卡在 O 附近，从右 50 mm 一次向左移则能通过。
 * 只在原地连续观测到这一工况时借用第一题的右点，每个选点最多尝试一次。 */
static void check_left_runup(struct position_run *r, uint16_t advance, uint32_t dt, uint32_t now)
{
	if (r->runup_used || r->leg != MOVE_TARGET || abs(r->mm) > 12 ||
	    r->target_mm > r->mm - 12 || abs(r->velocity) > 12 || advance != 1 || dt > 85 ||
	    (r->blocked_ms && abs(r->mm - r->blocked_mm) > 2)) {
		r->blocked_ms = 0;
		return;
	}
	if (!r->blocked_ms)
		r->blocked_mm = r->mm;
	r->blocked_ms += dt;
	if (r->blocked_ms >= 600) {
		r->runup = r->runup_used = true;
		r->runup_at = now;
		r->trim_milli_cd = 0;
		r->active_bands = 0;
		r->still_ms = 0;
	}
}

int position_target_mm(const struct position_run *r)
{
	if (r->fixed_target)
		return r->target_mm;
	if (r->leg == MOVE_RIGHT)
		return 50;
	if (r->leg == MOVE_LEFT || r->leg == HOLD_LEFT)
		return -50;
	return 0;
}

/* 球低速过 O 时直接给定轴角。驶离中心、已有足够球速或接近标记就交还速度制动。 */
static void cross_center(struct position_run *r, enum position_leg leg, int target, uint32_t now)
{
	int dir = leg == MOVE_RIGHT ? 1 : leg == MOVE_LEFT ? -1 : 0;
	int remain = dir * (target - r->mm);
	bool active = r->crossing_fast;
	r->crossing_fast = false;
	if (!dir || dir * target < 15 || abs(r->mm) > 14 || remain < 20) {
		r->kick_direction = 0;
		return;
	}
	if (dir != r->kick_direction && dir * r->mm <= 2 && abs(r->velocity) <= 24) {
		r->kick_direction = (int8_t)dir;
		r->kick_at = now;
		active = true;
	}
	if (active && r->kick_direction == dir && now - r->kick_at < 300 && abs(r->velocity) < 40) {
		r->kick_return_cd = r->goal_cd;
		r->goal_cd = dir > 0 ? 1700 : -2200;
		r->crossing_fast = true;
	}
}

/* 每次新实测先估球速，再判定停稳及换段，最后将位置、速度和积分补偿合成为限幅的目标轴角。 */
enum contest_notice position_update(struct position_run *r, enum ball_status view, int x, uint16_t seq,
				    uint32_t sample_at, uint32_t now)
{
	if (r->crossing_fast && now - r->kick_at >= 300) {
		r->crossing_fast = false;
		r->goal_cd = r->kick_return_cd;
	}
	if (r->leg == FIND_START_BALL) {
		bool fresh = view == BALL_TRACKED && now - sample_at < 120;
		if (fresh && x > RULER_X_LEFT)
			return NOTICE_VISION;
		/* 起步先执行固定轴位。相机尚未发帧时，从编码器到位起留一秒等首帧。 */
		if (!r->lift_confirmed)
			return now - r->steady_at >= 2400 ? NOTICE_DRIVE : NOTICE_NONE;
		if (view == BALL_OFFLINE && now - r->steady_at >= 1000)
			return NOTICE_VISION;
		if (!fresh || now - sample_at > now - r->steady_at || x <= RIGHT_STOP_INNER_X)
			view = BALL_MISSING;
		if (!reacquire_position(r, view, x, seq, sample_at))
			return NOTICE_NONE;
		if (r->fixed_target)
			r->approach_sign = r->target_mm ? direction_to(r->target_mm, contest_ball_mm(x)) : 0;
		r->leg = r->fixed_target ? MOVE_TARGET : MOVE_CENTER;
		r->waiting_vision = false;
		r->goal_cd = 0;
	}
	uint32_t gap = now - r->sample_at;
	bool left_staging = r->offset_staging && r->target_mm < 0 && !r->driving;
	if (view != BALL_TRACKED)
		r->blocked_ms = 0;
	if ((r->runup || r->blocked_ms) && (!left_staging ||
	    gap >= 120 || (r->runup && now - r->runup_at >= 5000)))
		position_cancel_runup(r);
	bool preparing = r->leg == LEVEL_SHAFT;
	int target = position_target_mm(r);
	bool quiet = (r->leg == MOVE_LEFT || r->leg == HOLD_LEFT || r->fixed_target) &&
		r->have_sample && abs(r->mm - target) <= POSITION_TOLERANCE_MM && abs(r->velocity) <= 12;
	if (!preparing && gap >= 120) {
		r->crossing_fast = false;
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
	/* 预测包和同一序号的重复包都不能提供新的位移或延长停稳时间。 */
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
		int change = (v - r->velocity) * (int)dt / (100 + (int)dt);
		if (left_staging && !movement && !change)
			r->velocity = 0;
		else
			r->velocity += change;
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
	if (left_staging) {
		check_left_runup(r, advance, dt, now);
		if (r->runup)
			target = 50;
	}
	int error = target - mm;
	if (r->leg == HOLD_TARGET && abs(error) > POSITION_TOLERANCE_MM) {
		r->approach_sign = direction_to(target, mm);
		r->leg = MOVE_TARGET;
	}
	if (r->leg != HOLD_LEFT && r->leg != HOLD_TARGET &&
	    position_settled(r, error, sample_at, advance == 2 ? dt : 0)) {
		if (r->runup) {
			position_cancel_runup(r);
			r->approach_sign = -1;
			target = r->target_mm;
			error = target - mm;
		} else if (r->leg == MOVE_TARGET) {
			r->leg = HOLD_TARGET;
		} else if (r->leg == MOVE_LEFT) {
			/* 以最后一段满足条件的相机采样时刻锁存成绩，随后仍执行左点闭环。 */
			r->finished_ms = sample_at - r->start;
			r->leg = HOLD_LEFT;
		} else {
			int old_target = target;
			r->leg = r->leg == MOVE_CENTER ? MOVE_RIGHT : MOVE_LEFT;
			target = r->leg == MOVE_RIGHT ? 50 : -50;
			error = target - mm;
			transfer_trim(r, old_target, target);
		}
		r->active_bands = 0;
		r->still_ms = 0;
	}
	/* 实车轴角增大时球向右加速。位置误差给出纠偏量，球速项用于提前制动。角度单位为 0.01°。 */
	enum position_leg control_leg = position_control_leg(r);
	bool holding = control_leg == HOLD_LEFT;
	int pd_cd = holding ? 30 * error - 22 * r->velocity : 35 * error - 18 * r->velocity;
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
	/* 左点实测一切换保持就把已经积累的负补偿截到 −2.5°，球从 −52 滑回
	 * −38 mm。保留到点时的负补偿，低速偏向 O 时也允许继续修正。 */
	bool left_hold = holding && (r->trim_milli_cd < -250000 ||
				     (r->trim_milli_cd == -250000 && error < -3));
	int trim_limit = holding && !left_hold ? 250000 : 900000;
	int demand_cd = base_cd + r->trim_milli_cd / 1000;
	bool pushing_limit = (demand_cd <= -1600 && error < 0) || (demand_cd >= 1600 && error > 0);
	if (!pushing_limit && dt < 120 && abs(error) <= 30 &&
	    abs(error) > (left_hold ? 1 : 3) && abs(r->velocity) <= (left_hold ? 12 : 25)) {
		r->trim_milli_cd += (left_hold ? 24 : holding ? 12 : 42) * error * (int)dt;
	}
	if (r->trim_milli_cd < -trim_limit)
		r->trim_milli_cd = -trim_limit;
	if (r->trim_milli_cd > trim_limit)
		r->trim_milli_cd = trim_limit;
	int cd = base_cd + r->trim_milli_cd / 1000;
	/* 向左经过 O 时过早放平会停在 −30 mm。只有剩余距离足以覆盖
	 * 180 ms 响应和 250 mm/s² 制动时，才维持向左滚动的倾角。 */
	if (control_leg == MOVE_LEFT && error >= -50 && error <= -25 &&
	    r->velocity <= -20 && r->velocity >= -100) {
		int speed = -r->velocity;
		if (-error * 500 > 90 * speed + speed * speed) {
			int cruise_cd = position_bias_cd(mm) - 350;
			if (cd > cruise_cd)
				cd = cruise_cd;
		}
	}
	/* 高速进入右标记时只放宽反向制动。到点实测仍有 94 mm/s。 */
	int min_cd = -1600;
	if (control_leg == MOVE_RIGHT && error <= 20 && r->velocity > 60) {
		int extra = (r->velocity - 60) * 20;
		min_cd -= extra < 1000 ? extra : 1000;
	}
	r->goal_cd = cd < min_cd ? min_cd : cd > 1600 ? 1600 : cd;
	cross_center(r, control_leg, target, now);
	return NOTICE_NONE;
}
