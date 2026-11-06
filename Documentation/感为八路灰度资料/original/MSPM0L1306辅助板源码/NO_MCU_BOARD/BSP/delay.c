#include "delay.h"
// SysTick中断服务函数(1ms)
void SysTick_Handler(void) {
    Tick_SysTickCallback();
}
volatile uint32_t Tick = 0;
volatile uint32_t Reg_tick=0;
void delay_us(uint32_t us) {
    // 根据实际测试调整此值
    // 32MHz下大约需要 (us * 32) 次循环（需校准）
    volatile uint32_t count = us * 32; 
    while(count--);
}
/**
 * @brief 延时(使用SysTick中断计时)
 * @param t 延时时间(ms)
*/
void Tick_delay(uint32_t t) {
    uint32_t tEnd = Tick + t;
    while (Tick < tEnd);
}

// SysTick中断回调(1ms)
void Tick_SysTickCallback(void) {
  Tick++;
}

void TIMER_0_INST_IRQHandler(void){
	extern volatile uint8_t index;
	index=0;
	Reg_tick++;
}