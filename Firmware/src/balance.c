#include <limits.h>
#include <stdlib.h>
#include "balance.h"
#include "classmate_26h/balance.h"
#include "classmate_26h/bsp_system.h"

struct f407_site site;
float Now_Angle;
static int requested_rpm;

/* Partical/balance.c calls this with dir=1 for CCW, including zero speed. */
void Contron_X_Speed(uint8_t dir, uint16_t rpm)
{
	requested_rpm = dir ? (int)rpm : -(int)rpm;
}

static bool valid_reference(int32_t raw)
{
	return raw >= INT32_MIN + 65536 + 273 && raw <= INT32_MAX - 65536 - 273;
}

int balance_shaft(const struct balance *b, int32_t actual)
{
	int64_t cd = ((int64_t)actual - b->zero) * 36000 / 65536;
	return cd < -36000 ? -36000 : cd > 36000 ? 36000 : (int)cd;
}

int32_t balance_goal(const struct balance *b)
{
	return b->zero + b->command * 65536 / 36000;
}

bool balance_level(struct balance *b, int32_t actual)
{
	if (b->active || !valid_reference(actual))
		return false;
	b->zero = actual;
	b->manual_level = true;
	b->level_hint = actual;
	b->leveled = true;
	b->state = BAL_READY;
	return true;
}

static void reset_pid_history(PID_BALANCE *p)
{
	p->Actual = p->Actual1 = p->Target = p->Last_Target = 0.0f;
	p->Out = p->FeedForward = p->Error0 = p->Error1 = p->ErrorInt = 0.0f;
}

static int f407_site_x(const struct balance *b, const struct ball_view *ball)
{
	/* Bearing-side clockwise raises the right end. On this drive this is
	 * F6 direction 1, the archive's positive speed. */
	int x = BAL_F407_O + (b->reverse ? ball->offset_px : -ball->offset_px);
	return x < 1 ? 1 : x > 319 ? 319 : x; /* The archive reserves site 0 for loss. */
}

bool balance_f407_start(struct balance *b, int32_t reference, int32_t actual,
			const struct ball_view *ball, uint32_t now)
{
	if (b->active || !valid_reference(reference) || ball->status != BALL_TRACKED ||
	    llabs((int64_t)actual - reference) > BAL_LIMIT_RAW)
		return false;
	*b = (struct balance){ .active = true, .leveled = true, .f407 = true, .reverse = b->reverse,
		.zero = reference, .state = BAL_WAIT, .started = now, .control_at = now,
		.outer_at = now, .inner_at = now, .sample_at = now, .last_position = ball->permille };
	b->lower = reference - BAL_LIMIT_RAW;
	b->upper = reference + BAL_LIMIT_RAW;
	b->origin = actual;
	b->last_site = f407_site_x(b, ball);
	b->still_site = b->last_site;
	b->still_at = now;
	reset_pid_history(&keep_out);
	reset_pid_history(&keep_inter);
	balance_state = 1;
	chose_TI = 4; /* The archive's fixed-center profile, target 160. */
	best_hard();
	site.X_0 = (int16_t)b->last_site;
	/* A fresh run has no previous image. Seed the original derivative with
	 * the real first observation so an unmoved ball produces no startup kick. */
	keep_out.Actual1 = (float)site.X_0;
	Now_Angle = (float)((int64_t)actual - reference) / 65535.0f * 360.0f;
	keep_inter.Actual1 = -(Now_Angle / 360.0f * 3200.0f * 0.039f);
	requested_rpm = 0;
	return true;
}

bool balance_probe(struct balance *b, int32_t actual, uint32_t now)
{
	if (b->active || !b->leveled || llabs((int64_t)actual - b->zero) > BAL_LIMIT_RAW)
		return false;
	int shaft = balance_shaft(b, actual);
	*b = (struct balance){ .active = true, .leveled = true, .zero = b->zero, .reverse = b->reverse,
		.state = BAL_PROBE, .command = shaft, .shaft_rpm = 15, .started = now, .control_at = now };
	return true;
}

bool balance_park(struct balance *b, int32_t origin, int32_t actual, uint32_t now)
{
	if (b->active || !valid_reference(origin) ||
	    llabs((int64_t)actual - origin) > BAL_SEEK_RAW + BAL_LIMIT_RAW)
		return false;
	*b = (struct balance){ .active = true, .zero = origin, .reverse = b->reverse, .state = BAL_PARK,
		.shaft_rpm = 20, .started = now, .control_at = now };
	return true;
}

void balance_stop(struct balance *b, bool fault)
{
	balance_state = 0;
	b->active = b->centered = b->speed_due = false;
	b->command = b->demand = b->native_rpm = 0;
	if (fault)
		b->leveled = false;
	b->state = fault ? BAL_FAULT : b->leveled ? BAL_READY : BAL_NEED_LEVEL;
}

static int clamp_int(int value, int lo, int hi)
{
	return value < lo ? lo : value > hi ? hi : value;
}

static void tracking_limits(struct balance *b)
{
	int32_t lo = b->origin - BAL_ROLL_CCW_RAW;
	int32_t hi = b->origin + BAL_ROLL_CW_RAW;
	int32_t center = clamp_int(b->zero + b->trim_mdeg * 65536 / 360000, lo, hi);
	b->lower = clamp_int(center - BAL_LIMIT_RAW, lo, hi);
	b->upper = clamp_int(center + BAL_LIMIT_RAW, lo, hi);
}

static void observe_ball(struct balance *b, const struct ball_view *ball, uint16_t seq, uint32_t now)
{
	if (ball->status != BALL_TRACKED || (b->have_sample && b->sequence == seq))
		return;
	uint32_t dt = now - b->sample_at;
	if (b->have_sample && dt && dt < BAL_VISION_TIMEOUT_MS) {
		int v = ((int)ball->permille - b->last_position) * 1000 / (int)dt;
		b->velocity += (v - b->velocity) * (int)dt / (120 + (int)dt);
	} else {
		b->velocity = 0;
		b->centered = false;
	}
	b->sample_at = now;
	b->have_sample = true;
	b->last_position = ball->permille;
	b->last_site = f407_site_x(b, ball);
	b->sequence = seq;
	/* Near O, a stationary ball can sit in the F407 tilt deadband. Learn a
	 * bounded offset from direct observations. Far from O, adjust the
	 * rolling-onset estimate only after the actuator has reached its tilt. */
	int error = b->last_site - BAL_F407_O;
	bool far = abs(error) > 48;
	if (!dt || dt >= BAL_VISION_TIMEOUT_MS || abs(error) <= BALL_CENTER_RADIUS_PX ||
	    (far && (!b->lock_frames || abs(b->target_beam_cd - b->actual_beam_cd) > 85)) ||
	    abs(b->velocity) > 60 || abs(b->last_site - b->still_site) > 3 ||
	    ((error > 0) != (b->still_site > BAL_F407_O))) {
		b->still_site = b->last_site;
		b->still_at = now;
	} else if (now - b->still_at >= (far ? 500U : 240U)) {
		int delta = (int)dt * (far ? 4 : abs(error) > 6 ? 2 : 1);
		int limit = b->lock_frames ? 12000 : 4000;
		b->trim_mdeg += error > 0 ? delta : -delta;
		if (b->trim_mdeg > limit)
			b->trim_mdeg = limit;
		if (b->trim_mdeg < -limit)
			b->trim_mdeg = -limit;
		if (b->lock_frames)
			tracking_limits(b);
	}
	if (abs(ball->offset_px) <= BALL_CENTER_RADIUS_PX && abs(b->velocity) <= 60) {
		if (!b->centered)
			b->centered_at = now;
		b->centered = true;
	} else {
		b->centered = false;
	}
}

static bool due(uint32_t *at, unsigned int period, uint32_t now)
{
	uint32_t elapsed = now - *at;
	if (elapsed < period)
		return false;
	*at += elapsed - elapsed % period;
	return true;
}

bool balance_roll_start(struct balance *b, int32_t origin, int32_t actual,
			const struct ball_view *ball, uint32_t now)
{
	if (b->active || !valid_reference(origin) || actual < origin - BAL_ROLL_CCW_RAW - 273 ||
	    actual > origin + BAL_ROLL_CW_RAW + 273)
		return false;
	/* An endpoint can choose which way to tilt first. It cannot certify a track. */
	int dir = ball->status == BALL_TRACKED && ball->offset_px < -80 ? 1 : -1;
	*b = (struct balance){ .active = true, .f407 = true, .reverse = b->reverse,
		.manual_level = b->manual_level, .level_hint = b->level_hint,
		.state = BAL_FIND_BALL, .zero = origin, .origin = origin, .roll_start_raw = actual,
		.lower = origin - BAL_ROLL_CCW_RAW, .upper = origin + BAL_ROLL_CW_RAW,
		.roll_dir = b->reverse ? -dir : dir, .legs = 1, .started = now,
		.control_at = now, .leg_at = now, .sample_at = now, .inner_at = now };
	balance_state = 0;
	return true;
}

static void seed_motion(struct ball_motion *m, uint16_t seq, int x, int32_t shaft, uint32_t now)
{
	*m = (struct ball_motion){ .since = now, .last_at = now, .seq = seq, .first = x,
		.last = x, .frames = 1, .first_shaft = shaft };
}

/* A static false detection, one jump between features, duplicate packets and
 * extrapolated positions cannot qualify as a moving ball. */
static bool rolling_track(struct ball_motion *m, const struct ball_view *ball, uint16_t seq,
			  int32_t shaft, uint32_t now)
{
	if (ball->status != BALL_TRACKED) {
		if (ball->status != BALL_MISSING || now - m->last_at > 100)
			m->frames = 0;
		return false;
	}
	if (m->frames && m->seq == seq)
		return false;
	int x = ball->offset_px, dx = x - m->last;
	uint32_t dt = now - m->last_at;
	/* A single missed image may sit between two measured positions. It
	 * contributes neither a frame nor a step to the lock criteria. */
	if (!m->frames || (uint16_t)(seq - m->seq) > 2 || !dt || dt > 100 ||
	    now - m->since > 450 || abs(dx) > 7 + (int)dt ||
	    (!m->direction && abs(dx) < 2) || (m->direction && dx * m->direction < -2)) {
		seed_motion(m, seq, x, shaft, now);
		return false;
	}
	if (abs(dx) >= 2) {
		if (!m->direction)
			m->direction = dx > 0 ? 1 : -1;
		if (m->direction * dx > 0)
			m->steps++;
	}
	m->last = x;
	m->last_at = now;
	m->seq = seq;
	m->frames++;
	return m->frames >= 5 && m->steps >= 3 && now - m->since >= 95 && abs(x - m->first) >= 40;
}

static void sweep_rpm(struct balance *b, int rpm, uint32_t now)
{
	bool brake = b->native_rpm && !rpm;
	b->native_rpm = rpm;
	b->speed_due = due(&b->inner_at, BAL_F407_INNER_MS, now) || brake;
}

static void take_control(struct balance *b, const struct ball_view *ball, uint16_t seq,
			 int32_t actual, uint32_t now)
{
	const struct ball_motion m = b->motion;
	int32_t lower = b->lower, upper = b->upper, origin = b->origin, hint = b->level_hint;
	uint32_t started = b->started;
	unsigned int legs = b->legs;
	bool manual = b->manual_level;
	/* The shaft at first observed roll is a working estimate. Back off 2 deg
	 * against the roll; keep all control within the original sweep envelope. */
	int32_t zero = manual ? hint : m.first_shaft - (b->reverse ? -1 : 1) * m.direction * 364;
	zero = clamp_int(clamp_int(zero, lower, upper), actual - BAL_LIMIT_RAW, actual + BAL_LIMIT_RAW);
	b->active = false;
	if (!balance_f407_start(b, zero, actual, ball, now)) {
		balance_stop(b, true);
		return;
	}
	b->started = started;
	b->origin = origin;
	tracking_limits(b);
	b->legs = legs;
	b->manual_level = manual;
	b->level_hint = hint;
	b->lock_at = now;
	b->lock_frames = (unsigned int)m.frames;
	b->lock_travel = m.last - m.first;
	b->lock_rate = b->lock_travel * 1000 / (int)(m.last_at - m.since);
	int site_rate = b->reverse ? b->lock_rate : -b->lock_rate;
	keep_out.Actual1 = (float)b->last_site - (float)site_rate * (BAL_F407_OUTER_MS / 1000.0f);
	b->velocity = b->lock_rate * 500 / (ball->offset_px < 0 ? BALL_LEFT_SPAN : BALL_RIGHT_SPAN);
	b->sequence = seq;
	b->have_sample = true;
	b->outer_at = now - BAL_F407_OUTER_MS;
	b->inner_at = now - BAL_F407_INNER_MS;
}

static void verify_roll(struct balance *b, const struct ball_view *ball, uint16_t seq,
			int32_t actual, uint32_t now)
{
	struct ball_motion *m = &b->motion;
	sweep_rpm(b, 0, now);
	/* Keep the acquisition limits until the F6 stop has actually settled.
	 * A moving image of the pipe end cannot finish this test on its own. */
	if (llabs((int64_t)actual - b->brake_raw) > 18) {
		b->brake_raw = actual;
		b->still_at = now;
		b->quiet_frames = 0;
	}
	if (ball->status == BALL_TRACKED && seq != m->seq) {
		uint32_t gap = now - m->last_at;
		int x = ball->offset_px, dx = (x - m->last) * m->direction;
		if (gap > 160 || dx < -3 || dx > 7 + (int)gap)
			goto resume;
		m->seq = seq;
		m->last = x;
		m->last_at = now;
		m->frames++;
		if (now - b->still_at >= 45) {
			if (!b->quiet_frames)
				b->still_site = x;
			b->quiet_frames++;
			if (b->quiet_frames >= 3 && (x - b->still_site) * m->direction >= 8) {
				take_control(b, ball, seq, actual, now);
				return;
			}
		}
	}
	if (now - b->verify_at < 650)
		return;
resume:
	b->motion.frames = 0;
	b->leg_at = now;
	b->state = BAL_FIND_BALL;
}

static void sweep_for_ball(struct balance *b, const struct ball_view *ball, uint16_t seq,
			   int32_t actual, uint32_t now)
{
	if (ball->status == BALL_OFFLINE) {
		b->motion.frames = 0;
		b->state = BAL_CAMERA_OFF;
		sweep_rpm(b, 0, now);
		return;
	}
	if (b->state == BAL_CAMERA_OFF) {
		b->at_end = false;
		b->leg_at = now;
		b->state = BAL_FIND_BALL;
	}
	if (llabs((int64_t)actual - b->roll_start_raw) >= 364)
		b->roll_moved = true;
	if (b->state == BAL_VERIFY_ROLL) {
		verify_roll(b, ball, seq, actual, now);
		return;
	}
	if (b->roll_moved && rolling_track(&b->motion, ball, seq, actual, now) &&
	    b->motion.direction == (b->reverse ? -b->roll_dir : b->roll_dir)) {
		b->state = BAL_VERIFY_ROLL;
		b->verify_at = b->still_at = now;
		b->brake_raw = actual;
		b->quiet_frames = 0;
		sweep_rpm(b, 0, now);
		return;
	}
	int goal = b->roll_dir > 0 ? BAL_ROLL_CW_CD : -BAL_ROLL_CCW_CD;
	int error = goal - balance_shaft(b, actual);
	if (abs(error) <= 65) {
		if (!b->at_end) {
			b->at_end = true;
			b->end_at = now;
		}
		b->state = BAL_NO_TRACK;
	} else {
		if (b->at_end) {
			b->at_end = false;
			b->leg_at = now;
		}
		if (now - b->leg_at > 9000) {
			balance_stop(b, true);
			return;
		}
	}
	b->command = b->demand = goal;
	/* Positive shaft travel is the drive's CW direction (negative signed F6). */
	int rpm = clamp_int(abs(error) / 50, 3, 30);
	sweep_rpm(b, abs(error) <= 65 ? 0 : error > 0 ? -rpm : rpm, now);
}

void balance_update(struct balance *b, const struct ball_view *ball, uint16_t sequence,
		    int32_t actual, uint32_t now)
{
	uint32_t dt = now - b->control_at;
	if (!b->active || dt < 5)
		return;
	int shaft = balance_shaft(b, actual);
	b->control_at = now;
	int32_t trip_margin = (BAL_TRIP_CD - BAL_LIMIT_CD) * 65536 / 36000;
	bool outside = b->f407 ? actual <= b->lower - trip_margin || actual >= b->upper + trip_margin :
		       b->state != BAL_PARK && abs(shaft) >= BAL_TRIP_CD;
	if (dt >= 100 || outside) {
		balance_stop(b, true);
		return;
	}
	if (!b->f407) {
		if (b->state == BAL_PARK) {
			if (abs(shaft) <= 25)
				balance_stop(b, false);
			else if (now - b->started >= 35000)
				balance_stop(b, true);
			return;
		}
		b->state = now - b->started < 2800 ? BAL_PROBE : BAL_RETURN;
		b->demand = b->state == BAL_PROBE ? 600 : 0;
		int delta = b->demand - b->command, limit = (int)dt * 10;
		b->command += delta > limit ? limit : delta < -limit ? -limit : delta;
		if (!b->demand && !b->command && abs(shaft) <= 60)
			balance_stop(b, false);
		else if (now - b->started >= 5500)
			balance_stop(b, true);
		return;
	}

	if (b->state == BAL_FIND_BALL || b->state == BAL_CAMERA_OFF || b->state == BAL_NO_TRACK ||
	    b->state == BAL_VERIFY_ROLL) {
		sweep_for_ball(b, ball, sequence, actual, now);
		if (b->state != BAL_WAIT || !b->active)
			return;
	}
	struct ball_view accepted = *ball;
	uint32_t gap = now - b->sample_at;
	if (b->have_sample && ball->status == BALL_TRACKED && sequence != b->sequence) {
		int distance = abs(f407_site_x(b, ball) - b->last_site);
		/* A sudden jump after lock can be a different shiny object. A coherent
		 * moving track may rejoin after a gap, or a new frame near the last site. */
		if ((b->vision_paused || gap >= BAL_VISION_TIMEOUT_MS ? distance > 8 : distance > 7 + (int)gap) &&
		    !rolling_track(&b->motion, ball, sequence, actual, now))
			accepted.status = BALL_UNCERTAIN;
		else
			b->motion.frames = 0;
	} else if (ball->status != BALL_TRACKED) {
		rolling_track(&b->motion, ball, sequence, actual, now);
	}
	ball = &accepted;
	observe_ball(b, ball, sequence, now);
	if (now - b->sample_at >= BAL_VISION_PARK_MS) {
		bool search = b->lock_frames != 0;
		int32_t origin = b->origin;
		balance_stop(b, false);
		if (search && balance_roll_start(b, origin, actual, ball, now)) {
			b->state = ball->status == BALL_OFFLINE ? BAL_CAMERA_OFF : BAL_FIND_BALL;
			b->speed_due = true;
		} else {
			b->state = BAL_WAIT;
		}
		return;
	}
	if (ball->status == BALL_UNCERTAIN || now - b->sample_at >= BAL_VISION_TIMEOUT_MS ||
	    (b->vision_paused && ball->status != BALL_TRACKED)) {
		b->state = BAL_WAIT;
		b->centered = false;
		b->vision_paused = true;
		b->still_at = now;
		b->speed_due = b->native_rpm != 0 || due(&b->inner_at, BAL_F407_INNER_MS, now);
		b->native_rpm = 0;
		return;
	}
	if (b->vision_paused) {
		keep_out.Actual1 = (float)b->last_site;
		b->vision_paused = false;
		b->outer_at = now - BAL_F407_OUTER_MS;
	}
	best_hard();
	if (due(&b->outer_at, BAL_F407_OUTER_MS, now)) {
		site.X_0 = ball->status == BALL_TRACKED || !b->outer_started ? (int16_t)b->last_site : 0;
		Keep_Blance_out();
		b->outer_started = true;
		b->target_beam_cd = (int)(keep_inter.Target * 100.0f);
		/* Equivalent shaft target for the existing diagnostic display only. */
		b->demand = b->command = b->trim_mdeg / 10 -
			(int)(keep_inter.Target * 36000.0f / (3200.0f * 0.039f));
		b->state = b->centered && now - b->centered_at >= 1000 ? BAL_STABLE : BAL_FOLLOW;
	}
	if (due(&b->inner_at, BAL_F407_INNER_MS, now)) {
		/* motor_X.c decodes signed Emm 0x36 using 65535.0f. */
		Now_Angle = (float)((int64_t)actual - b->zero) / 65535.0f * 360.0f -
			    (float)b->trim_mdeg / 1000.0f;
		trace_balance_inter();
		b->actual_beam_cd = (int)(keep_inter.Actual * 100.0f);
		b->native_rpm = requested_rpm;
		b->speed_due = true;
	}
}

void balance_time_start(struct balance_timer *timer, unsigned int op, uint32_t now)
{
	*timer = (struct balance_timer){ .operation = op, .counting = true, .started = now };
}

void balance_time_update(struct balance_timer *timer, const struct balance *b, bool running,
			 uint32_t now)
{
	if (!timer->counting)
		return;
	timer->elapsed = now - timer->started;
	if (!timer->tracking && (b->state == BAL_FOLLOW || b->state == BAL_STABLE))
		timer->tracking = timer->elapsed;
	if (!timer->settled && b->state == BAL_STABLE)
		timer->settled = timer->elapsed;
	timer->counting = running;
}

const char *balance_name(enum balance_state state)
{
	static const char *const names[] = {
		"RUN TO CENTER", "READY", "PROBE +", "RETURN", "WAIT BALL",
		"CENTERING", "STEADY", "CHECK DRIVE", "LIFT", "ROLL TO O", "SEEK LIMIT", "BACK TO REST",
		"CHECK LIFT", "ROLL FIND BALL", "NO BALL ROLL", "CAM OFFLINE", "VERIFY ROLL"
	};
	return (unsigned int)state < sizeof(names)/sizeof(names[0]) ? names[state] : "ERROR";
}
