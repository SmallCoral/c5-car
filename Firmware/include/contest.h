/* 比赛菜单、状态编号与稳球运行数据。球位和误差用毫米，目标轴角用百分之一度，停留时间由新鲜观测的毫秒时间戳计算。 */

#ifndef CORAL_CONTEST_H
#define CORAL_CONTEST_H

#include "ball.h"
#include "lap.h"
#include "logic.h"

enum contest_page { MENU_DIAGNOSTIC, MENU_HOME, MENU_DETAIL, MENU_RUN, MENU_RESULT, MENU_TOOLS };
enum contest_item { EVENT_POSITION, EVENT_LAP, EVENT_HALF, EVENT_FULL, EVENT_OFFSET, EVENT_VIDEO, EVENT_COUNT };
enum contest_notice { NOTICE_NONE, NOTICE_CENTER, NOTICE_DRIVE, NOTICE_VISION, NOTICE_TIMEOUT,
	NOTICE_STOPPED, NOTICE_FINISHED, NOTICE_LINE, NOTICE_PENDING, NOTICE_TRAVEL };
enum menu_action { MENU_NONE, MENU_START_EVENT, MENU_STOP_EVENT, MENU_OPEN_TOOL, MENU_RELEVEL, MENU_LAP_SPEED, MENU_START_LAP };

enum { CONTEST_LAP_LIMIT_MS = 30000 };

struct contest_menu {
	enum contest_page page;
	enum contest_item item;
	enum contest_notice notice;
	unsigned int tool;
	int target_mm;
	bool offset_driving;
};

/* 按当前页面解释同一个按键，返回交给主循环执行的动作。发挥一起跑前允许改点，行驶时锁定目标。 */
enum menu_action contest_key(struct contest_menu *menu, enum key_id key);
/* 从调试菜单索引取得对应设备任务。 */
enum task_id contest_tool(unsigned int index);
/* 把循线结束原因和圈时换成比赛结果页提示。 */
enum contest_notice contest_lap_notice(enum lap_status status, uint32_t elapsed_ms, uint32_t limit_ms);

/* 枚举编号用于历史遥测。第一题的实际执行顺序由状态转换决定，先水平，再 O、右、左，最后驻留左点。 */
enum position_leg { MOVE_RIGHT, MOVE_LEFT, HOLD_LEFT, LEVEL_SHAFT, MOVE_CENTER, MOVE_TARGET, HOLD_TARGET };
enum { POSITION_CENTER_DWELL_MS = 300, POSITION_RIGHT_DWELL_MS = 400, POSITION_LEFT_DWELL_MS = 400 };
enum { POSITION_TOLERANCE_MM = 10, POSITION_SPAN_MM = 4,
	POSITION_BANDS = 2 * POSITION_TOLERANCE_MM - POSITION_SPAN_MM + 1 };
struct position_run {
	enum position_leg leg;
	/* 前三项是主控毫秒时钟的时间戳，finished_ms 是从本轮开始算起的完成耗时。 */
	uint32_t start, sample_at, steady_at, finished_ms;
	uint16_t seq;
	/* 实测球位单位为毫米，滤波球速为毫米/秒，步进目标轴角为百分之一度。 */
	int mm, velocity, goal_cd;
	bool fixed_target, driving;
	int target_mm;
	unsigned int still_ms;
	/* 每一位代表一个仍可能满足停稳条件的窄带，每个窄带保留自己的进入时刻。 */
	uint32_t active_bands, band_since[POSITION_BANDS];
	/* 累计跨过的短时失测间隔。确认停稳时这部分时间必须扣除。 */
	uint8_t band_unseen_ms[POSITION_BANDS];
	bool steady, have_sample;
	bool waiting_vision, reacquire_seen;
	uint16_t reacquire_seq;
	uint32_t reacquire_at;
	int reacquire_mm;
	int trim_milli_cd;
};

/* 以 O 点为零，按像素落在中点哪侧选择对应标定斜率，得到带符号毫米数。 */
int contest_ball_mm(int raw_x);
/* 新一轮从水平确认开始，同时保存起始时间及首个相机序号。 */
void position_begin(struct position_run *run, int x, uint16_t seq, uint32_t now);
/* 复用第一题的位置控制器，将三段目标切换为操作者选择的固定点。 */
void position_begin_target(struct position_run *run, int target_mm, int x, uint16_t seq, uint32_t now);
/* 更换目标时清除旧点的停留时间和积分，避免带着上一个目标的偏置启动。 */
void position_retarget(struct position_run *run, int target_mm);
/* 按编码器反馈时间确认轴在水平位置连续稳定，再进入第一段移球。 */
void position_check_level(struct position_run *run, bool at_level, uint32_t encoder_at);
/* 每次新实测先估球速，再判定停稳及换段，最后将位置、速度和积分补偿合成为限幅的目标轴角。 */
enum contest_notice position_update(struct position_run *run, enum ball_status view, int x, uint16_t seq,
				    uint32_t sample_at, uint32_t now);
#endif
