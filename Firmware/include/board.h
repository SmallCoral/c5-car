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

bool board_init(void);
void board_camera_uart_init(void);
uint32_t board_millis(void);
void board_delay(uint32_t ms);
void board_heartbeat(void);
void board_led(bool on);
uint8_t board_keys(void);
uint8_t board_gray(void);
uint8_t board_faults(void);
uint8_t board_encoder_pins(void);
uint32_t board_encoder(unsigned int side);
bool board_wheels_start(int left, int right, bool continuous);
bool board_lap_set_wheels(int left, int right);
void board_wheels_poll(void);
void board_wheels_stop(void);
bool board_wheels_active(void);
enum wheel_trip board_wheels_trip(void);
uint8_t board_wheels_faults(void);
void board_lcd_reset(bool high);
void board_lcd_backlight(bool on);
bool board_lcd_write(bool data, const uint8_t *bytes, size_t length);
_Noreturn void board_panic(void);

#endif
