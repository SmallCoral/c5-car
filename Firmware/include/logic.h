/* 设备调试任务、按键枚举以及可独立测试的时间、消抖和档位逻辑。 */

#ifndef CORAL_LOGIC_H
#define CORAL_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

enum task_id {
	TASK_MONITOR,
	TASK_LEFT,
	TASK_RIGHT,
	TASK_BOTH,
	TASK_STEPPER,
	TASK_ALL,
	TASK_GRAY,
	TASK_LAP,
	TASK_BALANCE,
	TASK_COUNT
};

enum key_id { KEY_MODE, KEY_NEXT, KEY_OK, KEY_START, KEY_COUNT };

struct debounce {
	uint32_t changed_at;
	bool candidate;
	bool stable;
};

struct controls {
	enum task_id task;
	unsigned int level;
	bool reverse;
	bool gray_white;
	unsigned int lap_level;
	bool lap_reverse;
	bool running;
	uint32_t started_at;
};

/* 用无符号减法判断是否经过指定间隔，跨毫秒计数回绕仍有效。 */
bool elapsed(uint32_t now, uint32_t then, uint32_t interval);
/* 记录按键候选状态的变化时间，达到消抖间隔后才报告一次按下。 */
bool key_update(struct debounce *key, bool down, uint32_t now);
/* 将相邻两次编码器读数的模差转换为有符号位移。 */
int32_t encoder_delta(uint32_t current, uint32_t previous);
/* 根据调试任务和按键更新速度档、方向及运行开关。 */
void controls_key(struct controls *ctl, enum key_id key, uint32_t now);
/* 随时间检查调试任务的故障和运行期限。 */
void controls_tick(struct controls *ctl, uint32_t now, bool fault);
/* 将当前调试档位转换成轮电机的 PWM 千分比。 */
int controls_duty(const struct controls *ctl);
/* 取得调试档对应的步进转速，单位为转/分。 */
uint16_t controls_rpm(const struct controls *ctl);
/* 判断所选任务是否需要轮电机。 */
bool task_uses_wheels(enum task_id task);
/* 判断所选任务是否需要步进电机。 */
bool task_uses_stepper(enum task_id task);
/* 判断任务是否持续运行至人工停止或保护触发。 */
bool task_continuous(enum task_id task);
/* 把调试任务编号转换为屏幕显示名。 */
const char *task_name(enum task_id task);

#endif
