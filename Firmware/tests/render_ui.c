#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

int main(int argc, char **argv)
{
	static uint8_t buffer[UI_BYTES + 32];
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
		state.updating = true;
		state.update_status = "RECEIVING";
		state.update_bytes = 450;
		state.update_total = 1000;
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
	for (index = 0; index < 16; index++) {
		assert(buffer[index] == 0xa5);
		assert(buffer[UI_BYTES + 16 + index] == 0xa5);
	}
	puts("PASS: UI rendering and framebuffer guards");
	return 0;
}
