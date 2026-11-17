#ifndef CORAL_GRAYSCALE_H
#define CORAL_GRAYSCALE_H

#include <stdbool.h>
#include <stdint.h>

enum gray_status {
	GRAY_WARMUP,
	GRAY_LOST,
	GRAY_LINE,
	GRAY_FULL,
	GRAY_SPLIT,
};

struct gray_reading {
	uint8_t raw;
	uint8_t black;
	uint8_t line;
	uint8_t count;
	uint8_t groups;
	/* User verified: left CH1 = -3500, right CH8 = +3500; not millimetres. */
	int16_t position;
	enum gray_status status;
};

/* Zero-initialize before first use. Owned entirely by the main loop. */
struct grayscale {
	struct gray_reading reading;
	uint32_t sampled_at;
	uint8_t candidate;
	uint8_t stable;
	uint8_t settled;
	uint8_t streak[8];
	bool started;
};

bool gray_update(struct grayscale *gray, uint8_t raw, uint32_t now,
		 bool white_line);
const char *gray_status_name(enum gray_status status);

#endif
