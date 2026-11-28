/* 第一题实车轨迹回放。逐样本检查水平确认、三段移球与完成后保持。 */

/* 按实车采集时间逐帧回放，核对控制阶段与目标轴角。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contest.h"

/* 读取指定实车轨迹，按主控时钟回放并逐行对照阶段、目标轴角和完成时刻。 */
int main(int argc, char **argv)
{
	if (argc != 3 && argc != 6)
		return 1;
	unsigned long kick_from = argc == 6 ? strtoul(argv[3], NULL, 10) : 0;
	unsigned long kick_to = argc == 6 ? strtoul(argv[4], NULL, 10) : 0;
	int kick_cd = argc == 6 ? atoi(argv[5]) : 0;
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
		/* 历史 CSV 的 direct_mm 是当时实测的物理位置。按现行标定投影后，逐帧核对原始轴角、阶段和停稳时间。 */
		if (view == BALL_TRACKED) {
			const char *last = strrchr(line, ',');
			int mm;
			if (!last || sscanf(last + 1, "%d", &mm) != 1)
				return 1;
			x = ball_x_from_mm(mm);
		}
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
		/* 已采集的球位继续作为输入，只有已知过 O 快抬的采样窗口使用新目标。 */
		bool kick = argc == 6 && ms >= kick_from && ms <= kick_to;
		if (kick)
			goal = kick_cd;
		if (run.crossing_fast != kick || run.leg != (enum position_leg)leg || run.goal_cd != goal) {
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
