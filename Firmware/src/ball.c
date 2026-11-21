#include <stdlib.h>
#include <string.h>
#include "ball.h"

_Static_assert(BALL_LEFT_SPAN > BALL_CENTER_RADIUS_PX && BALL_RIGHT_SPAN > BALL_CENTER_RADIUS_PX,
	       "The marked center must lie between the two pipe ends");

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | p[1] << 8);
}

static int16_t signed_le16(const uint8_t *p)
{
	int value = le16(p);
	return (int16_t)(value >= 32768 ? value - 65536 : value);
}

bool ball_v2_measured(const struct ball_receiver *rx, uint32_t now)
{
	return rx->received && now - rx->last_ms < BALL_TIMEOUT_MS &&
	       (rx->flags & 15U) == 3U && rx->age <= 150 && rx->x <= 639;
}

static uint8_t frame_crc(const uint8_t *frame)
{
	uint8_t crc = 0;

	for (unsigned int i = 2; i < 14; i++) {
		crc ^= frame[i];
		for (unsigned int bit = 0; bit < 8; bit++)
			crc = (uint8_t)((crc << 1) ^ ((crc & 0x80U) ? 0x07U : 0));
	}
	return crc;
}

static unsigned int resync(uint8_t *window)
{
	for (unsigned int i = 1; i < BALL_FRAME_SIZE; i++) {
		if (window[i] == 0xaa &&
		    (i + 1 == BALL_FRAME_SIZE || window[i + 1] == 0x55)) {
			memmove(window, window + i, BALL_FRAME_SIZE - i);
			return BALL_FRAME_SIZE - i;
		}
	}
	return 0;
}

void ball_expect_right(struct ball_receiver *rx, uint32_t now)
{
	rx->have_anchor = true;
	rx->anchor_x = BALL_X_RIGHT;
	rx->anchor_at = now;
	rx->anchor_speed_px_s = 0;
	rx->uncertain = true;
	rx->candidate_frames = 0;
}

static void observe_lcd(struct ball_receiver *rx, uint32_t now)
{
	rx->lcd_direct = false;
	if (!ball_v2_measured(rx, now))
		return;
	/* Allow endpoint jitter while rejecting detections beyond the tube. */
	if (rx->x < RULER_X_RIGHT - 16 || rx->x > RULER_X_LEFT + 8) {
		rx->lcd_candidate_frames = 0;
		return;
	}
	if (rx->lcd_seen && abs((int)rx->x - rx->lcd_x) > 56) {
		/* The operator can relocate a stationary ball. Reacquire from
		 * neighboring direct observations without requiring it to roll. */
		if (now - rx->lcd_candidate_at > 750 || abs((int)rx->x - rx->lcd_candidate_x) > 32)
			rx->lcd_candidate_frames = 0;
		rx->lcd_candidate_x = rx->x;
		rx->lcd_candidate_at = now;
		if (++rx->lcd_candidate_frames < 3)
			return;
	}
	rx->lcd_direct = rx->lcd_seen = true;
	rx->lcd_x = rx->x;
	rx->lcd_at = now;
	rx->lcd_candidate_frames = 0;
}

/* A rolling ball may slow or stop while occluded. Confirm a local cluster
 * only ahead of a recent measured roll, within its plausible travel. */
static bool follows_roll(const struct ball_receiver *rx, int dx, uint32_t gap, uint32_t now)
{
	if (gap <= 100 || gap > 450 || rx->candidate_frames < 3 ||
	    now - rx->candidate_since < 60 || now - rx->candidate_since > 160)
		return false;
	int speed = rx->anchor_speed_px_s;
	return abs(speed) >= 60 && dx * speed > 0 && abs(dx) <= 96 &&
	       abs(dx) <= 16 + abs(speed) * (int)gap / 1000 &&
	       abs((int)rx->x - rx->candidate_start) <= 20;
}

static void observe_x(struct ball_receiver *rx, uint32_t now)
{
	if (!ball_v2_measured(rx, now))
		return;
	uint32_t gap = now - rx->anchor_at;
	int displacement = (int)rx->x - rx->anchor_x;
	/* Missing frames do not make a static reflection on the far side plausible. */
	int reach = gap <= 100 ? 16 + 2 * (int)gap : 36;
	if (reach > 110)
		reach = 110;
	if (rx->have_anchor && abs(displacement) > reach) {
		uint32_t dt = now - rx->candidate_at;
		int dx = (int)rx->x - rx->candidate_x;
		int travel = (int)rx->candidate_x - rx->candidate_start;
		if (!rx->candidate_frames || (uint16_t)(rx->sequence - rx->candidate_seq) > 3 ||
		    dt > 100 || now - rx->candidate_since > 450 || abs(dx) > 14 + 2 * (int)dt ||
		    (travel > 6 && dx < -4) || (travel < -6 && dx > 4)) {
			rx->candidate_frames = 0;
			rx->candidate_start = rx->x;
			rx->candidate_since = now;
		}
		rx->candidate_frames++;
		rx->candidate_x = rx->x;
		rx->candidate_at = now;
		rx->candidate_seq = rx->sequence;
		bool moving = rx->candidate_frames >= 5 && now - rx->candidate_since >= 95 &&
			      abs((int)rx->x - rx->candidate_start) >= 80;
		rx->uncertain = !moving && !follows_roll(rx, displacement, gap, now);
		if (rx->uncertain)
			return;
	}
	rx->anchor_speed_px_s = rx->have_anchor && gap >= 12 && gap <= 100 ?
				displacement * 1000 / (int)gap : 0;
	rx->have_anchor = true;
	rx->uncertain = false;
	rx->anchor_x = rx->x;
	rx->anchor_at = now;
	rx->candidate_frames = 0;
}

void ball_feed(struct ball_receiver *rx, uint8_t byte, uint32_t now)
{
	uint8_t *frame = rx->window;
	uint16_t sequence;

	if (!rx->used && byte != 0xaa)
		return;
	if (rx->used == 1 && byte != 0x55) {
		rx->used = byte == 0xaa;
		return;
	}
	frame[rx->used++] = byte;
	if (rx->used < BALL_FRAME_SIZE)
		return;
	if (frame[15] != 0x17 || frame[2] != 2 || frame[14] != frame_crc(frame)) {
		rx->rejected++;
		rx->used = resync(frame);
		return;
	}
	rx->used = 0;
	rx->frames++;
	sequence = le16(frame + 4);
	if (rx->received && sequence == rx->sequence)
		return;
	rx->last_ms = now;
	rx->received = true;
	rx->flags = frame[3];
	rx->sequence = sequence;
	rx->x = le16(frame + 6);
	rx->position_centi_cm = signed_le16(frame + 8);
	rx->speed_deci_cm_s = signed_le16(frame + 10);
	rx->age = le16(frame + 12);
	observe_lcd(rx, now);
	observe_x(rx, now);
}

static struct ball_view position_view(enum ball_status status, uint16_t x)
{
	struct ball_view view = {
		.status = status, .offset_px = BALL_RIGHT_SIGN * ((int)x - BALL_X_CENTER) / 2,
	};
	int distance = abs(view.offset_px);
	int span = view.offset_px < 0 ? BALL_LEFT_SPAN : BALL_RIGHT_SPAN;
	view.permille = 500;
	if (distance > BALL_CENTER_RADIUS_PX) {
		int scaled = distance >= span ? 500 :
			(distance - BALL_CENTER_RADIUS_PX) * 500 / (span - BALL_CENTER_RADIUS_PX);
		view.permille = view.offset_px < 0 ? 500 - scaled : 500 + scaled;
	}
	return view;
}

struct ball_view ball_view_at(const struct ball_receiver *rx, uint32_t now)
{
	if (!rx->received || now - rx->last_ms >= BALL_TIMEOUT_MS)
		return (struct ball_view){ .status = BALL_OFFLINE };
	/* A removed ball can still produce predicted X values. */
	if (!ball_v2_measured(rx, now))
		return (struct ball_view){ .status = BALL_MISSING };
	if (rx->uncertain)
		return (struct ball_view){ .status = BALL_UNCERTAIN };
	return position_view(BALL_TRACKED, rx->x);
}

struct ball_view ball_display_at(const struct ball_receiver *rx, uint32_t now)
{
	if (!rx->received || now - rx->last_ms >= BALL_TIMEOUT_MS)
		return (struct ball_view){ .status = BALL_OFFLINE };
	if (rx->lcd_seen && now - rx->lcd_at < BALL_DISPLAY_LAST_MS)
		return (struct ball_view){
			.status = rx->lcd_direct ? BALL_TRACKED : BALL_LAST_SEEN,
			.permille = ball_ruler_position(rx->lcd_x),
			.offset_px = RULER_X_O - (int)rx->lcd_x,
		};
	return (struct ball_view){ .status = ball_v2_measured(rx, now) ? BALL_UNCERTAIN : BALL_MISSING };
}
