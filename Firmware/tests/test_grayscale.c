#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "config.h"
#include "grayscale.h"
#include "logic.h"
#include "gray_map.h"

static void settle(struct grayscale *gray, uint8_t raw, bool white)
{
	for (unsigned int i = 0; i < GRAY_FILTER_SAMPLES; i++)
		assert(gray_update(gray, raw, gray->sampled_at + GRAY_SAMPLE_MS, white));
}

static void test_patterns(void)
{
	/* Independent reference: find each contiguous run, then use its endpoints. */
	for (unsigned int mask = 0; mask < 256; mask++) {
		for (unsigned int white = 0; white < 2; white++) {
			struct grayscale gray = { 0 };
			unsigned int first = 8, last = 0, count = 0, groups = 0;
			bool in_run = false;

			for (unsigned int ch = 0; ch < 8; ch++) {
				if (mask & (1U << ch)) {
					if (first == 8)
						first = ch;
					last = ch;
					count++;
					if (!in_run)
						groups++;
					in_run = true;
				} else {
					in_run = false;
				}
			}
			settle(&gray, white ? (uint8_t)mask : (uint8_t)~mask, white);
			assert(gray.reading.line == mask);
			assert(gray.reading.black == (white ? (uint8_t)~mask : mask));
			assert(gray.reading.count == count && gray.reading.groups == groups);
			if (!count)
				assert(gray.reading.status == GRAY_LOST);
			else if (count == 8)
				assert(gray.reading.status == GRAY_FULL);
			else if (groups > 1)
				assert(gray.reading.status == GRAY_SPLIT);
			else {
				assert(gray.reading.status == GRAY_LINE);
				assert(gray.reading.position == (int)(first + last) * 500 - 3500);
			}
			if (gray.reading.status != GRAY_LINE)
				assert(gray.reading.position == 0);
		}
	}
}

static void test_filter(void)
{
	struct grayscale gray = { 0 };

	assert(gray_update(&gray, 0xff, UINT32_MAX - 2, false));
	assert(gray.reading.status == GRAY_WARMUP);
	assert(!gray_update(&gray, 0, UINT32_MAX - 1, false));
	assert(gray.reading.raw == 0xff);
	assert(gray_update(&gray, 0xff, UINT32_MAX, false));
	assert(gray.reading.status == GRAY_WARMUP);
	assert(gray_update(&gray, 0xff, 1, false));
	assert(gray.reading.status == GRAY_LOST);
	/* A one-sample black glitch must not move the filtered line. */
	assert(gray_update(&gray, 0xfe, 3, false));
	assert(gray.reading.raw == 0xfe && gray.reading.black == 0);
	settle(&gray, 0xff, false);
	assert(gray.reading.status == GRAY_LOST);
	/* CH1 chatter must not prevent stable CH4/5 from being accepted. */
	assert(gray_update(&gray, 0xe6, 11, false));
	assert(gray_update(&gray, 0xe7, 13, false));
	assert(gray_update(&gray, 0xe6, 15, false));
	assert(gray.reading.black == 0x18);
	assert(gray.reading.status == GRAY_LINE && gray.reading.position == 0);
	/* A confirmed line loss clears its position instead of reusing an old turn. */
	settle(&gray, 0xfe, false);
	assert(gray.reading.position == -3500);
	settle(&gray, 0xff, false);
	assert(gray.reading.status == GRAY_LOST && gray.reading.position == 0);
	assert(gray_update(&gray, 0xff, gray.sampled_at + 2, true));
	assert(gray.reading.status == GRAY_FULL);
}

static void test_gray_controls(void)
{
	struct controls ctl = { .task = TASK_ALL, .level = WHEEL_DEFAULT_LEVEL };

	controls_key(&ctl, KEY_MODE, 0);
	assert(ctl.task == TASK_GRAY);
	controls_key(&ctl, KEY_START, 1);
	assert(!ctl.running && !task_uses_wheels(ctl.task) && !task_uses_stepper(ctl.task));
	controls_key(&ctl, KEY_NEXT, 2);
	assert(ctl.gray_white && ctl.level == WHEEL_DEFAULT_LEVEL);
	controls_key(&ctl, KEY_OK, 3);
	assert(!ctl.reverse);
	controls_key(&ctl, KEY_NEXT, 4);
	assert(!ctl.gray_white);
	controls_key(&ctl, KEY_MODE, 5);
	assert(ctl.task == TASK_LAP);
	controls_key(&ctl, KEY_MODE, 6);
	assert(ctl.task == TASK_MONITOR);
}

int main(void)
{
	/* User measured PE14 as physical left and PE7 as physical right. */
	assert(gray_input_order(0x7f) == 0xfe);
	assert(gray_input_order(0xfe) == 0x7f);
	assert(gray_input_order(0xe7) == 0xe7);
	assert(gray_input_order(0xff) == 0xff && gray_input_order(0) == 0);
	test_patterns();
	test_filter();
	test_gray_controls();
	puts("PASS: 512 polarity/pattern cases, per-channel filtering, wrap, safe gray controls");
	return 0;
}
