#include <limits.h>
#include <string.h>
#include "emm.h"
#include "logic.h"

size_t emm_enable(uint8_t *out, uint8_t address, bool enable)
{
	const uint8_t frame[] = { address, 0xf3, 0xab, enable, 0, 0x6b };

	memcpy(out, frame, sizeof(frame));
	return sizeof(frame);
}

size_t emm_stop(uint8_t *out, uint8_t address)
{
	const uint8_t frame[] = { address, 0xfe, 0x98, 0, 0x6b };

	memcpy(out, frame, sizeof(frame));
	return sizeof(frame);
}

size_t emm_velocity(uint8_t *out, uint8_t address, bool reverse,
		    uint16_t rpm, uint8_t accel)
{
	const uint8_t frame[] = {
		address, 0xf6, reverse, (uint8_t)(rpm >> 8), (uint8_t)rpm,
		accel, 0, 0x6b
	};

	memcpy(out, frame, sizeof(frame));
	return sizeof(frame);
}

static unsigned int reply_size(uint8_t command)
{
	switch (command) {
	case 0x00:
	case 0xf3:
	case 0xf6:
	case 0xfe:
	case 0x3a:
		return 4;
	case 0x35:
		return 6;
	case 0x36:
		return 8;
	default:
		return 0;
	}
}

bool emm_feed(struct emm_parser *parser, uint8_t address, uint8_t byte,
	      uint32_t now, struct emm_reply *reply)
{
	unsigned int length;
	unsigned int index;
	uint32_t magnitude = 0;

	if (parser->used && elapsed(now, parser->last_byte, 30)) {
		parser->used = 0;
		parser->rejected++;
	}
	parser->last_byte = now;
	parser->bytes[parser->used++] = byte;
	while (parser->used) {
		if (parser->bytes[0] != address)
			goto discard;
		if (parser->used < 2)
			return false;
		length = reply_size(parser->bytes[1]);
		if (!length)
			goto discard;
		if (parser->used < length)
			return false;
		if (parser->bytes[length - 1] != 0x6b)
			goto discard;
		if (length > 4 && parser->bytes[2] > 1)
			goto discard;
		for (index = 3; index < length - 1; index++)
			magnitude = (magnitude << 8) | parser->bytes[index];
		if (magnitude > INT32_MAX)
			goto discard;
		reply->command = parser->bytes[1];
		reply->status = parser->bytes[2];
		reply->value = parser->bytes[2] ? -(int32_t)magnitude :
						       (int32_t)magnitude;
		parser->used = 0;
		return true;
discard:
		parser->used--;
		memmove(parser->bytes, parser->bytes + 1, parser->used);
		parser->rejected++;
	}
	return false;
}
