/* Emm 串口帧的编码和应答解析接口，缓冲区由调用方提供。 */

#ifndef CORAL_EMM_H
#define CORAL_EMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct emm_reply {
	uint8_t command;
	uint8_t status;
	int32_t value;
	uint16_t absolute;
};

struct emm_parser {
	uint8_t bytes[33];
	unsigned int used;
	uint32_t last_byte;
	uint32_t rejected;
};

/* 按驱动地址和使能状态组装 Emm 使能指令，返回帧长。 */
size_t emm_enable(uint8_t *out, uint8_t address, bool enable);
/* 组装 FE 停止帧以请求当前轴停转。 */
size_t emm_stop(uint8_t *out, uint8_t address);
/* 将方向、转速及加速度写入 F6 速度帧。 */
size_t emm_velocity(uint8_t *out, uint8_t address, bool reverse,
		    uint16_t rpm, uint8_t accel);
/* 将方向、脉冲数及定位模式写入 FD 位移帧。 */
size_t emm_position(uint8_t *out, uint8_t address, int32_t raw,
		    uint32_t pulses_per_rev, uint16_t rpm);
/* 逐字节组装应答，校验地址、命令和结束字节后交回上层。 */
bool emm_feed(struct emm_parser *parser, uint8_t address, uint8_t byte,
	      uint32_t now, struct emm_reply *reply);

#endif
