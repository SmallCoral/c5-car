/* 三个外设串口的收发、空闲状态及错误统计接口，调用者按端口选择对应队列。 */

#ifndef CORAL_SERIAL_H
#define CORAL_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum serial_port { SERIAL_BT, SERIAL_STEP, SERIAL_CAMERA, SERIAL_COUNT };

struct serial_stats {
	uint32_t rx_bytes;
	uint32_t tx_bytes;
	uint32_t errors;
	uint32_t framing, noise, overrun, parity;
	uint32_t rx_drops;
	uint32_t tx_drops;
};

/* 启用蓝牙与步进串口，并初始化各自的软件环形队列。 */
bool serial_init(void);
/* 单独初始化相机接收通道。 */
bool serial_camera_init(void);
/* 在临界区向发送队列排入数据，再交给中断逐字节发出。 */
bool serial_write(enum serial_port port, const uint8_t *data, size_t size);
/* 从指定串口的接收队列取出下一个可用字节。 */
bool serial_read(enum serial_port port, uint8_t *byte);
/* 检查发送队列及硬件是否已将最后一字节发完。 */
bool serial_tx_idle(enum serial_port port);
/* 读取当前串口累计的错误、丢字节和发送拥塞计数。 */
struct serial_stats serial_stats(enum serial_port port);

#endif
