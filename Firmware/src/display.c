/* LCD 传输调度。先由 UI 生成一小段像素，再写屏幕窗口，避免整屏传输占满主循环；长度条还可独立刷新。 */

#include "board.h"
#include "config.h"
#include "display.h"
#include "logic.h"

static uint8_t framebuffer[UI_BYTES];
static struct ui_state frame_state;
static uint32_t frame_time;
static uint32_t stripe_time;
static unsigned int next_y = UI_HEIGHT;
static bool ready;
static struct ball_view pipe_frame, pipe_shown;
static unsigned int pipe_y = UI_PIPE_END;
static uint32_t pipe_time, pipe_updates;
static bool pipe_valid;

/* 向 LCD 发送一个命令及其随附的参数字节。 */
static bool command(uint8_t cmd, const uint8_t *data, size_t length)
{
	return board_lcd_write(false, &cmd, 1) &&
	       (!length || board_lcd_write(true, data, length));
}

/* 按厂家时序复位屏幕并配置扫描方向、像素格式和显示窗口。 */
bool display_init(void)
{
	/* 按厂家竖屏序列初始化 ZJY180S0800TG01。实车确认当前安装方向使用 0xc0 会倒置。 */
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

/* 设置行窗口并将对应的 RGB565 像素传给 LCD。 */
static bool write_rows(unsigned int y)
{
	uint8_t columns[] = { 0, 2, 0, 129 };
	uint8_t rows[] = { 0, (uint8_t)(y + 1), 0, (uint8_t)(y + 2) };

	if (command(0x2a, columns, sizeof(columns)) &&
	    command(0x2b, rows, sizeof(rows)) &&
	    command(0x2c, &framebuffer[y * UI_WIDTH * 2], UI_WIDTH * 2 * 2))
		return true;
	ready = false;
	board_lcd_backlight(false);
	return false;
}

/* 读取最近实际绘到长度条上的球位快照和更新计数。 */
uint32_t display_last_ball(struct ball_view *shown)
{
	*shown = pipe_shown;
	return pipe_valid ? pipe_updates : 0;
}

/* 优先刷新实时长度条所在的行，减少球位显示延迟。 */
static bool poll_pipe(const struct ball_view *ball, uint32_t now)
{
	if (!elapsed(now, pipe_time, 2))
		return true;
	if (pipe_y == UI_PIPE_END) {
		if (pipe_valid && ball->status == pipe_shown.status &&
		    ball->permille == pipe_shown.permille)
			return true;
		pipe_frame = *ball;
		pipe_y = UI_PIPE_TOP;
	}
	pipe_time = now;
	ui_render_pipe_rows(framebuffer, &pipe_frame, pipe_y, 2);
	if (!write_rows(pipe_y))
		return false;
	pipe_y += 2;
	if (pipe_y == UI_PIPE_END) {
		pipe_shown = pipe_frame;
		pipe_updates++;
		pipe_valid = true;
	}
	return true;
}

/* 判断当前页面是否有需要单独更新的长度条。 */
static bool shows_pipe(const struct ui_state *ui)
{
	return ui->menu.page == MENU_DIAGNOSTIC && !ui->updating && !ui->calibrating &&
	       (ui->task == TASK_LAP || ui->task == TASK_BALANCE);
}

/* 分次取两行进行绘制和传输，完成当前快照后再换到新状态。 */
bool display_poll(const struct ui_state *state, uint32_t now)
{
	bool lap_page;

	if (!ready)
		return false;
	bool menu_change = state->menu.page != frame_state.menu.page ||
			   state->menu.item != frame_state.menu.item || state->menu.tool != frame_state.menu.tool ||
			   state->menu.notice != frame_state.menu.notice ||
			   state->menu.offset_driving != frame_state.menu.offset_driving;
	/* 即使长按每 50 ms 改一次目标，也要把当前轮的屏幕扫描画完。 */
	if (menu_change || (next_y == UI_HEIGHT && (state->menu.target_mm != frame_state.menu.target_mm ||
						    elapsed(now, frame_time, 200)))) {
		frame_state = *state;
		frame_time = now;
		next_y = 0;
		if (!shows_pipe(state)) {
			pipe_y = UI_PIPE_END;
			pipe_valid = false;
		}
	}
	/* LAP 页的这些行由实时球位条独占，慢速整页扫描须避开，防止用旧快照覆盖球位。 */
	lap_page = shows_pipe(&frame_state);
	if (lap_page && next_y == UI_PIPE_TOP)
		next_y = UI_PIPE_END;
	if (next_y == UI_HEIGHT || !elapsed(now, stripe_time, frame_state.menu.page == MENU_DIAGNOSTIC ? 5 : 1)) {
		if (lap_page && shows_pipe(state))
			return poll_pipe(&state->ball, now);
		return true;
	}
	stripe_time = now;
	/* 每次只传两行，主循环才能在短暂经过 A 横线时多次采样。 */
	ui_render_rows(framebuffer, &frame_state, next_y, 2);
	if (!write_rows(next_y))
		return false;
	next_y += 2;
	if (next_y == UI_HEIGHT)
		board_lcd_backlight(true);
	return true;
}
