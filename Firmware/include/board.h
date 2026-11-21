/* 主控板硬件接口。上层按实物左右轮进行读写，底层负责处理接头换边、计数方向和驱动保护。 */

#ifndef CORAL_BOARD_H
#define CORAL_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum wheel_trip {
	WHEEL_OK,
	WHEEL_FAULT,
	WHEEL_DEADLINE,
	WHEEL_STALL,
	WHEEL_ENABLE_LOW,
};

/* 初始化系统时钟、引脚、定时器和屏幕接口，保证启动时驱动输出处于已知状态。 */
bool board_init(void);
/* 将相机输入脚交给对应 UART 并配置串口。 */
void board_camera_uart_init(void);
/* 返回 SysTick 维护的毫秒时钟。 */
uint32_t board_millis(void);
/* 按毫秒时钟等待指定间隔，供板级初始化时序使用。 */
void board_delay(uint32_t ms);
/* 刷新主循环活动时间，供独立的驱动保护检查卡死。 */
void board_heartbeat(void);
/* 设置板载指示灯的电平。 */
void board_led(bool on);
/* 读取实物按键并转换为按下掩码。 */
uint8_t board_keys(void);
/* 读取灰度输入并按实物左到右的顺序排列。 */
uint8_t board_gray(void);
/* 读取驱动故障引脚并映射到实际左右轮。 */
uint8_t board_faults(void);
/* 采样编码器 A/B 引脚电平，供架空检查判断接线。 */
uint8_t board_encoder_pins(void);
/* 按物理轮号读取对应定时器的累计计数。 */
uint32_t board_encoder(unsigned int side);
/* 先使能驱动、保持零 PWM，等唤醒和故障检查通过后才输出。 */
bool board_wheels_start(int left, int right, bool continuous);
/* 将循线输出映射到左右 PWM，已触发保护时拒绝重新驱动。 */
bool board_lap_set_wheels(int left, int right);
/* 在主循环内推进唤醒等待和运行状态。 */
void board_wheels_poll(void);
/* 清除轮 PWM 和驱动使能，可由中断直接停车。 */
void board_wheels_stop(void);
/* 返回轮驱动当前的活动状态。 */
bool board_wheels_active(void);
/* 读取已锁存的轮驱动保护原因。 */
enum wheel_trip board_wheels_trip(void);
/* 读取已锁存的左右轮故障位。 */
uint8_t board_wheels_faults(void);
/* 控制屏幕复位引脚。 */
void board_lcd_reset(bool high);
/* 控制屏幕背光引脚。 */
void board_lcd_backlight(bool on);
/* 区分命令和数据，将字节经 SPI 写入屏幕。 */
bool board_lcd_write(bool data, const uint8_t *bytes, size_t length);
/* 失效时停止驱动并进入板级故障处理。 */
_Noreturn void board_panic(void);

#endif
