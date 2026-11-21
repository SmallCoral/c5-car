/* 界面快照及绘制接口，采样、控制、绘图之间通过此结构传递同一时刻的显示值。 */

#ifndef CORAL_UI_H
#define CORAL_UI_H

#include "logic.h"
#include "contest.h"
#include "ball.h"
#include "grayscale.h"
#include "lap.h"

#define UI_WIDTH 128
#define UI_HEIGHT 160
#define UI_BYTES (UI_WIDTH * UI_HEIGHT * 2)
#define UI_PIPE_TOP 36U
#define UI_PIPE_END 56U

struct ui_state {
	struct contest_menu menu;
	enum position_leg leg;
	uint32_t event_ms;
	int ball_mm;
	bool position_seen;
	bool waiting_vision;
	bool level_ready;
	bool offset_ready;
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
	struct ball_view ball;
	const char *balance_status;
	bool balance_reverse;
	int balance_shaft, balance_target;
	bool balance_speed_mode;
	bool balance_jog;
	bool calibrating, shaft_referenced;
	int cal_camera_x;
	int32_t cal_encoder;
	int balance_rpm;
	uint32_t balance_time;
	bool balance_timed;
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

/* 根据当前任务选择页面，并绘制完整界面。 */
void ui_render(uint8_t *pixels, const struct ui_state *state);
/* 将页面裁切到指定行，供屏幕按小块传输。 */
void ui_render_rows(uint8_t *pixels, const struct ui_state *state,
		    unsigned int first, unsigned int rows);
/* 仅重画给定行范围内的实时球位条。 */
void ui_render_pipe_rows(uint8_t *pixels, const struct ball_view *ball,
			 unsigned int first, unsigned int rows);

#endif
