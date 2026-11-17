#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

int main(int argc, char **argv)
{
	static uint8_t buffer[UI_BYTES + 32];
	static uint8_t sliced[UI_BYTES];
	struct ui_state state = {
		.task = TASK_ALL, .running = true, .duty = 450,
		.remaining_ms = 2400, .encoder = { 1284, -1279 },
		.counts_per_second = { 456, -452 }, .gray = 0xe7,
		.phases = 9, .wheel_requested = true, .step_online = true,
		.step_status = "MOVING", .bt_tx = true,
		.step_target_rpm = 30, .step_rpm = 30, .step_rpm_valid = true
	};
	FILE *output;
	unsigned int index;

	assert(argc == 2 || argc == 3);
	if (argc == 3) {
		if (!strcmp(argv[2], "lap")) {
			state.task = TASK_LAP;
			state.lap_status = LAP_RUNNING;
			state.lap_speed = 1800;
			state.lap_ms = 12500;
			state.lap_armed = true;
			state.lap_pwm[0] = 120;
			state.lap_pwm[1] = 160;
			state.lap_cps[0] = 800;
			state.lap_cps[1] = 790;
			state.grayscale = (struct gray_reading) {
				.raw = 0xe7, .black = 0x18, .line = 0x18,
				.count = 2, .groups = 1, .status = GRAY_LINE,
			};
		} else if (!strcmp(argv[2], "gray")) {
			state.task = TASK_GRAY;
			state.running = false;
			state.grayscale = (struct gray_reading) {
				.raw = 0xe7, .black = 0x18, .line = 0x18,
				.count = 2, .groups = 1, .status = GRAY_LINE,
			};
		} else {
			state.updating = true;
			state.update_status = "RECEIVING";
			state.update_bytes = 450;
			state.update_total = 1000;
		}
	}
	memset(buffer, 0xa5, sizeof(buffer));
	ui_render(buffer + 16, &state);
	output = fopen(argv[1], "wb");
	assert(output);
	fprintf(output, "P6\n%d %d\n255\n", UI_WIDTH, UI_HEIGHT);
	for (index = 16; index < UI_BYTES + 16; index += 2) {
		unsigned int color = buffer[index] * 256U + buffer[index + 1];
		uint8_t rgb[] = {
			(uint8_t)(((color >> 11) & 31) * 255 / 31),
			(uint8_t)(((color >> 5) & 63) * 255 / 63),
			(uint8_t)((color & 31) * 255 / 31)
		};

		assert(fwrite(rgb, 1, 3, output) == 3);
	}
	assert(fclose(output) == 0);
	memset(sliced, 0xa5, sizeof(sliced));
	ui_render_rows(sliced, &state, 8, 8);
	for (index = 0; index < UI_BYTES; index++) {
		bool inside = index >= 8U * UI_WIDTH * 2 && index < 16U * UI_WIDTH * 2;

		assert(sliced[index] == (inside ? buffer[16 + index] : 0xa5));
	}
	for (unsigned int row = 0; row < UI_HEIGHT; row += 8)
		ui_render_rows(sliced, &state, row, 8);
	assert(!memcmp(sliced, buffer + 16, UI_BYTES));
	ui_render_rows(sliced, &state, UI_HEIGHT, UINT_MAX);
	ui_render_rows(sliced, &state, 152, UINT_MAX);
	assert(!memcmp(sliced, buffer + 16, UI_BYTES));
	state.running = false;
	state.wheel_requested = false;
	ui_render(buffer + 16, &state);
	state.task = TASK_STEPPER;
	state.encoder[0] = INT32_MIN;
	state.encoder[1] = INT32_MAX;
	state.counts_per_second[1] = INT32_MIN;
	state.faults = 3;
	state.step_status = "STOPPING";
	ui_render(buffer + 16, &state);
	for (index = 0; index < 16; index++) {
		assert(buffer[index] == 0xa5);
		assert(buffer[UI_BYTES + 16 + index] == 0xa5);
	}
	state.updating = true;
	state.update_status = "READY TO INSTALL";
	state.update_bytes = UINT32_MAX;
	state.update_total = 1;
	ui_render(buffer + 16, &state);
	state.updating = false;
	state.task = TASK_GRAY;
	for (unsigned int status = GRAY_WARMUP; status <= GRAY_SPLIT; status++) {
		state.grayscale.status = (enum gray_status)status;
		state.gray_white = !!(status & 1U);
		state.grayscale.position = status & 1U ? -3500 : 3500;
		ui_render(buffer + 16, &state);
	}
	state.task = TASK_LAP;
	state.lap_pwm[0] = 350;
	state.lap_cps[0] = -500;
	for (unsigned int status = LAP_IDLE; status <= LAP_NO_MARKER; status++) {
		state.lap_status = (enum lap_status)status;
		state.lap_reverse = !!(status & 1U);
		ui_render(buffer + 16, &state);
	}
	/* Every page, including LEAVE A, must be identical in two-row slices. */
	for (unsigned int task = TASK_MONITOR; task <= TASK_LAP; task++) {
		state.task = (enum task_id)task;
		state.lap_departing = true;
		state.lap_approach = true;
		ui_render(buffer + 16, &state);
		memset(sliced, 0xa5, sizeof(sliced));
		for (unsigned int row = 0; row < UI_HEIGHT; row += 2)
			ui_render_rows(sliced, &state, row, 2);
		assert(!memcmp(sliced, buffer + 16, UI_BYTES));
	}
	for (index = 0; index < 16; index++) {
		assert(buffer[index] == 0xa5);
		assert(buffer[UI_BYTES + 16 + index] == 0xa5);
	}
	puts("PASS: UI rendering and framebuffer guards");
	return 0;
}
