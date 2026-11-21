/* 下载会话及 Flash 后端回调。相同的暂存与安装逻辑既用于板上写入，也用于主机掉电回归。 */

#ifndef CORAL_OTA_STORE_H
#define CORAL_OTA_STORE_H

#include "ota.h"

/* 硬件后端只允许操作两个镜像区及其元数据页。 */
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

/* 读取指定分区的元数据，验证镜像头和实际载荷。 */
bool ota_image_valid(const struct ota_flash *flash, uint32_t address,
		     uint32_t metadata, struct ota_header *header);
/* 建立或恢复下载会话，在允许的时机准备暂存区。 */
enum ota_status ota_begin(const struct ota_flash *flash, struct ota_download *download,
			 const struct ota_header *header);
/* 按当前偏移写入一个数据块，对重复包返回已确认进度。 */
enum ota_status ota_data(const struct ota_flash *flash, struct ota_download *download,
			uint32_t offset, const uint8_t *data, size_t length);
/* 验证完整暂存镜像后才提交元数据，使其成为可安装的副本。 */
enum ota_status ota_end(const struct ota_flash *flash, struct ota_download *download);
/* 从已提交的暂存区安装到应用区，完成校验后再处理暂存标记。 */
bool ota_install(const struct ota_flash *flash);

#endif
