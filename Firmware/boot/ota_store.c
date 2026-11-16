#include <string.h>
#include "ota_store.h"

static bool verify_image(const struct ota_flash *flash, uint32_t address,
			 const struct ota_header *header)
{
	uint8_t data[OTA_CHUNK];
	uint32_t crc = UINT32_MAX;

	if (!ota_header_valid(header) || !flash->read(address, data, 8) ||
	    !ota_vectors_valid(data, header->length))
		return false;
	for (uint32_t offset = 0; offset < header->length; offset += sizeof(data)) {
		size_t size = header->length - offset;

		if (size > sizeof(data))
			size = sizeof(data);
		if (!flash->read(address + offset, data, size))
			return false;
		crc = ota_crc_update(crc, data, size);
	}
	return ~crc == header->crc;
}

bool ota_image_valid(const struct ota_flash *flash, uint32_t address,
		     uint32_t metadata, struct ota_header *header)
{
	return flash->read(metadata, header, sizeof(*header)) &&
	       verify_image(flash, address, header);
}

enum ota_status ota_begin(const struct ota_flash *flash, struct ota_download *download,
			 const struct ota_header *header)
{
	if (!ota_header_valid(header))
		return OTA_BAD_ARGUMENT;
	/* A lost BEGIN ACK must not erase chunks already received. */
	if (download->active && !memcmp(header, &download->header, sizeof(*header)))
		return OTA_OK;
	if (download->committed)
		return OTA_BAD_STATE;
	download->active = false;
	if (!flash->erase(OTA_STAGE_META))
		return OTA_FLASH_ERROR;
	for (uint32_t offset = 0; offset < header->length; offset += OTA_PAGE_SIZE) {
		if (!flash->erase(OTA_STAGE_ADDR + offset))
			return OTA_FLASH_ERROR;
	}
	download->header = *header;
	download->received = 0;
	download->active = true;
	return OTA_OK;
}

enum ota_status ota_data(const struct ota_flash *flash, struct ota_download *download,
			uint32_t offset, const uint8_t *data, size_t length)
{
	uint8_t buffer[OTA_CHUNK];
	size_t padded = (length + 15U) & ~(size_t)15U;

	if (!download->active || download->committed)
		return OTA_BAD_STATE;
	if (!length || length > OTA_CHUNK || (offset % OTA_CHUNK) ||
	    offset >= download->header.length || length > download->header.length - offset ||
	    (length != OTA_CHUNK && offset + length != download->header.length))
		return OTA_BAD_ARGUMENT;
	if (offset < download->received) {
		/* Never program a quadword twice, even after a lost ACK. */
		return offset + length <= download->received &&
		       flash->read(OTA_STAGE_ADDR + offset, buffer, length) &&
		       !memcmp(buffer, data, length) ? OTA_OK : OTA_BAD_OFFSET;
	}
	if (offset != download->received)
		return OTA_BAD_OFFSET;
	memset(buffer, 0xff, padded);
	memcpy(buffer, data, length);
	if (!flash->program(OTA_STAGE_ADDR + offset, buffer, padded)) {
		download->active = false; /* Retry requires an erase, not a second write. */
		return OTA_FLASH_ERROR;
	}
	download->received += (uint32_t)length;
	return OTA_OK;
}

enum ota_status ota_end(const struct ota_flash *flash, struct ota_download *download)
{
	if (download->committed)
		return OTA_OK;
	if (!download->active || download->received != download->header.length)
		return OTA_BAD_STATE;
	if (!verify_image(flash, OTA_STAGE_ADDR, &download->header))
		return OTA_BAD_IMAGE;
	if (!flash->program(OTA_STAGE_META, &download->header, sizeof(download->header))) {
		download->active = false;
		return OTA_FLASH_ERROR;
	}
	download->committed = true;
	return OTA_OK;
}

bool ota_install(const struct ota_flash *flash)
{
	struct ota_header staged;
	struct ota_header installed;
	uint8_t data[OTA_CHUNK];

	if (!ota_image_valid(flash, OTA_STAGE_ADDR, OTA_STAGE_META, &staged))
		return false;
	/* A reset after committing the app but before retiring the stage is harmless. */
	if (ota_image_valid(flash, OTA_APP_ADDR, OTA_APP_META, &installed) &&
	    !memcmp(&installed, &staged, sizeof(staged)))
		return flash->erase(OTA_STAGE_META);
	if (!flash->erase(OTA_APP_META))
		return false;
	for (uint32_t offset = 0; offset < staged.length; offset += OTA_PAGE_SIZE) {
		if (!flash->erase(OTA_APP_ADDR + offset))
			return false;
	}
	for (uint32_t offset = 0; offset < staged.length; offset += sizeof(data)) {
		size_t size = staged.length - offset;

		if (size > sizeof(data))
			size = sizeof(data);
		memset(data, 0xff, sizeof(data));
		if (!flash->read(OTA_STAGE_ADDR + offset, data, size) ||
		    !flash->program(OTA_APP_ADDR + offset, data, (size + 15U) & ~(size_t)15U))
			return false;
		if (flash->progress)
			flash->progress(offset + (uint32_t)size, staged.length);
	}
	if (!verify_image(flash, OTA_APP_ADDR, &staged) ||
	    !flash->program(OTA_APP_META, &staged, sizeof(staged)))
		return false;
	return flash->erase(OTA_STAGE_META);
}
