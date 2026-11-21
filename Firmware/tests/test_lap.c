/* 模拟轮编码器和灰度轨迹，核验两道行驶题的转向、减速、里程停车及故障保护。 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "lap.h"
#include "logic.h"
#include "wheel_map.h"
#include "gray_map.h"

/* 构造普通窄线的灰度读数。 */
static struct gray_reading line(int position)
{
	return (struct gray_reading) {
		.raw = 0xe7, .black = 0x18, .line = 0x18, .count = 2,
		.groups = 1, .status = GRAY_LINE, .position = (int16_t)position,
	};
}

/* 构造横向标记的灰度读数。 */
static struct gray_reading marker(void)
{
	return (struct gray_reading) {
		.black = 0xff, .line = 0xff, .count = 8,
		.groups = 1, .status = GRAY_FULL,
	};
}

/* 按一个控制节拍推进模拟编码器和循线器。 */
static void tick(struct lap *lap, struct gray_reading gray, int left, int right)
{
	int left_sign = lap->reverse ? -LAP_LEFT_ENCODER_SIGN : LAP_LEFT_ENCODER_SIGN;
	int right_sign = lap->reverse ? -LAP_RIGHT_ENCODER_SIGN : LAP_RIGHT_ENCODER_SIGN;
	uint32_t lc = lap->wheel[0].previous + (uint32_t)(left_sign * left);
	uint32_t rc = lap->wheel[1].previous + (uint32_t)(right_sign * right);

	assert(lap_update(lap, &gray, lap->updated_at + 10, lc, rc));
}

/* 模拟驶离起点再经过终点，验证横线判停。 */
static void test_lap_and_marker(void)
{
	struct lap lap;
	struct gray_reading gray = marker();

	assert(lap_start_surface(&gray) == LAP_START_MARKER);
	assert(lap_start(&lap, &gray, 0, 4, 4, 0, false));
	for (int i = 0; i < 10; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_RUNNING && !lap.departed);
	for (int i = 0; i < 210; i++)
		tick(&lap, line(0), 10, 10);
	assert(!lap.armed && lap_progress(&lap) == 2200);
	/* 圈初即使出现真实横线，也不得提前完圈。 */
	for (int i = 0; i < 5; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_RUNNING);
	for (int i = 0; i < 2800; i++)
		tick(&lap, line(0), 10, 10);
	assert(lap.armed);
	/* 横线连续可见达到规定时间才能停车，短暂噪声不能触发。 */
	tick(&lap, gray, 10, 10);
	tick(&lap, line(0), 10, 10);
	assert(lap.status == LAP_RUNNING);
	tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_RUNNING);
	tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_DONE);
	assert(!lap.wheel[0].pwm && !lap.wheel[1].pwm && !lap.wheel[0].target);
	assert(!lap_update(&lap, &gray, lap.updated_at + 10, 0, 0));
	gray = line(0);
	assert(lap_start_surface(&gray) == LAP_START_NARROW);
	assert(lap_start(&lap, &gray, 3000, 100, 100, 1, true));
	assert(!lap.armed && !lap.departed && !lap_progress(&lap));
	assert(!lap.wheel[0].integral && !lap.wheel[1].pwm);
	for (int i = 0; i < 20; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_RUNNING && lap.wheel[0].cps > 0);
	lap_stop(&lap, LAP_CANCELLED);
	assert(!lap.wheel[0].pwm && !lap.wheel[1].target);
}

/* 从任意计数起点模拟一圈，核对按平均里程停车。 */
static void test_distance_lap(void)
{
	struct lap lap;
	struct gray_reading lost = { .status = GRAY_LOST };
	struct gray_reading gray = line(0);
	struct gray_reading partial = gray;
	partial.count = 3;
	partial.line = partial.black = 0x1c;

	/* SW2 分别记录左右轮起点，覆盖计数器即将回绕的情况。 */
	assert(lap_start_distance(&lap, &lost, 2000, UINT32_MAX - 100U, 513U, LAP_RACE_LEVEL, LAP_DISTANCE_COUNTS));
	assert(lap.status == LAP_RUNNING && lap.from_a && lap.departing && !lap_progress(&lap));
	for (int i = 0; i < 8; i++)
		tick(&lap, lost, 3, 3);
	assert(lap.wheel[0].target > 0 && lap.wheel[1].target > 0);
	tick(&lap, gray, 3, 3);
	assert(!lap.departing);
	while (lap.status == LAP_RUNNING) {
		uint32_t progress = lap_progress(&lap);
		/* 旧方案的两个停车标记均落在对应识别窗口内。 */
		struct gray_reading input = progress >= 35000 && progress < 35120 ? marker() :
			progress >= 41100 && progress < 41250 ? partial : gray;

		tick(&lap, input, 36, 24);
		if (lap_progress(&lap) < LAP_DISTANCE_COUNTS)
			assert(lap.status == LAP_RUNNING);
	}
	assert(lap.status == LAP_DONE && !lap.armed && !lap.marker_hits);
	assert(lap_progress(&lap) >= LAP_DISTANCE_COUNTS && lap_progress(&lap) < LAP_DISTANCE_COUNTS + 30U);
	assert(!lap.wheel[0].target && !lap.wheel[1].target && !lap.wheel[0].pwm && !lap.wheel[1].pwm);
	assert(!lap_update(&lap, &gray, lap.updated_at + 10, 0, 0));

	/* 重新起跑时按当前编码器读数设置新零点。 */
	gray = marker();
	assert(lap_start_distance(&lap, &gray, 17000, 55000, 38000, LAP_FAST_LEVEL, LAP_DISTANCE_COUNTS));
	assert(lap.departing && lap.from_a && !lap_progress(&lap));
	tick(&lap, line(0), 10, 10);
	assert(!lap.departing && lap.status == LAP_RUNNING);
	for (int i = 0; i < 15; i++)
		tick(&lap, lost, 10, 10);
	assert(lap.status == LAP_NO_LINE && !lap.wheel[1].pwm);

	assert(!lap_start_distance(&lap, &lost, 0, 0, 0, LAP_RACE_LEVEL, 0));
	assert(lap_start_distance(&lap, &lost, 0, 0, 0, LAP_RACE_LEVEL, LAP_DISTANCE_COUNTS));
	for (unsigned int i = 0; i < LAP_DEPART_TIMEOUT_MS / 10; i++)
		tick(&lap, lost, 2, 2);
	assert(lap.status == LAP_NO_LINE && !lap.wheel[0].pwm);
}

/* 检查剩余里程减少时的末段轮速曲线。 */
static void test_distance_finish_speed(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);
	unsigned int normal = 0, blending = 0, cutoff = 0;

	assert(lap_start_distance(&lap, &gray, 0, 0, 0, LAP_RACE_LEVEL, LAP_DISTANCE_COUNTS));
	assert(lap.from_a && !lap.departing);
	while (lap.status == LAP_RUNNING) {
		int previous = lap.base_target;
		tick(&lap, gray, (lap.wheel[0].target + 50) / 100,
		     (lap.wheel[1].target + 50) / 100);
		uint32_t counts = lap_progress(&lap);

		if (counts >= LAP_APPROACH_COUNTS && counts <= LAP_DISTANCE_COUNTS - LAP_FINISH_BLEND_COUNTS) {
			assert(lap.base_target > 2400);
			normal++;
		}
		if (counts > LAP_DISTANCE_COUNTS - LAP_FINISH_BLEND_COUNTS && counts < LAP_DISTANCE_COUNTS) {
			assert(lap.base_target >= previous - LAP_DECEL_CPS_S / 100);
			if (lap.base_target > 1500 && lap.base_target < 2300)
				blending++;
			if (counts + 30 >= LAP_DISTANCE_COUNTS) {
				assert(lap.base_target >= LAP_FINISH_CPS && lap.base_target < LAP_FINISH_CPS + 40);
				cutoff++;
			}
		}
	}
	assert(lap.status == LAP_DONE && normal && blending && cutoff);
}

/* 给定左右偏差与轮速，检查前进速度和纠偏方向。 */
static void test_steering_and_speed(void)
{
	struct lap left, right;
	struct gray_reading gray = line(0);

	assert(lap_start(&left, &gray, 0, 0, 0, 1, false));
	assert(lap_start(&right, &gray, 0, 0, 0, 1, false));
	for (int i = 0; i < 100; i++) {
		tick(&left, line(-2500), 10, 10);
		tick(&right, line(2500), 10, 10);
		assert(left.wheel[0].pwm >= 0 && left.wheel[0].pwm <= LAP_MAX_PERMILLE);
		assert(left.wheel[1].pwm >= 0 && left.wheel[1].pwm <= LAP_MAX_PERMILLE);
	}
	assert(left.wheel[0].target < left.wheel[1].target);
	assert(right.wheel[0].target > right.wheel[1].target);
	assert(left.wheel[0].target == right.wheel[1].target);

	/* 模拟两轮每千分之一 PWM 分别产生 7 和 5 计数/秒，验证闭环能将两侧调到同一速度。 */
	struct lap balanced;
	int fraction[2] = { 0, 0 };
	assert(lap_start(&balanced, &gray, 0, 0, 0, 0, false));
	for (int i = 0; i < 2000; i++) {
		int counts[2];

		for (unsigned int side = 0; side < 2; side++) {
			fraction[side] += balanced.wheel[side].pwm * (side ? 5 : 7);
			counts[side] = fraction[side] / 100;
			fraction[side] %= 100;
		}
		tick(&balanced, gray, counts[0], counts[1]);
		assert(balanced.status == LAP_RUNNING);
	}
	assert(balanced.wheel[0].cps > 750 && balanced.wheel[0].cps < 850);
	assert(balanced.wheel[1].cps > 750 && balanced.wheel[1].cps < 850);
	assert(balanced.wheel[1].pwm > balanced.wheel[0].pwm);
}

/* 注入失线、异常计数与超时，核对停止原因。 */
static void test_failures(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);
	struct gray_reading lost = { .status = GRAY_LOST };
	struct gray_reading split = { .status = GRAY_SPLIT, .count = 2, .groups = 2 };

	assert(lap_start_surface(&lost) == LAP_START_UNALIGNED);
	assert(!lap_start(&lap, &lost, 0, 0, 0, 0, false));
	assert(lap.status == LAP_NOT_READY && !lap.wheel[0].pwm);
	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	for (int i = 0; i < 14; i++)
		tick(&lap, lost, 5, 5);
	assert(lap.status == LAP_RUNNING);
	tick(&lap, lost, 5, 5);
	assert(lap.status == LAP_NO_LINE && !lap.wheel[0].pwm);

	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	for (int i = 0; i < 25; i++)
		tick(&lap, split, 5, 5);
	assert(lap.status == LAP_AMBIGUOUS);

	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	for (int i = 0; i < 120; i++) {
		if (lap.status != LAP_RUNNING)
			break;
		tick(&lap, gray, 0, 0);
	}
	assert(lap.status == LAP_ENCODER && !lap.wheel[0].pwm);
	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	for (int i = 0; i < 10; i++)
		tick(&lap, gray, -10, -10);
	assert(lap.status == LAP_ENCODER);
	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	assert(lap_update(&lap, &gray, 51, 0, 0));
	assert(lap.status == LAP_TIMING);
	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	lap.updated_at = LAP_TIMEOUT_MS - 10;
	tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_TIMEOUT);
	assert(lap_start(&lap, &gray, 0, 0, 0, 0, false));
	assert(lap_update(&lap, &gray, 10, 0x80000000U, 0));
	assert(lap.status == LAP_ENCODER);
}

/* 模拟编码器回绕及调试按键。 */
static void test_wrap_and_controls(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);
	struct controls ctl = { .task = TASK_LAP };

	assert(lap_start(&lap, &gray, UINT32_MAX - 20, UINT32_MAX - 4, UINT32_MAX - 4, 0, false));
	for (int i = 0; i < 10; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_RUNNING && lap.elapsed_ms == 100);
	assert(lap_progress(&lap) == 100);
	assert(task_uses_wheels(TASK_LAP) && !task_uses_stepper(TASK_LAP));
	assert(task_continuous(TASK_LAP));
	controls_key(&ctl, KEY_NEXT, 0);
	assert(ctl.lap_level == 1 && ctl.level == 0);
	controls_key(&ctl, KEY_OK, 0);
	assert(ctl.lap_reverse && !ctl.reverse);
	controls_key(&ctl, KEY_START, 0);
	controls_tick(&ctl, 4000, false);
	assert(ctl.running);
	controls_key(&ctl, KEY_MODE, 4001);
	assert(!ctl.running && ctl.task == TASK_LAP);
}

/* 校验架空测向的输出和时间上限。 */
static void test_direction_check(void)
{
	struct lap lap;
	struct gray_reading lost = { .status = GRAY_LOST };

	lap_check_start(&lap, 0, 0, 0, false);
	assert(lap.status == LAP_CHECK && lap.wheel[0].pwm == 150);
	for (int i = 0; i < 149; i++) {
		tick(&lap, lost, 10, 10);
		assert(lap.status == LAP_CHECK && lap.wheel[1].pwm == 150);
	}
	tick(&lap, lost, 10, 10);
	assert(lap.status == LAP_CANCELLED && !lap.wheel[0].pwm && !lap.wheel[1].pwm);
	assert(lap.elapsed_ms == 1500);
}

/* 让探头在相邻通道间跳变，检查微分滤波。 */
static void test_binary_edge_smoothing(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);
	int previous_steering = 0;

	assert(lap_start(&lap, &gray, 0, 0, 0, LAP_FAST_LEVEL, false));
	assert(lap_speed(LAP_FAST_LEVEL) == 3300);
	tick(&lap, line(500), 15, 15);
	assert(lap.last_position > 0 && lap.last_position < 500);
	/* 只有一次反向观测时，不能立即输出反方向的最大差速。 */
	tick(&lap, line(-500), 15, 15);
	assert(lap.last_position > -500 && lap.last_position < 500);
	previous_steering = lap.steering;
	for (int i = 0; i < 300; i++) {
		int position = (i / 10) & 1 ? 500 : -500;
		int delta;

		tick(&lap, line(position), 15, 15);
		assert(lap.status == LAP_RUNNING);
		delta = lap.steering - previous_steering;
		assert(delta <= 60 && delta >= -60);
		assert(lap.steering < 250 && lap.steering > -250);
		previous_steering = lap.steering;
	}
	/* 持续大偏差经过滤波后，仍应保留足够的纠偏能力。 */
	for (int i = 0; i < 100; i++)
		tick(&lap, line(3000), 15, 15);
	assert(lap.wheel[0].target > lap.wheel[1].target + 1000);
}

/* 验证共同加速时仍能建立转向差速。 */
static void test_turn_during_acceleration(void)
{
	struct lap lap;
	struct gray_reading gray = line(-1500);

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 25; i++) {
		int previous = lap.base_target;

		tick(&lap, gray, 5, 5);
		assert(lap.base_target - previous <= LAP_ACCEL_CPS_S / 100);
	}
	/* 旧版起步时即使探头已偏左，两轮仍曾同速爬升约 0.6 s。 */
	assert(lap.wheel[1].target - lap.wheel[0].target >= 400);
	assert(lap.wheel[0].target >= LAP_TARGET_MIN_CPS);
	assert(lap.base_target >= 500);
	assert((lap.wheel[0].target + lap.wheel[1].target) / 2 == lap.base_target);
}

/* 检查轮速反馈能抑制过度转弯并平顺接回黑线。 */
static void test_encoder_damping_and_reacquisition(void)
{
	struct lap clockwise, counterclockwise;
	struct gray_reading gray = line(0);
	struct gray_reading lost = { .status = GRAY_LOST };

	assert(lap_start(&clockwise, &gray, 0, 0, 0, 2, false));
	assert(lap_start(&counterclockwise, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 100; i++) {
		tick(&clockwise, gray, 20, 10);
		tick(&counterclockwise, gray, 10, 20);
	}
	/* 即便灰度输入不变，也应根据轮速反馈抑制仍在持续的转向。 */
	assert(clockwise.wheel[0].target < clockwise.wheel[1].target);
	assert(counterclockwise.wheel[0].target > counterclockwise.wheel[1].target);
	assert(clockwise.steering == -counterclockwise.steering);

	for (int i = 0; i < 10; i++)
		tick(&clockwise, lost, 15, 15);
	assert(clockwise.status == LAP_RUNNING && !clockwise.position_rate);
	tick(&clockwise, line(-3000), 15, 15);
	assert(clockwise.status == LAP_RUNNING && !clockwise.position_rate);
	lap_stop(&clockwise, LAP_CANCELLED);
	assert(!clockwise.base_target && !clockwise.steering);
	assert(!clockwise.wheel[0].integral && !clockwise.wheel[1].pwm);
}

/* 模拟回中与反向，检查差速释放和边界。 */
static void test_turn_release_and_bounds(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 200; i++)
		tick(&lap, line(3000), 15, 15);
	assert(lap.wheel[0].target > lap.wheel[1].target);
	assert(lap.base_target < 1800);
	/* 线回到中央后，应在 100 ms 内释放上一次转向量。 */
	for (int i = 0; i < 10; i++)
		tick(&lap, line(0), 15, 15);
	assert(lap.wheel[0].target <= lap.wheel[1].target + 100);
	for (int i = 0; i < 400; i++) {
		tick(&lap, line(i / 25 % 2 ? -3500 : 3500), 15, 15);
		for (int side = 0; side < 2; side++) {
			assert(lap.wheel[side].target >= LAP_TARGET_MIN_CPS);
			assert(lap.wheel[side].target <= LAP_TARGET_MAX_CPS);
			assert(lap.wheel[side].pwm >= 0);
			assert(lap.wheel[side].pwm <= LAP_MAX_PERMILLE);
		}
	}
}

/* 用实测的分段横线掩码复现驶离 A 点。 */
static void test_a_marker_departure(void)
{
	/* 实车进入 A 横线时采到从分段的 0x0d 转为全黑。 */
	struct gray_reading entry = {
		.line = 0x0d, .black = 0x0d, .count = 3, .groups = 2,
		.status = GRAY_SPLIT,
	};
	struct lap lap;

	assert(lap_start(&lap, &entry, 0, 0, 0, 2, false));
	assert(lap.departing && !lap.armed);
	for (int i = 0; i < 30; i++) {
		tick(&lap, i < 15 ? entry : marker(), 5, 5);
		assert(lap.status == LAP_RUNNING && lap.departing);
		assert(!lap.last_position && !lap.position_rate);
		assert(lap.wheel[0].target == lap.wheel[1].target);
		assert(lap.base_target <= LAP_SPEED_SLOW_CPS);
	}
	for (int i = 0; i < 11; i++)
		tick(&lap, line(0), 5, 5);
	assert(lap.departed && !lap.departing && !lap.armed);

	assert(lap_start(&lap, &entry, 0, 0, 0, 2, false));
	for (int i = 0; i < 100; i++)
		tick(&lap, marker(), 10, 10);
	assert(lap.status == LAP_AMBIGUOUS && !lap.wheel[0].pwm);
	assert(lap_start(&lap, &entry, 0, 0, 0, 2, false));
	for (int i = 0; i < 150; i++)
		tick(&lap, entry, 1, 1);
	assert(lap.status == LAP_AMBIGUOUS && !lap.wheel[1].pwm);
	entry.line = entry.black = 0x28;
	entry.count = 2;
	assert(lap_start(&lap, &entry, 0, 0, 0, 2, false) && lap.departing);
	entry.line = entry.black = 0x81;
	assert(!lap_start(&lap, &entry, 0, 0, 0, 2, false));
}

/* 分别在终点窗口内外注入部分覆盖标记。 */
static void test_partial_marker_window(void)
{
	struct lap lap;
	struct gray_reading gray = line(0);
	struct gray_reading partial = {
		.line = 0x70, .black = 0x70, .count = 3, .groups = 1,
		.status = GRAY_LINE, .position = 1500,
	};

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 4000; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.base_target <= LAP_APPROACH_CPS);
	/* 终点窗口前，普通弯道的三个黑探头不能触发停车。 */
	for (int i = 0; i < 90; i++)
		tick(&lap, partial, 10, 10);
	assert(lap.status == LAP_RUNNING && !lap.marker_hits);
	for (int i = 0; i < 20; i++)
		tick(&lap, gray, 10, 10);
	/* 即使进入 A 窗口，孤立的一次观测也不能判作标记。 */
	tick(&lap, partial, 10, 10);
	tick(&lap, gray, 10, 10);
	for (int i = 0; i < 3; i++)
		tick(&lap, partial, 10, 10);
	assert(lap.status == LAP_RUNNING);
	tick(&lap, partial, 10, 10);
	assert(lap.status == LAP_DONE && lap.marker_peak == 3);
	assert(!lap.wheel[0].pwm && !lap.wheel[1].pwm);

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	assert(!lap.marker_hits && !lap_progress(&lap));
	for (int i = 0; i < 4800; i++)
		tick(&lap, gray, 10, 10);
	for (int i = 0; i < 100; i++)
		tick(&lap, partial, 10, 10);
	assert(lap.status == LAP_RUNNING && !lap.marker_hits);
}

/* 用短标记与遗漏标记核对两种停车路径。 */
static void test_short_a_marker_and_missed_marker(void)
{
	struct lap lap;
	struct grayscale sensor = { 0 };
	struct gray_reading gray = line(0);
	uint32_t count = 0;

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 3010; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.armed);
	count = lap.wheel[0].previous;
	uint32_t start = lap.updated_at;
	/* 每 2 ms 采样一次，四探头横线持续 30 ms 后应停车。掩码 0x3c 来自实车横线边缘。 */
	for (uint32_t ms = 2; ms <= 30 && lap.status == LAP_RUNNING; ms += 2) {
		gray_update(&sensor, 0xc3, start + ms, false);
		count += 2;
		lap_update(&lap, &sensor.reading, start + ms, count, count);
	}
	assert(lap.status == LAP_DONE && lap.marker_hits == 1);
	assert(lap.marker_peak == 4 && !lap.wheel[0].pwm);

	assert(lap_start(&lap, &gray, 0, 0, 0, 2, false));
	for (int i = 0; i < 6000 && lap.status == LAP_RUNNING; i++)
		tick(&lap, gray, 10, 10);
	assert(lap.status == LAP_NO_MARKER && !lap.wheel[1].pwm);
}

/* 从高速巡航进入终点减速范围，核对速度目标。 */
static void test_fast_cruise_and_finish_approach(void)
{
	struct gray_reading gray = line(0);
	struct lap lap;

	assert(lap_speed(0) == 800 && lap_speed(1) == 1800 && lap_speed(2) == 2200);
	assert(lap_speed(LAP_SPEED_LEVELS) == lap_speed(0));
	assert(lap_start(&lap, &gray, 0, 0, 0, LAP_FAST_LEVEL, false));
	for (int i = 0; i < 250; i++)
		tick(&lap, gray, 33, 33);
	assert(lap.wheel[0].target == 3300 && lap.wheel[1].target == 3300);

	for (int sign = -1; sign <= 1; sign += 2) {
		struct lap corner = lap;

		for (int i = 0; i < 8; i++)
			tick(&corner, line(sign * 3500), 33, 33);
		assert(sign * corner.steering >= 600);
		assert(corner.wheel[sign > 0 ? 0 : 1].target <= LAP_TARGET_MAX_CPS);
		assert(corner.wheel[sign > 0 ? 1 : 0].target <= 2300);
	}

	/* 模拟以巡航速度跨过真实减速门槛并保留两轮惯性。 */
	lap.wheel[0].progress = lap.wheel[1].progress = LAP_APPROACH_COUNTS - 33;
	for (int i = 0; i < 40; i++)
		tick(&lap, gray, 33, 33);
	assert(lap.base_target == LAP_APPROACH_CPS && lap.status == LAP_RUNNING);
}

/* 模拟持续弯道，检查转向积分的保留与消退。 */
static void test_bend_memory(void)
{
	struct gray_reading gray = line(0);
	struct gray_reading lost = { .status = GRAY_LOST };
	struct lap lap;

	for (int sign = -1; sign <= 1; sign += 2) {
		assert(lap_start(&lap, &gray, 0, 0, 0, LAP_DEFAULT_LEVEL, false));
		assert(!lap.bend_ratio_ppm);
		/* 把已输出的转向通过两轮编码器反馈给闭环。 */
		for (int i = 0; i < 160; i++)
			tick(&lap, line(sign * 1500), (lap.wheel[0].target + 50) / 100,
			     (lap.wheel[1].target + 50) / 100);
		assert(sign * lap.bend_ratio_ppm > 100000);
		assert(sign * lap.bend_ratio_ppm <= LAP_BEND_MAX_PERMILLE * 1000);
		for (int i = 0; i < 50; i++)
			tick(&lap, gray, (lap.wheel[0].target + 50) / 100,
			     (lap.wheel[1].target + 50) / 100);
		assert(sign * lap.steering > 100);
		int ratio = lap.bend_ratio_ppm;
		for (int i = 0; i < 5; i++)
			tick(&lap, lost, 10, 10);
		assert(lap.status == LAP_RUNNING && lap.bend_ratio_ppm == ratio);
		tick(&lap, marker(), 10, 10);
		assert(lap.bend_ratio_ppm == ratio);
		lap_stop(&lap, LAP_CANCELLED);
		assert(!lap.bend_ratio_ppm && !lap.wheel[0].pwm && !lap.wheel[1].pwm);
	}
}

/* 从 A 点起跑，对照轮程检查两段弯的预设控制。 */
static void test_course_from_a(void)
{
	struct gray_reading bar = marker();
	struct gray_reading center = line(0);
	struct lap lap;

	assert(lap_start(&lap, &bar, 0, 0, 0, LAP_FAST_LEVEL, false));
	assert(lap.from_a);
	for (int i = 0; i < 20; i++)
		tick(&lap, center, 5, 5);
	assert(lap.departed);
	for (int arc = 0; arc < 2; arc++) {
		int counts = arc * LAP_HALF_COURSE_COUNTS;

		lap.wheel[0].progress = lap.wheel[1].progress = counts + LAP_BEND_BRAKE_COUNTS;
		for (int i = 0; i < 30; i++)
			tick(&lap, center, 22, 22);
		assert(lap.base_target <= LAP_COURSE_BEND_CPS && !lap.steering);
		lap.wheel[0].progress = lap.wheel[1].progress =
			counts + LAP_BEND_ENTRY_COUNTS + LAP_BEND_BLEND_COUNTS;
		for (int i = 0; i < 40; i++)
			tick(&lap, center, 26, 18);
		assert(lap.steering >= 300 && lap.steering <= 600);

		/* 首次高速实跑中，偏右 0.4 s 后黑线移到左侧，旧版仍以 2800/200 的目标速度继续右转。 */
		for (int i = 0; i < 40; i++) {
			tick(&lap, line(2000), 28, 12);
			assert(lap.steering * 1000 <= lap.base_target * LAP_COURSE_TURN_PERMILLE);
		}
		for (int i = 0; i < 10; i++)
			tick(&lap, line(-1500), 28, 12);
		assert(lap.steering <= 0);
		/* 核对两弯之间的直道能否恢复巡航速度。 */
		if (arc == 0) {
			lap.wheel[0].progress = lap.wheel[1].progress = LAP_BEND_END_COUNTS;
			for (int i = 0; i < 120; i++)
				tick(&lap, center, 33, 33);
			assert(lap.base_target >= 3200);
		}
	}
	assert(lap_start(&lap, &center, 0, 0, 0, LAP_DEFAULT_LEVEL, false));
	assert(!lap.from_a);
	assert(lap_start(&lap, &bar, 0, 0, 0, LAP_DEFAULT_LEVEL, true));
	assert(!lap.from_a);
}

/* 检查比赛档的速度上限及回中能力。 */
static void test_race_speed_and_centering(void)
{
	struct gray_reading bar = marker();
	struct gray_reading center = line(0);
	struct lap lap;
	struct controls ctl = { .task = TASK_LAP, .lap_level = LAP_FAST_LEVEL };

	controls_key(&ctl, KEY_NEXT, 0);
	assert(ctl.lap_level == LAP_DEFAULT_LEVEL && lap_speed(ctl.lap_level) == 4600);
	assert(lap_start(&lap, &bar, 0, 0, 0, ctl.lap_level, false));
	for (int i = 0; i < 15; i++)
		tick(&lap, center, 5, 5);
	for (int arc = 0; arc < 2; arc++) {
		int start = arc * LAP_HALF_COURSE_COUNTS;

		lap.wheel[0].progress = lap.wheel[1].progress = start + 1000;
		for (int i = 0; i < 150; i++)
			tick(&lap, center, 46, 46);
		assert(lap.base_target == 4600 && abs(lap.steering) < 10);
		assert(lap.wheel[0].pwm > LAP_MAX_PERMILLE &&
		       lap.wheel[0].pwm <= LAP_RACE_MAX_PERMILLE);
		lap.wheel[0].progress = lap.wheel[1].progress = start + LAP_RACE_BRAKE_COUNTS;
		for (int i = 0; i < 20; i++)
			tick(&lap, center, 24, 24);
		assert(lap.base_target == 3100 && abs(lap.steering) < 60);
		lap.wheel[0].progress = lap.wheel[1].progress = start + 11400;
		for (int i = 0; i < 25; i++)
			tick(&lap, center, 35, 27);
		assert(lap.steering >= 320 && lap.steering <= 520);
		lap.wheel[0].progress = lap.wheel[1].progress = start + 16000;
		for (int i = 0; i < 35; i++)
			tick(&lap, center, 38, 24);
		assert(lap.base_target == 3100 && lap.steering >= 600 && lap.steering <= 800);
		/* 持续的内侧偏差须能覆盖里程表预设的右转量。 */
		for (int i = 0; i < 22; i++) {
			tick(&lap, line(-2000), 38, 24);
			assert(lap.wheel[0].target <= LAP_RACE_WHEEL_MAX_CPS);
			assert(lap.wheel[1].pwm <= LAP_RACE_MAX_PERMILLE);
		}
		assert(lap.steering < 0 && lap.base_target < 3100);
	}
	lap.wheel[0].progress = lap.wheel[1].progress = LAP_APPROACH_COUNTS;
	for (int i = 0; i < 50; i++)
		tick(&lap, center, 12, 10);
	assert(lap.base_target == LAP_APPROACH_CPS && lap.status == LAP_RUNNING);
	controls_key(&ctl, KEY_NEXT, 1);
	assert(ctl.lap_level == 0);
}

/* 在载球直道制造短时丢线，验证受限减速度。 */
static void test_carried_ball_brakes_on_centered_gap(void)
{
	struct gray_reading center = line(0);
	struct gray_reading lost = { .status = GRAY_LOST };
	struct lap lap;
	assert(lap_start_distance(&lap, &center, 0, 0, 0, LAP_OFFSET_LEVEL, LAP_DISTANCE_COUNTS));
	lap.carry_ball = true;
	for (int i = 0; i < 250; i++)
		tick(&lap, center, 14, 14);
	assert(lap.base_target == 1400);
	struct lap ordinary = lap, edge = lap;
	ordinary.carry_ball = false;
	edge.last_position = 1500;
	tick(&ordinary, lost, 18, 18);
	tick(&edge, lost, 18, 18);
	assert(ordinary.base_target == 1320 && edge.base_target == 1320);
	for (int i = 0; i < 10; i++)
		tick(&lap, lost, 18, 18);
	assert(lap.status == LAP_RUNNING && lap.base_target == 1220);
	struct lap recovered = lap;
	tick(&recovered, center, 16, 16);
	assert(recovered.status == LAP_RUNNING && recovered.base_target > lap.base_target);
	for (int i = 0; i < 5; i++)
		tick(&lap, lost, 16, 16);
	assert(lap.status == LAP_NO_LINE && !lap.wheel[0].pwm && !lap.wheel[1].pwm);
}

/* 核对轮组交换后实物 J6、J4 与编码器的映射。 */
static void test_fitted_wheel_mapping(void)
{
	struct wheel_channels right_turn = wheel_to_channels(
		220 * LAP_LEFT_DRIVE_SIGN, 80 * LAP_RIGHT_DRIVE_SIGN);
	struct wheel_channels left_only = wheel_to_channels(123, 0);
	struct wheel_channels right_only = wheel_to_channels(0, -234);

	/* 按实物线束核对左右，J6 左轮、J4 右轮，此处只换通道。 */
	assert(left_only.j6 == 123 && left_only.j4 == 0);
	assert(right_only.j4 == -234 && right_only.j6 == 0);
	assert(right_turn.j6 == 220 && right_turn.j4 == -80);
	assert(wheel_channel(0) == 1 && wheel_channel(1) == 0);
	assert(wheel_faults_to_physical(1) == 2);
	assert(wheel_faults_to_physical(2) == 1);
	assert(wheel_phases_to_physical(12) == 3);
	assert(wheel_phases_to_physical(3) == 12);
}

/* 让载球状态经过两段弯道，检查巡航目标连续性。 */
static void test_carried_ball_speed_in_both_arcs(void)
{
	struct gray_reading center = line(0);
	struct lap lap;
	assert(lap_start_distance(&lap, &center, 0, 0, 0, LAP_OFFSET_LEVEL, LAP_DISTANCE_COUNTS));
	lap.carry_ball = true;
	for (int i = 0; i < 260; i++)
		tick(&lap, center, 18, 18);
	assert(lap.base_target == LAP_OFFSET_CRUISE_CPS);
	for (int arc = 0; arc < 2; arc++) {
		int brake = arc * LAP_HALF_COURSE_COUNTS + LAP_BEND_BRAKE_COUNTS;
		lap.wheel[0].progress = lap.wheel[1].progress = brake;
		for (int i = 0; i < 40; i++) {
			tick(&lap, center, 14, 14);
			assert(lap.base_target == LAP_OFFSET_CRUISE_CPS);
		}
		for (int i = 0; i < 120; i++)
			tick(&lap, center, 17, 11);
		assert(lap.base_target == LAP_OFFSET_CRUISE_CPS);
		if (!arc) {
			lap.wheel[0].progress = lap.wheel[1].progress = LAP_BEND_END_COUNTS;
			for (int i = 0; i < 80; i++)
				tick(&lap, center, 18, 18);
			assert(lap.base_target == LAP_OFFSET_CRUISE_CPS);
		}
	}
}

/* 分别触发最左和最右探头，核对实际加速的一侧车轮。 */
static void test_probe_to_motor_direction(void)
{
	const uint8_t electrical[] = { 0x7f, 0xfe };
	struct gray_reading centered = line(0);

	for (unsigned int side = 0; side < 2; side++) {
		struct grayscale sensor = { 0 };
		struct lap lap;
		struct wheel_channels command;

		for (unsigned int sample = 0; sample < GRAY_FILTER_SAMPLES; sample++)
			gray_update(&sensor, gray_input_order(electrical[side]),
				    sample * GRAY_SAMPLE_MS, false);
		assert(sensor.reading.status == GRAY_LINE);
		assert(sensor.reading.position == (side ? 3500 : -3500));
		assert(lap_start(&lap, &centered, 0, 0, 0, 2, false));
		for (unsigned int sample = 0; sample < 100; sample++)
			tick(&lap, sensor.reading, 10, 10);
		command = wheel_to_channels(lap.wheel[0].pwm * LAP_LEFT_DRIVE_SIGN,
					    lap.wheel[1].pwm * LAP_RIGHT_DRIVE_SIGN);
		assert(command.j6 >= 0 && command.j4 <= 0);
		if (side) {
			/* PE7 压黑表示黑线在实物右侧，应提高左轮 J6 的速度。 */
			assert(lap.wheel[0].target > lap.wheel[1].target);
			assert(command.j6 > -command.j4);
		} else {
			/* PE14 压黑表示黑线在实物左侧，应提高右轮 J4 的速度。 */
			assert(lap.wheel[1].target > lap.wheel[0].target);
			assert(-command.j4 > command.j6);
		}
	}
}

/* 执行循线、停车、映射和保护条件的所有回归。 */
int main(void)
{
	test_lap_and_marker();
	test_distance_lap();
	test_distance_finish_speed();
	test_steering_and_speed();
	test_failures();
	test_wrap_and_controls();
	test_direction_check();
	test_binary_edge_smoothing();
	test_turn_during_acceleration();
	test_encoder_damping_and_reacquisition();
	test_turn_release_and_bounds();
	test_a_marker_departure();
	test_short_a_marker_and_missed_marker();
	test_partial_marker_window();
	test_fast_cruise_and_finish_approach();
	test_bend_memory();
	test_course_from_a();
	test_race_speed_and_centering();
	test_carried_ball_brakes_on_centered_gap();
	test_carried_ball_speed_in_both_arcs();
	test_fitted_wheel_mapping();
	test_probe_to_motor_direction();
	puts("PASS: distance and marker finishes, terminal speed, PD, PI, loss/stall/timeout, wrap and controls");
	return 0;
}
