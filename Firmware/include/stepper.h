#ifndef CORAL_STEPPER_H
#define CORAL_STEPPER_H

#include "emm.h"

enum step_phase {
	STEP_BOOT, STEP_IDLE, STEP_ENABLE, STEP_SPEED, STEP_MOVING,
	STEP_STOP, STEP_DISABLE, STEP_OFFLINE, STEP_POSITION, STEP_BRAKE, STEP_PARKED,
	STEP_LATCH, STEP_BAL_SPEED
};
enum step_direction_probe { DIR_NONE, DIR_ALIGN, DIR_SETTLE, DIR_MEASURE };

/* Physical shaft limits relative to the single-turn horizontal datum, 2026-11-19. */
enum { SHAFT_LEVEL_ABS = 10260, SHAFT_HIGH_OFFSET = -12780, SHAFT_LOW_OFFSET = 19394 };

struct stepper {
	struct emm_parser parser;
	enum step_phase phase;
	uint8_t pending;
	uint8_t flags;
	uint8_t error;
	bool reverse;
	bool seen;
	uint32_t sent_at;
	uint32_t last_reply;
	uint32_t last_poll;
	uint32_t rx_errors;
	uint8_t last_bad_phase, last_bad_command;
	uint32_t replies;
	uint32_t timeouts;
	int32_t position;
	int32_t shaft_level;
	bool travel_valid;
	bool position_valid;
	uint8_t poll_index;
	bool cleanup_only;
	bool boot_parked;
	uint16_t target_rpm, sent_rpm;
	int32_t rpm;
	bool rpm_valid;
	bool hold, goal_sent, retried, speed_due, sent_reverse;
	int8_t cw_sign;
	bool probing_ccw;
	enum step_direction_probe direction_probe;
	int32_t direction_origin;
	uint32_t direction_at, direction_started;
	uint32_t pulses_per_rev, position_at, config_at, flags_at, goal_at;
	int32_t goal, sent_goal, lower, upper;
};

void stepper_init(struct stepper *step);
void stepper_resume_hold(struct stepper *step, uint32_t now);
void stepper_poll(struct stepper *step, uint32_t now);
bool stepper_start(struct stepper *step, bool reverse, uint16_t rpm, uint32_t now);
void stepper_stop(struct stepper *step, uint32_t now);
bool stepper_online(const struct stepper *step, uint32_t now);
bool stepper_running(const struct stepper *step);
void stepper_request_config(struct stepper *step, uint32_t now);
bool stepper_latch(struct stepper *step, uint32_t now);
bool stepper_measure_direction(struct stepper *step, int32_t origin, bool ccw, uint32_t now);
bool stepper_hold(struct stepper *step, int32_t lower, int32_t upper, uint32_t now);
bool stepper_speed_loop(struct stepper *step, int32_t lower, int32_t upper, uint32_t now);
void stepper_balance_rpm(struct stepper *step, int rpm);
void stepper_brake(struct stepper *step, uint32_t now);
const char *stepper_name(const struct stepper *step, uint32_t now);

#endif
