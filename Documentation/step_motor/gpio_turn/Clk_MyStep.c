#include "clk_mystep.h"


/**
  * @brief  初始化步进电机
  * @retval None
  */
void Stepper_Init(Stepper_Motor_t*motor)
{
    /* 初始化使能引脚为高电平（使能） */
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);
    
    /* 初始化方向引脚 */
    HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_RESET);
    
    /* 初始化脉冲引脚 */
    HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_RESET);
    
    /* 启动定时器 */
    HAL_TIM_Base_Start_IT(&htimx);
    
    motor->state = STEPPER_STOP;
    motor->remaining_steps = 0;
}

/**
  * @brief  使能电机
  * @retval None
  */
void Stepper_Enable(void)
{
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);
}

/**
  * @brief  禁用电机
  * @retval None
  */
void Stepper_Disable(void)
{
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_RESET);
}


/**
  * @brief  移动指定步数（使用默认速度）
  * @param  steps: 要移动的步数
  * @retval None
  */
void Stepper_MoveSteps(Stepper_Motor_t*motor,int32_t steps)
{
	
	 motor->mode = MODE_POSITION ;
    if(steps == 0) return;
    
	// 设定方向
	if(steps >= 0)
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_SET);
	}
	else
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_RESET);
	}
	
    motor->total_steps = abs(steps);
	motor->remaining_steps =abs(steps);
	motor->state = STEPPER_RUNNING;
}

/**
  * @brief  移动指定步数（自定义速度）
  * @param  steps: 要移动的步数
  * @param  speed_factor: 速度因子，数值越小越快
  * @retval None
  */
void Stepper_MoveStepsWithSpeed(Stepper_Motor_t*motor,int32_t steps, uint32_t speed_factor)
{
    if(steps == 0) return;
  // 设定方向
	if(steps >= 0)
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_SET);
	}
	else
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_RESET);
	}
	  motor->mode = MODE_POSITION ;
    motor->total_steps = abs(steps);
	motor->remaining_steps =abs(steps);
	motor->state = STEPPER_RUNNING;
    motor->step_interval = speed_factor;
}

/**
  * @brief  立即停止电机
  * @retval None
  */
void Stepper_Stop(Stepper_Motor_t*motor)
{
    motor->state = STEPPER_STOP;
    motor->remaining_steps = 0;
}

/**
  * @brief  获取电机当前状态
  * @retval 电机状态
  */
Stepper_State_t Stepper_GetState(Stepper_Motor_t*motor)
{
    return motor->state;
}

/**
  * @brief  获取剩余步数
  * @retval 剩余步数
  */
uint32_t Stepper_GetRemainingSteps(Stepper_Motor_t*motor)
{
    return motor->remaining_steps;
}




void Stepper_SetSpeed(Stepper_Motor_t*motor,int speed)
{
	
	if (speed == 0) {
		
		motor->state = STEPPER_STOP;
    motor->remaining_steps = 0;
	}
	
else {
  // 设定方向
	if(speed > 0)
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_SET);
	}
	else
	{
		 HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_RESET);
	}
//    if(speed < 1) speed = 1;          // 最小速度限制
//    if(speed > 1000) speed = 1000;    // 最大速度限制
	motor->state = STEPPER_RUNNING;
     motor->mode = MODE_VELOCITY ;
    motor->step_interval = abs(speed);
}
}



/**
  * @brief  定时器中断回调函数（在1us定时器中断中调用）
  * @retval None
  */
void Stepper_TickCallback(Stepper_Motor_t*motor)
{
    static uint32_t tick_counter = 0; 
    
    /* 如果电机停止，直接返回 */
    if(motor->state != STEPPER_RUNNING) {
        return;
    }
    
 
    /* 检查是否到达步进时间 */
    tick_counter++;
	//step_interval决定转速
    if(tick_counter >= motor->step_interval) 
	{
        tick_counter = 0;
        
        /* 生成脉冲：先拉高，然后在下一个周期拉低 */
        static uint8_t pulse_state = 0;
        if(pulse_state == 0) 
		{
            HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_SET);
            pulse_state = 1;
        } else 
		{
            HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_RESET);
            pulse_state = 0;
			
			if(motor->mode == MODE_POSITION) 
			{
						
				/* 减少剩余步数 */
				motor->remaining_steps--;
				
				/* 如果剩余步数为0，更新状态 */
				if(motor->remaining_steps == 0) 
				{
					motor->state = STEPPER_COMPLETE;
				}
			}
		
    }

}}