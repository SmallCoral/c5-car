#ifndef CORAL_CONTEST_H
#define CORAL_CONTEST_H

#include "ball.h"
#include "lap.h"
#include "logic.h"

enum contest_page { MENU_DIAGNOSTIC, MENU_HOME, MENU_DETAIL, MENU_RUN, MENU_RESULT, MENU_TOOLS };
enum contest_item { EVENT_POSITION, EVENT_LAP, EVENT_HALF, EVENT_FULL, EVENT_OFFSET, EVENT_VIDEO, EVENT_COUNT };
enum contest_notice { NOTICE_NONE, NOTICE_CENTER, NOTICE_DRIVE, NOTICE_VISION, NOTICE_TIMEOUT,
	NOTICE_STOPPED, NOTICE_FINISHED, NOTICE_LINE, NOTICE_PENDING, NOTICE_TRAVEL };
enum menu_action { MENU_NONE, MENU_START_EVENT, MENU_STOP_EVENT, MENU_OPEN_TOOL, MENU_RELEVEL, MENU_LAP_SPEED };

enum { CONTEST_LAP_LIMIT_MS = 30000 };

struct contest_menu {
	enum contest_page page;
	enum contest_item item;
	enum contest_notice notice;
	unsigned int tool;
	int target_cm;
};

enum menu_action contest_key(struct contest_menu *menu, enum key_id key);
enum task_id contest_tool(unsigned int index);
enum contest_notice contest_lap_notice(enum lap_status status, uint32_t elapsed_ms);

enum position_leg { MOVE_RIGHT, MOVE_LEFT, HOLD_LEFT, LEVEL_SHAFT, MOVE_CENTER };
enum { POSITION_CENTER_DWELL_MS = 300, POSITION_RIGHT_DWELL_MS = 400, POSITION_LEFT_DWELL_MS = 400 };
enum { POSITION_TOLERANCE_MM = 10, POSITION_SPAN_MM = 4,
	POSITION_BANDS = 2 * POSITION_TOLERANCE_MM - POSITION_SPAN_MM + 1 };
struct position_run {
	enum position_leg leg;
	uint32_t start, sample_at, steady_at, finished_ms;
	uint16_t seq;
	int mm, velocity, goal_cd;
	unsigned int still_ms;
	uint32_t active_bands, band_since[POSITION_BANDS];
	uint8_t band_unseen_ms[POSITION_BANDS];
	bool steady, have_sample;
	bool waiting_vision, reacquire_seen;
	uint16_t reacquire_seq;
	uint32_t reacquire_at;
	int reacquire_mm;
	int trim_milli_cd;
};

int contest_ball_mm(int raw_x);
void position_begin(struct position_run *run, int x, uint16_t seq, uint32_t now);
void position_check_level(struct position_run *run, bool at_level, uint32_t encoder_at);
enum contest_notice position_update(struct position_run *run, enum ball_status view, int x, uint16_t seq,
				    uint32_t sample_at, uint32_t now);
#endif
