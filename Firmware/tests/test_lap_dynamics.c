/* Synthetic differential-drive regression, not a calibrated vehicle model.
 * Units/geometry below are explicit test assumptions. The controller is fed
 * quantized GPIO patterns and integer encoder counts, never desired speeds.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "grayscale.h"
#include "lap.h"

static bool simulate(double lookahead, double motor_ms, bool bend, bool jitter)
{
	const double dt = 0.002;
	const double counts_per_mm = 7.0;
	const double track_mm = 210.0;
	const double gain[2] = { 6.5, 5.5 };
	struct grayscale gray = { 0 };
	struct lap lap;
	double speed[2] = { 0, 0 }, counts[2] = { 0, 0 };
	double lateral = 10.0, heading = 0.08;
	double sum = 0, squared = 0, high = -999, low = 999;
	unsigned int measured = 0;
	uint32_t next_control = 10;
	bool running = true;

	for (uint32_t now = 0; now <= 12000; now += 2) {
		double curvature = bend && now >= 4000 ? 1.0 / 500.0 : 0;
		double front = lateral + lookahead * sin(heading) -
			curvature * pow(lookahead * cos(heading), 2) / 2;
		uint8_t raw = 0xff;

		for (int probe = 0; probe < 8; probe++) {
			double distance = front + (probe * 10.0 - 35.0) * cos(heading);

			if (fabs(distance) <= 10.0)
				raw &= (uint8_t)~(1U << probe);
		}
		gray_update(&gray, raw, now, false);
		if (now == 4 && !lap_start(&lap, &gray.reading, now, 0, 0, 2, false))
			return false;
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
		lateral += forward * sin(heading) * dt;
		heading += (yaw - forward * cos(heading) * curvature /
			    (1 - curvature * lateral)) * dt;
		if (now >= 8000) {
			sum += lateral;
			squared += lateral * lateral;
			measured++;
			if (lateral > high)
				high = lateral;
			if (lateral < low)
				low = lateral;
		}
	}
	double mean = measured ? sum / measured : 999;
	double variance = measured ? squared / measured - mean * mean : 999;
	double rms = sqrt(fmax(0, variance));
	/* The centered two-probe mask cannot resolve +/-5 mm: distinguish bias
	 * from oscillation. Require convergence within one probe interval.
	 * Curved-path bias also depends on the synthetic sensor lookahead.
	 */
	bool passed = running && measured && fabs(mean) < (bend ? 40.0 : 5.0) &&
		high - low < 10.0;

	printf("lookahead=%.0f motor_ms=%.0f bend=%u jitter=%u status=%s "
	       "bias_mm=%.3f sway_rms_mm=%.3f sway_pp_mm=%.3f %s\n",
	       lookahead, motor_ms, bend, jitter, lap_status_name(lap.status),
	       mean, rms, high - low, passed ? "PASS" : "FAIL");
	return passed;
}

int main(int argc, char **argv)
{
	bool passed = true;
	bool report = argc == 2 && !strcmp(argv[1], "--report");

	for (int lookahead = 80; lookahead <= 160; lookahead += 40)
		for (int motor = 50; motor <= 150; motor += 50)
			for (int bend = 0; bend <= 1; bend++)
				for (int jitter = 0; jitter <= 1; jitter++)
					passed &= simulate(lookahead, motor, bend, jitter);
	return passed || report ? 0 : 1;
}
