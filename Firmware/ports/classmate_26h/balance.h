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

void Keep_Blance_out(void);

void best_hard(void);

void Keep_Blance_out(void);
void trace_balance_inter(void);

void PID_Keep_Blance_inter(PID_BALANCE* p);
void PID_Keep_Blance_out(PID_BALANCE* p);

#endif
