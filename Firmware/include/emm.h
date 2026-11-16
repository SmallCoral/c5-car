#ifndef CORAL_EMM_H
#define CORAL_EMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct emm_reply {
	uint8_t command;
	uint8_t status;
	int32_t value;
};

struct emm_parser {
	uint8_t bytes[8];
	unsigned int used;
	uint32_t last_byte;
	uint32_t rejected;
};

size_t emm_enable(uint8_t *out, uint8_t address, bool enable);
size_t emm_stop(uint8_t *out, uint8_t address);
size_t emm_velocity(uint8_t *out, uint8_t address, bool reverse,
		    uint16_t rpm, uint8_t accel);
bool emm_feed(struct emm_parser *parser, uint8_t address, uint8_t byte,
	      uint32_t now, struct emm_reply *reply);

#endif
