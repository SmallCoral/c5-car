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

size_t emm_position(uint8_t *out, uint8_t address, int32_t raw,
		    uint32_t pulses_per_rev, uint16_t rpm)
{
	uint64_t magnitude = raw < 0 ? -(int64_t)raw : raw;
	uint64_t pulses = (magnitude * pulses_per_rev + 32768U) / 65536U;
	const uint8_t frame[] = {
		address, 0xfd, raw < 0, (uint8_t)(rpm >> 8), (uint8_t)rpm, 180,
		(uint8_t)(pulses >> 24), (uint8_t)(pulses >> 16),
		(uint8_t)(pulses >> 8), (uint8_t)pulses, 1, 0, 0x6b
	};

	if (!pulses_per_rev || pulses > UINT32_MAX)
		return 0;
	memcpy(out, frame, sizeof(frame));
	return sizeof(frame);
}

static unsigned int reply_size(uint8_t command)
{
	switch (command) {
	case 0x00:
	case 0xf3:
	case 0xf6:
	case 0xfd:
	case 0xfe:
	case 0x3a:
		return 4;
	case 0x35:
		return 6;
	case 0x36:
		return 8;
	case 0x42:
		return 33;
	case 0x43:
		return 31;
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
		if (parser->bytes[1] == 0x43) {
			const uint8_t *p = parser->bytes;
			if (p[2] != 31 || p[3] != 9 || p[10] > 1 || p[15] > 1 ||
			    p[18] > 1 || p[23] > 1)
				goto discard;
			magnitude = (uint32_t)p[19] << 24 | (uint32_t)p[20] << 16 |
				    (uint32_t)p[21] << 8 | p[22];
			if (magnitude > INT32_MAX)
				goto discard;
			reply->command = 0x43;
			reply->absolute = (uint16_t)(p[8] << 8 | p[9]);
			reply->value = p[18] ? -(int32_t)magnitude : (int32_t)magnitude;
			reply->status = p[29];
			parser->used = 0;
			return true;
		} else if (parser->bytes[1] == 0x42) {
			/* X42S Emm configuration: motor type and configured microsteps. */
			uint8_t type = parser->bytes[4], micro = parser->bytes[9];

			if (parser->bytes[2] != 33 || parser->bytes[3] != 21 ||
			    (type != 25 && type != 50))
				goto discard;
			magnitude = (uint32_t)type * 8U * (micro ? micro : 256U);
		} else if (length > 4 && parser->bytes[2] > 1) {
			goto discard;
		} else {
			magnitude = 0;
			for (index = 3; index < length - 1; index++)
				magnitude = (magnitude << 8) | parser->bytes[index];
		}
		if (magnitude > INT32_MAX)
			goto discard;
		reply->command = parser->bytes[1];
		reply->status = parser->bytes[2];
		reply->value = length != 33 && parser->bytes[2] ? -(int32_t)magnitude :
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
