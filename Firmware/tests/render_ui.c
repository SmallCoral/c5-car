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
		if (!strcmp(argv[2], "home") || !strcmp(argv[2], "detail")) {
			state.menu.page = !strcmp(argv[2], "home") ? MENU_HOME : MENU_DETAIL;
			state.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = 500 };
		} else if (!strcmp(argv[2], "offset")) {
			state.menu = (struct contest_menu){ .page = MENU_RUN, .item = EVENT_OFFSET, .target_mm = 50 };
			state.leg = MOVE_TARGET;
			state.offset_ready = state.position_seen = true;
			state.ball_mm = contest_ball_mm(233);
			state.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = ball_ruler_position(233) };
		} else if (!strcmp(argv[2], "lap")) {
			state.task = TASK_LAP;
			state.lap_status = LAP_RUNNING;
			state.lap_speed = 1800;
			state.lap_ms = 12500;
			state.lap_armed = true;
			state.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = 500 };
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
	state.menu.page = MENU_DIAGNOSTIC;
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
	/* The marker follows the track and disappears from its last position. */
	for (unsigned int i = 0; i <= 2; i++) {
		unsigned int x = 8 + 56 * i;
		unsigned int dot = 16 + (50 * UI_WIDTH + x) * 2;
		state.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = i * 500 };
		ui_render(buffer + 16, &state);
		assert(buffer[dot] == 0xfd && buffer[dot + 1] == 0x48);
		for (unsigned int y = UI_PIPE_TOP; y < UI_PIPE_END; y += 2)
			ui_render_pipe_rows(sliced, &state.ball, y, 2);
		size_t start = UI_PIPE_TOP * UI_WIDTH * 2;
		size_t length = (UI_PIPE_END - UI_PIPE_TOP) * UI_WIDTH * 2;
		assert(!memcmp(sliced + start, buffer + 16 + start, length));
		state.ball.status = BALL_MISSING;
		ui_render(buffer + 16, &state);
		assert(buffer[dot] == 0x2e && buffer[dot + 1] == 0x7d);
	}
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
	const int targets[] = { -106, -50, 0, 50, 95 };
	const int raw_x[] = { 531, 400, 282, 160, 49 };
	const int columns[] = { 8, 37, 64, 93, 120 };
	for (unsigned int i = 0; i < 5; i++) {
		state.menu = (struct contest_menu){ .item = EVENT_OFFSET, .target_mm = targets[i],
			.page = i ? MENU_RUN : MENU_DETAIL, .offset_driving = i >= 3 };
		state.ball = (struct ball_view){ .status = BALL_TRACKED,
			.permille = ball_ruler_position(raw_x[i]) };
		ui_render(buffer + 16, &state);
		size_t marker = 16 + 2 * (96 * UI_WIDTH + columns[i]);
		size_t ball = 16 + 2 * (103 * UI_WIDTH + columns[i]);
		assert(buffer[marker] == 0x47 && buffer[marker + 1] == 0xb2);
		assert(buffer[ball] == 0xfd && buffer[ball + 1] == 0x48);
		for (unsigned int row = 0; row < UI_HEIGHT; row += 2)
			ui_render_rows(sliced, &state, row, 2);
		assert(!memcmp(sliced, buffer + 16, UI_BYTES));
		state.ball.status = BALL_LAST_SEEN;
		ui_render(buffer + 16, &state);
		assert(buffer[ball] == 0x08 && buffer[ball + 1] == 0x62);
		assert(buffer[ball + 4] == 0xfd && buffer[ball + 5] == 0x48);
		state.ball.status = BALL_MISSING;
		ui_render(buffer + 16, &state);
		assert(buffer[ball] == 0x2e && buffer[ball + 1] == 0x7d);
		assert(buffer[marker] == 0x47 && buffer[marker + 1] == 0xb2);
	}
	/* Both events show the same scale, including the hollow last sighting.
	 * Compare every pixel of the strip after each Task 1 stage change. */
	const enum position_leg legs[] = { LEVEL_SHAFT, LEVEL_SHAFT, MOVE_CENTER, MOVE_RIGHT, MOVE_LEFT, HOLD_LEFT };
	const int route_mm[] = { 0, 0, 0, 50, -50, -50 };
	for (unsigned int i = 0; i < sizeof(legs) / sizeof(legs[0]); i++) {
		state.menu = (struct contest_menu){ .page = MENU_RUN, .item = EVENT_OFFSET, .target_mm = route_mm[i] };
		state.ball = (struct ball_view){ .status = i == 5 ? BALL_LAST_SEEN : BALL_TRACKED,
			.permille = ball_ruler_position(i == 5 ? RULER_X_MINUS_5 : RULER_X_O) };
		ui_render(sliced, &state);
		state.menu.item = EVENT_POSITION;
		state.menu.page = i ? MENU_RUN : MENU_DETAIL;
		state.leg = legs[i];
		ui_render(buffer + 16, &state);
		int y = i ? 118 : 125;
		/* Status and timing text above the ruler have their own layout. */
		size_t strip = 23 * UI_WIDTH * 2;
		assert(!memcmp(buffer + 16 + (y - 11) * UI_WIDTH * 2,
			       sliced + (103 - 11) * UI_WIDTH * 2, strip));
	}
	puts("PASS: UI rendering and framebuffer guards");
	return 0;
}
