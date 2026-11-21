/* 两轮循线。灰度位置决定左右速度差，轮编码器反馈决定 PWM；比赛用双轮平均里程减速停车，调试模式也可识别 A 横线。 */

#include <string.h>
#include "config.h"
#include "lap.h"
#include "logic.h"

/* 将整数限制在给定上下界内。 */
static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

/* 取两个整数中较小的一个，用于叠加多个速度上限。 */
static int min_int(int a, int b)
{
	return a < b ? a : b;
}

/* 取幅值，便于对左右两侧使用同一阈值。 */
static int magnitude(int value)
{
	return value < 0 ? -value : value;
}

/* 判断是否为没有 A 点行程参考的高速循线。 */
static bool unmapped_race(const struct lap *lap)
{
	return lap->level == LAP_RACE_LEVEL && !lap->from_a;
}

/* 按覆盖的探头数和黑线段数判断宽横线。 */
static bool wide_marker(const struct gray_reading *gray)
{
	return gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 &&
	       gray->count >= LAP_MARKER_CHANNELS;
}

/* 识别起步时探头压在窄线、A 横线或其他表面，选择驶离方式。 */
enum lap_start_surface lap_start_surface(const struct gray_reading *gray)
{
	/* 实车进入 A 线时先出现分段的 0x28、0x0d、0x3d，随后宽线覆盖。中央纵线与侧标记可能同时出现。 */
	if (wide_marker(gray) || (gray->status == GRAY_SPLIT &&
	    gray->groups == 2 && gray->count >= 2 && (gray->line & 0x3c)))
		return LAP_START_MARKER;
	return gray->status == GRAY_LINE ? LAP_START_NARROW : LAP_START_UNALIGNED;
}

/* 在终点里程窗口内识别仅覆盖三个探头的 A 标记。 */
static bool partial_marker(const struct lap *lap, const struct gray_reading *gray)
{
	uint32_t progress = lap_progress(lap);

	/* 弯道也可能同时压黑三个探头。部分覆盖只在实测终点窗口内有效，并要求更长的持续时间。 */
	return progress >= LAP_PARTIAL_MIN_COUNTS &&
	       progress <= LAP_PARTIAL_MAX_COUNTS &&
	       gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 && gray->count >= LAP_PARTIAL_CHANNELS;
}

/* 将速度档位转换为以编码器计数/秒表示的巡航速度。 */
int lap_speed(unsigned int level)
{
	static const int speeds[LAP_SPEED_LEVELS] = {
		LAP_SPEED_SLOW_CPS, LAP_SPEED_NORMAL_CPS, LAP_SPEED_TRACK_CPS,
		LAP_SPEED_FAST_CPS, LAP_SPEED_RACE_CPS
	};

	return speeds[level % LAP_SPEED_LEVELS];
}

/* 取两轮累计行程的平均值作为圈内里程，求和时加宽以免溢出。 */
uint32_t lap_progress(const struct lap *lap)
{
	return (uint32_t)(((uint64_t)lap->wheel[0].progress +
			   lap->wheel[1].progress) / 2);
}

/* 锁存结束原因并清零目标和驱动输出，供结果页读取。 */
void lap_stop(struct lap *lap, enum lap_status status)
{
	lap->status = status;
	lap->steering = 0;
	lap->base_target = 0;
	lap->position_rate = 0;
	lap->bend_ratio_ppm = 0;
	lap->departing = false;
	for (unsigned int side = 0; side < 2; side++) {
		lap->wheel[side].target = 0;
		lap->wheel[side].pwm = 0;
		lap->wheel[side].integral = 0;
	}
}

/* 清零圈内控制状态，记录起点编码器与首次控制时刻。 */
static void initialize(struct lap *lap, uint32_t now, uint32_t left,
		       uint32_t right, unsigned int level, bool reverse)
{
	memset(lap, 0, sizeof(*lap));
	lap->started_at = lap->updated_at = lap->line_at = now;
	lap->level = level % LAP_SPEED_LEVELS;
	lap->reverse = reverse;
	lap->wheel[0].previous = left;
	lap->wheel[1].previous = right;
	lap->wheel[0].moved_at = lap->wheel[1].moved_at = now;
}

/* 根据起步灰度选择横线驶离或窄线跟踪，启动调试循线。 */
bool lap_start(struct lap *lap, const struct gray_reading *gray, uint32_t now,
	       uint32_t left, uint32_t right, unsigned int level, bool reverse)
{
	enum lap_start_surface surface = lap_start_surface(gray);
	initialize(lap, now, left, right, level, reverse);
	if (surface == LAP_START_UNALIGNED) {
		lap_stop(lap, LAP_NOT_READY);
		return false;
	}
	lap->departing = surface == LAP_START_MARKER;
	lap->from_a = lap->departing && !reverse;
	lap->last_position = lap->departing ? 0 : gray->position;
	lap->status = LAP_RUNNING;
	return true;
}

/* 从按键处建立圈内里程零点，使用传入的终点门槛启动比赛。 */
bool lap_start_distance(struct lap *lap, const struct gray_reading *gray, uint32_t now,
			uint32_t left, uint32_t right, unsigned int level, uint32_t finish_counts)
{
	initialize(lap, now, left, right, level, false);
	if (!finish_counts) {
		lap_stop(lap, LAP_NOT_READY);
		return false;
	}
	/* 车身对齐 A 线后按 SW2，记录两轮编码器起点。此后用双轮平均行程判定终点。 */
	lap->from_a = true;
	lap->finish_counts = finish_counts;
	lap->departing = gray->status != GRAY_LINE || wide_marker(gray);
	lap->last_position = lap->departing ? 0 : gray->position;
	lap->status = LAP_RUNNING;
	return true;
}

/* 启动限时的架空双轮方向检查。 */
void lap_check_start(struct lap *lap, uint32_t now, uint32_t left,
		     uint32_t right, bool reverse)
{
	initialize(lap, now, left, right, 0, reverse);
	lap->status = LAP_CHECK;
	lap->wheel[0].pwm = lap->wheel[1].pwm = LAP_CHECK_PERMILLE;
}

/* 对单轮计数求差，按采样周期估计速度，同时检查异常跳变和停转。 */
static bool sample_wheel(struct lap *lap, unsigned int side, uint32_t count,
			 uint32_t now, uint32_t dt)
{
	struct lap_wheel *wheel = &lap->wheel[side];
	int sign = side ? LAP_RIGHT_ENCODER_SIGN : LAP_LEFT_ENCODER_SIGN;
	int32_t delta = encoder_delta(count, wheel->previous);

	wheel->previous = count;
	/* 计算乘积或对带符号增量取负前，先排除异常跳变。 */
	if (delta < -10000 || delta > 10000) {
		lap_stop(lap, LAP_ENCODER);
		return false;
	}
	delta *= lap->reverse ? -sign : sign;
	wheel->cps += (int32_t)(((int64_t)delta * 1000 -
				(int64_t)wheel->cps * dt) /
			       (LAP_ENCODER_FILTER_MS + dt));
	if (delta > 0) {
		wheel->progress += (uint32_t)delta;
		wheel->moved_at = now;
	}
	if (delta < -2)
		wheel->reverse_ms += dt;
	else
		wheel->reverse_ms = 0;
	if (wheel->reverse_ms >= 100 ||
	    (elapsed(now, lap->started_at, LAP_STALL_GRACE_MS) &&
	     elapsed(now, wheel->moved_at, LAP_STALL_MS))) {
		lap_stop(lap, LAP_ENCODER);
		return false;
	}
	return true;
}

/* 按目标轮速计算前馈，再用 PI 补偿实测速度误差并限制 PWM。 */
static void speed_control(struct lap_wheel *wheel, int target, uint32_t dt, int pwm_limit)
{
	int error;
	int integral;
	int drive;
	int pwm;

	/* 共同前进加速度与差速变化已分别限幅。再逐轮限幅会拖慢反打，甚至让起步时两轮目标相同而失去转向。 */
	wheel->target = target;
	error = wheel->target - wheel->cps;
	integral = clamp(wheel->integral + error * (int)dt, -400000, 800000);
	/* 前馈来自 10% 轮驱动实测，PI 修正两轮差异。积分单位为（计数/秒）× 毫秒，到 PWM 限幅时停止同向累积。 */
	drive = target / LAP_CPS_PER_PERMILLE + error / 6;
	pwm = drive + integral / 3200;
	if (!((pwm > pwm_limit && error > 0) || (pwm < 0 && error < 0)))
		wheel->integral = integral;
	wheel->pwm = clamp(drive + wheel->integral / 3200, 0, pwm_limit);
}

struct course_target {
	int speed;
	int bend_permille;
};

/* 从 A 点起跑，按轮程在两段弯道的实测点间插值。两处入口较平，第一弯中段需要更大差速。表值是半轮速差占平均轮速的千分比。 */
static int race_bend_ratio(int counts, unsigned int arc)
{
	static const struct {
		int16_t counts;
		int16_t ratio[2];
	} bend[] = {
		{  9800, {   0,   0 } }, { 10400, {   0,  80 } },
		{ 11400, { 130, 155 } }, { 12500, { 135, 130 } },
		{ 14000, { 235, 230 } }, { 15300, { 330, 230 } },
		{ 16100, { 225, 230 } }, { 20500, { 230, 230 } },
		{ 21500, { 140, 145 } }, { 22200, {  35,   0 } },
		{ 23600, {  45,   0 } }, { 24400, {   0,   0 } },
	};

	if (counts <= bend[0].counts)
		return 0;
	for (unsigned int i = 1; i < sizeof(bend) / sizeof(bend[0]); i++) {
		if (counts <= bend[i].counts) {
			int from = bend[i - 1].ratio[arc];
			int span = bend[i].counts - bend[i - 1].counts;

			return from + (bend[i].ratio[arc] - from) *
				(counts - bend[i - 1].counts) / span;
		}
	}
	return 0;
}

/* 把当前轮程映射到直道或两段圆弧，给出巡航上限和转弯差速。 */
static struct course_target course_target(const struct lap *lap)
{
	struct course_target target = {
		.speed = lap->carry_ball ? LAP_OFFSET_CRUISE_CPS : lap_speed(lap->level)
	};
	bool race = lap->level == LAP_RACE_LEVEL;
	int brake = race ? LAP_RACE_BRAKE_COUNTS : LAP_BEND_BRAKE_COUNTS;
	int counts = (int)lap_progress(lap);
	bool second = counts >= LAP_HALF_COURSE_COUNTS + brake;
	int blend;

	if (!lap->from_a || lap->departing)
		return target;
	if (second)
		counts -= LAP_HALF_COURSE_COUNTS;
	if (counts < brake)
		return target;
	if (counts < LAP_BEND_END_COUNTS)
		target.speed = min_int(target.speed, race ? LAP_RACE_BEND_CPS : LAP_COURSE_BEND_CPS);
	if (race) {
		target.bend_permille = race_bend_ratio(counts, second);
		return target;
	}
	if (counts >= LAP_BEND_END_COUNTS)
		return target;

	blend = min_int(counts - LAP_BEND_ENTRY_COUNTS, LAP_BEND_END_COUNTS - counts);
	target.bend_permille = clamp(blend, 0, LAP_BEND_BLEND_COUNTS) *
		LAP_COURSE_BEND_PERMILLE / LAP_BEND_BLEND_COUNTS;
	return target;
}

/* 结合灰度误差与实际轮速差，累积弯道所需的持续差速。 */
static void update_bend_ratio(struct lap *lap, int error, int dt)
{
	int sign = error < 0 ? -1 : 1;
	int lag = sign * (lap->steering - lap->differential);
	int ratio = lap->bend_ratio_ppm;
	int gain = lap->from_a ? LAP_COURSE_TRIM_KI : LAP_BEND_KI;
	int limit = 1000 * (lap->from_a ? LAP_COURSE_TRIM_PERMILLE : LAP_BEND_MAX_PERMILLE);

	if (unmapped_race(lap))
		gain = LAP_RACE_BEND_KI;
	/* 靠近中线时逐步消退小修正。已建立的弯道差速在短暂居中时继续保持，避免靠持续偏离中线才能转弯。 */
	if (magnitude(lap->last_position) <= LAP_CENTER_POSITION + LAP_POSITION_DEADBAND &&
	    magnitude(ratio) < 100000)
		ratio -= ratio * dt / (LAP_TRIM_DECAY_MS + dt);

	/* 编码器确认实际转向且两轮跟得上指令时才继续积分。反向消除积累量始终允许。 */
	if (sign * ratio < 0 ||
	    (sign * lap->differential > lap->base_target / 16 && lag < LAP_TURN_LAG_CPS))
		ratio += error * dt * gain / 1000;
	lap->bend_ratio_ppm = clamp(ratio, -limit, limit);
}

/* 将探头偏差、偏移速度和弯道前馈合成两轮所需的半速度差。 */
static int line_steering(struct lap *lap, int dt, bool tracking, int bend_permille)
{
	int position = lap->last_position;
	int error = position - clamp(position, -LAP_POSITION_DEADBAND,
				     LAP_POSITION_DEADBAND);
	int outer = position - clamp(position, -LAP_CENTER_POSITION, LAP_CENTER_POSITION);
	int outer_gain = lap->from_a ? LAP_COURSE_OUTER_KP : LAP_OUTER_KP;
	int rate_limit = min_int(LAP_CENTER_RATE_CPS + 2 * magnitude(outer),
				 LAP_RATE_LIMIT_CPS);
	int derivative = clamp((int)((int64_t)lap->position_rate *
		LAP_LINE_KD / 1000), -rate_limit, rate_limit);
	int slew = magnitude(position) > 1000 || magnitude(lap->steering) > 500 ?
		LAP_EDGE_SLEW_CPS_S : LAP_STEER_SLEW_CPS_S;
	int step = slew * dt / 1000;
	int bend;
	int steer;

	/* 没有 A 点里程参考时，入弯和回正都依据灰度探头。 */
	if (unmapped_race(lap))
		step += step / 2;
	if (tracking)
		update_bend_ratio(lap, error, dt);
	/* 学到的差速按当前前进速度缩放，接近 A 时也如此。阻尼以该差速为基准。 */
	bend = (bend_permille + lap->bend_ratio_ppm / 1000) * lap->base_target / 1000;
	steer = error * LAP_LINE_KP / 1000 + outer * outer_gain / 1000;
	if (lap->from_a && lap->level == LAP_RACE_LEVEL)
		steer = steer * clamp(lap->base_target, LAP_COURSE_BEND_CPS,
				      LAP_RACE_BEND_CPS) / LAP_COURSE_BEND_CPS;
	steer += derivative;
	steer += bend + bend * LAP_TURN_DAMPING / 1000 -
		lap->differential * LAP_TURN_DAMPING / 1000;
	steer = clamp(steer, -LAP_STEER_MAX_CPS, LAP_STEER_MAX_CPS);
	return lap->steering + clamp(steer - lap->steering, -step, step);
}

/* 用剩余计数平滑地从巡航速度过渡到停车前的末速。 */
static int distance_finish_speed(const struct lap *lap, int cruise, uint32_t remaining)
{
	uint32_t distance = lap->carry_ball ? LAP_OFFSET_FINISH_COUNTS : LAP_FINISH_BLEND_COUNTS;
	int finish = lap->carry_ball ? LAP_OFFSET_FINISH_CPS : LAP_FINISH_CPS;
	uint32_t t, blend;

	if (remaining >= distance || cruise <= finish)
		return cruise;
	/* 按剩余编码器计数平滑降低目标轮速。曲线在减速区首尾的斜率均为零，减少球在速度突变时的偏移。 */
	t = remaining * 1000U / distance;
	blend = t * t * (3000U - 2U * t) / 1000000U;

	return finish + (cruise - finish) * (int)blend / 1000;
}

/* 结合探头偏差、转向余量和剩余里程，求此刻允许的前进速度。 */
static int forward_speed(const struct lap *lap, int steer, int cruise, bool narrow,
			 int pwm_limit)
{
	int ceiling = lap->level == LAP_RACE_LEVEL ? LAP_SPEED_RACE_CPS : LAP_SPEED_FAST_CPS;
	int offset = magnitude(lap->last_position);
	int projected = magnitude(lap->last_position +
		lap->position_rate * LAP_BRAKE_LOOKAHEAD_MS / 1000);
	int turn = magnitude(steer);
	int turn_cost = unmapped_race(lap) ? 5 : 2;
	uint32_t progress = lap_progress(lap);
	int base, turn_cap;

	if (projected > offset)
		offset = projected;
	offset = clamp(offset - LAP_CENTER_POSITION, 0, 3000);
	/* 灰度阵列测得黑线居中时允许提速。取当前偏差和预测偏差中较大的一项，超出居中区域一个探头间距时扣完额外速度。 */
	base = ceiling - offset * LAP_CORNER_SLOWDOWN / 1000 -
		(ceiling - LAP_SPEED_FAST_CPS) * min_int(offset, 1000) / 1000;
	if (magnitude(lap->differential) > turn)
		turn = magnitude(lap->differential);
	turn_cap = clamp(ceiling - turn_cost * turn, 1500, ceiling);
	base = min_int(base, min_int(turn_cap, cruise));

	/* 用实测轮速和 PWM 余量估算还能提多少速。负载较大时先降低共同前进速度，保留纠偏需要的轮速差。 */
	for (unsigned int side = 0; side < 2; side++) {
		const struct lap_wheel *wheel = &lap->wheel[side];
		int headroom = pwm_limit - LAP_PWM_RESERVE - wheel->pwm;
		int available = wheel->cps + headroom * LAP_CPS_PER_PERMILLE;

		base = min_int(base, available - (side ? -steer : steer));
	}
	if (lap->finish_counts)
		base = min_int(base, distance_finish_speed(lap, cruise, lap->finish_counts - progress));
	else if (progress >= LAP_APPROACH_COUNTS)
		base = min_int(base, LAP_APPROACH_CPS);
	if (!narrow || lap->departing)
		base = min_int(base, LAP_SPEED_SLOW_CPS);
	return clamp(base, LAP_SPEED_SLOW_CPS / 2, ceiling);
}

/* 分别限制共同轮速和差速的变化，再给左右轮下发各自目标。 */
static void steer_control(struct lap *lap, uint32_t dt, bool narrow, bool lost_line)
{
	struct course_target course = course_target(lap);
	bool race = lap->level == LAP_RACE_LEVEL;
	int pwm_limit = race ? LAP_RACE_MAX_PERMILLE : LAP_MAX_PERMILLE;
	int wheel_limit = race ? LAP_RACE_WHEEL_MAX_CPS : LAP_TARGET_MAX_CPS;
	int accel = lap->carry_ball ? LAP_OFFSET_ACCEL_CPS_S :
		    race ? LAP_RACE_ACCEL_CPS_S : LAP_ACCEL_CPS_S;
	int steer = line_steering(lap, (int)dt, narrow && !lap->departing, course.bend_permille);
	int base = forward_speed(lap, steer, course.speed, narrow, pwm_limit);
	/* 实车直道居中时短暂丢线，急减速曾让球偏移 30 mm。仅在这种短缺口放缓减速，靠边纠偏和持续丢线停车仍按原条件执行。 */
	bool centered_gap = lap->carry_ball && lost_line && !lap->departing &&
			    magnitude(lap->last_position) <= LAP_CENTER_POSITION;
	int decel = centered_gap ? LAP_OFFSET_GAP_DECEL_CPS_S : LAP_DECEL_CPS_S;
	int step = (base < lap->base_target ? decel : accel) * (int)dt / 1000;
	int limit;

	lap->base_target += clamp(base - lap->base_target, -step, step);

	/* 外轮接近速度上限时先留出转向差速，再决定共同前进速度。起步时两轮沿同一加速斜坡提升。 */
	lap->base_target = min_int(lap->base_target, wheel_limit - magnitude(steer));
	limit = clamp(lap->base_target - LAP_TARGET_MIN_CPS, 0, LAP_STEER_MAX_CPS);
	/* 一次高速实跑在通常只需约 20% 半差速比的弯道达到 87%，随后两次越线。约束差速可保持两轮前进并留出灰度修正余量。 */
	if (lap->from_a)
		limit = min_int(limit, lap->base_target * LAP_COURSE_TURN_PERMILLE / 1000);
	lap->steering = clamp(steer, -limit, limit);
	speed_control(&lap->wheel[0], lap->base_target + lap->steering, dt, pwm_limit);
	speed_control(&lap->wheel[1], lap->base_target - lap->steering, dt, pwm_limit);
}

/* 从起点驶离、圈内里程与横线持续时间判断是否回到 A。 */
static bool marker_finish(struct lap *lap, const struct gray_reading *gray,
			  bool wide, uint32_t now)
{
	bool candidate = wide || partial_marker(lap, gray);

	lap->armed = lap->departed && lap->elapsed_ms >= LAP_ARM_MS &&
		     lap_progress(lap) >= LAP_ARM_COUNTS;
	if (lap->armed && gray->count > lap->marker_peak)
		lap->marker_peak = gray->count;
	if (candidate) {
		if (!lap->marker) {
			lap->marker_at = now;
			if (lap->armed)
				lap->marker_hits++;
		}
		if (lap->armed && elapsed(now, lap->marker_at,
			wide ? LAP_MARKER_HOLD_MS : LAP_PARTIAL_HOLD_MS)) {
			lap_stop(lap, LAP_DONE);
			return true;
		}
	}
	lap->marker = candidate;
	if (lap_progress(lap) >= LAP_MAX_COUNTS) {
		lap_stop(lap, LAP_NO_MARKER);
		return true;
	}
	return false;
}

/* 按控制周期读轮速与灰度，检查保护条件，更新转向或锁存停车状态。 */
bool lap_update(struct lap *lap, const struct gray_reading *gray, uint32_t now,
		uint32_t left, uint32_t right)
{
	uint32_t dt = now - lap->updated_at;
	bool wide;
	bool narrow;
	int position;
	int rate;

	if ((lap->status != LAP_RUNNING && lap->status != LAP_CHECK) || dt < LAP_CONTROL_MS)
		return false;
	lap->updated_at = now;
	lap->control_ms = dt;
	lap->elapsed_ms = now - lap->started_at;
	if (dt > LAP_CONTROL_MAX_MS || lap->elapsed_ms >= LAP_TIMEOUT_MS) {
		lap_stop(lap, dt > LAP_CONTROL_MAX_MS ? LAP_TIMING : LAP_TIMEOUT);
		return true;
	}
	if (lap->status == LAP_CHECK && lap->elapsed_ms >= LAP_CHECK_MS) {
		lap_stop(lap, LAP_CANCELLED);
		return true;
	}
	if (!sample_wheel(lap, 0, left, now, dt) ||
	    !sample_wheel(lap, 1, right, now, dt))
		return true;
	lap->differential = (lap->wheel[0].cps - lap->wheel[1].cps) / 2;
	if (lap->status == LAP_CHECK)
		return true;
	if (lap->finish_counts && lap_progress(lap) >= lap->finish_counts) {
		lap_stop(lap, LAP_DONE);
		return true;
	}
	wide = wide_marker(gray);
	narrow = gray->status == GRAY_LINE && !wide;
	if (lap->finish_counts && narrow)
		lap->departing = false;
	if (lap->departing && (lap->elapsed_ms >= LAP_DEPART_TIMEOUT_MS ||
			     lap_progress(lap) >= LAP_DEPART_MAX_COUNTS)) {
		lap_stop(lap, lap->finish_counts && gray->status == GRAY_LOST ?
			 LAP_NO_LINE : LAP_AMBIGUOUS);
		return true;
	}
	if (narrow) {
		if (!lap->narrow)
			lap->narrow_at = now;
		if (elapsed(now, lap->narrow_at, LAP_DEPART_HOLD_MS)) {
			lap->departed = true;
			lap->departing = false;
		}
		lap->line_at = now;
		/* 二值探头的位置按整通道跳变。先平滑位置再求导，避免跨探头瞬间产生过大转向。 */
		position = lap->last_position +
			(gray->position - lap->last_position) * (int)dt /
			(int)(LAP_POSITION_FILTER_MS + dt);
		rate = (position - lap->last_position) * 1000 / (int)dt;
		if (lap->narrow)
			lap->position_rate += (rate - lap->position_rate) *
				(int)dt / (int)(LAP_RATE_FILTER_MS + dt);
		else
			lap->position_rate = 0;
		lap->last_position = position;
	} else {
		/* 横线或空白区间没有可靠的位置导数。重获线时重新建立差分基准，防止把整个缺口位移算在一个周期内。 */
		lap->position_rate = 0;
	}
	if (lap->departing) {
		/* 刚起步时低速直行直到找到纵向窄线，避免把越过起点横线时的重心变化误作弯道。 */
		lap->last_position = 0;
		lap->position_rate = 0;
	}
	lap->narrow = narrow;
	if (!lap->finish_counts && marker_finish(lap, gray, wide, now))
		return true;
	/* 短时丢线以最近位置低速纠偏，持续丢线则停车。从起点横线驶离也设有独立的时间和里程上限。 */
	if (!lap->departing && !narrow && elapsed(now, lap->line_at,
		!lap->finish_counts && wide && !lap->departed ? 800U :
		gray->status == GRAY_LOST ? LAP_LINE_LOST_MS : LAP_AMBIGUOUS_MS)) {
		lap_stop(lap, gray->status == GRAY_LOST ? LAP_NO_LINE : LAP_AMBIGUOUS);
		return true;
	}
	steer_control(lap, dt, narrow, gray->status == GRAY_LOST);
	return true;
}

/* 将循线状态编号转换为屏幕和遥测使用的名字。 */
const char *lap_status_name(enum lap_status status)
{
	static const char *const names[] = {
		"READY", "FOLLOW", "DONE", "STOPPED", "LOST LINE", "BAD LINE",
		"ENC ERROR", "TIMEOUT", "DRIVE ERR", "ALIGN LINE", "LOOP LATE", "DIR CHECK",
		"NO A MARK"
	};

	return (unsigned int)status <= LAP_NO_MARKER ? names[status] : "INVALID";
}
