#include <motor_X.h>
#include <bsp_system.h>

uint8_t Cal_data[30];  //数据接收缓冲区

uint8_t zero_set = 0;  //1表示成功设置了零点,0表示没有设置, 10错误命令返回

uint8_t zero_begin = 0;  //0表示未开始回零操作，1表示回0成功，2表示条件不满足，10错误命令返回。


//uint32_t hex;


///*在 HAL_UARTEx_ReceiveToIdle_IT 函数的行为中，每次调用都会重新初始化接收缓冲区指针，

//因此数据会从缓冲区的起始地址（Cal_data[0]）开始存储，而不是从上一次中断结束的位置继续累加*/

//void Motor_X_Init(void) {
//	
//	HAL_UARTEx_ReceiveToIdle_IT(&huart3, Cal_data, sizeof(Cal_data));  //空闲接收函数
//}

/**
  * @brief    读取系统参数
  * @param    addr  ：电机地址
  * @param    s     ：系统参数类型
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Read_Sys_Params(uint8_t addr, SysParams_t s)
{
  uint8_t i = 0;
  uint8_t cmd[16] = {0};
  
  // 装载命令
  cmd[i] = addr; ++i;                   // 地址

  switch(s)                             // 功能码
  {
    case S_VER  : cmd[i] = 0x1F; ++i; break;
    case S_RL   : cmd[i] = 0x20; ++i; break;
    case S_PID  : cmd[i] = 0x21; ++i; break;
    case S_VBUS : cmd[i] = 0x24; ++i; break;
    case S_CPHA : cmd[i] = 0x27; ++i; break;
    case S_ENCL : cmd[i] = 0x31; ++i; break;
    case S_TPOS : cmd[i] = 0x33; ++i; break;
    case S_VEL  : cmd[i] = 0x35; ++i; break;
    case S_CPOS : cmd[i] = 0x36; ++i; break;
    case S_PERR : cmd[i] = 0x37; ++i; break;
    case S_FLAG : cmd[i] = 0x3A; ++i; break;
    case S_ORG  : cmd[i] = 0x3B; ++i; break;
    case S_Conf : cmd[i] = 0x42; ++i; cmd[i] = 0x6C; ++i; break;
    case S_State: cmd[i] = 0x43; ++i; cmd[i] = 0x7A; ++i; break;
    default: break;
  }

  cmd[i] = 0x6B; ++i;                   // 校验字节
  
  // 发送命令
  HAL_UART_Transmit(&huart3, (uint8_t*)cmd, i, HAL_MAX_DELAY);
}


/**
  * @brief    设置单圈回零的零点位置
  * @param    addr  ：电机地址
  * @param    svF   ：是否存储标志，false为不存储，true为存储
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Set_O(uint8_t addr, bool svF)
{
  uint8_t cmd[16] = {0};
  
  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x93;                       // 功能码
  cmd[2] =  0x88;                       // 辅助码
  cmd[3] =  svF;                        // 是否存储标志，false为不存储，true为存储
  cmd[4] =  0x6B;                       // 校验字节
  
  // 发送命令
  HAL_UART_Transmit(&huart3, cmd, 5, HAL_MAX_DELAY);
}


/**
  * @brief    触发回零
  * @param    addr   ：电机地址
  * @param    o_mode ：回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  * @param    snF   ：多机同步标志，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF)
{
	//定义为局部静态变量，防止数据丢失，因为DMA是一位一位传输的，没传输一次地址就会自增一次，使用静态变量防止数据丢失.
  static uint8_t cmd[16] = {0};
  
  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x9A;                       // 功能码
  cmd[2] =  o_mode;                     // 回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  cmd[3] =  snF;                        // 多机同步运动标志，false为不启用，true为启用
  cmd[4] =  0x6B;                       // 校验字节
  
  // 发送命令
 HAL_UART_Transmit(&huart3, (uint8_t *)cmd, 5, HAL_MAX_DELAY);
}

/**
  * @brief    位置模式
  * @param    addr：电机地址
  * @param    dir ：方向        ，0为CW，其余值为CCW
  * @param    vel ：速度(RPM)   ，范围0 - 5000RPM
  * @param    acc ：加速度      ，范围0 - 255，注意：0是直接启动
  * @param    clk ：脉冲数      ，范围0- (2^32 - 1)个
  * @param    raF ：相位/绝对标志，false为相对运动，true为绝对值运动
  * @param    snF ：多机同步标志 ，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF)
{
  static uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0]  =  addr;                      // 地址
  cmd[1]  =  0xFD;                      // 功能码
  cmd[2]  =  dir;                       // 方向
  cmd[3]  =  (uint8_t)(vel >> 8);       // 速度(RPM)高8位字节
  cmd[4]  =  (uint8_t)(vel >> 0);       // 速度(RPM)低8位字节 
  cmd[5]  =  acc;                       // 加速度，注意：0是直接启动
  cmd[6]  =  (uint8_t)(clk >> 24);      // 脉冲数(bit24 - bit31)
  cmd[7]  =  (uint8_t)(clk >> 16);      // 脉冲数(bit16 - bit23)
  cmd[8]  =  (uint8_t)(clk >> 8);       // 脉冲数(bit8  - bit15)
  cmd[9]  =  (uint8_t)(clk >> 0);       // 脉冲数(bit0  - bit7 )
  cmd[10] =  raF;                       // 相位/绝对标志，false为相对运动，true为绝对值运动
  cmd[11] =  snF;                       // 多机同步运动标志，false为不启用，true为启用
  cmd[12] =  0x6B;                      // 校验字节
  
  // 发送命令
 HAL_UART_Transmit_DMA(&huart3, (uint8_t *)cmd, 13);
}

/**
  * @brief    速度模式
  * @param    addr：电机地址
  * @param    dir ：方向       ，0为CW，其余值为CCW
  * @param    vel ：速度       ，范围0 - 5000RPM
  * @param    acc ：加速度     ，范围0 - 255，注意：0是直接启动
  * @param    snF ：多机同步标志，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF)
{
  uint8_t cmd[16] = {0};


//  if(HAL_UART_GetState(&huart3) == HAL_UART_STATE_BUSY_TX)
//  {
//      return; // 或者加入队列等待
//  }

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0xF6;                       // 功能码
  cmd[2] =  dir;                        // 方向
  cmd[3] =  (uint8_t)(vel >> 8);        // 速度(RPM)高8位字节
  cmd[4] =  (uint8_t)(vel >> 0);        // 速度(RPM)低8位字节
  cmd[5] =  acc;                        // 加速度，注意：0是直接启动
  cmd[6] =  snF;                        // 多机同步运动标志
  cmd[7] =  0x6B;                       // 校验字节
  
  // 发送命令
  //HAL_UART_Transmit_DMA(&huart3, (uint8_t *)cmd, 8);
  HAL_UART_Transmit(&huart3, (uint8_t *)cmd, 8, HAL_MAX_DELAY);
}

void Contron_X_Speed(uint8_t dir, uint16_t vel) {
	
	
	Emm_V5_Vel_Control(01, dir, vel, 00, 00);
}


//raF:01时候是表示绝对运动值，是表示对于初始时候上电出所处的位置移动多少
//00是相对运动，是相对于当前位置的运动
void Contron_X_Position(uint8_t dir, uint16_t vel, uint32_t clk, bool raF) {
	
	Emm_V5_Pos_Control(01, dir, vel, 00, clk, raF, 00);
}


void motor_complete_data(void){

  if (serial_Motor_X_Flag == 1) {
 
		//设置零点
		//发送01 93 88 01 6B，正确返回01 93 02 6B，错误命令返回01 00 EE 6B
		if (Motor_X_RxBuffer[0] == 0x93 && Motor_X_RxBuffer[1] == 0x02) {
			
			zero_set = 1;
		}
		else if (Motor_X_RxBuffer[0] == 0x00 && Motor_X_RxBuffer[1] == 0xEE) {
			
			zero_set = 10;
			zero_begin = 10;
		}

		//触发回零
		//发送01 9A 00 00 6B，正 确返回01 9A 02 6B，条件不满足返回01 9A E2 6B，错误命令返回01 00 EE 6B
		else if(Motor_X_RxBuffer[0] == 0x9A && Motor_X_RxBuffer[1] == 0x02){
			
			zero_begin = 1;
		}
		else if (Motor_X_RxBuffer[0] == 0x9A && Motor_X_RxBuffer[1] == 0xE2) {
			
			zero_begin = 2;
		}
		
		
	
   
    serial_Motor_X_Flag = 0;
    
  }
}

//发送01 36 6B，正确返回01 36 01 00 01 00 00 6B，错误命令返回01 00 EE 6B
//处理电机实时位置的函数
//void motor_complete_data(void){

//  if (serial_Motor_X_Flag == 1) {
//    // 处理完整的数据包，数据存储在 Motor_X_RxBuffer 中，长度为 pMotor_X_RxBuffer
//    // 例如，可以解析坐标位置等信息
//    if (Motor_X_RxBuffer[0] == 0x01) {

//      hex = ((uint32_t)Motor_X_RxBuffer[1] << 24) | ((uint32_t)Motor_X_RxBuffer[2] << 16) | (Motor_X_RxBuffer[3] << 8) | Motor_X_RxBuffer[4];
//      Now_Angle = (float)hex / 65535.0f * 360.0f;
//      Now_Angle = -Now_Angle;  // 根据实际需要取反
//    }
//    else {

//      hex = ((uint32_t)Motor_X_RxBuffer[1] << 24) | ((uint32_t)Motor_X_RxBuffer[2] << 16) | (Motor_X_RxBuffer[3] << 8) | Motor_X_RxBuffer[4];
//      Now_Angle = (float)hex / 65535.0f * 360.0f;
//    }

//    // 处理完成后，重置标志和缓冲区指针
//    serial_Motor_X_Flag = 0;
//    
//  }
//}



//	
////既然是空闲中断，那么就利用好空闲中断的特性只处理每次接收到的数据
// void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
//	
//	
//	
//     if (huart == &huart3) {
//			
//			
//         // 在接收缓冲区 Cal_data 中查找有效帧
//         for (int i = 0; i < Size; i++) {
//					
//             // 检查是否有可能的帧头（地址0x01，功能码0x36）
//             if (i + 1 < Size && Cal_data[i] == 0x01 && Cal_data[i+1] == 0x36) {
//							
//                 // 需要确保帧完整（至少8字节）正确返回01 36 01 00 01 00 00 6B
//                 if (i + 7 < Size && Cal_data[i+7] == 0x6B) {
//									
//                     // 正确响应，状态字节在 i+2
//                     if (Cal_data[i+2] == 0x01) {
//											
//                          hex = ((uint32_t)Cal_data[i+3] << 24) | ((uint32_t)Cal_data[i+4] << 16) | (Cal_data[i+5] << 8) | Cal_data[i+6];
//											
//                         Now_Angle = (float)hex / 65535.0f * 360.0f;
//                         Now_Angle = -Now_Angle;  // 根据实际需要取反
//                     } 
//										
// 										// 其他状态，可提取数据或做错误处理
// 										else {
//                        
//                         hex = ((uint32_t)Cal_data[i+3] << 24) | ((uint32_t)Cal_data[i+4] << 16) | (Cal_data[i+5] << 8) | Cal_data[i+6];
//											
//                         Now_Angle = (float)hex / 65535.0f * 360.0f;
//                     }
//                 }
//                 break; // 找到一帧后退出，避免重复处理
//             }
//             // 检查错误命令响应（01 00 EE 6B）
//             else if (i + 1 < Size && Cal_data[i] == 0x01 && Cal_data[i+1] == 0x00) {
//							
//                 if (i + 3 < Size && Cal_data[i+3] == 0x6B) {
//									
//                     // 错误命令，可设置标志
//                     hex = 0x01; // 示例
//                 }
//                 break;
//             }
//         }

//         // 重新启动空闲中断接收
//         HAL_UARTEx_ReceiveToIdle_IT(&huart3, Cal_data, sizeof(Cal_data));
//     }
// }













