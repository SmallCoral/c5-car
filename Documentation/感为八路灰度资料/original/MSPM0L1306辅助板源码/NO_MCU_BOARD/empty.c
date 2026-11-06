
#include "ti_msp_dl_config.h"
#include "No_Mcu_Ganv_Grayscale_Sensor_Config.h"
#include "delay.h"
#include "adc.h"
#include "led.h"
#include "uart.h"
#include "stdio.h"
#include "key.h"
#include "flash.h"
/*
 * =======================================================
 * 硬件引脚映射 (Hardware Pinout Map)
 * =======================================================
 * 
 * ----------------------------
 * LED控制引脚 (LED Control Pins)
 * ----------------------------
 * PA18  -> LED1        // 指示灯1
 * PA17  -> LED2        // 指示灯2
 * PA16  -> LED3        // 指示灯3
 * PA10  -> LED4        // 指示灯4
 * PA9   -> LED5        // 指示灯5
 * PA8   -> LED6        // 指示灯6
 * PA7   -> LED7        // 指示灯7
 * PA6   -> LED8        // 指示灯8
 * PA11  -> ERR_LED     // 错误状态指示灯
 * PA12  -> KEY_LED     // 按键状态指示灯
 * 
 * ----------------------------
 * 用户输入引脚 (User Input Pin)
 * ----------------------------
 * PA22  -> KEY         // 用户按键输入
 * 
 * ----------------------------
 * 无MCU传感器接口
 * (Dedicated Sensor Interface)
 * ----------------------------
 * PA15  -> AD0         // 地址输入通道0
 * PA14  -> AD1         // 地址输入通道1
 * PA13  -> AD2         // 地址输入通道2
 * PA26  -> ERR         // 传感器错误信号输入
 * PA27  -> OUT         // 传感器模拟量输出信号(接入ADC)
 * 
 * =======================================================
 */



/*                  模拟量转数字量的滞回比较器(施密特触发器)示意图               
 *          /\
 *   Digital |     
 *           |
 *        0  |                 +----------------+--------------
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |
 *           |                 |                |     
 *         1 |    -------------+----------------+
 *      -----+----------------------------------------------------------> analog
 *           |   0             1/3              2/3            1 
 *           |   黑            灰黑             灰白            白
 *               Calibrated    Gray             Gray           Calibrated
 *               black         black            white          white
 */



// 全局变量定义
unsigned short Anolog[8] = {0};    // 存储当前模拟量值的数组
unsigned short white[8] = {0};     // 存储白色校准值的数组 
unsigned short black[8] = {0};     // 存储黑色校准值的数组
unsigned short Normal[8];          // 归一化值数组

No_MCU_Sensor sensor;              // 传感器数据结构体
unsigned char Digtal;              // 数字输出值

volatile int8_t index=-1;
// 寄存器结构体
typedef struct {
    unsigned char Digital;     // 数字输出值
} DigitalRegister;

// 更新寄存器值（设置更新标志）
void UpdateRegister(DigitalRegister *reg, unsigned char new_value) {
    if (reg->Digital != new_value) {
        reg->Digital = new_value;
    }
}

DigitalRegister reg;
// 质检代码
void QualityCheck(void){
	configure_QualityCheck_GPIO();
	Tick_delay(100);
	if((DL_GPIO_readPins(LED_PORT, LED_ERR_PIN) > 0)&&(DL_GPIO_readPins(GRAY_IN_PORT, GRAY_IN_IN_KEY_PIN) > 0)){
		for(uint8_t i=0;i<20;i++){
			LED_on(LED_ERR);
			LED_on(LED_KEY);
			if(Digtal==0xFF){
				Digtal=0x7F;
			}
			else{
				Digtal=0xFF;
			};
			LED_setByDigital(Digtal);
			Tick_delay(100);
		}
	}
	restore_QualityCheck_GPIO();
}
int main(void)
{
    // 初始化系统配置
    SYSCFG_DL_init();
	
    // 质检代码，实际使用中可以选择注释
		Tick_delay(100);
		QualityCheck();
	
    // 初始化LED
    LED_init();
    
    /* DMA配置 - 用于ADC数据传输 */
    // 设置DMA源地址(ADC存储器)
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) &ADC0->ULLMEM.MEMRES[0]);
    // 设置DMA目标地址(ADC_VALUE缓冲区)
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t) &ADC_VALUE[0]);		
    // 使能DMA通道
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);;
    // 启动ADC转换
    DL_ADC12_startConversion(ADC_VOLTAGE_INST);
    
    // 使能按键中断
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOA_INT_IRQN);
		
		DL_TimerG_startCounter(TIMER_0_INST);
		NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
	
    // 从Flash存储器读取校准值
    readWhiteFromFlash();  // 读取白色校准值
    readBlackFromFlash();  // 读取黑色校准值

    // 使用校准值初始化传感器
    No_MCU_Ganv_Sensor_Init(&sensor, white, black);

    //无MCU灰度传感器硬件起振需要时间
    Tick_delay(100);
    state.value=KEY_IDLE;
		
    /* 主应用程序循环 */
    while (1) {
        if (state.value == KEY_IDLE||state.value == KEY_DISABLE||state.value == KEY_WAIT_LOSS ) {
            // 正常操作模式(非校准状态)
            
            // 执行无时基依赖的传感器任务
            No_Mcu_Ganv_Sensor_Task_Without_tick(&sensor);
            
            // 从传感器获取当前模拟量值
            Get_Anolog_Value(&sensor, Anolog);
            
            // 将模拟量转换为数字输出
            Digtal = Get_Digtal_For_User(&sensor);
					
        } else {
            // 校准模式 - 将数字输出置0，八路LED灯关闭
            Digtal = 0;
        }
				
        // 处理按键输入
        Key_Process();
        
        // 根据数字值更新LED显示
        LED_setByDigital(Digtal);
				UpdateRegister(&reg, Digtal);
        
        // 更新KEY和ERR LED的状态
        LED_KEY_Blink_Update();
				if(state.value!=KEY_LIGHT_ERROR){
					// 错误LED控制（长期错误标志或过曝状态）
					bool err_state = long_err_flag ? 1 :
													(DL_GPIO_readPins(GRAY_IN_PORT, GRAY_IN_IN_ERR_PIN) > 0);								
					err_state ? LED_on(LED_ERR) : LED_off(LED_ERR);
				}
				else{
					LED_ERR_Blink_Update();
				}
    }
}
/**
 * @brief 外部中断处理函数（CLK和按键）
 * @note  检测到有效按键后设置key_pressed标志
 */
void GROUP1_IRQHandler(void)
{
	    // 读取Group1的中断寄存器并清除中断标志位
    uint32_t pending = DL_GPIO_getPendingInterrupt(GPIOA);
	
    if(pending == GRAY_IN_IN_KEY_IIDX){
            /* 防抖处理 */
            if ((Tick - last_key_time) < DEBOUNCE_TIME_MS) {
                return;
            }
            /* 确认按键按下 */
            if (DL_GPIO_readPins(GRAY_IN_PORT, GRAY_IN_IN_KEY_PIN) == 0) {
                key_pressed = 1;
								long_pressed_key_time=Tick;
                last_key_time = Tick;
            }
    }
		if(pending == Serial_CLK_IIDX){
				 DL_TimerG_setTimerCount(TIMER_0_INST, 0);
				(reg.Digital & (1 << index)) ? DL_GPIO_setPins(Serial_PORT, Serial_DAT_PIN) : DL_GPIO_clearPins(Serial_PORT, Serial_DAT_PIN);
				index++;
				if(index==8){//收到第9个时钟后，更新下一帧数据
					index=0;
				}
		}
}