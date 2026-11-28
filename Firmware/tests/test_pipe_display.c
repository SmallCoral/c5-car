/* 截取 LCD 像素传输，核验实时长度条、目标和失测标记的显示。 */

#include <assert.h>
#include <string.h>
#include "board.h"
#include "display.h"

static uint8_t lcd[UI_BYTES];
static uint8_t cmd;
static unsigned int row;
static unsigned int transfers;
static uint32_t now;

/* 记录背光控制以代替实物引脚。 */
void board_lcd_backlight(bool on) { (void)on; }
/* 模拟屏幕复位时序。 */
void board_lcd_reset(bool high) { (void)high; }
/* 在主机测试中代替实际延时。 */
void board_delay(uint32_t ms) { now += ms; }

/* 捕获发给 LCD 的命令和像素以还原屏幕。 */
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

/* 按模拟时钟推进局部刷新。 */
static void advance(const struct ui_state *ui, unsigned int ms)
{
	for (unsigned int i = 0; i < ms; i++) {
		unsigned int before = transfers;

		assert(display_poll(ui, ++now));
		assert(transfers - before <= 1);
	}
}

/* 将捕获的像素与预期屏幕对照。 */
static void expect_screen(const struct ui_state *ui, bool whole)
{
	static uint8_t reference[UI_BYTES];
	unsigned int start = whole ? 0 : UI_PIPE_TOP * UI_WIDTH * 2;
	unsigned int end = whole ? UI_BYTES : UI_PIPE_END * UI_WIDTH * 2;

	ui_render(reference, ui);
	assert(!memcmp(lcd + start, reference + start, end - start));
}

/* 检查球位、目标和失测标记能在慢速整页扫描间及时更新。 */
int main(void)
{
	struct ui_state ui = { .task = TASK_LAP, .ball = { .status = BALL_MISSING } };
	struct ball_view shown;

	assert(display_init());
	advance(&ui, 700);
	expect_screen(&ui, true);
	/* 在静态仪表完成一次慢扫描之前，让球位移动多次。 */
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
	/* 五个位置取自操作者的观测中位数，独立于固件内的标定常量。 */
	struct ball_receiver rx = { .received = true, .flags = 3, .lcd_seen = true };
	const int raw[] = { 521, 388, 253, 128, 24 }, columns[] = { 8, 36, 64, 95, 120 };
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
		/* 右端失测后，传到 LCD 的像素应在上次位置显示琥珀色空心圆。 */
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
	/* 长按选点每次重复移动 1 mm，原地稳球时也适用。 */
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
		int expected = columns[2] + ((columns[3] - columns[2]) * mm + 25) / 50;
		assert(tip >= expected - 2 && tip <= expected + 1);
	}
	advance(&ui, 250);
	expect_screen(&ui, true);
	return 0;
}
