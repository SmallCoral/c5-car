/* 把物理按键的点动或长按转换为步进位置请求，保存目标、限位和到位状态。 */

#ifndef CORAL_JOG_H
#define CORAL_JOG_H

#include "stepper.h"

enum jog_action { JOG_NONE, JOG_LEFT, JOG_RIGHT, JOG_MAP, JOG_ZERO };
enum jog_state { JOG_IDLE, JOG_LOCKING, JOG_MOVING, JOG_HELD, JOG_FAILED };

struct jog_keys {
	uint8_t pressed;
	bool consumed, repeating;
	uint32_t since;
};

struct shaft_jog {
	enum jog_state state;
	bool referenced, limited, calibrating;
	int steps, held_steps;
	int32_t origin, target, held_at, lower, upper;
	uint32_t since;
};

/* 区分点动按键的按下、释放和连续长按，给出请求步数。 */
enum jog_action jog_key_update(struct jog_keys *keys, uint8_t down, bool enabled, uint32_t now);
/* 标定页单按中间键产生左右点动，按住 450 ms 后每 120 ms 重复，组合键不触发点动。 */
enum jog_action jog_cal_key(struct jog_keys *keys, uint8_t down, uint32_t now);
/* 判断点动器是否仍在等待抱持或执行定位。 */
bool jog_busy(const struct shaft_jog *jog);
/* 将本次点击叠加到已确认的轴位上并发起点动。 */
bool jog_press(struct shaft_jog *jog, struct stepper *step, int direction, uint32_t now);
/* 等待新鲜的轴位与到位反馈，按阶段完成抱持、定位及收尾。 */
void jog_poll(struct shaft_jog *jog, struct stepper *step, uint32_t now);

#endif
