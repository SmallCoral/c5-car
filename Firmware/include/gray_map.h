/* 灰度线束的逻辑顺序映射，将 MCU 的输入位转换为实物左到右的 CH1～CH8。 */

#ifndef CORAL_GRAY_MAP_H
#define CORAL_GRAY_MAP_H

#include <stdint.h>
#include "config.h"

/* 保留黑白电平，只调整位序，使位 0 表示实物最左侧探头。 */
/* 将电气引脚位序换为实物左到右的位序，保留原输入电平。 */
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
