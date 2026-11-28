/* 灰度滤波状态与识别输出，包括目标通道位图、黑线段数和以探头间距为基准的位置。 */

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
	/* 实车确认左侧 CH1 为 −3500，右侧 CH8 为 +3500，单位是探头间距的千分之一。 */
	int16_t position;
	enum gray_status status;
};

/* 首次使用前清零，随后由主循环独占更新。 */
struct grayscale {
	struct gray_reading reading;
	uint32_t sampled_at;
	uint8_t candidate;
	uint8_t stable;
	uint8_t settled;
	uint8_t streak[8];
	bool started;
};

/* 对多次采样做时间滤波，再按黑白极性识别线并计算偏差。 */
bool gray_update(struct grayscale *gray, uint8_t raw, uint32_t now,
		 bool white_line);
/* 将识别结果转为屏幕和遥测共用的文字。 */
const char *gray_status_name(enum gray_status status);

#endif
