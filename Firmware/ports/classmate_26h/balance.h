/* F407 回中算法在 C5 上的移植接口，保留原有符号名，方便与旧工程和实车轨迹对照。 */

#ifndef __BALANCE_H__
#define __BALANCE_H__

#include <stdint.h>

typedef struct {
	float Actual;
	float Actual1;

	float Target;
	float Out;

	float FeedForward;
	float Last_Target;
	float Kf;

	float Kp;
	float Ki;
	float Kd;

	float Error0;
	float Error1;
	float ErrorInt;

	float ErrorIntMax;
	float ErrorIntMin;

	float OutMax;
	float OutMin;
}PID_BALANCE;

extern uint8_t balance_state;
extern uint8_t chose_TI;

extern float pipe_angle;

extern PID_BALANCE keep_out;
extern PID_BALANCE keep_inter;

extern int16_t speed ;

extern float bias_Kp;
extern float bias_Kd;

extern float bias_X;

/* 位置外环将钢球坐标误差换算成所需管角，再交给内环。短时丢球时沿用最近实测。 */
void Keep_Blance_out(void);

/* 按旧题号选择位置环目标和增益。调试回中使用第 4 组参数，目标为控制坐标 160。 */
void best_hard(void);

/* 位置外环将钢球坐标误差换算成所需管角，再交给内环。短时丢球时沿用最近实测。 */
void Keep_Blance_out(void);
/* 按旧机构的传动比例把轴角换成估计管角，再将内环结果拆成方向和转速。 */
void trace_balance_inter(void);

/* 将管角偏差换成电机转速，在小角差死区内给零速，其余情况按增益和上下限计算。 */
void PID_Keep_Blance_inter(PID_BALANCE* p);
/* 按位置误差与球位差分计算倾角，用输出限幅约束外环请求。 */
void PID_Keep_Blance_out(PID_BALANCE* p);

#endif
