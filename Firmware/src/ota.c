#include <string.h>
#include "ota.h"

static const uint8_t frame_magic[] = { 0xc5, 0x5a, 0xb0, 0x07 };

_Static_assert(sizeof(struct ota_header) == 32, "OTA header layout");

uint32_t ota_crc_update(uint32_t crc, const void *data, size_t length)
{
	const uint8_t *bytes = data;

	while (length--) {
		crc ^= *bytes++;
		for (unsigned int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
	}
	return crc;
}

uint32_t ota_crc(const void *data, size_t length)
{
	return ~ota_crc_update(UINT32_MAX, data, length);
}

uint32_t ota_get32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
	       (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

void ota_put32(uint8_t *bytes, uint32_t value)
{
	for (unsigned int i = 0; i < 4; i++)
		bytes[i] = (uint8_t)(value >> (i * 8));
}

bool ota_header_valid(const struct ota_header *header)
{
	return header->magic == OTA_HEADER_MAGIC && header->version == OTA_VERSION &&
	       header->board == OTA_BOARD_ID && header->address == OTA_APP_ADDR &&
	       header->length >= 512 && header->length <= OTA_IMAGE_MAX &&
	       header->reserved == 0 &&
	       ota_crc(header, offsetof(struct ota_header, header_crc)) == header->header_crc;
}

bool ota_vectors_valid(const uint8_t *vectors, uint32_t length)
{
	uint32_t sp = ota_get32(vectors);
	uint32_t entry = ota_get32(vectors + 4);

	return sp >= 0x20000008U && sp <= OTA_REQUEST_ADDR && !(sp & 7U) &&
	       (entry & 1U) && entry >= OTA_APP_ADDR &&
	       entry - OTA_APP_ADDR < length;
}

void ota_make_header(struct ota_header *header, uint32_t length, uint32_t crc)
{
	*header = (struct ota_header) {
		.magic = OTA_HEADER_MAGIC, .version = OTA_VERSION,
		.board = OTA_BOARD_ID, .address = OTA_APP_ADDR,
		.length = length, .crc = crc,
	};
	header->header_crc = ota_crc(header, offsetof(struct ota_header, header_crc));
}

static void discard_byte(struct ota_parser *parser)
{
	parser->used--;
	memmove(parser->bytes, parser->bytes + 1, parser->used);
}

bool ota_parse(struct ota_parser *parser, uint8_t byte, struct ota_frame *frame)
{
	uint8_t *bytes = parser->bytes;

	if (parser->used == sizeof(parser->bytes))
		discard_byte(parser);
	bytes[parser->used++] = byte;
	while (parser->used >= 4) {
		size_t length;

		if (memcmp(bytes, frame_magic, 4)) {
			discard_byte(parser);
			continue;
		}
		if (parser->used < 12)
			return false;
		length = (size_t)bytes[6] | (size_t)bytes[7] << 8;
		if (bytes[4] != OTA_VERSION || length > OTA_PAYLOAD_MAX) {
			discard_byte(parser);
			continue;
		}
		if (parser->used < 16 + length)
			return false;
		if (ota_crc(bytes, 12 + length) != ota_get32(bytes + 12 + length)) {
			discard_byte(parser);
			continue;
		}
		frame->command = bytes[5];
		frame->sequence = ota_get32(bytes + 8);
		frame->length = (uint16_t)length;
		memcpy(frame->payload, bytes + 12, length);
		parser->used -= 16 + length;
		memmove(bytes, bytes + 16 + length, parser->used);
		return true;
	}
	return false;
}

size_t ota_encode(uint8_t *bytes, const struct ota_frame *frame)
{
	if (frame->length > OTA_PAYLOAD_MAX)
		return 0;
	memcpy(bytes, frame_magic, 4);
	bytes[4] = OTA_VERSION;
	bytes[5] = frame->command;
	bytes[6] = (uint8_t)frame->length;
	bytes[7] = (uint8_t)(frame->length >> 8);
	ota_put32(bytes + 8, frame->sequence);
	memcpy(bytes + 12, frame->payload, frame->length);
	ota_put32(bytes + 12 + frame->length, ota_crc(bytes, 12 + frame->length));
	return 16U + frame->length;
}
