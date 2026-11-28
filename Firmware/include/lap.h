/* 循线控制器的输入输出与状态。分开保存单轮反馈、共同速度、转向差速、里程和停车原因。 */

#ifndef CORAL_LAP_H
#define CORAL_LAP_H

#include "grayscale.h"

enum lap_status {
	LAP_IDLE,
	LAP_RUNNING,
	LAP_DONE,
	LAP_CANCELLED,
	LAP_NO_LINE,
	LAP_AMBIGUOUS,
	LAP_ENCODER,
	LAP_TIMEOUT,
	LAP_DRIVE,
	LAP_NOT_READY,
	LAP_TIMING,
	LAP_CHECK,
	LAP_NO_MARKER,
};

struct lap_wheel {
	uint32_t previous;
	uint32_t progress;
	uint32_t moved_at;
	uint32_t reverse_ms;
	int32_t cps;
	int32_t integral;
	int target;
	int pwm;
};

struct lap {
	enum lap_status status;
	struct lap_wheel wheel[2];
	uint32_t started_at;
	uint32_t updated_at;
	uint32_t elapsed_ms;
	uint32_t line_at;
	uint32_t narrow_at;
	uint32_t marker_at;
	uint32_t finish_counts;	/* 比赛的预计终点里程用于减速，直线题也按此值停车。设备调试圈可取零。 */
	uint32_t control_ms;
	unsigned int marker_hits;
	unsigned int marker_peak;
	int last_position;
	int position_rate;
	int differential;
	int bend_ratio_ppm;	/* 由循线反馈学得的半轮速差与平均轮速之比。 */
	/* 两轮共同的前进目标，单位为计数/秒。转弯时左右轮分别加上和减去 steering。 */
	int base_target;
	int steering;
	unsigned int level;
	bool reverse;
	bool carry_ball;
	bool departed;
	bool departing;
	bool from_a;
	bool narrow;
	bool marker;
	bool armed;
	bool finish_on_marker; /* 整圈赛题的正常终点由 A 标记确认，里程只控制减速和保护。 */
};

enum lap_start_surface { LAP_START_UNALIGNED, LAP_START_NARROW, LAP_START_MARKER };

/* 识别起步时探头压在窄线、A 横线或其他表面，选择驶离方式。 */
enum lap_start_surface lap_start_surface(const struct gray_reading *gray);
/* 根据起步灰度选择横线驶离或窄线跟踪，启动调试循线。 */
bool lap_start(struct lap *lap, const struct gray_reading *gray, uint32_t now,
	       uint32_t left, uint32_t right, unsigned int level, bool reverse);
/* 从按键处建立圈内里程零点，使用传入的终点门槛启动比赛。 */
bool lap_start_distance(struct lap *lap, const struct gray_reading *gray, uint32_t now,
			uint32_t left, uint32_t right, unsigned int level, uint32_t finish_counts);
/* 启动限时的架空双轮方向检查。 */
void lap_check_start(struct lap *lap, uint32_t now, uint32_t left,
		     uint32_t right, bool reverse);
/* 按控制周期读轮速与灰度，检查保护条件，更新转向或锁存停车状态。 */
bool lap_update(struct lap *lap, const struct gray_reading *gray, uint32_t now,
		uint32_t left, uint32_t right);
/* 锁存结束原因并清零目标和驱动输出，供结果页读取。 */
void lap_stop(struct lap *lap, enum lap_status status);
/* 取两轮累计行程的平均值作为圈内里程，求和时加宽以免溢出。 */
uint32_t lap_progress(const struct lap *lap);
/* 将速度档位转换为以编码器计数/秒表示的巡航速度。 */
int lap_speed(unsigned int level);
/* 将循线状态编号转换为屏幕和遥测使用的名字。 */
const char *lap_status_name(enum lap_status status);

#endif
