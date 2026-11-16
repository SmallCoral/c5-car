#include "board.h"
#include "config.h"
#include "display.h"
#include "logic.h"

static uint8_t framebuffer[UI_BYTES];
static uint32_t frame_time;
static uint32_t stripe_time;
static unsigned int next_y = UI_HEIGHT;
static bool ready;

static bool command(uint8_t cmd, const uint8_t *data, size_t length)
{
	return board_lcd_write(false, &cmd, 1) &&
	       (!length || board_lcd_write(true, data, length));
}

bool display_init(void)
{
	/* ZJY180S0800TG01 supplier initialization, portrait.
	 * User verified that 0xc0 was upside down in this mounting orientation.
	 */
	static const uint8_t init[] = {
		0xb1, 3, 0x05, 0x3c, 0x3c,
		0xb2, 3, 0x05, 0x3c, 0x3c,
		0xb3, 6, 0x05, 0x3c, 0x3c, 0x05, 0x3c, 0x3c,
		0xb4, 1, 0x03,
		0xc0, 3, 0x28, 0x08, 0x04,
		0xc1, 1, 0xc0,
		0xc2, 2, 0x0d, 0x00,
		0xc3, 2, 0x8d, 0x2a,
		0xc4, 2, 0x8d, 0xee,
		0xc5, 1, 0x1a,
		0x36, 1, LCD_MADCTL,
		0xe0, 16, 0x04, 0x22, 0x07, 0x0a, 0x2e, 0x30, 0x25, 0x2a,
			  0x28, 0x26, 0x2e, 0x3a, 0x00, 0x01, 0x03, 0x13,
		0xe1, 16, 0x04, 0x16, 0x06, 0x0d, 0x2d, 0x26, 0x23, 0x27,
			  0x27, 0x25, 0x2d, 0x3b, 0x00, 0x01, 0x04, 0x13,
		0x3a, 1, 0x05,
		0x29, 0
	};
	size_t index = 0;

	board_lcd_backlight(false);
	board_lcd_reset(false);
	board_delay(100);
	board_lcd_reset(true);
	board_delay(100);
	if (!command(0x11, NULL, 0))
		return false;
	board_delay(120);
	while (index < sizeof(init)) {
		uint8_t cmd = init[index++];
		uint8_t length = init[index++];

		if (!command(cmd, &init[index], length))
			return false;
		index += length;
	}
	ready = true;
	return true;
}

bool display_poll(const struct ui_state *state, uint32_t now)
{
	uint8_t columns[] = { 0, 2, 0, 129 };
	uint8_t rows[] = { 0, 0, 0, 0 };

	if (!ready)
		return false;
	if (next_y == UI_HEIGHT) {
		if (!elapsed(now, frame_time, 200))
			return true;
		ui_render(framebuffer, state);
		frame_time = now;
		next_y = 0;
	}
	if (!elapsed(now, stripe_time, 5))
		return true;
	stripe_time = now;
	rows[1] = (uint8_t)(next_y + 1);
	rows[3] = (uint8_t)(next_y + 8);
	if (!command(0x2a, columns, sizeof(columns)) ||
	    !command(0x2b, rows, sizeof(rows)) ||
	    !command(0x2c, &framebuffer[next_y * UI_WIDTH * 2], UI_WIDTH * 8 * 2)) {
		ready = false;
		board_lcd_backlight(false);
		return false;
	}
	next_y += 8;
	if (next_y == UI_HEIGHT)
		board_lcd_backlight(true);
	return true;
}
