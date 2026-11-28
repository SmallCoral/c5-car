/* 构造相机真实或损坏的字节序列，检查重捕获、观测年龄和长度条标定。 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "ball.h"

/* 这些帧取自钢球沿实车管子移动时的采集记录。 */
static const uint8_t samples[][BALL_FRAME_SIZE] = {
	{ 0xaa,0x55,2,3,0x2c,0x95,44,0,0x78,0xfb,0xd3,0xff,0,0,0x31,0x17 },
	{ 0xaa,0x55,2,3,0xad,0x95,146,0,0x73,0xff,9,0,0,0,0,0x17 },
	{ 0xaa,0x55,2,3,0x1a,0x96,0x1a,1,0xc2,4,0x18,0,0,0,0xf6,0x17 },
	{ 0xaa,0x55,2,5,0x7c,0x95,0x89,0,0x17,0xff,0x7d,0,0x11,0,0x29,0x17 },
	{ 0xaa,0x55,2,0,0x87,0x91,150,0,0x9c,0xff,0,0,0xff,0xff,0x87,0x17 },
};

/* 将测试报文逐字节送入相机接收器。 */
static void feed(struct ball_receiver *rx, const uint8_t *bytes, size_t size,
		 uint32_t now)
{
	for (size_t i = 0; i < size; i++)
		ball_feed(rx, bytes[i], now);
}

/* 按相机协议构造一帧带序号与校验的测试数据。 */
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

/* 注入给定位置的测试观测。 */
static struct ball_view point(struct ball_receiver *rx, uint16_t sequence, uint16_t x, uint32_t now)
{
	return packet(rx, sequence, x, now, 51);
}

/* 回放滚动、遮挡和重新出现的观测，核对何时恢复跟踪。 */
static void test_reacquire_rolling_ball(void)
{
	/* 在 4.746 s 因视觉停车的实测中，球在左标记附近丢测并减速，重新检测到的一簇位置只相差 3 像素。 */
	const int x[] = {324,334,341,351,359,368,377,386,350,390,390,390,393};
	const uint8_t flags[] = {3,5,3,5,5,5,5,5,0,5,3,3,3};
	const unsigned int ms[] = {4176,4210,4244,4279,4310,4344,4378,4412,4446,4478,4512,4546,4579};
	struct ball_receiver rx = {0};
	for (unsigned int i = 0; i < sizeof(flags); i++) {
		enum ball_status s = packet(&rx, 10950 + i, x[i], ms[i], flags[i]).status;
		assert(s == (flags[i] != 3 ? BALL_MISSING : i == 10 || i == 11 ? BALL_UNCERTAIN : BALL_TRACKED));
	}
	/* 静止的起点或与最近滚动方向相反的跳点，均不足以在失测后确认一个静态反光点。 */
	for (int dx = 0; dx <= 20; dx += 20) {
		rx = (struct ball_receiver){0};
		point(&rx, 1, 341 - dx, 100);
		point(&rx, 2, 341, 160);
		for (int i = 0; i < 5; i++)
			assert(point(&rx, 9 + i, dx ? 290 : 390, 405 + i * 33).status == BALL_UNCERTAIN);
	}
}

/* 按右侧实物标定复现端点与反光跳点。 */
static void test_recalibrated_right(void)
{
	struct ball_receiver rx = {0};
	ball_expect_right(&rx, 70);
	assert(point(&rx, 1, BALL_X_RIGHT, 100).permille == 1000);
	assert(point(&rx, 2, 51, 133).offset_px > 0);
	/* 转轴抱停、球贴右挡杆时，相机曾跳到 315 并持续标记为直接观测。持续时间不能单独确认球位。 */
	for (int i = 3; i < 65; i++)
		assert(point(&rx, i, 529 + i % 2, i * 33 + 67).status == BALL_UNCERTAIN);
	assert(point(&rx, 65, 49, 2212).status == BALL_TRACKED);
	for (unsigned int i = 1; i <= 15; i++) {
		struct ball_view v = point(&rx, 65 + i, BALL_X_CENTER - 240 + i * 16, 2212 + i * 33);
		assert(v.status == BALL_TRACKED && v.offset_px == 120 - (int)i * 8);
	}
	assert(ball_view_at(&rx, 2707).permille == 500);
	/* 遮挡结束后，沿原方向连贯移动的球应能恢复跟踪。 */
	for (int i = 0; i < 5; i++) {
		struct ball_view v = point(&rx, 100 + i, 530 - 22 * i, 4000 + i * 33);
		assert(v.status == (i == 4 ? BALL_TRACKED : BALL_UNCERTAIN));
	}
}

/* 检查短暂漏检时屏幕保留最后位置的时效。 */
static void test_last_seen_on_lcd(void)
{
	struct ball_receiver rx = {0};
	uint32_t start = UINT32_MAX - 100;

	ball_expect_right(&rx, start);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start);
	assert(ball_display_at(&rx, start).status == BALL_MISSING);
	assert(point(&rx, 1, BALL_X_RIGHT, start + 10).permille == 1000);
	assert(ball_display_at(&rx, start + 10).status == BALL_TRACKED);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start + 50);
	assert(ball_view_at(&rx, start + 50).status == BALL_MISSING);
	struct ball_view last = ball_display_at(&rx, start + 50);
	assert(last.status == BALL_LAST_SEEN && last.permille == 1000);
	assert(ball_display_at(&rx, start + 50 + BALL_TIMEOUT_MS).status == BALL_OFFLINE);
	feed(&rx, samples[4], BALL_FRAME_SIZE, start + 10 + BALL_DISPLAY_LAST_MS - 1);
	assert(ball_display_at(&rx, start + 10 + BALL_DISPLAY_LAST_MS - 1).status == BALL_LAST_SEEN);
	assert(ball_display_at(&rx, start + 10 + BALL_DISPLAY_LAST_MS).status == BALL_MISSING);
	point(&rx, 2, BALL_X_RIGHT, start + 2200);
	point(&rx, 3, 620, start + 2233);
	last = ball_display_at(&rx, start + 2233);
	assert(last.status == BALL_LAST_SEEN && last.permille == 1000);
	feed(&rx, samples[3], BALL_FRAME_SIZE, start + 2266);
	assert(ball_display_at(&rx, start + 2266).status == BALL_LAST_SEEN);
	point(&rx, 4, 620, start + 4200);
	assert(ball_display_at(&rx, start + 4200).status == BALL_UNCERTAIN);
}

/* 对照五点标记核对原图坐标换算。 */
static void test_manual_ruler(void)
{
	struct ball_receiver rx = {0};
	const int marks[] = {521, 388, 253, 128, 24};
	const int positions[] = {0, 248, 500, 773, 1000};
	unsigned int seq = 1;

	point(&rx, seq, 49, 33);
	for (unsigned int i = 0; i < 5; i++) {
		for (unsigned int n = 0; n < 3; n++) {
			seq++;
			point(&rx, seq, marks[i], seq * 33);
			/* 重复序号不应增加确认帧数。 */
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

/* 运行相机解析、重捕获与显示回归。 */
int main(void)
{
	/* 样本来自 send_v2，实测状态也可能带有非零观测年龄。 */
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
	/* 依次注入噪声、重叠帧头及跨多次轮询才完整接收的一帧。 */
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
	/* 残缺帧不应吞掉下一条有效帧。 */
	feed(&rx, samples[2], 9, now);
	feed(&rx, samples[1], BALL_FRAME_SIZE, now);
	assert(rx.frames == 2 && rx.rejected == 2);
	assert(rx.x == 146 && ball_view_at(&rx, now).status == BALL_UNCERTAIN);
	rx.uncertain = false; /* 用独立的实测标记核对换算尺度。 */
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
	/* 历史报文的球心是 282 px。新 O 点在 253 px，该样本应落在中心左侧。 */
	assert(ball_view_at(&rx, now + 251).permille == 452);
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
