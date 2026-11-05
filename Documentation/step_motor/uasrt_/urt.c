#include <urt.h>
#include <bsp_system.h>

uint8_t udata_LY;
uint8_t udata_Y_motor;
uint8_t udata_X_motor;
uint8_t udata_Maxicam;


//X轴电机参数
uint8_t serial_Motor_X_Flag = 0;  //0表示可以开始接收数据， 1表示数据正在处理
uint8_t Motor_X_RxBuffer[10];
uint16_t pMotor_X_RxBuffer = 0;


//Y轴电机参数
uint8_t serial_Motor_Y_Flag = 0;  //0表示可以开始接收数据， 1表示数据正在处理
uint8_t Motor_Y_RxBuffer[10];
uint16_t pMotor_Y_RxBuffer = 0;





//电机返回指令接收，串口3
void Motor_X_Init(void) {

	HAL_UART_Receive_IT(&huart3, &udata_X_motor, 1);
}

//电机返回指令接收，串口3
void Motor_Y_Init(void) {

	HAL_UART_Receive_IT(&huart6, &udata_Y_motor, 1);
}







//maxicam接收数据初始化，串口2
void Maxicam_Init(void) {

	HAL_UART_Receive_IT(&huart2, &udata_Maxicam, 1);
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	
	
		//X轴电机参数
	static uint8_t RxState_X = 0;
	
		//Y轴电机参数
	static uint8_t RxState_Y = 0;
	
	
	
	
		
		
		//X轴云台坐标数据接收
	if (huart == &huart3) {

		uint8_t RX_motor_data = udata_X_motor;

		//注意着三个RXstate_X,的if判断是并列的，不会出现在Rxtate_X== 0这个条件中改变了RXstate_X的值而直接进入下一个if判断
		if (RxState_X == 0)
		{
			if (RX_motor_data == 0x01 && serial_Motor_X_Flag == 0)
			{
				RxState_X = 1;
				pMotor_X_RxBuffer = 0;
			}
		}
		else if (RxState_X == 1)
		{

			if (RX_motor_data == 0x6B) {
				RxState_X = 0;
				serial_Motor_X_Flag = 1;
			}
			else {
				Motor_X_RxBuffer[pMotor_X_RxBuffer] = RX_motor_data;
				pMotor_X_RxBuffer++;

			}
			// 继续接收电机数据，直到接收到完整的数据包
		

		}
		
		HAL_UART_Receive_IT(&huart3, &udata_X_motor, 1);
	}
		
		
	
		//Y轴云台坐标数据接收
	if (huart == &huart6) {

		uint8_t RY_motor_data = udata_Y_motor;

		//注意着三个RXstate_X,的if判断是并列的，不会出现在Rxtate_X== 0这个条件中改变了RXstate_X的值而直接进入下一个if判断
		if (RxState_Y == 0)
		{
			if (RY_motor_data == 0x01 && serial_Motor_Y_Flag == 0)
			{
				RxState_Y = 1;
				pMotor_Y_RxBuffer = 0;
			}
		}
		else if (RxState_Y == 1)
		{

			if (RY_motor_data == 0x6B) {
				RxState_Y = 0;
				serial_Motor_Y_Flag = 1;
			}
			else {
				Motor_Y_RxBuffer[pMotor_Y_RxBuffer] = RY_motor_data;
				pMotor_Y_RxBuffer++;

			}
			// 继续接收电机数据，直到接收到完整的数据包
		

		}
		
		HAL_UART_Receive_IT(&huart6, &udata_Y_motor, 1);
	}
	
	
	

}




uint8_t Order[3] = {0x01, 0x36, 0x6B}; //读取X轴电机坐标的指令


//X轴电机参数
uint8_t serial_Motor_X_Flag = 0;  //0表示可以开始接收数据， 1表示数据正在处理
uint8_t Motor_X_RxBuffer[10];
uint16_t pMotor_X_RxBuffer = 0;

	//X轴电机参数
	static uint8_t RxState_X = 0;


	//X轴云台坐标数据接收
	if (huart == &huart3) {

		uint8_t RX_motor_data = udata_X_motor;

		//注意着三个RXstate_X,的if判断是并列的，不会出现在Rxtate_X== 0这个条件中改变了RXstate_X的值而直接进入下一个if判断
		if (RxState_X == 0)
		{
			if (RX_motor_data == 0x01)
			{
				RxState_X = 1;

			}
		}
		else if (RxState_X == 1)
		{


			// 处理接收到的电机数据,成功检测到数据包
			if (RX_motor_data == 0x36 && serial_Motor_X_Flag == 0)
			{
				RxState_X = 2;
				pMotor_X_RxBuffer = 0;
				// 这里可以添加对电机数据的处理逻辑，例如解析坐标位置等

			}
			else {
				RxState_X = 0; // 如果接收到的数据不符合预期，重置状态
			}

		}
		else if (RxState_X == 2)
		{
			if (RX_motor_data == 0x6B) {
				RxState_X = 0;
				serial_Motor_X_Flag = 1;
			}
			else {
				Motor_X_RxBuffer[pMotor_X_RxBuffer] = RX_motor_data;
				pMotor_X_RxBuffer++;

			}
			// 继续接收电机数据，直到接收到完整的数据包


		}

		HAL_UART_Receive_IT(&huart3, &udata_X_motor, 1);
	}

