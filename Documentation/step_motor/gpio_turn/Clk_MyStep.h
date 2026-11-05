#ifndef __CLK_MYSTEP_H
#define __CLK_MYSTEP_H

#include "main.h"
#include "tim.h"
#include "gpio.h"
#include <stdio.h>
#include "math.h"
#include <stdlib.h>

/* 步进电机状态定义 */
typedef enum {
    STEPPER_STOP = 0,    // 停止
    STEPPER_RUNNING,     // 运行中
    STEPPER_COMPLETE     // 完成
} Stepper_State_t;


/* 步进电机工作模式定义 */
typedef enum {
    MODE_POSITION = 0,   // 位置模式（指定步数）
    MODE_VELOCITY        // 速度模式（连续转动）
} Stepper_Mode_t;


/* 步进电机结构体 */
typedef struct {
    Stepper_State_t state;        // 当前状态
    uint32_t total_steps;         // 总步数
    uint32_t remaining_steps;     // 剩余步数
    uint32_t step_interval;       // 步进间隔(us)决定脉冲f)
	 Stepper_Mode_t mode;          // 工作模式
} Stepper_Motor_t;

/* 电机引脚定义 */
#define MOTOR_EN_PIN     GPIO_PIN_5   // 使能引脚 PA5  En  绿色
#define MOTOR_EN_PORT    GPIOA
#define MOTOR_DIR_PIN    GPIO_PIN_7    // 方向引脚 PA7  Dir  蓝色
#define MOTOR_DIR_PORT   GPIOA
#define MOTOR_PUL_PIN    GPIO_PIN_6    // 脉冲引脚 PA6		Stp   黄色                                                       
#define MOTOR_PUL_PORT   GPIOA
#define htimx htim1 // 定时器
/* 函数声明 */
void Stepper_Init(Stepper_Motor_t*motor);                                     // 初始化步进电机
void Stepper_Enable(void);                                    // 使能电机
void Stepper_Disable(void);                                   // 禁用电机
void Stepper_MoveSteps(Stepper_Motor_t*motor,int32_t steps);                       // 移动指定步数
void Stepper_MoveStepsWithSpeed(Stepper_Motor_t*motor,int32_t steps, uint32_t speed_factor); // 带速度的移动
void Stepper_Stop(Stepper_Motor_t*motor);                                      // 立即停止
Stepper_State_t Stepper_GetState(Stepper_Motor_t*motor);                       // 获取当前状态
uint32_t Stepper_GetRemainingSteps(Stepper_Motor_t*motor);                     // 获取剩余步数
void Stepper_TickCallback(Stepper_Motor_t*motor);                              // 定时器中断回调函数

void Stepper_SetSpeed(Stepper_Motor_t*motor,int speed) ; // 设置转动速度
#endif
