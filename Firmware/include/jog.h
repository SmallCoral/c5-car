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

enum jog_action jog_key_update(struct jog_keys *keys, uint8_t down, bool enabled, uint32_t now);
enum jog_action jog_cal_key(struct jog_keys *keys, uint8_t down, uint32_t now);
bool jog_busy(const struct shaft_jog *jog);
bool jog_press(struct shaft_jog *jog, struct stepper *step, int direction, uint32_t now);
void jog_poll(struct shaft_jog *jog, struct stepper *step, uint32_t now);

#endif
