/* 八路灰度数字滤波和线位识别。按实物从左到右统计目标通道和连续段，为循线器提供带符号偏差。 */

#include "config.h"
#include "grayscale.h"

/* 从八位目标掩码统计探头数、连续段数和位置重心。 */
static void evaluate(struct grayscale *gray, bool white_line)
{
	struct gray_reading *reading = &gray->reading;
	int sum = 0;
	bool previous = false;
	unsigned int index;

	reading->black = (uint8_t)~gray->stable;
	reading->line = white_line ? gray->stable : reading->black;
	reading->count = 0;
	reading->groups = 0;
	reading->position = 0;
	for (index = 0; index < 8; index++) {
		bool active = !!(reading->line & (1U << index));

		if (active) {
			reading->count++;
			reading->groups += !previous;
			sum += (int)index * 1000 - 3500;
		}
		previous = active;
	}
	if (gray->settled != 0xff)
		reading->status = GRAY_WARMUP;
	else if (!reading->count)
		reading->status = GRAY_LOST;
	else if (reading->count == 8)
		reading->status = GRAY_FULL;
	else if (reading->groups != 1)
		reading->status = GRAY_SPLIT;
	else {
		reading->status = GRAY_LINE;
		reading->position = (int16_t)(sum / reading->count);
	}
}

/* 对多次采样做时间滤波，再按黑白极性识别线并计算偏差。 */
bool gray_update(struct grayscale *gray, uint8_t raw, uint32_t now,
		 bool white_line)
{
	unsigned int index;

	if (gray->started && now - gray->sampled_at < GRAY_SAMPLE_MS)
		return false;
	if (!gray->started) {
		/* 滤波器初值全为零，等待采满后才判黑线，避免显示八路全黑。 */
		gray->stable = 0xff;
		gray->started = true;
	}
	gray->sampled_at = now;
	gray->reading.raw = raw;
	for (index = 0; index < 8; index++) {
		uint8_t bit = (uint8_t)(1U << index);

		if ((raw ^ gray->candidate) & bit) {
			gray->candidate ^= bit;
			gray->streak[index] = 1;
		} else if (gray->streak[index] < GRAY_FILTER_SAMPLES) {
			gray->streak[index]++;
		}
		if (gray->streak[index] == GRAY_FILTER_SAMPLES) {
			gray->stable = (gray->stable & (uint8_t)~bit) | (raw & bit);
			gray->settled |= bit;
		}
	}
	evaluate(gray, white_line);
	return true;
}

/* 将识别结果转为屏幕和遥测共用的文字。 */
const char *gray_status_name(enum gray_status status)
{
	static const char *const names[] = {
		"WAIT", "NO LINE", "LINE", "ALL", "MULTI"
	};

	return (unsigned int)status <= GRAY_SPLIT ? names[status] : "INVALID";
}
