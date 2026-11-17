#include <stdio.h>
#include <string.h>
#include "ui.h"

/* Compact original 5x7 glyph table, rows encoded from bit 4 to bit 0. */
static const char glyph_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-+:/%.? ";
static const uint8_t glyphs[][7] = {
	{14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
	{14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
	{2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
	{14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
	{14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
	{14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
	{14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
	{31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
	{14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
	{14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
	{17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
	{17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
	{14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
	{14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
	{15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
	{17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
	{17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
	{17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
	{0,0,0,31,0,0,0}, {0,4,4,31,4,4,0},
	{0,4,4,0,4,4,0}, {1,1,2,4,8,16,16},
	{25,25,2,4,8,19,19}, {0,0,0,0,0,6,6},
	{14,17,1,2,4,0,4}, {0,0,0,0,0,0,0}
};

enum color {
	BG = 0x0862, PANEL = 0x10e4, EDGE = 0x21a7, MUTED = 0x8cb3,
	WHITE = 0xef9e, CYAN = 0x2e7d, GREEN = 0x47b2, AMBER = 0xfd48,
	RED = 0xfacc, DARK_GREEN = 0x1249
};

/* Rendering is main-loop owned. Slice clipping bounds each LCD service call. */
static int clip_top;
static int clip_bottom = UI_HEIGHT;

static void rect(uint8_t *pixels, int x, int y, int w, int h, uint16_t color)
{
	int row;
	int col;

	for (row = y > clip_top ? y : clip_top;
	     row < y + h && row < clip_bottom; row++) {
		for (col = x; col < x + w && col < UI_WIDTH; col++) {
			unsigned int offset;

			if (row < 0 || col < 0)
				continue;
			offset = (unsigned int)(row * UI_WIDTH + col) * 2;
			pixels[offset] = (uint8_t)(color >> 8);
			pixels[offset + 1] = (uint8_t)color;
		}
	}
}

static void card(uint8_t *pixels, int x, int y, int w, int h, uint16_t color)
{
	rect(pixels, x + 2, y, w - 4, h, color);
	rect(pixels, x, y + 2, w, h - 4, color);
}

static void text(uint8_t *pixels, int x, int y, const char *label,
		 unsigned int scale, uint16_t color)
{
	/* Entire off-slice labels used to traverse every glyph on every stripe. */
	if (y >= clip_bottom || y + 7 * (int)scale <= clip_top)
		return;
	for (; *label && x < UI_WIDTH; label++, x += 6 * (int)scale) {
		const char *glyph = strchr(glyph_chars, *label);
		unsigned int row;
		unsigned int col;
		size_t index = glyph ? (size_t)(glyph - glyph_chars) :
				      sizeof(glyph_chars) - 3;

		for (row = 0; row < 7; row++) {
			for (col = 0; col < 5; col++) {
				if (glyphs[index][row] & (1U << (4 - col)))
					rect(pixels, x + (int)(col * scale),
					     y + (int)(row * scale), scale,
					     scale, color);
			}
		}
	}
}

static void wheel_card(uint8_t *pixels, const struct ui_state *state,
		       unsigned int side)
{
	int x = side ? 66 : 4;
	uint16_t color = state->faults & (1U << side) ? RED : CYAN;
	char line[24];
	unsigned int scale;

	card(pixels, x, 66, 58, 44, PANEL);
	rect(pixels, x + 6, 72, 2, 6, color);
	text(pixels, x + 12, 72, side ? "RIGHT" : "LEFT", 1, MUTED);
	/* A/B input dots help distinguish a static input from a counter issue. */
	for (unsigned int phase = 0; phase < 2; phase++)
		rect(pixels, x + 49 + (int)phase * 4, 73, 3, 3,
		     state->phases & (1U << (side * 2 + phase)) ? CYAN : EDGE);
	snprintf(line, sizeof(line), "%ld", (long)state->encoder[side]);
	scale = strlen(line) <= 4 ? 2 : 1;
	/* Full signed counts remain available in telemetry when > 8 characters. */
	if (strlen(line) > 8)
		strcpy(line, "SEE PC");
	text(pixels, x + 6, 83, line, scale, WHITE);
	snprintf(line, sizeof(line), "%ld/S", (long)state->counts_per_second[side]);
	if (strlen(line) > 8)
		strcpy(line, "FAST");
	text(pixels, x + 6, 101, line, 1, color);
}

static void update_page(uint8_t *pixels, const struct ui_state *state)
{
	unsigned int percent = state->update_total ?
		(unsigned int)((uint64_t)state->update_bytes * 100 / state->update_total) : 0;
	char line[24];

	if (percent > 100)
		percent = 100;
	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	text(pixels, 6, 8, "CORAL / WIRELESS", 1, CYAN);
	text(pixels, 6, 29, "UPDATE", 2, WHITE);
	card(pixels, 4, 54, 120, 63, PANEL);
	text(pixels, 10, 62, state->update_status, 1, CYAN);
	snprintf(line, sizeof(line), "%u%%", percent);
	text(pixels, 10, 78, line, 2, WHITE);
	rect(pixels, 10, 102, 108, 5, EDGE);
	rect(pixels, 10, 102, (int)(108U * percent / 100U), 5, GREEN);
	text(pixels, 6, 127, "MOTORS STOPPED", 1, MUTED);
	text(pixels, 6, 145, "KEEP POWER ON", 1, AMBER);
}

static void gray_page(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "VIEW", "-", "B/W", "TASK" };
	const struct gray_reading *gray = &state->grayscale;
	char line[24];
	unsigned int index;

	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	for (index = 0; index < 4; index++) {
		card(pixels, index * 32, 0, 30, 13, PANEL);
		text(pixels, index * 32 + 3, 3, keys[index], 1, MUTED);
	}
	text(pixels, 5, 20, state->gray_white ? "GRAY / WHITE" : "GRAY / BLACK",
	     1, CYAN);
	for (index = 0; index < 8; index++) {
		int x = 6 + (int)index * 15;
		bool active = gray->status != GRAY_WARMUP &&
			      (gray->line & (1U << index));

		snprintf(line, sizeof(line), "%u", index + 1);
		text(pixels, x + 4, 35, line, 1, MUTED);
		card(pixels, x, 46, 12, 18, active ? GREEN : PANEL);
	}
	for (index = 0; index < 8; index++)
		line[index] = gray->raw & (1U << index) ? '1' : '0';
	line[8] = '\0';
	text(pixels, 5, 72, "RAW", 1, MUTED);
	text(pixels, 36, 72, line, 1, WHITE);
	snprintf(line, sizeof(line), "N:%u GROUP:%u", gray->count, gray->groups);
	text(pixels, 5, 86, line, 1, MUTED);
	text(pixels, 5, 102, gray_status_name(gray->status), 2,
	     gray->status == GRAY_LINE ? GREEN : AMBER);
	if (gray->status == GRAY_LINE)
		snprintf(line, sizeof(line), "POS %+d", (int)gray->position);
	else
		snprintf(line, sizeof(line), "POS --");
	text(pixels, 5, 123, line, 1, WHITE);
	text(pixels, 5, 139, "CH1 - / CH8 +", 1, MUTED);
	text(pixels, 5, 151, "B/W: TARGET COLOR", 1, CYAN);
}

static void lap_page(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "RUN", "MAP", "SPD", "TASK" };
	char line[24];
	unsigned int index;

	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	for (index = 0; index < 4; index++) {
		card(pixels, index * 32, 0, 30, 13, PANEL);
		text(pixels, index * 32 + 3, 3,
		     index == 0 && state->running ? "STOP" : keys[index], 1, MUTED);
	}
	text(pixels, 5, 20, "LAP", 2, WHITE);
	text(pixels, 53, 25, "1 TO 8 L-R", 1, CYAN);
	text(pixels, 5, 43, state->lap_departing ? "LEAVE A" :
	     lap_status_name(state->lap_status), 1,
	     state->lap_status == LAP_RUNNING || state->lap_status == LAP_DONE ? GREEN : AMBER);
	snprintf(line, sizeof(line), "%u CPS / MAP %s", state->lap_speed,
		 state->lap_reverse ? "B" : "A");
	text(pixels, 5, 57, line, 1, MUTED);
	if (state->grayscale.status == GRAY_LINE)
		snprintf(line, sizeof(line), "POS %+d", (int)state->grayscale.position);
	else
		snprintf(line, sizeof(line), "POS -- %s", gray_status_name(state->grayscale.status));
	text(pixels, 5, 71, line, 1, CYAN);
	for (index = 0; index < 2; index++) {
		int x = index ? 66 : 4;

		card(pixels, x, 84, 58, 35, PANEL);
		text(pixels, x + 5, 88, index ? "RIGHT" : "LEFT", 1, MUTED);
		snprintf(line, sizeof(line), "%d/S", state->lap_cps[index]);
		text(pixels, x + 5, 99, line, 1, WHITE);
		snprintf(line, sizeof(line), "PWM %d%%", state->lap_pwm[index] / 10);
		text(pixels, x + 5, 110, line, 1, CYAN);
	}
	text(pixels, 5, 130, "GRAY", 1, MUTED);
	for (index = 0; index < 8; index++) {
		int x = 36 + (int)index * 11;
		bool black = state->grayscale.status != GRAY_WARMUP &&
			     (state->grayscale.black & (1U << index));

		card(pixels, x, 127, 9, 12, black ? GREEN : PANEL);
		snprintf(line, sizeof(line), "%u", index + 1);
		text(pixels, x + 2, 130, line, 1, black ? BG : MUTED);
	}
	snprintf(line, sizeof(line), "A %s %lu.%luS", state->lap_approach ? "NEAR" :
		 state->lap_armed ? "ARMED" : "WAIT",
		 (unsigned long)(state->lap_ms / 1000),
		 (unsigned long)(state->lap_ms / 100 % 10));
	text(pixels, 5, 150, line, 1, MUTED);
}

void ui_render(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "RUN", "DIR", "PWR", "TASK" };
	uint8_t faults = state->faults | state->wheel_faults;
	bool fault = faults || state->wheel_enable_fault;
	const char *status = fault ? "ERR" : state->running ? "RUN" : "IDLE";
	uint16_t status_color = fault ? RED : state->running ? GREEN : AMBER;
	char line[24];
	unsigned int index;

	if (state->updating) {
		update_page(pixels, state);
		return;
	}
	if (state->task == TASK_GRAY) {
		gray_page(pixels, state);
		return;
	}
	if (state->task == TASK_LAP) {
		lap_page(pixels, state);
		return;
	}
	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	/* Photo orientation: left-to-right SW5, SW4, SW3, SW2, above the LCD. */
	for (index = 0; index < 4; index++) {
		const char *label = keys[index];

		if (index == 0 && state->running)
			label = "STOP";
		if (index == 2 && state->task == TASK_STEPPER)
			label = "SPD";
		if (index == 2 && state->task == TASK_ALL)
			label = "LVL";
		card(pixels, index * 32, 0, 30, 13, PANEL);
		text(pixels, index * 32 + 3, 3, label, 1, MUTED);
	}
	text(pixels, 5, 19, "CORAL / C5", 1, CYAN);
	text(pixels, 105, 19, "BT", 1, state->bt_tx ? CYAN : MUTED);
	rect(pixels, 121, 20, 3, 3, state->bt_tx ? CYAN : EDGE);
	text(pixels, 4, 34, task_name(state->task), 2, WHITE);
	card(pixels, 94, 34, 30, 13, state->running ? DARK_GREEN : PANEL);
	text(pixels, 97, 37, status, 1, status_color);
	if (state->task == TASK_ALL)
		snprintf(line, sizeof(line), "%u%% %uRPM %s", state->duty / 10,
			 state->step_target_rpm, state->reverse ? "R" : "F");
	else if (state->task == TASK_STEPPER)
		snprintf(line, sizeof(line), "%uRPM %s", state->step_target_rpm,
			 state->reverse ? "CCW" : "CW");
	else
		snprintf(line, sizeof(line), "%u%%  %s", state->duty / 10,
			 state->reverse ? "REV" : "FWD");
	text(pixels, 5, 53, line, 1, MUTED);
	if (task_continuous(state->task)) {
		text(pixels, 97, 53, "CONT", 1, state->running ? GREEN : MUTED);
	} else if (state->running) {
		snprintf(line, sizeof(line), "%lu.%luS",
			 (unsigned long)(state->remaining_ms / 1000),
			 (unsigned long)(state->remaining_ms / 100 % 10));
		text(pixels, 97, 53, line, 1, GREEN);
	}
	wheel_card(pixels, state, 0);
	wheel_card(pixels, state, 1);
	rect(pixels, 5, 117, 3, 5, state->step_online ? GREEN : AMBER);
	text(pixels, 13, 116, "EMM", 1, MUTED);
	if (task_uses_stepper(state->task) && state->running &&
	    state->step_rpm_valid && !state->step_error) {
		snprintf(line, sizeof(line), "%+ldRPM", (long)state->step_rpm);
		text(pixels, 38, 116, line, 1, GREEN);
	} else {
		const char *label = state->step_status;

		if (state->step_error) {
			snprintf(line, sizeof(line), "ERR %02X", state->step_error);
			label = line;
		}
		text(pixels, 38, 116, label, 1,
		     state->step_error ? RED : state->step_online ? GREEN : AMBER);
	}
	text(pixels, 5, 129, "GRAY", 1, MUTED);
	for (index = 0; index < 8; index++) {
		int x = 36 + (int)index * 11;
		bool low = state->grayscale.status != GRAY_WARMUP &&
			   (state->grayscale.black & (1U << index));

		card(pixels, x, 127, 9, 12, low ? CYAN : PANEL);
		snprintf(line, sizeof(line), "%u", index + 1);
		text(pixels, x + 2, 130, line, 1, low ? BG : MUTED);
	}
	rect(pixels, 4, 144, 120, 1, EDGE);
	if (faults)
		snprintf(line, sizeof(line), "FAULT L:%u R:%u",
			 faults & 1U, (faults >> 1) & 1U);
	else if (state->wheel_enable_fault)
		snprintf(line, sizeof(line), "ENABLE LOW / PA3");
	else if (task_continuous(state->task) && state->running)
		snprintf(line, sizeof(line), "ANY KEY TO STOP");
	else if (state->wheel_requested)
		snprintf(line, sizeof(line), "EN REQ  AB:%u%u %u%u",
			 state->phases & 1U, (state->phases >> 1) & 1U,
			 (state->phases >> 2) & 1U, (state->phases >> 3) & 1U);
	else if (state->task == TASK_ALL)
		snprintf(line, sizeof(line), "RUN: ALL MOTORS");
	else
		snprintf(line, sizeof(line), "WHEELS OFF / J10 ?");
	text(pixels, 5, 150, line, 1, fault ? RED : MUTED);
}

void ui_render_rows(uint8_t *pixels, const struct ui_state *state,
		    unsigned int first, unsigned int rows)
{
	if (first >= UI_HEIGHT || !rows)
		return;
	clip_top = (int)first;
	clip_bottom = rows >= UI_HEIGHT - first ? UI_HEIGHT : (int)(first + rows);
	ui_render(pixels, state);
	clip_top = 0;
	clip_bottom = UI_HEIGHT;
}
