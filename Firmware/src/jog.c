/* 步进点动和标定。按键产生离散步数，编码器提供轴位，组合成受上下限约束的绝对目标，并在反馈到位后保持。 */

#include <limits.h>
#include <stdlib.h>
#include "balance.h"
#include "jog.h"
#include "logic.h"

enum { JOG_PER_TURN = 200, JOG_LONG_MS = 700 };

/* 区分点动按键的按下、释放和连续长按，给出请求步数。 */
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

/* 判断点动器是否仍在等待抱持或执行定位。 */
bool jog_busy(const struct shaft_jog *jog)
{
	return jog->state == JOG_LOCKING || jog->state == JOG_MOVING;
}

/* 标定页单按中间键产生左右点动，按住 450 ms 后每 120 ms 重复，组合键不触发点动。 */
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

/* 将轴位换算时取最接近的整数格点。 */
static int64_t nearest(int64_t value, int32_t divisor)
{
	return (value + (value < 0 ? -divisor / 2 : divisor / 2)) / divisor;
}

/* 从实测微步位置计算下一个绝对目标，每步为 1/200 圈，并检查机械边界。重试时沿用同一个 FD 目标。 */
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

/* 将本次点击叠加到已确认的轴位上并发起点动。 */
bool jog_press(struct shaft_jog *jog, struct stepper *step, int direction, uint32_t now)
{
	bool retry = jog->calibrating && (step->error == 0xe0 || step->error == 0xe3);
	if ((direction != -1 && direction != 1) || !step->pulses_per_rev ||
	    step->pulses_per_rev % JOG_PER_TURN || !stepper_online(step, now) ||
	    (step->flags & 0x0cU) || (step->error && !retry))
		return false;
	if (jog->calibrating) {
		if (jog_busy(jog))
			return false;
		jog->referenced = false;
	}
	if (!jog_busy(jog)) {
		if (step->phase != STEP_PARKED || !step->hold)
			jog->referenced = false;
		if (!jog->referenced)
			jog->steps = 0;
		jog->held_at = jog->target;
		jog->held_steps = jog->steps;
	}
	if ((direction > 0 && jog->steps == INT_MAX) || (direction < 0 && jog->steps == INT_MIN)) {
		jog->limited = true;
		return false;
	}
	int next = jog->steps + direction;
	jog->limited = jog->referenced && !set_target(jog, step->pulses_per_rev, next);
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

/* 标记点动失败并停止继续发出定位请求。 */
static void jog_fault(struct shaft_jog *jog, struct stepper *step, uint32_t now)
{
	jog->state = JOG_FAILED;
	jog->referenced = false;
	if (stepper_running(step) || step->phase == STEP_LATCH)
		stepper_brake(step, now);
}

/* 等待新鲜的轴位与到位反馈，按阶段完成抱持、定位及收尾。 */
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
		if (jog->calibrating) {
			jog->origin = step->position;
			jog->lower = INT32_MIN;
			jog->upper = INT32_MAX;
			if (!set_target(jog, step->pulses_per_rev, jog->steps) ||
			    !stepper_cal_jog(step, jog->target, now)) {
				jog_fault(jog, step, now);
				return;
			}
			jog->lower = step->lower;
			jog->upper = step->upper;
			jog->referenced = true;
			jog->state = JOG_MOVING;
			jog->since = now;
			return;
		}
		if (jog->referenced && llabs((int64_t)step->position - jog->held_at) > 40) {
			/* 两次按键之间机构已被移动，在最新轴位上仅叠加本次点击量，继续执行行程限位。 */
			jog->steps -= jog->held_steps;
			jog->referenced = false;
		}
		if (!jog->referenced) {
			if (!step->travel_valid) {
				jog_fault(jog, step, now);
				return;
			}
			jog->lower = INT32_MIN;
			jog->upper = INT32_MAX;
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
	enum step_phase expected = jog->calibrating ? STEP_CAL_JOG : STEP_POSITION;
	if (step->phase != expected || elapsed(now, step->position_at, 240) ||
	    (int64_t)step->position < (int64_t)jog->lower - 32 ||
	    (int64_t)step->position > (int64_t)jog->upper + 32) {
		jog_fault(jog, step, now);
		return;
	}
	/* 等待本次 FD 的新位置应答和到位标志。转入驻车轮询后让驱动继续使能。 */
	if (step->goal_sent && step->sent_goal == jog->target && !step->pending &&
	    now - step->position_at < now - step->goal_at && now - step->flags_at < now - step->goal_at &&
	    (step->flags & 3U) == 3U && llabs((int64_t)step->position - jog->target) <= 24) {
		step->phase = STEP_PARKED;
		jog->state = JOG_HELD;
	}
}
