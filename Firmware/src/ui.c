#include <stdio.h>
#include <string.h>
#include "config.h"
#include "ui.h"
#include "menu_font.h"

/* Compact original 5x7 glyph table, rows encoded from bit 4 to bit 0. */
static const char glyph_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-+:/%.? >";
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
	{14,17,1,2,4,0,4}, {0,0,0,0,0,0,0}, {16,8,4,2,4,8,16}
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
				      (size_t)(strchr(glyph_chars, '?') - glyph_chars);

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
	static const char *const keys[] = { "BACK", "-", "B/W", "-" };
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

static int pipe_column(int permille)
{
	return 8 + (permille * 112 + 500) / 1000;
}

static void pipe_strip(uint8_t *pixels, const struct ball_view *ball)
{
	static const struct {
		int raw_x;
		const char *label;
	} marks[] = {
		{ RULER_X_LEFT, "L" }, { RULER_X_MINUS_5, "-5" }, { RULER_X_O, "O" },
		{ RULER_X_PLUS_5, "+5" }, { RULER_X_RIGHT, "R" },
	};
	const int left = pipe_column(0), right = pipe_column(1000), y = 50;

	/* The fixed labels below the live strip belong to the normal page scan. */
	if (clip_bottom <= (int)UI_PIPE_TOP || clip_top >= 64)
		return;
	rect(pixels, 0, UI_PIPE_TOP, UI_WIDTH, UI_PIPE_END - UI_PIPE_TOP, BG);
	text(pixels, 5, UI_PIPE_TOP, ball->status == BALL_TRACKED ? "BALL" :
	     ball->status == BALL_LAST_SEEN ? "LAST SEEN" :
	     ball->status == BALL_UNCERTAIN ? "CHECK X" :
	     ball->status == BALL_MISSING ? "NO BALL" : "CAM OFFLINE", 1,
	     ball->status == BALL_TRACKED ? WHITE : MUTED);
	text(pixels, 111, UI_PIPE_TOP, "CM", 1, MUTED);
	rect(pixels, left, y, right - left + 1, 1, CYAN);
	for (unsigned int i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
		int x = pipe_column(ball_ruler_position(marks[i].raw_x));
		int width = (int)strlen(marks[i].label) * 6 - 1;
		rect(pixels, x, y, 1, 6, CYAN);
		text(pixels, x - width / 2, 57, marks[i].label, 1, WHITE);
	}
	if (ball->status == BALL_TRACKED || ball->status == BALL_LAST_SEEN) {
		static const uint8_t widths[] = { 3, 5, 7, 9, 9, 9, 7, 5, 3 };
		int x = pipe_column(ball->permille);

		for (unsigned int i = 0; i < sizeof(widths); i++) {
			rect(pixels, x - widths[i] / 2, y - 4 + (int)i, widths[i], 1, AMBER);
			if (ball->status == BALL_LAST_SEEN && i >= 2 && i <= 6)
				rect(pixels, x - widths[i] / 2 + 2, y - 4 + (int)i, widths[i] - 4, 1, BG);
		}
	}
}

void ui_render_pipe_rows(uint8_t *pixels, const struct ball_view *ball,
			 unsigned int first, unsigned int rows)
{
	if (first < UI_PIPE_TOP || first >= UI_PIPE_END || !rows)
		return;
	clip_top = (int)first;
	clip_bottom = rows >= UI_PIPE_END - first ? (int)UI_PIPE_END : (int)(first + rows);
	pipe_strip(pixels, ball);
	clip_top = 0;
	clip_bottom = UI_HEIGHT;
}

static void balance_page(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "BACK", "LEFT", "RIGHT", "RUN" };
	static const char *const cal_keys[] = { "BACK", "UP", "DOWN", "STOP" };
	char line[23];

	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	for (unsigned int i = 0; i < 4; i++) {
		card(pixels, i * 32, 0, 30, 13, PANEL);
		const char *label = state->running && (!state->balance_jog || i == 0 || i == 3) ?
				    "STOP" : keys[i];
		text(pixels, i * 32 + 1, 3, state->calibrating ? cal_keys[i] : label, 1, MUTED);
	}
	if (state->calibrating) {
		text(pixels, 5, 19, "CALIBRATE", 2, WHITE);
		if (state->cal_camera_x >= 0)
			snprintf(line, sizeof(line), "BALL X %d PX", state->cal_camera_x);
		else
			snprintf(line, sizeof(line), "BALL X --");
		text(pixels, 5, 48, line, 1, CYAN);
		snprintf(line, sizeof(line), "ENC %ld", (long)state->cal_encoder);
		text(pixels, 5, 67, line, 1, WHITE);
		text(pixels, 5, 88, state->balance_jog ? state->balance_status : "READY", 1, AMBER);
		if (state->shaft_referenced) {
			int cd = state->balance_shaft;
			unsigned mag = (unsigned)(cd < 0 ? -cd : cd);
			snprintf(line, sizeof(line), "SHAFT %c%u.%02u DEG", cd < 0 ? '-' : '+', mag / 100, mag % 100);
		} else {
			snprintf(line, sizeof(line), "CHECK SHAFT RANGE");
		}
		text(pixels, 5, 101, line, 1, CYAN);
		text(pixels, 5, 114, "TAP 1.8 DEG", 1, WHITE);
		text(pixels, 5, 129, "HOLD TO REPEAT", 1, MUTED);
		return;
	}
	text(pixels, 5, 19, "BALANCE", 2, WHITE);
	pipe_strip(pixels, &state->ball);
	text(pixels, 5, 65, state->balance_status ? state->balance_status : "SET LEVEL",
	     1, state->running ? GREEN : AMBER);
	if (state->balance_jog) {
		int cd = state->balance_shaft;
		unsigned int mag = cd < 0 ? (unsigned int)(-(int64_t)cd) : (unsigned int)cd;
		snprintf(line, sizeof(line), "SHAFT %c%u.%02u DEG", cd < 0 ? '-' : '+', mag / 100, mag % 100);
	} else {
		snprintf(line, sizeof(line), "SHAFT %+d CD", state->balance_shaft);
	}
	text(pixels, 5, 85, line, 1, WHITE);
	if (state->balance_jog) {
		int cd = state->balance_target;
		unsigned int mag = cd < 0 ? (unsigned int)(-(int64_t)cd) : (unsigned int)cd;
		snprintf(line, sizeof(line), "GOAL  %c%u.%02u DEG", cd < 0 ? '-' : '+', mag / 100, mag % 100);
	} else if (state->balance_speed_mode)
		snprintf(line, sizeof(line), "%s %u RPM", state->balance_rpm < 0 ? "CW" :
			 state->balance_rpm > 0 ? "CCW" : "HOLD",
			 (unsigned)(state->balance_rpm < 0 ? -state->balance_rpm : state->balance_rpm));
	else
		snprintf(line, sizeof(line), "GOAL  %+d CD", state->balance_target);
	text(pixels, 5, 99, line, 1, CYAN);
	text(pixels, 5, 119, state->balance_reverse ? "MAP -  WHEELS OFF" : "MAP +  WHEELS OFF", 1, MUTED);
	snprintf(line, sizeof(line), "%s %lu.%02lu S", state->balance_timed ? "O IN" : "TIME",
		 (unsigned long)(state->balance_time / 1000), (unsigned long)(state->balance_time / 10 % 100));
	text(pixels, 5, 131, state->balance_jog || (!state->running && !state->balance_time) ?
	     "TAP L/R  1.8 DEG" : line, 1, state->balance_timed ? GREEN : WHITE);
	text(pixels, 5, 143, state->running && !state->balance_jog ? "ANY KEY TO STOP" :
	     "HOLD L MAP  R ZERO", 1, MUTED);
}

static void lap_probes(uint8_t *pixels, const struct gray_reading *gray, int y)
{
	text(pixels, 5, y + 3, "GRAY", 1, MUTED);
	for (unsigned int i = 0; i < 8; i++) {
		int x = 36 + (int)i * 11;
		bool black = gray->status != GRAY_WARMUP && (gray->black & (1U << i));
		char label[] = { (char)('1' + i), 0 };
		card(pixels, x, y, 9, 12, black ? GREEN : PANEL);
		text(pixels, x + 2, y + 3, label, 1, black ? BG : MUTED);
	}
}

static void lap_page(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "BACK", "MAP", "SPD", "RUN" };
	char line[24];
	unsigned int index;

	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	for (index = 0; index < 4; index++) {
		card(pixels, index * 32, 0, 30, 13, PANEL);
		text(pixels, index * 32 + 3, 3,
		     (index == 0 || index == 3) && state->running ? "STOP" : keys[index], 1, MUTED);
	}
	text(pixels, 5, 20, "LAP", 2, WHITE);
	text(pixels, 47, 25, state->lap_departing ? "LEAVE A" :
	     lap_status_name(state->lap_status), 1,
	     state->lap_status == LAP_RUNNING || state->lap_status == LAP_DONE ? GREEN : AMBER);
	pipe_strip(pixels, &state->ball);
	snprintf(line, sizeof(line), "%u CPS / MAP %s", state->lap_speed,
		 state->lap_reverse ? "B" : "A");
	text(pixels, 5, 65, line, 1, MUTED);
	if (state->grayscale.status == GRAY_LINE)
		snprintf(line, sizeof(line), "POS %+d", (int)state->grayscale.position);
	else
		snprintf(line, sizeof(line), "POS -- %s", gray_status_name(state->grayscale.status));
	text(pixels, 5, 75, line, 1, CYAN);
	for (index = 0; index < 2; index++) {
		int x = index ? 66 : 4;

		card(pixels, x, 84, 58, 35, PANEL);
		text(pixels, x + 5, 88, index ? "RIGHT" : "LEFT", 1, MUTED);
		snprintf(line, sizeof(line), "%d/S", state->lap_cps[index]);
		text(pixels, x + 5, 99, line, 1, WHITE);
		snprintf(line, sizeof(line), "PWM %d%%", state->lap_pwm[index] / 10);
		text(pixels, x + 5, 110, line, 1, CYAN);
	}
	lap_probes(pixels, &state->grayscale, 127);
	snprintf(line, sizeof(line), "A %s %lu.%luS", state->lap_approach ? "NEAR" :
		 state->lap_armed ? "ARMED" : "WAIT",
		 (unsigned long)(state->lap_ms / 1000),
		 (unsigned long)(state->lap_ms / 100 % 10));
	text(pixels, 5, 150, line, 1, MUTED);
}

static void menu_text(uint8_t *pixels, int x, int y, const char *label, uint16_t color)
{
	if (y >= clip_bottom || y + 14 <= clip_top)
		return;
	const uint8_t *p = (const uint8_t *)label;
	while (*p && x < UI_WIDTH) {
		if (*p < 128) {
			char letter[] = { (char)*p++, 0 };
			text(pixels, x, y + 3, letter, 1, color);
			x += 6;
			continue;
		}
		/* All non-ASCII literals on these pages are three-byte Chinese glyphs. */
		unsigned int code = (p[0] & 15U) << 12 | (p[1] & 63U) << 6 | (p[2] & 63U);
		p += 3;
		unsigned int lo = 0, hi = sizeof(menu_glyphs) / sizeof(menu_glyphs[0]);
		while (lo < hi) {
			unsigned int mid = lo + (hi - lo) / 2;
			if (menu_glyphs[mid].code < code)
				lo = mid + 1;
			else
				hi = mid;
		}
		if (lo < sizeof(menu_glyphs) / sizeof(menu_glyphs[0]) && menu_glyphs[lo].code == code) {
			for (int row = y < clip_top ? clip_top - y : 0; row < 14 && y + row < clip_bottom; row++) {
				uint16_t bits = menu_glyphs[lo].rows[row];
				for (int col = 0; col < 14; col++)
					if (bits & (1U << col))
						rect(pixels, x + col, y + row, 1, 1, color);
			}
		}
		x += 14;
	}
}

struct event_description {
	const char *number, *title, *route, *limit, *accuracy;
};

static const struct event_description events[EVENT_COUNT] = {
	{ "第一题", "静止移球", "O > +5 > -5 CM", "目标用时  8 S", "0.3 / 0.4 / 0.4 S" },
	{ "第二题", "循线停车", "A > A  顺时针一圈", "限时  30 S", "停车误差  3 CM" },
	{ "第三题", "载球半圈", "A > B  保持中心", "限时  12 S", "球位误差  2 CM" },
	{ "第四题", "载球整圈", "一圈  保持中心", "限时  40 S", "球位误差  2 CM" },
	{ "发挥一", "定点稳球", "一圈  指定球位", "目标  -5 / +5 CM", "球位误差  2 CM" },
	{ "发挥二", "图传回放", "实时显示钢球", "全程录像", "回放完整测试" },
};

static const char *const notices[] = {
	"", "请将球放在 O 点", "检查步进或驱动", "相机丢球", "超过题目限时",
	"已停止", "已完成", "检查赛道和灰度", "待接入", "SHAFT LIMIT",
};

static int ruler_mm_column(int mm)
{
	int span = mm < 0 ? RULER_X_MINUS_5 - RULER_X_O : RULER_X_O - RULER_X_PLUS_5;
	int dx = (mm * span + (mm < 0 ? -25 : 25)) / 50;
	return pipe_column(ball_ruler_position(RULER_X_O - dx));
}

static void contest_ruler(uint8_t *pixels, const struct ball_view *ball, int y, int target_mm)
{
	const int marks[] = { RULER_X_LEFT, RULER_X_MINUS_5, RULER_X_O, RULER_X_PLUS_5, RULER_X_RIGHT };
	const char *const labels[] = { "L", "-5", "O", "+5", "R" };
	const int left = pipe_column(0), right = pipe_column(1000);
	if (clip_top >= y + 12 || clip_bottom <= y - 18)
		return;
	rect(pixels, left, y - 3, right - left + 1, 7, MUTED);
	rect(pixels, left + 1, y - 2, right - left - 1, 5, PANEL);
	int first_cm = contest_ball_mm(RULER_X_LEFT) / 10;
	int last_cm = contest_ball_mm(RULER_X_RIGHT) / 10;
	for (int cm = first_cm; cm <= last_cm; cm++)
		rect(pixels, ruler_mm_column(cm * 10), y - 2, 1, 2, CYAN);
	rect(pixels, left, y, right - left + 1, 1, CYAN);
	for (unsigned int i = 0; i < 5; i++) {
		int x = pipe_column(ball_ruler_position(marks[i]));
		rect(pixels, x, y, 1, 4, CYAN);
		text(pixels, x - (int)strlen(labels[i]) * 3 + 1, y + 5, labels[i], 1, MUTED);
	}
	if (ball->status == BALL_TRACKED || ball->status == BALL_LAST_SEEN) {
		int x = pipe_column(ball->permille);
		card(pixels, x - 3, y - 4, 7, 7, AMBER);
		if (ball->status == BALL_LAST_SEEN)
			rect(pixels, x - 1, y - 2, 3, 3, BG);
	}
	/* Keep both marks visible when the ball reaches the target. */
	int col = ruler_mm_column(target_mm);
	for (int row = 0; row < 5; row++)
		rect(pixels, col - 4 + row, y - 11 + row, 9 - 2 * row, 1, GREEN);
	text(pixels, 109, y - 18, "CM", 1, MUTED);
}

static void contest_page(uint8_t *pixels, const struct ui_state *s)
{
	const struct contest_menu *m = &s->menu;
	const struct event_description *e = &events[m->item];
	bool home = m->page == MENU_HOME, tools_page = m->page == MENU_TOOLS;
	bool list = home || tools_page;
	bool offset = m->item == EVENT_OFFSET;
	bool tune = offset && (m->page == MENU_DETAIL || (m->page == MENU_RUN && !m->offset_driving));
	bool lap_ready = m->page == MENU_DETAIL && m->item == EVENT_LAP;
	const char *keys[] = { "返回", list ? "上项" : tune ? "-1" : "",
			       list ? "下项" : tune ? "+1" : lap_ready ? "SPD" : "",
			       list ? "进入" : tune ? (m->page == MENU_DETAIL ? "稳球" : "RUN") :
			       m->page == MENU_DETAIL ?
			       (m->item <= EVENT_LAP && m->notice != NOTICE_TRAVEL ? "启动" : "确认") :
			       m->page == MENU_RUN ? "停止" : "重选" };
	char buf[36];
	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	for (int i = 0; i < 4; i++) {
		if (!*keys[i])
			continue;
		card(pixels, i * 32, 0, 31, 17, i == 3 ? DARK_GREEN : PANEL);
		menu_text(pixels, i * 32 + 2, 1, keys[i], i == 3 ? GREEN : home && !i ? EDGE : MUTED);
	}
	if (list) {
		menu_text(pixels, 5, 21, home ? "校赛 A 题" : "设备调试", WHITE);
		rect(pixels, 4, 38, 120, 1, EDGE);
		if (home) {
			for (unsigned int i = 0; i < EVENT_COUNT; i++) {
				int y = 41 + (int)i * 17;
				if (i == (unsigned int)m->item) {
					card(pixels, 2, y, 124, 17, PANEL);
					rect(pixels, 2, y + 2, 3, 13, CYAN);
				}
				menu_text(pixels, 8, y + 1, events[i].number, i == (unsigned int)m->item ? CYAN : MUTED);
				menu_text(pixels, 62, y + 1, events[i].title, WHITE);
			}
			menu_text(pixels, 7, 145, "双中键进调试", MUTED);
		} else {
			unsigned int first = m->tool < 6 ? 0 : TASK_COUNT - 6;
			for (unsigned int i = first; i < first + 6; i++) {
				int y = 43 + (int)(i - first) * 18;
				if (i == m->tool)
					card(pixels, 2, y, 124, 17, PANEL);
				menu_text(pixels, 10, y + 1, task_name(contest_tool(i)), i == m->tool ? CYAN : WHITE);
			}
		}
		return;
	}
	menu_text(pixels, 6, 23, e->number, CYAN);
	menu_text(pixels, 62, 23, e->title, WHITE);
	rect(pixels, 4, 42, 120, 1, EDGE);
	if (offset && (m->page == MENU_DETAIL || m->page == MENU_RUN)) {
		snprintf(buf, sizeof(buf), "目标  %+d MM", m->target_mm);
		menu_text(pixels, 6, 49, buf, GREEN);
		if (s->position_seen)
			snprintf(buf, sizeof(buf), "实测  %+d MM", s->ball_mm);
		else
			snprintf(buf, sizeof(buf), "等待球位");
		menu_text(pixels, 6, 69, buf, s->position_seen ? AMBER : MUTED);
		contest_ruler(pixels, &s->ball, 103, m->target_mm);
		if (m->page == MENU_DETAIL) {
			menu_text(pixels, 6, 121, "SW4 -   SW3 +", MUTED);
			menu_text(pixels, 6, 141, m->notice ? notices[m->notice] : "SW2  稳球", m->notice ? AMBER : GREEN);
		} else if (!m->offset_driving) {
			menu_text(pixels, 6, 121, s->leg == LEVEL_SHAFT ? "LEVEL  0 DEG" :
				  !s->offset_ready ? "WAIT VISION" : s->leg == HOLD_TARGET ? "保持" : "调节中", CYAN);
			menu_text(pixels, 6, 141, s->offset_ready ? "SW2  RUN" : "SW4 -   SW3 +", GREEN);
		} else {
			snprintf(buf, sizeof(buf), "ENC %lu/%u", (unsigned long)s->lap_counts, LAP_OFFSET_DISTANCE_COUNTS);
			text(pixels, 6, 124, buf, 1, MUTED);
			snprintf(buf, sizeof(buf), "用时  %lu.%02lu S", (unsigned long)(s->event_ms / 1000),
				 (unsigned long)(s->event_ms / 10 % 100));
			menu_text(pixels, 6, 141, buf, WHITE);
		}
		return;
	}
	if (m->page == MENU_DETAIL) {
		menu_text(pixels, 6, 52, e->route, WHITE);
		menu_text(pixels, 6, lap_ready ? 70 : 78, e->limit, MUTED);
		menu_text(pixels, 6, lap_ready ? 87 : 98, e->accuracy, MUTED);
		if (lap_ready) {
			snprintf(buf, sizeof(buf), "%u CPS", s->lap_speed);
			text(pixels, 6, 107, buf, 1, CYAN);
			lap_probes(pixels, &s->grayscale, 119);
			menu_text(pixels, 6, 141, "A 点  SW2 启动", GREEN);
			return;
		}
		if (m->item == EVENT_POSITION)
			contest_ruler(pixels, &s->ball, 125, 0);
		const char *hint = m->notice ? notices[m->notice] :
				   m->item == EVENT_POSITION ? (s->level_ready ? "LEVEL OK" : "LEVEL  0 DEG") :
				   "控制待接入";
		menu_text(pixels, 6, 141, hint, m->notice || m->item > EVENT_LAP ? AMBER : CYAN);
	} else if (m->page == MENU_RUN) {
		if (m->item == EVENT_POSITION) {
			int target_mm = s->leg == MOVE_RIGHT ? 50 :
					(s->leg == MOVE_LEFT || s->leg == HOLD_LEFT) ? -50 : 0;
			menu_text(pixels, 6, 50, s->leg == LEVEL_SHAFT ? "LEVEL  0 DEG" :
				  target_mm == 0 ? "目标  O" :
				  target_mm > 0 ? "目标  +5 CM" : "目标  -5 CM", GREEN);
			if (s->position_seen)
				snprintf(buf, sizeof(buf), "实测  %+d MM", s->ball_mm);
			else
				snprintf(buf, sizeof(buf), "等待球位");
			menu_text(pixels, 6, 73, buf, s->position_seen ? AMBER : MUTED);
			menu_text(pixels, 6, 90, s->waiting_vision ? "WAIT VISION" :
				  s->leg == HOLD_LEFT ? "保持 -5 CM" : "调节中",
				  s->leg == HOLD_LEFT && !s->waiting_vision ? GREEN : AMBER);
			contest_ruler(pixels, &s->ball, 118, target_mm);
		} else {
			menu_text(pixels, 6, 51, s->lap_departing ? "FIND LINE" : "FOLLOW", CYAN);
			snprintf(buf, sizeof(buf), "L %d   R %d", s->lap_cps[0], s->lap_cps[1]);
			text(pixels, 6, 77, buf, 1, WHITE);
			snprintf(buf, sizeof(buf), "ENC %lu/%u", (unsigned long)s->lap_counts, LAP_DISTANCE_COUNTS);
			text(pixels, 6, 95, buf, 1, MUTED);
			lap_probes(pixels, &s->grayscale, 112);
		}
		snprintf(buf, sizeof(buf), "用时  %lu.%02lu S", (unsigned long)(s->event_ms / 1000),
			 (unsigned long)(s->event_ms / 10 % 100));
		menu_text(pixels, 6, 137, buf, MUTED);
	} else {
		bool lap_fault = m->item == EVENT_LAP && (m->notice == NOTICE_LINE || m->notice == NOTICE_DRIVE);
		menu_text(pixels, 6, 60, lap_fault ? lap_status_name(s->lap_status) : notices[m->notice],
			  m->notice == NOTICE_FINISHED ? GREEN : AMBER);
		if (m->notice == NOTICE_PENDING) {
			menu_text(pixels, 6, 87, e->route, WHITE);
		} else {
			snprintf(buf, sizeof(buf), "用时  %lu.%02lu S", (unsigned long)(s->event_ms / 1000),
				 (unsigned long)(s->event_ms / 10 % 100));
			menu_text(pixels, 6, 89, buf, WHITE);
		}
		if ((m->item == EVENT_LAP || offset) && m->notice == NOTICE_FINISHED)
			menu_text(pixels, 6, 118, "一圈停车", CYAN);
	}
}


void ui_render(uint8_t *pixels, const struct ui_state *state)
{
	static const char *const keys[] = { "BACK", "DIR", "PWR", "RUN" };
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
	if (state->menu.page != MENU_DIAGNOSTIC) {
		contest_page(pixels, state);
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
	if (state->task == TASK_BALANCE) {
		balance_page(pixels, state);
		return;
	}
	rect(pixels, 0, 0, UI_WIDTH, UI_HEIGHT, BG);
	/* Photo orientation: left-to-right SW5, SW4, SW3, SW2, above the LCD. */
	for (index = 0; index < 4; index++) {
		const char *label = keys[index];

		if ((index == 0 || index == 3) && state->running)
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
