#ifndef CORAL_WHEEL_MAP_H
#define CORAL_WHEEL_MAP_H

#include <stdint.h>
#include "config.h"

struct wheel_channels {
	int j4;
	int j6;
};

/* Public board APIs use physical left/right; schematic channels stay J4/J6. */
static inline unsigned int wheel_channel(unsigned int physical_side)
{
	return physical_side ^ WHEEL_SWAP_SIDES;
}

static inline struct wheel_channels wheel_to_channels(int left, int right)
{
	return (struct wheel_channels) {
		.j4 = WHEEL_SWAP_SIDES ? right : left,
		.j6 = WHEEL_SWAP_SIDES ? left : right,
	};
}

static inline uint8_t wheel_faults_to_physical(uint8_t channels)
{
	return WHEEL_SWAP_SIDES ?
		(uint8_t)(((channels & 1U) << 1) | ((channels >> 1) & 1U)) :
		(uint8_t)(channels & 3U);
}

static inline uint8_t wheel_phases_to_physical(uint8_t channels)
{
	return WHEEL_SWAP_SIDES ?
		(uint8_t)(((channels & 3U) << 2) | ((channels >> 2) & 3U)) :
		(uint8_t)(channels & 15U);
}

#endif
