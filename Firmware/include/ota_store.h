#ifndef CORAL_OTA_STORE_H
#define CORAL_OTA_STORE_H

#include "ota.h"

/* The hardware backend accepts only the two image slots and metadata pages. */
struct ota_flash {
	bool (*read)(uint32_t address, void *data, size_t length);
	bool (*erase)(uint32_t page);
	bool (*program)(uint32_t address, const void *data, size_t length);
	void (*progress)(uint32_t done, uint32_t total);
};

struct ota_download {
	struct ota_header header;
	uint32_t received;
	bool active;
	bool committed;
};

bool ota_image_valid(const struct ota_flash *flash, uint32_t address,
		     uint32_t metadata, struct ota_header *header);
enum ota_status ota_begin(const struct ota_flash *flash, struct ota_download *download,
			 const struct ota_header *header);
enum ota_status ota_data(const struct ota_flash *flash, struct ota_download *download,
			uint32_t offset, const uint8_t *data, size_t length);
enum ota_status ota_end(const struct ota_flash *flash, struct ota_download *download);
bool ota_install(const struct ota_flash *flash);

#endif
