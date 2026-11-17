#ifndef CORAL_GRAY_MAP_H
#define CORAL_GRAY_MAP_H

#include <stdint.h>
#include "config.h"

/* Preserve electrical levels; expose bit0 as the physical leftmost probe. */
static inline uint8_t gray_input_order(uint8_t electrical)
{
	if (!GRAY_REVERSE_CHANNELS)
		return electrical;
	electrical = (uint8_t)((electrical >> 4) | (electrical << 4));
	electrical = (uint8_t)(((electrical & 0xccU) >> 2) |
			       ((electrical & 0x33U) << 2));
	return (uint8_t)(((electrical & 0xaaU) >> 1) |
			((electrical & 0x55U) << 1));
}

#endif
