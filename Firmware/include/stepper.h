#ifndef CORAL_STEPPER_H
#define CORAL_STEPPER_H

#include "emm.h"

enum step_phase {
	STEP_BOOT, STEP_IDLE, STEP_ENABLE, STEP_SPEED, STEP_MOVING,
	STEP_STOP, STEP_DISABLE, STEP_OFFLINE
};

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
	uint32_t replies;
	uint32_t timeouts;
	int32_t position;
	bool position_valid;
	uint8_t poll_index;
	bool cleanup_only;
	uint16_t target_rpm;
	int32_t rpm;
	bool rpm_valid;
};

void stepper_init(struct stepper *step);
void stepper_poll(struct stepper *step, uint32_t now);
bool stepper_start(struct stepper *step, bool reverse, uint16_t rpm, uint32_t now);
void stepper_stop(struct stepper *step, uint32_t now);
bool stepper_online(const struct stepper *step, uint32_t now);
bool stepper_running(const struct stepper *step);
const char *stepper_name(const struct stepper *step, uint32_t now);

#endif
