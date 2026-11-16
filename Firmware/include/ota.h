#ifndef CORAL_OTA_H
#define CORAL_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed v1 partition contract, shared by the bootloader and applications. */
#define OTA_BOARD_ID 0xc5a30001U
#define OTA_APP_ADDR 0x08010000U
#define OTA_STAGE_ADDR 0x08080000U
#define OTA_IMAGE_MAX (448U * 1024U)
#define OTA_APP_META 0x080fc000U
#define OTA_STAGE_META 0x080fe000U
#define OTA_PAGE_SIZE 8192U
#define OTA_REQUEST_ADDR 0x2003ffe0U
#define OTA_REQUEST_MAGIC 0xc5b00701U
#define OTA_HEADER_MAGIC 0x3541544fU
#define OTA_VERSION 1U
#define OTA_CHUNK 128U
#define OTA_PAYLOAD_MAX (OTA_CHUNK + 4U)
#define OTA_FRAME_MAX (16U + OTA_PAYLOAD_MAX)

enum ota_command {
	OTA_HELLO = 1, OTA_BEGIN, OTA_DATA, OTA_END, OTA_RUN, OTA_ABORT
};

enum ota_status {
	OTA_OK, OTA_BAD_ARGUMENT, OTA_BAD_STATE, OTA_BAD_OFFSET,
	OTA_FLASH_ERROR, OTA_BAD_IMAGE
};

struct ota_header {
	uint32_t magic;
	uint32_t version;
	uint32_t board;
	uint32_t address;
	uint32_t length;
	uint32_t crc;
	uint32_t reserved;
	uint32_t header_crc;
};

struct ota_frame {
	uint8_t command;
	uint32_t sequence;
	uint16_t length;
	uint8_t payload[OTA_PAYLOAD_MAX];
};

struct ota_parser {
	uint8_t bytes[OTA_FRAME_MAX];
	size_t used;
};

uint32_t ota_crc_update(uint32_t crc, const void *data, size_t length);
uint32_t ota_crc(const void *data, size_t length);
uint32_t ota_get32(const uint8_t *bytes);
void ota_put32(uint8_t *bytes, uint32_t value);
bool ota_header_valid(const struct ota_header *header);
bool ota_vectors_valid(const uint8_t *vectors, uint32_t length);
void ota_make_header(struct ota_header *header, uint32_t length, uint32_t crc);
bool ota_parse(struct ota_parser *parser, uint8_t byte, struct ota_frame *frame);
size_t ota_encode(uint8_t *bytes, const struct ota_frame *frame);

#endif
