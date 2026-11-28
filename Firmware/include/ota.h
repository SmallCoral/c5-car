/* 引导、应用和上位机共用的分区、镜像头与帧格式。修改这些常量会影响升级兼容性。 */

#ifndef CORAL_OTA_H
#define CORAL_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 引导程序与应用共用的 v1 Flash 分区约定。 */
#define OTA_BOARD_ID 0xc5a30001U
#define OTA_APP_ADDR 0x08010000U
#define OTA_STAGE_ADDR 0x08080000U
#define OTA_IMAGE_MAX (448U * 1024U)
#define OTA_APP_META 0x080fc000U
#define OTA_STAGE_META 0x080fe000U
#define OTA_PAGE_SIZE 8192U
#define OTA_REQUEST_ADDR 0x2003ffe0U
#define OTA_REQUEST_MAGIC 0xc5b00701U
/* 有线更新可将已使能并静止的驱动交给应用继续抱持。邮箱前两个字归引导程序使用。 */
#define JLINK_HOLD_ADDR (OTA_REQUEST_ADDR + 8U)
#define JLINK_HOLD_MAGIC 0x484f4c44U
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

/* 在已有 CRC32 累计值上继续计算一段字节。 */
uint32_t ota_crc_update(uint32_t crc, const void *data, size_t length);
/* 计算完整缓冲区的 CRC32 校验值。 */
uint32_t ota_crc(const void *data, size_t length);
/* 按小端顺序从四字节字段读出整数。 */
uint32_t ota_get32(const uint8_t *bytes);
/* 按协议字节序写入一个 32 位整数。 */
void ota_put32(uint8_t *bytes, uint32_t value);
/* 检查镜像头的格式、地址、长度和校验等约束。 */
bool ota_header_valid(const struct ota_header *header);
/* 检查向量表的初始栈指针与复位入口是否位于允许区域。 */
bool ota_vectors_valid(const uint8_t *vectors, uint32_t length);
/* 将给定的载荷长度和 CRC 连同板号、版本及应用地址写入镜像头。 */
void ota_make_header(struct ota_header *header, uint32_t length, uint32_t crc);
/* 增量解析升级命令，只有帧长度和校验通过才交给调用方。 */
bool ota_parse(struct ota_parser *parser, uint8_t byte, struct ota_frame *frame);
/* 将命令、序号、数据和校验打包为完整的串口帧。 */
size_t ota_encode(uint8_t *bytes, const struct ota_frame *frame);

#endif
