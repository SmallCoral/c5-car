/* 逻辑轮号与 J4、J6 电气通道的映射。输出、故障与编码器相位共用同一组换算。 */

#ifndef CORAL_WHEEL_MAP_H
#define CORAL_WHEEL_MAP_H

#include <stdint.h>
#include "config.h"

struct wheel_channels {
	int j4;
	int j6;
};

/* 板级接口统一使用实物左右轮，底层保留原理图 J4、J6 通道编号。 */
/* 将实物左右轮编号映射成板上的驱动通道。 */
static inline unsigned int wheel_channel(unsigned int physical_side)
{
	return physical_side ^ WHEEL_SWAP_SIDES;
}

/* 按当前线束把一对物理轮值写入对应电气通道。 */
static inline struct wheel_channels wheel_to_channels(int left, int right)
{
	return (struct wheel_channels) {
		.j4 = WHEEL_SWAP_SIDES ? right : left,
		.j6 = WHEEL_SWAP_SIDES ? left : right,
	};
}

/* 将电气故障掩码转换为实物左右轮故障位。 */
static inline uint8_t wheel_faults_to_physical(uint8_t channels)
{
	return WHEEL_SWAP_SIDES ?
		(uint8_t)(((channels & 1U) << 1) | ((channels >> 1) & 1U)) :
		(uint8_t)(channels & 3U);
}

/* 将编码器 A/B 两相位组换回实际车轮顺序。 */
static inline uint8_t wheel_phases_to_physical(uint8_t channels)
{
	return WHEEL_SWAP_SIDES ?
		(uint8_t)(((channels & 3U) << 2) | ((channels >> 2) & 3U)) :
		(uint8_t)(channels & 15U);
}

#endif
