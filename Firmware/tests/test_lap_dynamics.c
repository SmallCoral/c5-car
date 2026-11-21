/* Synthetic differential-drive regression, not a calibrated vehicle model.
 * Units/geometry below are explicit test assumptions. The controller is fed
 * quantized GPIO patterns and integer encoder counts, never desired speeds.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "grayscale.h"
#include "lap.h"

enum path { STRAIGHT, RIGHT_BEND, LEFT_BEND, REVERSE_BENDS, A_COURSE, UNMARKED_COURSE };

/* Track geometry in mm, independent of the firmware's encoder windows. */
static const double half_lap_mm = 1500.0 + 500.0 * 3.141592653589793;

static bool on_arc(double distance)
{
	return (distance >= 1500 && distance < half_lap_mm) ||
	       (distance >= half_lap_mm + 1500 && distance < 2 * half_lap_mm);
}

static double arc_offset(double axle, double ahead, double start, double end)
{
	/* Integrate the bend over the portion actually in front of the axle.
	 * The probe bar crosses the tangent before the axle does.
	 */
	double a = fmin(ahead, fmax(0, start - axle));
	double b = fmin(ahead, fmax(0, end - axle));

	return ((ahead - a) * (ahead - a) - (ahead - b) * (ahead - b)) / 1000.0;
}

static double path_curvature(enum path path, uint32_t now)
{
	const double bend = 1.0 / 500.0;

	if (now < 4000 || path == STRAIGHT)
		return 0;
	if (path == RIGHT_BEND)
		return bend;
	if (path == LEFT_BEND)
		return -bend;
	/* A right arc, a short straight, a left arc and a straight exit. */
	if (now < 6500)
		return bend;
	if (now >= 7200 && now < 9700)
		return -bend;
	return 0;
}

static bool simulate(double lookahead, double motor_ms, enum path path, bool jitter,
		     bool weak_motors, unsigned int level, double counts_per_mm, uint32_t *a_counts)
{
	const double dt = 0.002;
	const double track_mm = 210.0;
	const bool course = path == A_COURSE || path == UNMARKED_COURSE;
	const double pitch = course ? 12.0 : 10.0;
	/* The original low-gain plant cannot reach 3300 CPS at 45% PWM.
	 * The second plant can; both have unequal left/right wheel gains.
	 */
	const double gain[2] = { weak_motors ? 6.5 : 8.2, weak_motors ? 5.5 : 7.7 };
	struct grayscale gray = { 0 };
	struct lap lap;
	double speed[2] = { 0, 0 }, counts[2] = { 0, 0 };
	double lateral = course ? 0.0 : 10.0, heading = course ? 0.0 : 0.08;
	double traveled = 0;
	double sum = 0, squared = 0, high = -999, low = 999, straight_speed = 0;
	double bend_speed = 0;
	unsigned int bend_samples = 0;
	unsigned int measured = 0, centered = 0, cruise_samples = 0, gaps = 0;
	uint32_t next_control = 10;
	bool running = true;

	for (uint32_t now = 0; now <= (course ? 45000U : 12000U); now += 2) {
		double curvature = path_curvature(path, now);
		double ahead = lookahead * cos(heading);
		double offset = curvature * ahead * ahead / 2;
		uint8_t raw = 0xff;

		if (course) {
			double axle = traveled - lookahead;

			curvature = on_arc(axle) ? 1.0 / 500.0 : 0;
			offset = arc_offset(axle, ahead, 1500, half_lap_mm) +
				arc_offset(axle, ahead, half_lap_mm + 1500, 2 * half_lap_mm);
		}
		double front = lateral + lookahead * sin(heading) - offset;
		for (int probe = 0; probe < 8; probe++) {
			double distance = front + (probe - 3.5) * pitch * cos(heading);

			if (fabs(distance) <= 10.0)
				raw &= (uint8_t)~(1U << probe);
		}
		if (path == A_COURSE && (traveled < 35 || fabs(traveled - 2 * half_lap_mm) < 15))
			raw = 0;	/* the physical transverse mark */
		gray_update(&gray, raw, now, false);
		if (now == 4) {
			if (path == UNMARKED_COURSE) {
				if (!lap_start_distance(&lap, &gray.reading, now, 0, 0, level, *a_counts))
					return false;
			} else if (!lap_start(&lap, &gray.reading, now, 0, 0, level, false)) {
				return false;
			}
		}
		if (now < 4)
			continue;
		if (now >= next_control) {
			lap_update(&lap, &gray.reading, now, (uint32_t)counts[0],
				   (uint32_t)counts[1]);
			next_control = now + (jitter && now % 3 ? 14 : 10);
		}
		if (lap.status != LAP_RUNNING) {
			running = false;
			break;
		}
		for (int side = 0; side < 2; side++) {
			speed[side] += (lap.wheel[side].pwm * gain[side] - speed[side]) *
				2.0 / (motor_ms + 2.0);
			counts[side] += speed[side] * dt;
		}
		double forward = (speed[0] + speed[1]) / (2 * counts_per_mm);
		double yaw = (speed[0] - speed[1]) / (counts_per_mm * track_mm);
		traveled += forward * cos(heading) * dt / (1 - curvature * lateral);
		lateral += forward * sin(heading) * dt;
		heading += (yaw - forward * cos(heading) * curvature /
			    (1 - curvature * lateral)) * dt;
		if (course ? (traveled >= 400 && traveled < 1250) ||
			     (traveled >= 3450 && traveled < 4250) :
		    now >= 2000 && now < 4000) {
			straight_speed += (speed[0] + speed[1]) / 2;
			cruise_samples++;
		}
		if (course && ((traveled >= 1850 && traveled < 2850) ||
			       (traveled >= 4900 && traveled < 5500))) {
			bend_speed += (speed[0] + speed[1]) / 2;
			bend_samples++;
		}
		if (now >= 4000 && gray.reading.status == GRAY_LOST)
			gaps++;
		/* Include every entry, reversal and exit on the alternating course.
		 * A short white gap counts even if it never trips the stop timeout.
		 */
		if (course ? traveled > 300 && traveled < 6000 :
		    now >= (path == REVERSE_BENDS ? 4000U : 8000U)) {
			sum += lateral;
			squared += lateral * lateral;
			measured++;
			if (lateral > high)
				high = lateral;
			if (lateral < low)
				low = lateral;
			if (gray.reading.status == GRAY_LINE &&
			    abs(gray.reading.position) <= 500)
				centered++;
		}
	}
	double mean = measured ? sum / measured : 999;
	double variance = measured ? squared / measured - mean * mean : 999;
	double rms = sqrt(fmax(0, variance));
	/* The centered two-probe mask cannot resolve +/-5 mm: distinguish bias
	 * from oscillation. Require convergence within one probe interval.
	 * Curved-path bias also depends on the synthetic sensor lookahead.
	 */
	double center_pct = measured ? 100.0 * centered / measured : 0;
	double cruise_cps = cruise_samples ? straight_speed / cruise_samples : 0;
	double bend_cps = bend_samples ? bend_speed / bend_samples : 0;
	bool race = level == LAP_RACE_LEVEL;
	bool axle_stable = path == REVERSE_BENDS ||
		(fabs(mean) < (path == STRAIGHT ? 5.0 : 40.0) && high - low < 10.0);
	/* A controller that only slows down must fail on a capable straight. */
	bool passed = running && !gaps && axle_stable &&
		(weak_motors || cruise_cps >= (race ? 4000.0 : 3000.0)) &&
		center_pct >= (path == REVERSE_BENDS ? 65.0 : 90.0);
	if (course)
		passed = lap.status == LAP_DONE && fabs(traveled - 2 * half_lap_mm) <= 25 &&
			!gaps && center_pct >= 75.0 &&
			(!race || weak_motors || (cruise_cps >= 4000.0 && bend_cps >= 2750.0));
	if (path == UNMARKED_COURSE)
		passed = passed && !lap.marker_hits && lap_progress(&lap) >= *a_counts &&
			 lap_progress(&lap) < *a_counts + 50U;
	if (path == A_COURSE && a_counts && passed)
		*a_counts = lap_progress(&lap);

	printf("level=%u counts_per_mm=%.2f lookahead=%.0f motor_ms=%.0f path=%d jitter=%u weak=%u status=%s "
	       "bias_mm=%.3f sway_rms_mm=%.3f sway_pp_mm=%.3f center_pct=%.1f "
	       "gap_ms=%u cruise_cps=%.0f bend_cps=%.0f %s\n",
	       level, counts_per_mm, lookahead, motor_ms, path, jitter, weak_motors, lap_status_name(lap.status),
	       mean, rms, high - low, center_pct, gaps * 2,
	       cruise_cps, bend_cps, passed ? "PASS" : "FAIL");
	return passed;
}

static bool test_gears(double lookahead, double motor_ms, enum path path, bool jitter,
		       bool weak)
{
	uint32_t fast_counts = 0, race_counts = 0;
	bool passed = simulate(lookahead, motor_ms, path, jitter, weak, LAP_FAST_LEVEL, 7.0, &fast_counts);

	passed &= simulate(lookahead, motor_ms, path, jitter, weak, LAP_RACE_LEVEL, 7.0, &race_counts);
	if (path == A_COURSE) {
		passed &= simulate(lookahead, motor_ms, path, jitter, weak, LAP_RACE_LEVEL, 6.86, NULL);
		passed &= simulate(lookahead, motor_ms, path, jitter, weak, LAP_RACE_LEVEL, 7.14, NULL);
		/* Use the distance measured on this chassis, as on the real cart.
		 * Remove both marks, then check against the independent track geometry. */
		passed &= simulate(lookahead, motor_ms, UNMARKED_COURSE, jitter, weak,
				   LAP_FAST_LEVEL, 7.0, &fast_counts);
		passed &= simulate(lookahead, motor_ms, UNMARKED_COURSE, jitter, weak,
				   LAP_RACE_LEVEL, 7.0, &race_counts);
	}
	return passed;
}

int main(int argc, char **argv)
{
	bool passed = true;
	bool report = argc == 2 && !strcmp(argv[1], "--report");

	for (int lookahead = 80; lookahead <= 160; lookahead += 40)
		for (int motor = 50; motor <= 150; motor += 50)
			for (enum path path = STRAIGHT; path <= A_COURSE; path++)
				for (int jitter = 0; jitter <= 1; jitter++)
					for (int weak = 0; weak <= 1; weak++)
						passed &= test_gears(lookahead, motor, path, jitter, weak);
	return passed || report ? 0 : 1;
}
