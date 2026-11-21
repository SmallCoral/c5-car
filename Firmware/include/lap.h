#ifndef CORAL_LAP_H
#define CORAL_LAP_H

#include "grayscale.h"

enum lap_status {
	LAP_IDLE,
	LAP_RUNNING,
	LAP_DONE,
	LAP_CANCELLED,
	LAP_NO_LINE,
	LAP_AMBIGUOUS,
	LAP_ENCODER,
	LAP_TIMEOUT,
	LAP_DRIVE,
	LAP_NOT_READY,
	LAP_TIMING,
	LAP_CHECK,
	LAP_NO_MARKER,
};

struct lap_wheel {
	uint32_t previous;
	uint32_t progress;
	uint32_t moved_at;
	uint32_t reverse_ms;
	int32_t cps;
	int32_t integral;
	int target;
	int pwm;
};

struct lap {
	enum lap_status status;
	struct lap_wheel wheel[2];
	uint32_t started_at;
	uint32_t updated_at;
	uint32_t elapsed_ms;
	uint32_t line_at;
	uint32_t narrow_at;
	uint32_t marker_at;
	uint32_t finish_counts;	/* calibrated A-to-A distance, zero in marker mode */
	uint32_t control_ms;
	unsigned int marker_hits;
	unsigned int marker_peak;
	int last_position;
	int position_rate;
	int differential;
	int bend_ratio_ppm;	/* learned half wheel-speed difference / mean */
	int base_target;
	int steering;
	unsigned int level;
	bool reverse;
	bool carry_ball;
	bool departed;
	bool departing;
	bool from_a;
	bool narrow;
	bool marker;
	bool armed;
};

enum lap_start_surface { LAP_START_UNALIGNED, LAP_START_NARROW, LAP_START_MARKER };

enum lap_start_surface lap_start_surface(const struct gray_reading *gray);
bool lap_start(struct lap *lap, const struct gray_reading *gray, uint32_t now,
	       uint32_t left, uint32_t right, unsigned int level, bool reverse);
bool lap_start_distance(struct lap *lap, const struct gray_reading *gray, uint32_t now,
			uint32_t left, uint32_t right, unsigned int level, uint32_t finish_counts);
void lap_check_start(struct lap *lap, uint32_t now, uint32_t left,
		     uint32_t right, bool reverse);
bool lap_update(struct lap *lap, const struct gray_reading *gray, uint32_t now,
		uint32_t left, uint32_t right);
void lap_stop(struct lap *lap, enum lap_status status);
uint32_t lap_progress(const struct lap *lap);
int lap_speed(unsigned int level);
const char *lap_status_name(enum lap_status status);

#endif
