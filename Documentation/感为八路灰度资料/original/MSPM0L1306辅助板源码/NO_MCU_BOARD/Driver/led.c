#include "led.h"
#include <stddef.h>

/* LED引脚映射表 */
static const uint32_t LED_PINS[LED_COUNT] = {
    LED_PIN_0_PIN,  // LED_0
    LED_PIN_1_PIN,  // LED_1  
    LED_PIN_2_PIN,  // LED_2
    LED_PIN_3_PIN,  // LED_3
    LED_PIN_4_PIN,  // LED_4
    LED_PIN_5_PIN,  // LED_5
    LED_PIN_6_PIN,  // LED_6
    LED_PIN_7_PIN,  // LED_7
    LED_KEY_PIN,    // LED_KEY
    LED_ERR_PIN     // LED_ERR
};

/* KEY灯闪烁控制结构体 */
typedef struct {
    uint32_t lastToggleTime;  // 上次状态切换时间戳
    uint32_t interval;        // 闪烁间隔(ms)
    uint8_t isActive;         // 闪烁使能标志
} LED_KEY_Blink_Control;

/* ERR灯闪烁控制结构体 */ 
typedef struct {
    uint32_t lastToggleTime;  // 上次状态切换时间戳
    uint32_t interval;        // 闪烁间隔(ms)
    uint8_t isActive;         // 闪烁使能标志
} ERR_Blink_Ctrl_t;

/* 静态全局变量 */
static LED_KEY_Blink_Control keyBlinkCtrl = {0};
static ERR_Blink_Ctrl_t errBlinkCtrl = {0};

void configure_QualityCheck_GPIO(void)
{
	  // 清除GRAY_IN_IN_KEY引脚的中断
		DL_GPIO_disableInterrupt(GPIOA, GRAY_IN_IN_KEY_PIN);
		// 清除GRAY_IN_IN_KEY引脚的任何现有配置
    DL_GPIO_clearPins(GPIOA, GRAY_IN_IN_KEY_PIN);
    // 将GRAY_IN_IN_KEY重新配置为下拉数字输入
    DL_GPIO_initDigitalInputFeatures(GRAY_IN_IN_KEY_IOMUX,
        DL_GPIO_INVERSION_DISABLE,      // 禁用信号反相
        DL_GPIO_RESISTOR_PULL_DOWN,     // 启用下拉电阻
        DL_GPIO_HYSTERESIS_DISABLE,     // 禁用迟滞
        DL_GPIO_WAKEUP_DISABLE);        // 禁用唤醒功能
	
    // 首先禁用LED_ERR的输出功能
    DL_GPIO_disableOutput(GPIOA, LED_ERR_PIN);
    // 清除LED_ERR引脚的任何现有配置
    DL_GPIO_clearPins(GPIOA, LED_ERR_PIN);  
    // 将LED_ERR重新配置为数字输入
    DL_GPIO_initDigitalInputFeatures(LED_ERR_IOMUX,
        DL_GPIO_INVERSION_DISABLE,      // 禁用信号反相
        DL_GPIO_RESISTOR_PULL_DOWN,  
        DL_GPIO_HYSTERESIS_DISABLE,     // 禁用迟滞
        DL_GPIO_WAKEUP_DISABLE);        // 禁用唤醒功能
}
// 检测完成后恢复为输出模式
void restore_QualityCheck_GPIO(void){
    
		// 将GRAY_IN_IN_KEY重新配置为上拉数字输入
    DL_GPIO_initDigitalInputFeatures(GRAY_IN_IN_KEY_IOMUX,
        DL_GPIO_INVERSION_DISABLE,      // 禁用信号反相
				DL_GPIO_RESISTOR_PULL_UP,     // 启用上拉电阻
        DL_GPIO_HYSTERESIS_DISABLE,     // 禁用迟滞
        DL_GPIO_WAKEUP_DISABLE);        // 禁用唤醒功能
    
	  // 将LED_ERR重新配置为数字输出
    DL_GPIO_initDigitalOutput(LED_ERR_IOMUX);
    DL_GPIO_clearPins(GPIOA, LED_ERR_PIN );
    DL_GPIO_enableOutput(GPIOA, LED_ERR_PIN);
    
    // 重新配置中断设置（根据原始配置）
    // 注意：原始配置中GRAY_IN_IN_KEY是有中断的，LED_ERR没有
		DL_GPIO_setUpperPinsPolarity(GPIOA, DL_GPIO_PIN_22_EDGE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOA, GRAY_IN_IN_KEY_PIN);
    DL_GPIO_enableInterrupt(GPIOA, GRAY_IN_IN_KEY_PIN);
}

/**
 * @brief LED模块初始化
 * @note 初始化后默认关闭所有LED
 */
void LED_init(void)
{
    // GPIO初始化已在SYSCFG_DL_GPIO_init中完成
    LED_setAll(0x00);  
}

/**
 * @brief 关闭指定LED
 * @param num LED编号(LED_Number枚举值)
 */
void LED_off(LED_Number num)
{
    if (num >= LED_COUNT) return;
    DL_GPIO_clearPins(GPIOA, LED_PINS[num]);
}

/**
 * @brief 点亮指定LED  
 * @param num LED编号(LED_Number枚举值)
 */
void LED_on(LED_Number num)
{
    if (num >= LED_COUNT) return;
    DL_GPIO_setPins(GPIOA, LED_PINS[num]);
}

/**
 * @brief 切换指定LED状态
 * @param num LED编号(LED_Number枚举值) 
 */
void LED_toggle(LED_Number num)
{
    if (num >= LED_COUNT) return;
    DL_GPIO_togglePins(GPIOA, LED_PINS[num]);
}

/**
 * @brief 批量控制所有LED状态
 * @param state 位掩码(bit0对应LED0, bit1对应LED1...)
 */
void LED_setAll(uint16_t state)
{
    uint32_t pinsToClear = 0;
    uint32_t pinsToSet = 0;
    
    for (LED_Number i = 0; i < LED_COUNT; i++) {
        if (state & (1 << i)) {
            pinsToSet |= LED_PINS[i];   // 需要点亮的LED
        } else {
            pinsToClear |= LED_PINS[i]; // 需要熄灭的LED
        }
    }
    
    DL_GPIO_clearPins(GPIOA, pinsToClear);
    DL_GPIO_setPins(GPIOA, pinsToSet);
}

/**
 * @brief 根据数字量控制前8路LED
 * @param digital 8位数据(每位对应一个LED状态)
 */
void LED_setByDigital(unsigned char digital)
{
    // 只控制前8个LED(LED_0到LED_7)
    for (uint8_t i = 0; i < 8; i++) {
        (digital & (1 << i)) ? LED_on(i) : LED_off(i);
    }
}

/******************** KEY灯闪烁控制 ********************/

/**
 * @brief 初始化KEY灯闪烁功能
 * @param interval 闪烁间隔时间(ms)
 */
void LED_KEY_Blink_Init(uint32_t interval)
{
    keyBlinkCtrl.interval = interval;
    keyBlinkCtrl.lastToggleTime = Tick;
    keyBlinkCtrl.isActive = 1;
    LED_off(LED_KEY);
}

/**
 * @brief 更新KEY灯闪烁状态(需在主循环调用)
 */
void LED_KEY_Blink_Update(void)
{
    if (!keyBlinkCtrl.isActive) return;
    
    if ((Tick - keyBlinkCtrl.lastToggleTime) >= keyBlinkCtrl.interval) {
        LED_toggle(LED_KEY);
        keyBlinkCtrl.lastToggleTime = Tick;
    }
}

/**
 * @brief 设置KEY灯闪烁间隔
 * @param newInterval 新的闪烁间隔(ms)
 */
void LED_KEY_Blink_SetInterval(uint32_t newInterval)
{
    keyBlinkCtrl.interval = newInterval;
}

/**
 * @brief 停止KEY灯闪烁
 */
void LED_KEY_Blink_Stop(void)
{
    keyBlinkCtrl.isActive = 0;
    LED_off(LED_KEY);
}

/**
 * @brief 开始KEY灯闪烁
 */
void LED_KEY_Blink_Start(void)
{
    keyBlinkCtrl.isActive = 1;
    keyBlinkCtrl.lastToggleTime = Tick;
}

/******************** ERR灯闪烁控制 ********************/

/**
 * @brief 初始化ERR灯闪烁功能
 * @param interval 闪烁间隔时间(ms)
 */
void LED_ERR_Blink_Init(uint32_t interval)
{
    errBlinkCtrl.interval = interval;
    errBlinkCtrl.lastToggleTime = Tick;
    errBlinkCtrl.isActive = 1;
    LED_off(LED_ERR);
}

/**
 * @brief 更新ERR灯闪烁状态(需在主循环调用)
 */
void LED_ERR_Blink_Update(void)
{
    if (!errBlinkCtrl.isActive) return;
    
    if ((Tick - errBlinkCtrl.lastToggleTime) >= errBlinkCtrl.interval) {
        LED_toggle(LED_ERR);
        errBlinkCtrl.lastToggleTime = Tick;
    }
}

/**
 * @brief 设置ERR灯闪烁间隔
 * @param newInterval 新的闪烁间隔(ms)
 */
void LED_ERR_Blink_SetInterval(uint32_t newInterval)
{
    errBlinkCtrl.interval = newInterval;
}

/**
 * @brief 停止ERR灯闪烁
 */
void LED_ERR_Blink_Stop(void)
{
    errBlinkCtrl.isActive = 0;
    LED_off(LED_ERR);
}

/**
 * @brief 开始ERR灯闪烁
 */
void LED_ERR_Blink_Start(void)
{
    errBlinkCtrl.isActive = 1;
    errBlinkCtrl.lastToggleTime = Tick;
}

/**
 * @brief 检查ERR灯闪烁是否激活
 * @return 1-激活 0-未激活
 */
uint8_t LED_ERR_Blink_IsActive(void)
{
    return errBlinkCtrl.isActive;
}