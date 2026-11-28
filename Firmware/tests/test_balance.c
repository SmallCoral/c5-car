/* 模拟球位和转轴反馈，验证寻球、回中、静摩擦补偿与行程边界。 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "balance.h"
#include "classmate_26h/balance.h"

/* 用给定位置构造可信的测试球位。 */
static struct ball_view at_x(int raw)
{
	struct ball_receiver rx = { .received = true, .flags = 3, .x = (uint16_t)raw };
	return ball_view_at(&rx, 0);
}

/* 用端点观测检查控制方向和输出限幅。 */
static void test_endpoint(int raw, int signed_rpm)
{
	struct balance b = { 0 };
	struct ball_view ball = at_x(raw);
	const int32_t reference = -21431;
	assert(balance_f407_start(&b, reference, reference, &ball, 0));
	for (uint32_t t = 5; t <= 75; t += 5) {
		b.speed_due = false;
		balance_update(&b, &ball, t / 30, reference, t);
		assert(b.active && b.speed_due == (t % 15 == 0));
		if (t >= 20) {
			assert(chose_TI == 4 && keep_out.Target == 160.0f);
			assert(keep_out.Kp == .08f && keep_out.Kd == .4f && keep_inter.Kd == .7f);
			assert(keep_inter.Target == (signed_rpm > 0 ? 3.0f : -3.0f));
		}
		assert(b.native_rpm == (t < 30 ? 0 : signed_rpm));
	}
	balance_stop(&b, false);
}

/* 比较首帧及新实测出现时的微分响应。 */
static void test_measured_derivative(void)
{
	struct balance b = { 0 };
	struct ball_view ball = at_x(BALL_X_CENTER);
	assert(balance_f407_start(&b, 0, 0, &ball, 0));
	for (uint32_t t = 5; t <= 80; t += 5) {
		if (t == 40)
			ball = at_x(BALL_X_CENTER - 12 * BALL_RIGHT_SIGN);
		balance_update(&b, &ball, t / 20, 0, t);
		if (t < 40)
			assert(!b.native_rpm && !b.target_beam_cd);
		if (t == 40)
			assert(fabsf(keep_inter.Target + 2.88f) < .0001f);
		if (t == 45)
			assert(b.native_rpm == -23);
		if (t == 60)
			assert(fabsf(keep_inter.Target + .48f) < .0001f && !b.native_rpm);
	}
	/* 短暂失测时，旧 F407 闭环沿用最近一次有效位置。 */
	ball.status = BALL_MISSING;
	for (uint32_t t = 85; t < 200; t += 5) {
		balance_update(&b, &ball, t / 20, 0, t);
		assert(b.active && keep_out.Actual == 166.0f);
	}
	balance_update(&b, &ball, 10, 0, 200);
	assert(b.active && b.state == BAL_WAIT && !b.native_rpm && b.vision_paused);
	ball = at_x(BALL_X_CENTER - 6 * BALL_RIGHT_SIGN);
	balance_update(&b, &ball, 11, 0, 205);
	assert(b.active && !b.vision_paused && fabsf(keep_inter.Target + .24f) < .0001f);
}

/* 检查回中判定和机械行程约束。 */
static void test_center_and_limits(void)
{
	struct balance b = { 0 };
	struct ball_view ball = at_x(BALL_X_CENTER);
	for (int i = 0; i < 2; i++) {
		int reference = i ? 89999 : -21431;
		assert(balance_f407_start(&b, reference, reference, &ball, 0));
		for (uint32_t t = 5; t <= 1300; t += 5) {
			balance_update(&b, &ball, t / 35, reference, t);
			assert(b.active && !b.native_rpm);
		}
		assert(b.state == BAL_STABLE && b.centered);
		struct ball_view ambiguous = { .status = BALL_UNCERTAIN };
		balance_update(&b, &ambiguous, 100, reference, 1305);
		assert(b.active && !b.centered && !b.native_rpm);
		for (uint32_t t = 1310; t <= 1440; t += 5)
			balance_update(&b, &ambiguous, 100, reference, t);
		assert(b.active && !b.native_rpm && !b.centered);
		for (uint32_t t = 1445; t <= 4350; t += 5)
			balance_update(&b, &ambiguous, 100, reference, t);
		assert(!b.active && !b.centered);
		assert(!balance_f407_start(&b, reference, reference, &ambiguous, 0));
	}
	assert(!balance_f407_start(&b, INT32_MAX, INT32_MAX, &ball, 0));
	for (int sign = -1; sign <= 1; sign += 2) {
		assert(!balance_f407_start(&b, 0, sign * (BAL_LIMIT_RAW + 1), &ball, 0));
		assert(balance_f407_start(&b, 0, sign * BAL_LIMIT_RAW, &ball, 0));
		balance_update(&b, &ball, 1, sign * 1730, 5);
		assert(!b.active && b.state == BAL_FAULT);
	}
}

/* 模拟不同工作姿态和静摩擦，检查额外倾角的建立与撤销。 */
static void test_working_pose_and_stiction(void)
{
	struct balance b = { 0 };
	struct ball_view ball = at_x(BALL_X_CENTER);
	for (int side = -1; side <= 1; side += 2) {
		struct ball_view stuck = at_x(BALL_X_CENTER - 8 * side * BALL_RIGHT_SIGN);
		assert(balance_f407_start(&b, 0, 0, &stuck, 0));
		for (uint32_t t = 5; t <= 5500; t += 5)
			balance_update(&b, &stuck, t/30, 0, t);
		assert(b.active && b.trim_mdeg == 4000 * side && b.native_rpm * side < 0);
		for (uint32_t t = 5505; t <= 7100; t += 5)
			balance_update(&b, &ball, t/30, 0, t);
		assert(b.state == BAL_STABLE && b.trim_mdeg == 4000 * side);
		balance_stop(&b, false);
	}
}

/* 检查进入保持后的控制响应。 */
static void test_maintenance(void)
{
	struct balance b = { 0 };
	struct ball_view none = { 0 };
	for (int sign = -1; sign <= 1; sign += 2) {
		int actual = -945 + sign * 15000;
		assert(balance_park(&b, -945, actual, 0));
		balance_update(&b, &none, 0, actual, 10);
		assert(b.active && balance_goal(&b) == -945);
		balance_update(&b, &none, 0, -945, 20);
		assert(!b.active);
	}
	assert(balance_level(&b, -1345) && balance_probe(&b, -1345, 0));
	int max = 0;
	for (uint32_t t = 10; b.active && t < 5600; t += 10) {
		balance_update(&b, &none, 0, balance_goal(&b), t);
		if (b.command > max)
			max = b.command;
	}
	assert(max == 600 && b.state == BAL_READY);
}

/* 用连续运动观测驱动启动寻球直到锁定。 */
static void test_boot_roll_lock(int roll_sign)
{
	struct balance b = { 0 };
	struct ball_view ball = at_x(roll_sign > 0 ? BALL_X_LEFT : BALL_X_RIGHT);
	const int32_t origin = 12561;
	assert(balance_roll_start(&b, origin, origin, &ball, UINT32_MAX - 250));
	uint32_t start = b.started;
	uint16_t seq = 65520;
	int32_t actual = origin;
	for (uint32_t dt = 5; dt <= 900; dt += 5) {
		if (dt % 30 == 0) {
			seq++;
			/* 依次模拟合理的端点、错误的静止坐标和单次跳点。 */
			ball = at_x(dt < 300 ? 309 : 263 + (int)(seq % 2));
		}
		if (dt < 125)
			actual -= 18;
		balance_update(&b, &ball, seq, actual, start + dt);
		assert(b.active && b.state == BAL_FIND_BALL && !b.lock_frames);
	}
	int first = BALL_X_CENTER - 180 * roll_sign * BALL_RIGHT_SIGN;
	int coast = 3;
	uint32_t next = 930;
	for (int i = 0; !b.lock_frames && i < 16; i++) {
		if (i == 3 && roll_sign < 0) {
			ball.status = BALL_MISSING;
			seq++;
			for (int j = 0; j < 6; j++)
				balance_update(&b, &ball, seq, actual, start + next + j * 5);
			next += 30;
			assert(!b.lock_frames);
		}
		ball = at_x(first + i * roll_sign * BALL_RIGHT_SIGN * 18);
		seq++;
		for (int j = 0; j < 6; j++) {
			if (b.state == BAL_VERIFY_ROLL && coast > 0) {
				/* 即使已请求零速，转轴仍可能因惯性继续移动。 */
				actual += roll_sign * 210;
				coast--;
				assert(b.lower == origin - BAL_ROLL_CCW_RAW && b.upper == origin + BAL_ROLL_CW_RAW);
			}
			balance_update(&b, &ball, seq, actual, start + next + j * 5);
		}
		next += 30;
	}
	assert(b.active && b.state == BAL_FOLLOW && b.lock_frames >= 5);
	assert(b.lock_travel * roll_sign >= 40 && b.lock_rate * roll_sign > 0);
	assert(b.started == start && b.lock_at - start == next - 30);
	assert(b.lower >= origin - BAL_ROLL_CCW_RAW && b.upper <= origin + BAL_ROLL_CW_RAW);
	assert(b.speed_due && b.outer_started);
	/* 锁定后出现一个远处的孤立观测时应请求零速，避免突然反转。 */
	ball = at_x(b.last_site < BAL_F407_O ? BALL_X_LEFT : BALL_X_RIGHT);
	balance_update(&b, &ball, ++seq, actual, start + next);
	assert(b.active && b.vision_paused && !b.native_rpm);
	for (uint32_t dt = 5; dt < 450; dt += 5) {
		balance_update(&b, &ball, (uint16_t)(seq + dt / 30), actual, start + next + dt);
		assert(b.vision_paused && !b.native_rpm && b.state == BAL_WAIT);
	}
	ball.status = BALL_OFFLINE;
	for (uint32_t dt = 450; dt < 3500; dt += 5)
		balance_update(&b, &ball, seq, actual, start + next + dt);
	assert(b.active && b.state == BAL_CAMERA_OFF && !b.native_rpm);
	assert(b.origin == origin && b.lower == origin - BAL_ROLL_CCW_RAW && b.upper == origin + BAL_ROLL_CW_RAW);
	balance_stop(&b, false);
	assert(!b.active);
}

/* 中途移除观测，检查寻球对漏帧与断流的处理。 */
static void test_boot_roll_missing(void)
{
	struct balance b = { 0 };
	struct ball_view missing = { .status = BALL_MISSING };
	int32_t actual = 0;
	assert(balance_roll_start(&b, 0, actual, &missing, 0));
	bool rested = false;
	for (uint32_t t = 5; t <= 28000; t += 5) {
		balance_update(&b, &missing, t/30, actual, t);
		actual -= b.native_rpm * 5;
		assert(b.active && !b.lock_frames && abs(b.native_rpm) <= 30);
		assert(actual >= -BAL_ROLL_CCW_RAW && actual <= BAL_ROLL_CW_RAW);
		if (b.state == BAL_NO_TRACK) {
			assert(!b.native_rpm);
			rested = true;
		}
	}
	assert(rested && b.legs == 1 && b.state == BAL_NO_TRACK);
	balance_stop(&b, false);
	assert(balance_roll_start(&b, 20000, 20000, &missing, 0));
	balance_update(&b, &missing, 4, 20000 + BAL_ROLL_CW_RAW + 300, 5);
	assert(b.state == BAL_FAULT && !b.active);
	assert(balance_roll_start(&b, 0, 0, &missing, 0));
	missing.status = BALL_OFFLINE;
	for (uint32_t t = 5; t <= 30000; t += 5) {
		balance_update(&b, &missing, 4, 0, t);
		assert(b.active && b.state == BAL_CAMERA_OFF && !b.native_rpm);
	}
	missing.status = BALL_MISSING;
	balance_update(&b, &missing, 5, 0, 30005);
	assert(b.state == BAL_FIND_BALL && b.native_rpm > 0 && b.legs == 1);
}

/* 从实物右挡杆构造起步条件，检查首次抬管方向。 */
static void test_right_stop_lift(void)
{
	struct balance b = { 0 };
	int32_t actual = 0;
	int x = BALL_X_RIGHT;
	struct ball_view ball = at_x(x);
	assert(balance_roll_start(&b, 0, actual, &ball, 0));
	for (uint32_t t = 5; t < 3200 && !b.lock_frames; t += 5) {
		if (t % 30 == 0 && actual < -8500)
			ball = at_x(x -= 12 * BALL_RIGHT_SIGN);
		balance_update(&b, &ball, t/30, actual, t);
		if (!b.lock_frames) {
			assert(b.active && b.native_rpm >= 0 && b.legs == 1);
			actual -= b.native_rpm * 5;
		}
	}
	assert(b.state == BAL_FOLLOW && b.lock_travel <= -24 && b.legs == 1);
	assert(b.zero < -7500 && b.zero >= -BAL_ROLL_CCW_RAW);
	assert(b.lower >= -BAL_ROLL_CCW_RAW && actual >= b.lower && actual <= b.upper);
}

/* 依次运行稳球与寻球的回归场景。 */
int main(void)
{
	/* 球在实物右侧时须抬起右端（逆时针），球在左侧时需顺时针。 */
	test_endpoint(BALL_X_RIGHT, 24);
	test_endpoint(BALL_X_LEFT, -24);
	test_measured_derivative();
	test_center_and_limits();
	test_working_pose_and_stiction();
	test_boot_roll_lock(1);
	test_boot_roll_lock(-1);
	test_boot_roll_missing();
	test_right_stop_lift();
	test_maintenance();
	struct balance b = { .state = BAL_WAIT };
	struct balance_timer timer;
	balance_time_start(&timer, 1, UINT32_MAX - 1000);
	balance_time_update(&timer, &b, true, 1999);
	assert(timer.counting && timer.elapsed == 3000 && !timer.tracking && !timer.settled);
	b.state = BAL_FOLLOW;
	balance_time_update(&timer, &b, true, 3499);
	b.state = BAL_STABLE;
	balance_time_update(&timer, &b, true, 7499);
	balance_time_update(&timer, &b, false, 9999);
	balance_time_update(&timer, &b, false, 15000);
	assert(timer.tracking == 4500 && timer.settled == 8500 && timer.elapsed == 11000);
	return 0;
}
