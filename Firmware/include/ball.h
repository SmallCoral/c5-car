/* MaixCAM2 的接收状态、实测年龄和长度尺标定。控制器与显示各有专门的球位读取入口。 */

#ifndef CORAL_BALL_H
#define CORAL_BALL_H

#include <stdbool.h>
#include <stdint.h>

#define BALL_FRAME_SIZE 16U
#define BALL_TIMEOUT_MS 200U
#define BALL_DISPLAY_LAST_MS 2000U

enum ball_status { BALL_OFFLINE, BALL_MISSING, BALL_TRACKED, BALL_UNCERTAIN, BALL_LAST_SEEN };
enum {
	/* 位置闭环偏移沿用宽 320 像素的控制坐标。 */
	BALL_X_LEFT = 521, BALL_X_CENTER = 253, BALL_X_RIGHT = 24, BALL_CENTER_RADIUS_PX = 1,
	BALL_RIGHT_SIGN = BALL_X_RIGHT > BALL_X_LEFT ? 1 : -1,
	BALL_LEFT_SPAN = BALL_RIGHT_SIGN * (BALL_X_CENTER - BALL_X_LEFT) / 2,
	BALL_RIGHT_SPAN = BALL_RIGHT_SIGN * (BALL_X_RIGHT - BALL_X_CENTER) / 2,
};

enum {
	RULER_X_LEFT = BALL_X_LEFT, RULER_X_MINUS_5 = 388, RULER_X_O = BALL_X_CENTER,
	RULER_X_PLUS_5 = 128, RULER_X_RIGHT = BALL_X_RIGHT,
};

/* 用左右各自的 50 mm 标定跨度，把目标毫米换回原图像素，与实测球位共用屏幕刻度。 */
static inline int ball_x_from_mm(int mm)
{
	int span = mm < 0 ? RULER_X_MINUS_5 - RULER_X_O : RULER_X_O - RULER_X_PLUS_5;
	return RULER_X_O - (mm * span + (mm < 0 ? -25 : 25)) / 50;
}

/* 按 O 和两侧实测挡杆分段，将原始像素映射为长度条的相对位置。 */
static inline int ball_ruler_position(int x)
{
	if (x >= RULER_X_LEFT)
		return 0;
	if (x <= RULER_X_RIGHT)
		return 1000;
	if (x > RULER_X_O) {
		const int span = RULER_X_LEFT - RULER_X_O;
		return ((RULER_X_LEFT - x) * 500 + span / 2) / span;
	}
	const int span = RULER_X_O - RULER_X_RIGHT;
	return 500 + ((RULER_X_O - x) * 500 + span / 2) / span;
}

struct ball_view {
	enum ball_status status;
	uint16_t permille;
	int16_t offset_px; /* 以实测中点为零，向实物右侧为正。 */
};

struct ball_receiver {
	uint8_t window[BALL_FRAME_SIZE];
	unsigned int used;
	uint32_t frames;
	uint32_t rejected;
	uint32_t last_ms;
	uint16_t sequence;
	uint16_t x;
	int16_t position_centi_cm, speed_deci_cm_s;
	uint16_t age;
	uint8_t flags;
	bool received;
	bool have_anchor, uncertain;
	uint16_t anchor_x, candidate_x, candidate_start, candidate_seq;
	int anchor_speed_px_s;
	unsigned int candidate_frames;
	uint32_t anchor_at, candidate_at, candidate_since;
	bool lcd_seen, lcd_direct;
	uint16_t lcd_x, lcd_candidate_x;
	unsigned int lcd_candidate_frames;
	uint32_t lcd_at, lcd_candidate_at;
};

/* 逐字节寻找帧头、验证长度和校验，再解析球位及其状态。 */
void ball_feed(struct ball_receiver *rx, uint8_t byte, uint32_t now);
/* 按实物右侧挡杆建立接下来的球位确认条件。 */
void ball_expect_right(struct ball_receiver *rx, uint32_t now);
/* 检查 V2 的实测标志与数据年龄，判断能否用作本次直接观测。 */
bool ball_v2_measured(const struct ball_receiver *rx, uint32_t now);
/* 根据最后收包及观测时间，返回闭环可使用的球位状态。 */
struct ball_view ball_view_at(const struct ball_receiver *rx, uint32_t now);
/* 返回屏幕使用的球位与可见性，供绘制最后观测标记。 */
struct ball_view ball_display_at(const struct ball_receiver *rx, uint32_t now);

#endif
