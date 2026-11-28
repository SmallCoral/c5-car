/* 可恢复的镜像安装。下载先写暂存区，校验后才提交元数据；安装中途复位时仍能使用已提交的暂存副本。 */

#include <string.h>
#include "ota_store.h"

/* 从后端分块读回镜像，检查向量和 CRC，避免半写镜像被当作可启动数据。 */
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

/* 读取指定分区的元数据，验证镜像头和实际载荷。 */
bool ota_image_valid(const struct ota_flash *flash, uint32_t address,
		     uint32_t metadata, struct ota_header *header)
{
	return flash->read(metadata, header, sizeof(*header)) &&
	       verify_image(flash, address, header);
}

/* 建立或恢复下载会话，在允许的时机准备暂存区。 */
enum ota_status ota_begin(const struct ota_flash *flash, struct ota_download *download,
			 const struct ota_header *header)
{
	if (!ota_header_valid(header))
		return OTA_BAD_ARGUMENT;
	/* BEGIN 应答丢失后，重复请求要保留之前已收到的数据。 */
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

/* 按当前偏移写入一个数据块，对重复包返回已确认进度。 */
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
		/* 同一 Flash 四字只能编程一次，重复 DATA 包只需重发确认。 */
		return offset + length <= download->received &&
		       flash->read(OTA_STAGE_ADDR + offset, buffer, length) &&
		       !memcmp(buffer, data, length) ? OTA_OK : OTA_BAD_OFFSET;
	}
	if (offset != download->received)
		return OTA_BAD_OFFSET;
	memset(buffer, 0xff, padded);
	memcpy(buffer, data, length);
	if (!flash->program(OTA_STAGE_ADDR + offset, buffer, padded)) {
		download->active = false; /* 重试编程前先擦除相应页。 */
		return OTA_FLASH_ERROR;
	}
	download->received += (uint32_t)length;
	return OTA_OK;
}

/* 验证完整暂存镜像后才提交元数据，使其成为可安装的副本。 */
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

/* 从已提交的暂存区安装到应用区，完成校验后再处理暂存标记。 */
bool ota_install(const struct ota_flash *flash)
{
	struct ota_header staged;
	struct ota_header installed;
	uint8_t data[OTA_CHUNK];

	if (!ota_image_valid(flash, OTA_STAGE_ADDR, OTA_STAGE_META, &staged))
		return false;
	/* 即使在应用提交后、暂存标记清除前复位，再次安装也能安全完成。 */
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
