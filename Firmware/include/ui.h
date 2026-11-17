#ifndef CORAL_UI_H
#define CORAL_UI_H

#include "logic.h"
#include "grayscale.h"
#include "lap.h"

#define UI_WIDTH 128
#define UI_HEIGHT 160
#define UI_BYTES (UI_WIDTH * UI_HEIGHT * 2)

struct ui_state {
	enum task_id task;
	bool running;
	bool reverse;
	unsigned int duty;
	uint32_t remaining_ms;
	int32_t encoder[2];
	int32_t counts_per_second[2];
	uint8_t phases;
	uint8_t gray;
	struct gray_reading grayscale;
	bool gray_white;
	enum lap_status lap_status;
	bool lap_reverse;
	bool lap_armed;
	bool lap_departing;
	bool lap_approach;
	unsigned int lap_speed;
	uint32_t lap_ms;
	uint32_t lap_counts;
	int lap_pwm[2];
	int lap_cps[2];
	uint8_t faults;
	uint8_t wheel_faults;
	bool wheel_requested;
	bool wheel_enable_fault;
	bool step_online;
	const char *step_status;
	uint8_t step_error;
	bool bt_tx;
	uint16_t step_target_rpm;
	int32_t step_rpm;
	bool step_rpm_valid;
	bool updating;
	const char *update_status;
	uint32_t update_bytes;
	uint32_t update_total;
};

void ui_render(uint8_t *pixels, const struct ui_state *state);
void ui_render_rows(uint8_t *pixels, const struct ui_state *state,
		    unsigned int first, unsigned int rows);

#endif
