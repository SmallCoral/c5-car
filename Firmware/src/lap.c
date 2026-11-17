#include <string.h>
#include "config.h"
#include "lap.h"
#include "logic.h"

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

static bool wide_marker(const struct gray_reading *gray)
{
	return gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 &&
	       gray->count >= LAP_MARKER_CHANNELS;
}

static bool start_marker(const struct gray_reading *gray)
{
	/* Measured A entry: 0x28/0x0d/0x3d (split), then a broad crossbar.
	 * A central line plus a side marking may be split before full coverage.
	 */
	return wide_marker(gray) || (gray->status == GRAY_SPLIT &&
		gray->groups == 2 && gray->count >= 2 && (gray->line & 0x3c));
}

static bool partial_marker(const struct lap *lap, const struct gray_reading *gray)
{
	uint32_t progress = lap_progress(lap);

	/* Three black probes can also occur on a curve. Accept a partial A only
	 * inside the measured lap's finish window, with a longer hold time.
	 * Odometry alone must never produce DONE.
	 */
	return progress >= LAP_PARTIAL_MIN_COUNTS &&
	       progress <= LAP_PARTIAL_MAX_COUNTS &&
	       gray->status != GRAY_WARMUP && gray->groups >= 1 &&
	       gray->groups <= 2 && gray->count >= LAP_PARTIAL_CHANNELS;
}

int lap_speed(unsigned int level)
{
	static const int speeds[LAP_SPEED_LEVELS] = {
		LAP_SPEED_SLOW_CPS, LAP_SPEED_NORMAL_CPS, LAP_SPEED_FAST_CPS
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
	lap->level = level;
	lap->reverse = reverse;
	lap->wheel[0].previous = left;
	lap->wheel[1].previous = right;
	lap->wheel[0].moved_at = lap->wheel[1].moved_at = now;
}

bool lap_start(struct lap *lap, const struct gray_reading *gray, uint32_t now,
	       uint32_t left, uint32_t right, unsigned int level, bool reverse)
{
	initialize(lap, now, left, right, level, reverse);
	if (gray->status != GRAY_LINE && !start_marker(gray)) {
		lap_stop(lap, LAP_NOT_READY);
		return false;
	}
	lap->departing = start_marker(gray);
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

static void speed_control(struct lap_wheel *wheel, int target, uint32_t dt)
{
	int error;
	int integral;
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
	pwm = wheel->target / 7 + error / 10 + integral / 4000;
	if (!((pwm > LAP_MAX_PERMILLE && error > 0) || (pwm < 0 && error < 0)))
		wheel->integral = integral;
	pwm = wheel->target / 7 + error / 10 + wheel->integral / 4000;
	wheel->pwm = clamp(pwm, 0, LAP_MAX_PERMILLE);
}

static void steer_control(struct lap *lap, uint32_t dt, bool narrow)
{
	int position = lap->last_position;
	int magnitude = position < 0 ? -position : position;
	int error = magnitude > LAP_POSITION_DEADBAND ?
		magnitude - LAP_POSITION_DEADBAND : 0;
	int derivative = clamp((int)((int64_t)lap->position_rate *
		LAP_LINE_KD / 1000), -LAP_RATE_LIMIT_CPS, LAP_RATE_LIMIT_CPS);
	int steer;
	int limit;
	int step;
	int base;

	if (position < 0)
		error = -error;
	/* Gray locates the line. Encoders damp a turn already in progress,
	 * including when the binary position has not changed yet. No heading
	 * is integrated: wheel slip and absolute yaw are not observable here.
	 */
	steer = error * LAP_LINE_KP / 1000 + derivative -
		lap->differential * LAP_TURN_DAMPING / 1000;
	steer = clamp(steer, -LAP_STEER_MAX_CPS, LAP_STEER_MAX_CPS);
	step = (int)(LAP_STEER_SLEW_CPS_S * dt / 1000);
	lap->steering += clamp(steer - lap->steering, -step, step);

	base = lap_speed(lap->level) - magnitude * LAP_CORNER_SLOWDOWN / 1000;
	if (lap_progress(lap) >= LAP_APPROACH_COUNTS && base > LAP_APPROACH_CPS)
		base = LAP_APPROACH_CPS;
	if ((!narrow || lap->departing) && base > LAP_SPEED_SLOW_CPS)
		base = LAP_SPEED_SLOW_CPS;
	base = clamp(base, LAP_SPEED_SLOW_CPS / 2, LAP_SPEED_FAST_CPS);
	step = (int)((base < lap->base_target ? LAP_DECEL_CPS_S :
		LAP_ACCEL_CPS_S) * dt / 1000);
	lap->base_target += clamp(base - lap->base_target, -step, step);

	/* Preserve the mean speed and turn direction under either wheel limit.
	 * At startup the common ramp may still be below the normal minimum.
	 */
	limit = clamp(lap->base_target - LAP_TARGET_MIN_CPS, 0,
		LAP_TARGET_MAX_CPS - lap->base_target);
	lap->steering = clamp(lap->steering, -limit, limit);
	speed_control(&lap->wheel[0], lap->base_target + lap->steering, dt);
	speed_control(&lap->wheel[1], lap->base_target - lap->steering, dt);
}

bool lap_update(struct lap *lap, const struct gray_reading *gray, uint32_t now,
		uint32_t left, uint32_t right)
{
	uint32_t dt = now - lap->updated_at;
	bool wide;
	bool partial;
	bool candidate;
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
	wide = wide_marker(gray);
	partial = partial_marker(lap, gray);
	candidate = wide || partial;
	narrow = gray->status == GRAY_LINE && !wide;
	if (lap->departing && (lap->elapsed_ms >= LAP_DEPART_TIMEOUT_MS ||
			     lap_progress(lap) >= LAP_DEPART_MAX_COUNTS)) {
		lap_stop(lap, LAP_AMBIGUOUS);
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
		/* Bounded, slow departure using encoder damping; the crossbar's
		 * changing centroid must not be interpreted as a curve to follow.
		 */
		lap->last_position = 0;
		lap->position_rate = 0;
	}
	lap->narrow = narrow;
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
	/* Brief gaps use the last line position at reduced speed; never drive blind
	 * indefinitely. A starting crossbar gets a bounded departure allowance.
	 */
	if (!lap->departing && !narrow && elapsed(now, lap->line_at,
		wide && !lap->departed ? 800U :
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
