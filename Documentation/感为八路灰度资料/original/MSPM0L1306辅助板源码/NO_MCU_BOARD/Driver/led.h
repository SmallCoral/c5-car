#ifndef LED_H
#define LED_H

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "delay.h"
/**
 * @brief LED 编号定义
 */
typedef enum {
    LED_0 = 0,
    LED_1,
    LED_2,
    LED_3,
    LED_4,
    LED_5,
    LED_6,
    LED_7,
    LED_KEY,
		LED_ERR,
    LED_COUNT
} LED_Number;
void configure_QualityCheck_GPIO(void);
void restore_QualityCheck_GPIO(void);
/**
 * @brief 初始化 LED 控制
 */
void LED_init(void);

/**
 * @brief 打开指定 LED
 * @param num LED 编号
 */
void LED_on(LED_Number num);

/**
 * @brief 关闭指定 LED
 * @param num LED 编号
 */
void LED_off(LED_Number num);

/**
 * @brief 切换指定 LED 状态
 * @param num LED 编号
 */
void LED_toggle(LED_Number num);

/**
 * @brief 设置所有 LED 状态
 * @param state 每个位对应一个 LED 的状态 (bit0: LED0, bit1: LED1, ...)
 */
void LED_setAll(uint16_t state);

void LED_setByDigital(unsigned char digital);

void LED_KEY_Blink_Update(void);

void LED_KEY_Blink_SetInterval(uint32_t newInterval);

void LED_KEY_Blink_Stop(void);

void LED_KEY_Blink_Start(void);

/**
 * @brief 初始化ERR灯闪烁功能
 * @param interval 闪烁间隔时间(ms)
 */
void LED_ERR_Blink_Init(uint32_t interval);

/**
 * @brief 更新ERR灯闪烁状态(需在主循环调用)
 */
void LED_ERR_Blink_Update(void);

/**
 * @brief 设置新的闪烁间隔
 * @param newInterval 新的闪烁间隔时间(ms)
 */
void LED_ERR_Blink_SetInterval(uint32_t newInterval);

/**
 * @brief 停止ERR灯闪烁
 */
void LED_ERR_Blink_Stop(void);

/**
 * @brief 开始ERR灯闪烁
 */
void LED_ERR_Blink_Start(void);

/**
 * @brief 检查ERR灯闪烁状态
 * @return 1-激活 0-未激活
 */
uint8_t LED_ERR_Blink_IsActive(void);
#endif // LED_H
