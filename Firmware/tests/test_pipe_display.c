#include <assert.h>
#include <string.h>
#include "board.h"
#include "display.h"

static uint8_t lcd[UI_BYTES];
static uint8_t cmd;
static unsigned int row;
static unsigned int transfers;
static uint32_t now;

void board_lcd_backlight(bool on) { (void)on; }
void board_lcd_reset(bool high) { (void)high; }
void board_delay(uint32_t ms) { now += ms; }

bool board_lcd_write(bool data, const uint8_t *bytes, size_t size)
{
	if (!data) {
		assert(size == 1);
		cmd = bytes[0];
	} else if (cmd == 0x2a) {
		assert(size == 4 && bytes[1] == 2 && bytes[3] == UI_WIDTH + 1);
	} else if (cmd == 0x2b) {
		assert(size == 4 && bytes[1] && bytes[3] == bytes[1] + 1);
		row = bytes[1] - 1;
	} else if (cmd == 0x2c) {
		assert(size == UI_WIDTH * 2 * 2 && row + 2 <= UI_HEIGHT);
		memcpy(lcd + row * UI_WIDTH * 2, bytes, size);
		transfers++;
	}
	return true;
}

static void advance(const struct ui_state *ui, unsigned int ms)
{
	for (unsigned int i = 0; i < ms; i++) {
		unsigned int before = transfers;

		assert(display_poll(ui, ++now));
		assert(transfers - before <= 1);
	}
}

static void expect_screen(const struct ui_state *ui, bool whole)
{
	static uint8_t reference[UI_BYTES];
	unsigned int start = whole ? 0 : UI_PIPE_TOP * UI_WIDTH * 2;
	unsigned int end = whole ? UI_BYTES : UI_PIPE_END * UI_WIDTH * 2;

	ui_render(reference, ui);
	assert(!memcmp(lcd + start, reference + start, end - start));
}

int main(void)
{
	struct ui_state ui = { .task = TASK_LAP, .ball = { .status = BALL_MISSING } };
	struct ball_view shown;

	assert(display_init());
	advance(&ui, 700);
	expect_screen(&ui, true);
	/* Move several times during one slow sweep of the static instruments. */
	for (unsigned int i = 0; i <= 20; i++) {
		ui.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = i * 50 };
		advance(&ui, 33);
		assert(display_last_ball(&shown));
		assert(shown.status == BALL_TRACKED && shown.permille == i * 50);
		expect_screen(&ui, false);
	}
	ui.ball = (struct ball_view){ .status = BALL_MISSING };
	advance(&ui, 33);
	expect_screen(&ui, false);
	ui.task = TASK_GRAY;
	advance(&ui, 900);
	expect_screen(&ui, true);
	assert(!display_last_ball(&shown));
	ui.task = TASK_LAP;
	ui.ball = (struct ball_view){ .status = BALL_TRACKED, .permille = 500 };
	advance(&ui, 900);
	expect_screen(&ui, true);
	/* Operator's five observed medians, independent of firmware constants. */
	struct ball_receiver rx = { .received = true, .flags = 3, .lcd_seen = true };
	const int raw[] = { 531, 400, 282, 160, 49 }, columns[] = { 8, 37, 64, 93, 120 };
	for (int page = 0; page < 2; page++) {
		ui.task = page ? TASK_BALANCE : TASK_LAP;
		for (int i = 0; i < 5; i++) {
			rx.lcd_direct = true;
			rx.lcd_x = rx.x = raw[i];
			rx.lcd_at = rx.last_ms = now;
			ui.ball = ball_display_at(&rx, now);
			advance(&ui, 650);
			for (int j = 0; j < 5; j++) {
				unsigned int offset = (50 * UI_WIDTH + columns[j]) * 2;
				bool yellow = lcd[offset] == 0xfd && lcd[offset + 1] == 0x48;
				assert(yellow == (i == j));
				unsigned int tick = (55 * UI_WIDTH + columns[j]) * 2;
				assert(lcd[tick] == 0x2e && lcd[tick + 1] == 0x7d);
				bool label = false;
				for (int y = 57; y < 64; y++) {
					for (int x = columns[j] - 5; x <= columns[j] + 5; x++) {
						unsigned int pos = (y * UI_WIDTH + x) * 2;
						label |= lcd[pos] == 0xef && lcd[pos + 1] == 0x9e;
					}
				}
				assert(label);
			}
		}
		/* A missed detection after a right-end sighting leaves a hollow
		 * amber ring at that end in the bytes transferred to the LCD. */
		rx.lcd_direct = false;
		rx.lcd_at = rx.last_ms = now;
		rx.flags = 5;
		rx.x = 135;
		ui.ball = ball_display_at(&rx, now);
		advance(&ui, 33);
		assert(display_last_ball(&shown) && shown.status == BALL_LAST_SEEN);
		const uint8_t *right = &lcd[(50 * UI_WIDTH + 120) * 2];
		assert(right[0] == 0x08 && right[1] == 0x62);
		assert(right[8] == 0xfd && right[9] == 0x48);
		rx.flags = 3;
	}
	/* A held selection key advances 1 mm per repeat, including while balancing. */
	ui.menu = (struct contest_menu){ .page = MENU_RUN, .item = EVENT_OFFSET };
	advance(&ui, 250);
	for (int mm = 1; mm <= 50; mm++) {
		ui.menu.target_mm = mm;
		advance(&ui, 50);
		int tip = -1;
		for (int x = 8; x <= 120; x++) {
			const uint8_t *p = &lcd[(96 * UI_WIDTH + x) * 2];
			if (p[0] == 0x47 && p[1] == 0xb2)
				tip = x;
		}
		int expected = 64 + (29 * mm + 25) / 50;
		assert(tip >= expected - 2 && tip <= expected + 1);
	}
	advance(&ui, 250);
	expect_screen(&ui, true);
	return 0;
}
