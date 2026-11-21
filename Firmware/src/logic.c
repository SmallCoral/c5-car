/* 可在主机回归的基础控制逻辑，集中处理时间回绕、按键消抖和设备调试任务的状态切换。 */

#include <limits.h>
#include "config.h"
#include "logic.h"

/* 用无符号减法判断是否经过指定间隔，跨毫秒计数回绕仍有效。 */
bool elapsed(uint32_t now, uint32_t then, uint32_t interval)
{
	return now - then >= interval;
}

/* 记录按键候选状态的变化时间，达到消抖间隔后才报告一次按下。 */
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

/* 将相邻两次编码器读数的模差转换为有符号位移。 */
int32_t encoder_delta(uint32_t current, uint32_t previous)
{
	uint32_t delta = current - previous;

	if (delta <= INT32_MAX)
		return (int32_t)delta;
	/* 显式处理计数回绕，避免依赖无符号转有符号时的编译器行为。 */
	return -1 - (int32_t)(UINT32_MAX - delta);
}

/* 根据调试任务和按键更新速度档、方向及运行开关。 */
void controls_key(struct controls *ctl, enum key_id key, uint32_t now)
{
	/* 运行中任意键先停车，换方向在停车后进行。 */
	if (ctl->running) {
		ctl->running = false;
		return;
	}
	switch (key) {
	case KEY_MODE:
		ctl->task = (ctl->task + 1) % TASK_COUNT;
		break;
	case KEY_NEXT:
		if (ctl->task == TASK_BALANCE)
			break;
		if (ctl->task == TASK_LAP) {
			ctl->lap_level = (ctl->lap_level + 1) % LAP_SPEED_LEVELS;
			break;
		}
		if (ctl->task == TASK_GRAY) {
			ctl->gray_white = !ctl->gray_white;
			break;
		}
		ctl->level = (ctl->level + 1) %
			(ctl->task == TASK_STEPPER ? 3U : WHEEL_LEVEL_COUNT);
		break;
	case KEY_OK:
		if (ctl->task == TASK_LAP)
			ctl->lap_reverse = !ctl->lap_reverse;
		else if (ctl->task != TASK_GRAY && ctl->task != TASK_BALANCE)
			ctl->reverse = !ctl->reverse;
		break;
	case KEY_START:
		if (ctl->task != TASK_MONITOR && ctl->task != TASK_GRAY) {
			ctl->running = true;
			ctl->started_at = now;
		}
		break;
	default:
		break;
	}
}

/* 随时间检查调试任务的故障和运行期限。 */
void controls_tick(struct controls *ctl, uint32_t now, bool fault)
{
	if (fault || (!task_continuous(ctl->task) &&
		      elapsed(now, ctl->started_at, TEST_DURATION_MS)))
		ctl->running = false;
}

/* 将当前调试档位转换成轮电机的 PWM 千分比。 */
int controls_duty(const struct controls *ctl)
{
	static const int duties[WHEEL_LEVEL_COUNT] = {
		100, 200, 300, WHEEL_MAX_PERMILLE
	};
	int duty = duties[ctl->level % WHEEL_LEVEL_COUNT];

	return ctl->reverse ? -duty : duty;
}

/* 取得调试档对应的步进转速，单位为转/分。 */
uint16_t controls_rpm(const struct controls *ctl)
{
	return (uint16_t)((ctl->level % 3U + 1U) * STEP_RPM_STEP);
}

/* 判断所选任务是否需要轮电机。 */
bool task_uses_wheels(enum task_id task)
{
	return task == TASK_LEFT || task == TASK_RIGHT ||
	       task == TASK_BOTH || task == TASK_ALL || task == TASK_LAP;
}

/* 判断所选任务是否需要步进电机。 */
bool task_uses_stepper(enum task_id task)
{
	return task == TASK_STEPPER || task == TASK_ALL || task == TASK_BALANCE;
}

/* 判断任务是否持续运行至人工停止或保护触发。 */
bool task_continuous(enum task_id task)
{
	return task == TASK_STEPPER || task == TASK_ALL || task == TASK_LAP ||
	       task == TASK_BALANCE;
}

/* 把调试任务编号转换为屏幕显示名。 */
const char *task_name(enum task_id task)
{
	static const char *const names[] = {
		"MONITOR", "LEFT", "RIGHT", "BOTH", "STEPPER", "ALL", "GRAY", "LAP", "BALANCE"
	};

	return (unsigned int)task < TASK_COUNT ? names[task] : "INVALID";
}
