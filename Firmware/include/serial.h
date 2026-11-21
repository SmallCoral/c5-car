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

bool serial_init(void);
bool serial_camera_init(void);
bool serial_write(enum serial_port port, const uint8_t *data, size_t size);
bool serial_read(enum serial_port port, uint8_t *byte);
bool serial_tx_idle(enum serial_port port);
struct serial_stats serial_stats(enum serial_port port);

#endif
