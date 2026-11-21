#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "ball.h"

/* Frames captured while the ball was moved along the fitted tube. */
static const uint8_t samples[][BALL_FRAME_SIZE] = {
	{ 0xaa,0x55,2,3,0x2c,0x95,44,0,0x78,0xfb,0xd3,0xff,0,0,0x31,0x17 },
	{ 0xaa,0x55,2,3,0xad,0x95,146,0,0x73,0xff,9,0,0,0,0,0x17 },
	{ 0xaa,0x55,2,3,0x1a,0x96,0x1a,1,0xc2,4,0x18,0,0,0,0xf6,0x17 },
	{ 0xaa,0x55,2,5,0x7c,0x95,0x89,0,0x17,0xff,0x7d,0,0x11,0,0x29,0x17 },
	{ 0xaa,0x55,2,0,0x87,0x91,150,0,0x9c,0xff,0,0,0xff,0xff,0x87,0x17 },
};

static void feed(struct ball_receiver *rx, const uint8_t *bytes, size_t size,
		 uint32_t now)
{
	for (size_t i = 0; i < size; i++)
		ball_feed(rx, bytes[i], now);
}

static struct ball_view packet(struct ball_receiver *rx, uint16_t sequence, uint16_t x, uint32_t now,
			       uint8_t flags)
{
	uint8_t p[BALL_FRAME_SIZE] = { 0xaa, 0x55, 2, flags, sequence & 255, sequence >> 8,
		x & 255, x >> 8, 0, 0, 0, 0, 0, 0, 0, 0x17 };
	for (unsigned int i = 2; i < 14; i++) {
		p[14] ^= p[i];
		for (unsigned int bit = 0; bit < 8; bit++)
			p[14] = (uint8_t)((p[14] << 1) ^ ((p[14] & 128U) ? 7U : 0U));
	}
	feed(rx, p, sizeof(p), now);
	return ball_view_at(rx, now);
}

static struct ball_view point(struct ball_receiver *rx, uint16_t sequence, uint16_t x, uint32_t now)
{
	return packet(rx, sequence, x, now, 51);
}

static void test_reacquire_rolling_ball(void)
{
	/* From the 4.746 s vision stop: the ball slowed down near the left mark
	 * while detections were missing. The returning cluster moved only 3 px. */
	const int x[] = {324,334,341,351,359,368,377,386,350,390,390,390,393};
	const uint8_t flags[] = {3,5,3,5,5,5,5,5,0,5,3,3,3};
	const unsigned int ms[] = {4176,4210,4244,4279,4310,4344,4378,4412,4446,4478,4512,4546,4579};
	struct ball_receiver rx = {0};
	for (unsigned int i = 0; i < sizeof(flags); i++) {
		enum ball_status s = packet(&rx, 10950 + i, x[i], ms[i], flags[i]).status;
		assert(s == (flags[i] != 3 ? BALL_MISSING : i == 10 || i == 11 ? BALL_UNCERTAIN : BALL_TRACKED));
	}
	/* A stationary anchor, or a jump against the recent roll, cannot
	 * validate a nearby stationary reflection after a gap. */
	for (int dx = 0; dx <= 20; dx += 20) {
		rx = (struct ball_receiver){0};
		point(&rx, 1, 341 - dx, 100);
		point(&rx, 2, 341, 160);
		for (int i = 0; i < 5; i++)
			assert(point(&rx, 9 + i, dx ? 290 : 390, 405 + i * 33).status == BALL_UNCERTAIN);
	}
}

static void test_recalibrated_right(void)
{
	struct ball_receiver rx = {0};
	ball_expect_right(&rx, 70);
	assert(point(&rx, 1, 49, 100).permille == 1000);
	assert(point(&rx, 2, 51, 133).offset_px > 0);
	/* Parked shaft, real ball at the right stop: the camera jumped to 315
	 * and kept reporting it as direct. Time alone cannot validate it. */
	for (int i = 3; i < 65; i++)
		assert(point(&rx, i, 529 + i % 2, i * 33 + 67).status == BALL_UNCERTAIN);
	assert(point(&rx, 65, 49, 2212).status == BALL_TRACKED);
	for (unsigned int i = 1; i <= 15; i++) {
		struct ball_view v = point(&rx, 65 + i, 42 + i * 16, 2212 + i * 33);
		assert(v.status == BALL_TRACKED && v.offset_px == 120 - (int)i * 8);
	}
	assert(ball_view_at(&rx, 2707).permille == 500);
	/* A coherent moving track can also be reacquired after an occlusion. */
	for (int i = 0; i < 5; i++) {
		struct ball_view v = point(&rx, 100 + i, 530 - 22 * i, 4000 + i * 33);
		assert(v.status == (i == 4 ? BALL_TRACKED : BALL_UNCERTAIN));
	}
}

static void test_last_seen_on_lcd(void)
{
	struct ball_receiver rx = {0};
	uint32_t start = UINT32_MAX - 100;

	ball_expect_right(&rx, start);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start);
	assert(ball_display_at(&rx, start).status == BALL_MISSING);
	assert(point(&rx, 1, 49, start + 10).permille == 1000);
	assert(ball_display_at(&rx, start + 10).status == BALL_TRACKED);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start + 50);
	assert(ball_view_at(&rx, start + 50).status == BALL_MISSING);
	struct ball_view last = ball_display_at(&rx, start + 50);
	assert(last.status == BALL_LAST_SEEN && last.permille == 1000);
	assert(ball_display_at(&rx, start + 50 + BALL_TIMEOUT_MS).status == BALL_OFFLINE);
	feed(&rx, samples[4], BALL_FRAME_SIZE, start + 10 + BALL_DISPLAY_LAST_MS - 1);
	assert(ball_display_at(&rx, start + 10 + BALL_DISPLAY_LAST_MS - 1).status == BALL_LAST_SEEN);
	assert(ball_display_at(&rx, start + 10 + BALL_DISPLAY_LAST_MS).status == BALL_MISSING);
	point(&rx, 2, 49, start + 2200);
	point(&rx, 3, 620, start + 2233);
	last = ball_display_at(&rx, start + 2233);
	assert(last.status == BALL_LAST_SEEN && last.permille == 1000);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start + 2266);
	assert(ball_display_at(&rx, start + 2266).status == BALL_LAST_SEEN);
	point(&rx, 4, 620, start + 4200);
	assert(ball_display_at(&rx, start + 4200).status == BALL_UNCERTAIN);
}

static void test_manual_ruler(void)
{
	struct ball_receiver rx = {0};
	const int marks[] = {531, 400, 282, 160, 49};
	const int positions[] = {0, 263, 500, 762, 1000};
	unsigned int seq = 1;

	point(&rx, seq, 49, 33);
	for (unsigned int i = 0; i < 5; i++) {
		for (unsigned int n = 0; n < 3; n++) {
			seq++;
			point(&rx, seq, marks[i], seq * 33);
			/* A duplicate packet must not count toward confirmation. */
			point(&rx, seq, marks[i], seq * 33 + 1);
			struct ball_view lcd = ball_display_at(&rx, seq * 33 + 1);
			assert(lcd.status == (n == 2 ? BALL_TRACKED : BALL_LAST_SEEN));
			if (n == 2)
				assert(lcd.permille == positions[i]);
		}
		if (!i)
			assert(ball_view_at(&rx, seq * 33 + 1).status == BALL_UNCERTAIN);
	}
	for (int x = 1; x <= 639; x++)
		assert(ball_ruler_position(x) <= ball_ruler_position(x - 1));
}

int main(void)
{
	/* Camera V2 example from send_v2; MEASURED may have nonzero age. */
	const uint8_t packet[] = {0xaa,0x55,2,3,1,0,0x30,1,0x64,0,0x64,0,10,0,0x13,0x17};
	struct ball_receiver v2 = {0};
	feed(&v2, packet, sizeof(packet), 200);
	assert(ball_v2_measured(&v2, 200) && v2.x == 304 && v2.age == 10);
	assert(v2.position_centi_cm == 100 && v2.speed_deci_cm_s == 100);
	point(&v2, 2, 639, 220);
	assert(ball_v2_measured(&v2, 220));
	point(&v2, 3, 640, 240);
	assert(!ball_v2_measured(&v2, 240));
	test_recalibrated_right();
	test_reacquire_rolling_ball();
	test_last_seen_on_lcd();
	test_manual_ruler();
	struct ball_receiver rx = { 0 };
	uint8_t damaged[BALL_FRAME_SIZE];
	uint32_t now = UINT32_MAX - 50;
	struct ball_view view;

	assert(ball_view_at(&rx, now).status == BALL_OFFLINE);
	/* Noise, an overlapping header, then a complete frame split across polls. */
	ball_feed(&rx, 0x17, now);
	ball_feed(&rx, 0xaa, now);
	feed(&rx, samples[0], 7, now);
	assert(!rx.received);
	feed(&rx, samples[0] + 7, 9, now);
	view = ball_view_at(&rx, now);
	assert(view.status == BALL_TRACKED && rx.x == 44);
	memcpy(damaged, samples[2], sizeof(damaged));
	damaged[6] ^= 1;
	feed(&rx, damaged, sizeof(damaged), now);
	assert(rx.frames == 1 && rx.rejected == 1 && rx.x == 44);
	/* A truncated packet must not consume the next valid frame. */
	feed(&rx, samples[2], 9, now);
	feed(&rx, samples[1], BALL_FRAME_SIZE, now);
	assert(rx.frames == 2 && rx.rejected == 2);
	assert(rx.x == 146 && ball_view_at(&rx, now).status == BALL_UNCERTAIN);
	rx.uncertain = false; /* Independent checks of the calibrated scale. */
	for (int offset = -BALL_CENTER_RADIUS_PX; offset <= BALL_CENTER_RADIUS_PX; offset++) {
		rx.x = BALL_X_CENTER + BALL_RIGHT_SIGN * offset * 2;
		view = ball_view_at(&rx, now);
		assert(view.permille == 500 && view.offset_px == offset);
	}
	rx.x = BALL_X_RIGHT;
	assert(ball_view_at(&rx, now).permille == 1000);
	rx.x = BALL_X_LEFT;
	assert(ball_view_at(&rx, now).permille == 0);
	rx.x = (BALL_X_CENTER + BALL_X_RIGHT) / 2;
	assert(ball_view_at(&rx, now).permille > 735 && ball_view_at(&rx, now).permille < 765);
	rx.x = (BALL_X_CENTER + BALL_X_LEFT) / 2;
	assert(ball_view_at(&rx, now).permille > 235 && ball_view_at(&rx, now).permille < 265);
	feed(&rx, samples[1], BALL_FRAME_SIZE, now + 100);
	assert(ball_view_at(&rx, now + BALL_TIMEOUT_MS).status == BALL_OFFLINE);
	feed(&rx, samples[2], BALL_FRAME_SIZE, now + 250);
	view = ball_view_at(&rx, now + 251);
	assert(view.status == BALL_UNCERTAIN && rx.x == 282);
	rx.uncertain = false;
	assert(ball_view_at(&rx, now + 251).permille == 500);
	rx.x = 290;
	assert(ball_view_at(&rx, now + 251).status == BALL_TRACKED);
	for (int x = 1; x < 639; x++) {
		rx.x = (uint16_t)x;
		unsigned int previous = ball_view_at(&rx, now + 251).permille;
		rx.x += BALL_RIGHT_SIGN;
		assert(ball_view_at(&rx, now + 251).permille >= previous);
	}
	for (unsigned int i = 3; i < 5; i++) {
		feed(&rx, samples[i], BALL_FRAME_SIZE, now + 252);
		assert(rx.x && ball_view_at(&rx, now + 252).status == BALL_MISSING);
	}
	return 0;
}
