#include <string.h>
#include "config.h"
#include "lap.h"
#include "logic.h"

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

static int min_int(int a, int b)
{
	return a < b ? a : b;
}

static int magnitude(int value)
{
	return value < 0 ? -value : value;
}

static bool unmapped_race(const struct lap *lap)
{
	return lap->level == LAP_RACE_LEVEL && !lap->from_a;
}

static bool wide_marker(const struct gray_reading *gray)
{
	return gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 &&
	       gray->count >= LAP_MARKER_CHANNELS;
}

enum lap_start_surface lap_start_surface(const struct gray_reading *gray)
{
	/* Measured A entry: 0x28/0x0d/0x3d (split), then a broad crossbar.
	 * A central line plus a side marking may be split before full coverage.
	 */
	if (wide_marker(gray) || (gray->status == GRAY_SPLIT &&
	    gray->groups == 2 && gray->count >= 2 && (gray->line & 0x3c)))
		return LAP_START_MARKER;
	return gray->status == GRAY_LINE ? LAP_START_NARROW : LAP_START_UNALIGNED;
}

static bool partial_marker(const struct lap *lap, const struct gray_reading *gray)
{
	uint32_t progress = lap_progress(lap);

	/* Three black probes can also occur on a curve. Accept a partial A only
	 * inside the measured lap's finish window, with a longer hold time.
	 */
	return progress >= LAP_PARTIAL_MIN_COUNTS &&
	       progress <= LAP_PARTIAL_MAX_COUNTS &&
	       gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 && gray->count >= LAP_PARTIAL_CHANNELS;
}

int lap_speed(unsigned int level)
{
	static const int speeds[LAP_SPEED_LEVELS] = {
		LAP_SPEED_SLOW_CPS, LAP_SPEED_NORMAL_CPS, LAP_SPEED_TRACK_CPS,
		LAP_SPEED_FAST_CPS, LAP_SPEED_RACE_CPS
	};

	return speeds[level % LAP_SPEED_LEVELS];
}

uint32_t lap_progress(const struct lap *lap)
{
	return (uint32_t)(((uint64_t)lap->wheel[0].progress +
			   lap->wheel[1].progress) / 2);
}

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

bool lap_start_distance(struct lap *lap, const struct gray_reading *gray, uint32_t now,
			uint32_t left, uint32_t right, unsigned int level, uint32_t finish_counts)
{
	initialize(lap, now, left, right, level, false);
	if (!finish_counts) {
		lap_stop(lap, LAP_NOT_READY);
		return false;
	}
	/* The operator places the car at A and SW2 establishes the encoder origin. */
	lap->from_a = true;
	lap->finish_counts = finish_counts;
	lap->departing = gray->status != GRAY_LINE || wide_marker(gray);
	lap->last_position = lap->departing ? 0 : gray->position;
	lap->status = LAP_RUNNING;
	return true;
}

void lap_check_start(struct lap *lap, uint32_t now, uint32_t left,
		     uint32_t right, bool reverse)
{
	initialize(lap, now, left, right, 0, reverse);
	lap->status = LAP_CHECK;
	lap->wheel[0].pwm = lap->wheel[1].pwm = LAP_CHECK_PERMILLE;
}

static bool sample_wheel(struct lap *lap, unsigned int side, uint32_t count,
			 uint32_t now, uint32_t dt)
{
	struct lap_wheel *wheel = &lap->wheel[side];
	int sign = side ? LAP_RIGHT_ENCODER_SIGN : LAP_LEFT_ENCODER_SIGN;
	int32_t delta = encoder_delta(count, wheel->previous);

	wheel->previous = count;
	/* Reject discontinuities before multiplying or negating a signed delta. */
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

static void speed_control(struct lap_wheel *wheel, int target, uint32_t dt, int pwm_limit)
{
	int error;
	int integral;
	int drive;
	int pwm;

	/* Forward acceleration and differential slew were limited separately.
	 * Limiting each wheel again would delay countersteering and erase turns
	 * during startup when both wheels hit the same acceleration limit.
	 */
	wheel->target = target;
	error = wheel->target - wheel->cps;
	integral = clamp(wheel->integral + error * (int)dt, -400000, 800000);
	/* Feedforward from the previous 10% wheel test; PI removes wheel mismatch.
	 * Integral units are count/s * ms. Reject windup at either PWM limit.
	 */
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

/* Measured at A starts on the fitted track. Both arc entries flatten;
 * the middle of the first arc needs more wheel difference than the second.
 * Values are half the wheel-speed difference / mean, in permille.
 * Interpolate in encoder distance so a change of speed preserves the shape.
 */
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

static struct course_target course_target(const struct lap *lap)
{
	struct course_target target = { .speed = lap_speed(lap->level) };
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

static void update_bend_ratio(struct lap *lap, int error, int dt)
{
	int sign = error < 0 ? -1 : 1;
	int lag = sign * (lap->steering - lap->differential);
	int ratio = lap->bend_ratio_ppm;
	int gain = lap->from_a ? LAP_COURSE_TRIM_KI : LAP_BEND_KI;
	int limit = 1000 * (lap->from_a ? LAP_COURSE_TRIM_PERMILLE : LAP_BEND_MAX_PERMILLE);

	if (unmapped_race(lap))
		gain = LAP_RACE_BEND_KI;
	/* Let small corrections decay near center. Hold an established bend
	 * through centered samples so it does not require a permanent offset.
	 */
	if (magnitude(lap->last_position) <= LAP_CENTER_POSITION + LAP_POSITION_DEADBAND &&
	    magnitude(ratio) < 100000)
		ratio -= ratio * dt / (LAP_TRIM_DECAY_MS + dt);

	/* Grow the integral only when the encoders confirm a turn in this
	 * direction and the wheels are keeping up. Always allow unwinding.
	 */
	if (sign * ratio < 0 ||
	    (sign * lap->differential > lap->base_target / 16 && lag < LAP_TURN_LAG_CPS))
		ratio += error * dt * gain / 1000;
	lap->bend_ratio_ppm = clamp(ratio, -limit, limit);
}

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

	/* With no A reference all turn entry and release come from the probes. */
	if (unmapped_race(lap))
		step += step / 2;
	if (tracking)
		update_bend_ratio(lap, error, dt);
	/* Scale the learned turn with forward speed, including on approach to A.
	 * Damp wheel-speed differences about that turn rather than about zero.
	 */
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

static int distance_finish_speed(int cruise, uint32_t remaining)
{
	uint32_t t, blend;

	if (remaining >= LAP_FINISH_BLEND_COUNTS || cruise <= LAP_FINISH_CPS)
		return cruise;
	/* Smoothstep in remaining distance: flat at both ends of the blend.
	 * Retain the measured cut-off speed so the subsequent coast is comparable. */
	t = remaining * 1000U / LAP_FINISH_BLEND_COUNTS;
	blend = t * t * (3000U - 2U * t) / 1000000U;

	return LAP_FINISH_CPS + (cruise - LAP_FINISH_CPS) * (int)blend / 1000;
}

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
	/* The extra cruise speed is available at center and tapers to zero
	 * over the next probe interval, including predicted outward drift. */
	base = ceiling - offset * LAP_CORNER_SLOWDOWN / 1000 -
		(ceiling - LAP_SPEED_FAST_CPS) * min_int(offset, 1000) / 1000;
	if (magnitude(lap->differential) > turn)
		turn = magnitude(lap->differential);
	turn_cap = clamp(ceiling - turn_cost * turn, 1500, ceiling);
	base = min_int(base, min_int(turn_cap, cruise));

	/* Estimate remaining wheel-speed headroom from measured speed and PWM.
	 * Under load, give up forward speed before the PI loses the differential.
	 */
	for (unsigned int side = 0; side < 2; side++) {
		const struct lap_wheel *wheel = &lap->wheel[side];
		int headroom = pwm_limit - LAP_PWM_RESERVE - wheel->pwm;
		int available = wheel->cps + headroom * LAP_CPS_PER_PERMILLE;

		base = min_int(base, available - (side ? -steer : steer));
	}
	if (lap->finish_counts)
		base = min_int(base, distance_finish_speed(cruise, lap->finish_counts - progress));
	else if (progress >= LAP_APPROACH_COUNTS)
		base = min_int(base, LAP_APPROACH_CPS);
	if (!narrow || lap->departing)
		base = min_int(base, LAP_SPEED_SLOW_CPS);
	return clamp(base, LAP_SPEED_SLOW_CPS / 2, ceiling);
}

static void steer_control(struct lap *lap, uint32_t dt, bool narrow)
{
	struct course_target course = course_target(lap);
	bool race = lap->level == LAP_RACE_LEVEL;
	int pwm_limit = race ? LAP_RACE_MAX_PERMILLE : LAP_MAX_PERMILLE;
	int wheel_limit = race ? LAP_RACE_WHEEL_MAX_CPS : LAP_TARGET_MAX_CPS;
	int accel = race ? LAP_RACE_ACCEL_CPS_S : LAP_ACCEL_CPS_S;
	int steer = line_steering(lap, (int)dt, narrow && !lap->departing, course.bend_permille);
	int base = forward_speed(lap, steer, course.speed, narrow, pwm_limit);
	int step = (base < lap->base_target ? LAP_DECEL_CPS_S : accel) * (int)dt / 1000;
	int limit;

	lap->base_target += clamp(base - lap->base_target, -step, step);

	/* Give a turn priority over cruise speed at the outer wheel ceiling.
	 * During launch keep both targets on the common forward ramp.
	 */
	lap->base_target = min_int(lap->base_target, wheel_limit - magnitude(steer));
	limit = clamp(lap->base_target - LAP_TARGET_MIN_CPS, 0, LAP_STEER_MAX_CPS);
	/* On the measured course the fast run reached an 87% half-difference
	 * on a bend that normally needs about 20%, then crossed the line twice.
	 * Keep both wheels moving and leave room for gray feedback.
	 */
	if (lap->from_a)
		limit = min_int(limit, lap->base_target * LAP_COURSE_TURN_PERMILLE / 1000);
	lap->steering = clamp(steer, -limit, limit);
	speed_control(&lap->wheel[0], lap->base_target + lap->steering, dt, pwm_limit);
	speed_control(&lap->wheel[1], lap->base_target - lap->steering, dt, pwm_limit);
}

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
		/* Binary probes move in whole-channel steps. Smooth the measurement
		 * before differentiating so adjacent-probe changes do not kick yaw.
		 */
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
		/* A gap/marker is not a measured derivative. Reacquisition must not
		 * turn its accumulated position change into a one-period D kick.
		 */
		lap->position_rate = 0;
	}
	if (lap->departing) {
		/* Drive straight and slowly until the probes acquire the course.
		 * A changing centroid across a start mark is not a curve.
		 */
		lap->last_position = 0;
		lap->position_rate = 0;
	}
	lap->narrow = narrow;
	if (!lap->finish_counts && marker_finish(lap, gray, wide, now))
		return true;
	/* Brief gaps use the last line position at reduced speed; never drive blind
	 * indefinitely. A starting crossbar gets a bounded departure allowance.
	 */
	if (!lap->departing && !narrow && elapsed(now, lap->line_at,
		!lap->finish_counts && wide && !lap->departed ? 800U :
		gray->status == GRAY_LOST ? LAP_LINE_LOST_MS : LAP_AMBIGUOUS_MS)) {
		lap_stop(lap, gray->status == GRAY_LOST ? LAP_NO_LINE : LAP_AMBIGUOUS);
		return true;
	}
	steer_control(lap, dt, narrow);
	return true;
}

const char *lap_status_name(enum lap_status status)
{
	static const char *const names[] = {
		"READY", "FOLLOW", "DONE", "STOPPED", "LOST LINE", "BAD LINE",
		"ENC ERROR", "TIMEOUT", "DRIVE ERR", "ALIGN LINE", "LOOP LATE", "DIR CHECK",
		"NO A MARK"
	};

	return (unsigned int)status <= LAP_NO_MARKER ? names[status] : "INVALID";
}
