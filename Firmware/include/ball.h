#ifndef CORAL_BALL_H
#define CORAL_BALL_H

#include <stdbool.h>
#include <stdint.h>

#define BALL_FRAME_SIZE 16U
#define BALL_TIMEOUT_MS 200U
#define BALL_DISPLAY_LAST_MS 2000U

enum ball_status { BALL_OFFLINE, BALL_MISSING, BALL_TRACKED, BALL_UNCERTAIN, BALL_LAST_SEEN };
enum {
	/* Position-loop offsets retain the 320-pixel controller scale. */
	BALL_X_LEFT = 531, BALL_X_CENTER = 282, BALL_X_RIGHT = 49, BALL_CENTER_RADIUS_PX = 1,
	BALL_RIGHT_SIGN = BALL_X_RIGHT > BALL_X_LEFT ? 1 : -1,
	BALL_LEFT_SPAN = BALL_RIGHT_SIGN * (BALL_X_CENTER - BALL_X_LEFT) / 2,
	BALL_RIGHT_SPAN = BALL_RIGHT_SIGN * (BALL_X_RIGHT - BALL_X_CENTER) / 2,
};

enum {
	RULER_X_LEFT = BALL_X_LEFT, RULER_X_MINUS_5 = 400, RULER_X_O = BALL_X_CENTER,
	RULER_X_PLUS_5 = 160, RULER_X_RIGHT = BALL_X_RIGHT,
};

/* Each half ends at its measured stop, with O at the center of the LCD. */
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
	int16_t offset_px; /* Relative to the observed midpoint, positive toward the physical right. */
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

void ball_feed(struct ball_receiver *rx, uint8_t byte, uint32_t now);
void ball_expect_right(struct ball_receiver *rx, uint32_t now);
bool ball_v2_measured(const struct ball_receiver *rx, uint32_t now);
struct ball_view ball_view_at(const struct ball_receiver *rx, uint32_t now);
struct ball_view ball_display_at(const struct ball_receiver *rx, uint32_t now);

#endif
