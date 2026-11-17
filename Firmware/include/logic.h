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

bool elapsed(uint32_t now, uint32_t then, uint32_t interval);
bool key_update(struct debounce *key, bool down, uint32_t now);
int32_t encoder_delta(uint32_t current, uint32_t previous);
void controls_key(struct controls *ctl, enum key_id key, uint32_t now);
void controls_tick(struct controls *ctl, uint32_t now, bool fault);
int controls_duty(const struct controls *ctl);
uint16_t controls_rpm(const struct controls *ctl);
bool task_uses_wheels(enum task_id task);
bool task_uses_stepper(enum task_id task);
bool task_continuous(enum task_id task);
const char *task_name(enum task_id task);

#endif
