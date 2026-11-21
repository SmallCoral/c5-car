/* 步进状态机的阶段、反馈、已发目标和机械行程，供点动、稳球与停车流程使用。 */

#ifndef CORAL_STEPPER_H
#define CORAL_STEPPER_H

#include "emm.h"

enum step_phase {
	STEP_BOOT, STEP_IDLE, STEP_ENABLE, STEP_SPEED, STEP_MOVING,
	STEP_STOP, STEP_DISABLE, STEP_OFFLINE, STEP_POSITION, STEP_BRAKE, STEP_PARKED,
	STEP_LATCH, STEP_BAL_SPEED
};
enum step_direction_probe { DIR_NONE, DIR_ALIGN, DIR_SETTLE, DIR_MEASURE };

/* 2026-11-19 实测的电机轴限位，以单圈绝对水平读数为基准。 */
enum { SHAFT_LEVEL_ABS = 10260, SHAFT_HIGH_OFFSET = -12780, SHAFT_LOW_OFFSET = 19394 };

struct stepper {
	struct emm_parser parser;
	enum step_phase phase;
	uint8_t pending;
	uint8_t flags;
	uint8_t error;
	bool reverse;
	bool seen;
	uint32_t sent_at;
	uint32_t last_reply;
	uint32_t last_poll;
	uint32_t rx_errors;
	uint8_t last_bad_phase, last_bad_command;
	uint32_t replies;
	uint32_t timeouts;
	int32_t position;
	int32_t shaft_level;
	bool travel_valid;
	bool position_valid;
	uint8_t poll_index;
	bool cleanup_only;
	bool boot_parked;
	uint16_t target_rpm, sent_rpm;
	int32_t rpm;
	bool rpm_valid;
	bool hold, goal_sent, retried, speed_due, sent_reverse;
	int8_t cw_sign;
	bool probing_ccw;
	enum step_direction_probe direction_probe;
	int32_t direction_origin;
	uint32_t direction_at, direction_started;
	uint32_t pulses_per_rev, position_at, config_at, flags_at, goal_at;
	int32_t goal, sent_goal, lower, upper;
};

/* 初始化状态机，使上电与重连从检查驱动状态开始。 */
void stepper_init(struct stepper *step);
/* 接手已上电的机构，先停住残余运动再核对使能状态。 */
void stepper_resume_hold(struct stepper *step, uint32_t now);
/* 主循环定期调用，负责收应答、超时、停车和下一次驱动命令的调度。 */
void stepper_poll(struct stepper *step, uint32_t now);
/* 在轴位、细分及反馈有效时建立受限行程并启动转动。 */
bool stepper_start(struct stepper *step, bool reverse, uint16_t rpm, uint32_t now);
/* 取消待办运动，按停止流程清除驱动输出。 */
void stepper_stop(struct stepper *step, uint32_t now);
/* 根据最近一次有效应答判断驱动是否在线。 */
bool stepper_online(const struct stepper *step, uint32_t now);
/* 判断驱动状态是否正处于使能、移动或闭环控制阶段。 */
bool stepper_running(const struct stepper *step);
/* 限频查询驱动配置，取得电机及细分信息供定位换算。 */
void stepper_request_config(struct stepper *step, uint32_t now);
/* 请求在当前轴位使能抱持。 */
bool stepper_latch(struct stepper *step, uint32_t now);
/* 记录原点和方向，启动受控测向流程。 */
bool stepper_measure_direction(struct stepper *step, int32_t origin, bool ccw, uint32_t now);
/* 设置允许行程并启动位置保持控制。 */
bool stepper_hold(struct stepper *step, int32_t lower, int32_t upper, uint32_t now);
/* 在给定轴位上下界内建立速度控制。 */
bool stepper_speed_loop(struct stepper *step, int32_t lower, int32_t upper, uint32_t now);
/* 将稳球环的带符号转速交给驱动状态机。 */
void stepper_balance_rpm(struct stepper *step, int rpm);
/* 发送停轴命令，在抱持模式中保留使能以托住水管。 */
void stepper_brake(struct stepper *step, uint32_t now);
/* 返回当前步进阶段的显示名。 */
const char *stepper_name(const struct stepper *step, uint32_t now);

#endif
