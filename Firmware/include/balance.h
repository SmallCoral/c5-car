/* 调试页寻球与稳球的状态、编码器原点、运动证据及计时器，供主循环和主机回归共用。 */

#ifndef CORAL_BALANCE_H
#define CORAL_BALANCE_H

#include "ball.h"
#include "stepper.h"

/* 保留旧遥测使用的状态编号，以便回放历史实车记录。 */
enum balance_state {
	BAL_NEED_LEVEL, BAL_READY, BAL_PROBE, BAL_RETURN, BAL_WAIT,
	BAL_FOLLOW, BAL_STABLE, BAL_FAULT, BAL_SEEK, BAL_COAST, BAL_SEEK_LIMIT, BAL_PARK,
	BAL_TESTED, BAL_FIND_BALL, BAL_NO_TRACK, BAL_CAMERA_OFF, BAL_VERIFY_ROLL
};

enum { BAL_LIMIT_CD = 800, BAL_TRIP_CD = 950, BAL_SEEK_CD = 9000,
       BAL_LIMIT_RAW = BAL_LIMIT_CD * 65536 / 36000, BAL_SEEK_RAW = BAL_SEEK_CD * 65536 / 36000,
       BAL_F407_O = 160, BAL_F407_OUTER_MS = 20, BAL_F407_INNER_MS = 15,
       BAL_VISION_TIMEOUT_MS = 120, BAL_VISION_PARK_MS = 3000,
       BAL_ROLL_CCW_RAW = -SHAFT_HIGH_OFFSET, BAL_ROLL_CW_RAW = SHAFT_LOW_OFFSET,
       BAL_ROLL_CCW_CD = BAL_ROLL_CCW_RAW * 36000 / 65536,
       BAL_ROLL_CW_CD = BAL_ROLL_CW_RAW * 36000 / 65536 };

struct ball_motion {
	uint32_t since, last_at;
	uint16_t seq;
	int first, last, direction, steps, frames;
	int32_t first_shaft;
};

struct balance {
	enum balance_state state;
	bool active, leveled, reverse, f407, centered, have_sample, speed_due, outer_started, vision_paused;
	int32_t zero, lower, upper;
	int command, demand, velocity, shaft_rpm, native_rpm, last_position, last_site;
	int target_beam_cd, actual_beam_cd, trim_mdeg, still_site;
	uint16_t sequence;
	uint32_t started, control_at, outer_at, inner_at, sample_at, centered_at, still_at;
	struct ball_motion motion;
	int32_t origin, roll_start_raw, level_hint;
	bool manual_level, at_end, roll_moved;
	int roll_dir;
	unsigned int legs, lock_frames;
	uint32_t leg_at, end_at, lock_at;
	int lock_travel, lock_rate;
	uint32_t verify_at;
	int32_t brake_raw;
	unsigned int quiet_frames;
};

struct balance_timer {
	unsigned int operation;
	bool counting;
	uint32_t started, elapsed, tracking, settled;
};

/* 在停止时以当前读数建立水平参考。 */
bool balance_level(struct balance *b, int32_t actual);
/* 从已标定水平开始试抬，检查当前轴位在允许的试探行程内。 */
bool balance_probe(struct balance *b, int32_t actual, uint32_t now);
/* 在允许的搜索范围内开始转轴寻球，记录起点及初始观测。 */
bool balance_roll_start(struct balance *b, int32_t origin, int32_t actual,
			const struct ball_view *ball, uint32_t now);
/* 根据真实首帧和参考轴位初始化旧控制器的回中闭环。 */
bool balance_f407_start(struct balance *b, int32_t reference, int32_t actual,
			const struct ball_view *ball, uint32_t now);
/* 以给定原点为目的地开始返回，先确认当前位置在允许范围内。 */
bool balance_park(struct balance *b, int32_t origin, int32_t actual, uint32_t now);
/* 结束寻球或稳球，清零转速请求并保存正常或故障结束状态。 */
void balance_stop(struct balance *b, bool fault);
/* 将多圈编码器相对水平的差值换算为百分之一度并限幅。 */
int balance_shaft(const struct balance *b, int32_t actual);
/* 把控制器目标轴角折算回驱动的绝对编码器位置。 */
int32_t balance_goal(const struct balance *b);
/* 按当前状态驱动试抬、寻球和回中，并按不同周期更新位置和转速环。 */
void balance_update(struct balance *b, const struct ball_view *ball, uint16_t sequence,
		    int32_t actual, uint32_t now);
/* 从操作被接受的时刻开始计时，清除前一次的分段成绩。 */
void balance_time_start(struct balance_timer *timer, unsigned int op, uint32_t now);
/* 随控制状态更新跟踪、居中和结束时刻，保留实测耗时。 */
void balance_time_update(struct balance_timer *timer, const struct balance *b, bool running,
			 uint32_t now);
/* 把稳球阶段转换成调试页可显示的名字。 */
const char *balance_name(enum balance_state state);

#endif
