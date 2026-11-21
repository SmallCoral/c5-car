/* 按实车采集时间逐帧回放，核对控制阶段与目标轴角。 */
#include <stdio.h>
#include <stdlib.h>
#include "contest.h"

int main(int argc, char **argv)
{
	if (argc != 3)
		return 1;
	FILE *trace = fopen(argv[1], "r");
	char line[256];
	if (!trace || !fgets(line, sizeof(line), trace))
		return 1;

	struct position_run run = {0};
	uint32_t epoch = 10000;
	unsigned int samples = 0, previous_ms = 0;
	unsigned int previous_seq = 0, previous_rx = 0, previous_view = BALL_OFFLINE;
	int previous_x = 0;
	while (fgets(line, sizeof(line), trace)) {
		unsigned int ms, seq, rx_age, view, leg;
		int x, goal;
		if (sscanf(line, "%u,%u,%u,%*u,%d,%*d,%d,%*d,%*u,%u,%u",
			   &ms, &seq, &rx_age, &x, &goal, &view, &leg) != 7)
			return 1;
		uint32_t now = epoch + ms, received = now - rx_age;
		if (!samples) {
			position_begin(&run, view == BALL_TRACKED ? x : -1, seq, now);
		} else {
			/* 两帧之间继续推进主控时钟，以复现等待相机期间的控制行为。 */
			for (unsigned int gap = previous_ms + 1; gap < ms; gap++) {
				if (position_update(&run, previous_view, previous_x, previous_seq,
						    previous_rx, epoch + gap) != NOTICE_NONE)
					return 1;
			}
			/* 记录只包含阶段切换。切到回中时补入已满足的水平确认时间。 */
			if (run.leg == LEVEL_SHAFT && leg == MOVE_CENTER) {
				position_check_level(&run, true, now - 75);
				position_check_level(&run, true, now);
			}
			if (position_update(&run, view, x, seq, received, now) != NOTICE_NONE)
				return 1;
		}
		if (run.leg != (enum position_leg)leg || run.goal_cd != goal) {
			fprintf(stderr, "%s at %u ms: leg %u/%u, goal %d/%d centidegrees\n",
				argv[1], ms, (unsigned)run.leg, leg, run.goal_cd, goal);
			return 1;
		}
		previous_ms = ms;
		previous_view = view;
		previous_seq = seq;
		previous_rx = received;
		previous_x = x;
		samples++;
	}
	if (ferror(trace) || previous_ms < 13900)
		return 1;
	fclose(trace);
	printf("%u samples; final leg %u; settled at %u ms\n", samples, (unsigned)run.leg, run.finished_ms);
	return run.leg != HOLD_LEFT || run.finished_ms != strtoul(argv[2], NULL, 10);
}
