/* 以可注入擦写中断的 Flash 模型检验引导程序的传输与掉电恢复。 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ota_store.h"

#define FLASH_BASE_TEST 0x08000000U
#define IMAGE_SIZE (OTA_PAGE_SIZE + 1157U)

static uint8_t memory[1024 * 1024];
static uint8_t snapshot[sizeof(memory)];
static uint8_t old_image[IMAGE_SIZE];
static uint8_t new_image[IMAGE_SIZE];
static int remaining = -1;
static unsigned int writes;

/* 把目标 Flash 地址映射到模拟数组位置。 */
static uint8_t *at(uint32_t address, size_t size)
{
	assert(address >= OTA_APP_ADDR && address - FLASH_BASE_TEST + size <= sizeof(memory));
	return memory + address - FLASH_BASE_TEST;
}

/* 从模拟 Flash 读取指定范围。 */
static bool read_flash(uint32_t address, void *data, size_t size)
{
	memcpy(data, at(address, size), size);
	return true;
}

/* 模拟整页擦除及可能的断电。 */
static bool erase_flash(uint32_t address)
{
	assert(address % OTA_PAGE_SIZE == 0);
	if (remaining == 0)
		return false;
	if (remaining > 0)
		remaining--;
	memset(at(address, OTA_PAGE_SIZE), 0xff, OTA_PAGE_SIZE);
	writes++;
	return true;
}

/* 模拟 Flash 四字编程及中途断电。 */
static bool program_flash(uint32_t address, const void *data, size_t size)
{
	const uint8_t *source = data;
	uint8_t *destination = at(address, size);

	assert(address % 16 == 0 && size % 16 == 0);
	for (size_t offset = 0; offset < size; offset += 16) {
		for (size_t i = 0; i < 16; i++)
			assert(destination[offset + i] == 0xff);
		if (remaining == 0) {
			/* 模拟写入 Flash 四字途中掉电，覆盖元数据写入。 */
			memcpy(destination + offset, source + offset, 7);
			return false;
		}
		if (remaining > 0)
			remaining--;
		memcpy(destination + offset, source + offset, 16);
		writes++;
	}
	return true;
}

static const struct ota_flash flash = {
	.read = read_flash, .erase = erase_flash, .program = program_flash,
};

/* 在模拟应用区放入可启动的旧镜像。 */
static void seed_old(void)
{
	struct ota_header header;

	memset(memory, 0xff, sizeof(memory));
	memcpy(at(OTA_APP_ADDR, IMAGE_SIZE), old_image, IMAGE_SIZE);
	ota_make_header(&header, IMAGE_SIZE, ota_crc(old_image, IMAGE_SIZE));
	memcpy(at(OTA_APP_META, sizeof(header)), &header, sizeof(header));
	remaining = -1;
	writes = 0;
}

/* 用分块协议将测试镜像传入暂存区。 */
static void upload(struct ota_download *download)
{
	struct ota_header header;

	ota_make_header(&header, IMAGE_SIZE, ota_crc(new_image, IMAGE_SIZE));
	assert(ota_begin(&flash, download, &header) == OTA_OK);
	for (uint32_t offset = 0; offset < IMAGE_SIZE; offset += OTA_CHUNK) {
		size_t size = IMAGE_SIZE - offset;

		if (size > OTA_CHUNK)
			size = OTA_CHUNK;
		assert(ota_data(&flash, download, offset, new_image + offset, size) == OTA_OK);
	}
	assert(ota_end(&flash, download) == OTA_OK);
}

/* 校验分帧、CRC 和损坏后的重新同步。 */
static void test_frames(void)
{
	struct ota_frame sent = { .command = OTA_DATA, .sequence = 123, .length = OTA_PAYLOAD_MAX };
	struct ota_frame got;
	struct ota_parser parser = { 0 };
	uint8_t bytes[OTA_FRAME_MAX];
	size_t size;

	assert(ota_crc("123456789", 9) == 0xcbf43926U);
	for (unsigned int i = 0; i < sizeof(sent.payload); i++)
		sent.payload[i] = (uint8_t)i;
	size = ota_encode(bytes, &sent);
	bytes[30] ^= 1;
	for (size_t i = 0; i < size; i++)
		assert(!ota_parse(&parser, bytes[i], &got));
	bytes[30] ^= 1;
	for (size_t i = 0; i < size; i++)
		assert(ota_parse(&parser, bytes[i], &got) == (i == size - 1));
	assert(got.command == sent.command && got.sequence == sent.sequence);
	assert(!memcmp(got.payload, sent.payload, sent.length));
	bytes[6] = 255;
	bytes[7] = 255;
	for (size_t i = 0; i < size; i++)
		assert(!ota_parse(&parser, bytes[i], &got));
	size = ota_encode(bytes, &sent);
	for (size_t i = 0; i < size; i++)
		assert(ota_parse(&parser, bytes[i], &got) == (i == size - 1));
}

/* 检查正常、重复或越界下载命令的结果。 */
static void test_transfer(void)
{
	struct ota_download download = { 0 };
	struct ota_header header;
	struct ota_header check;
	unsigned int before;

	seed_old();
	ota_make_header(&header, IMAGE_SIZE, ota_crc(new_image, IMAGE_SIZE));
	header.address = 0x08000000;
	assert(ota_begin(&flash, &download, &header) == OTA_BAD_ARGUMENT);
	assert(writes == 0);
	ota_make_header(&header, IMAGE_SIZE, ota_crc(new_image, IMAGE_SIZE));
	assert(ota_begin(&flash, &download, &header) == OTA_OK);
	assert(ota_end(&flash, &download) == OTA_BAD_STATE);
	assert(ota_data(&flash, &download, UINT32_MAX, new_image, OTA_CHUNK) == OTA_BAD_ARGUMENT);
	assert(ota_data(&flash, &download, OTA_CHUNK, new_image, OTA_CHUNK) == OTA_BAD_OFFSET);
	assert(ota_data(&flash, &download, 0, new_image, OTA_CHUNK) == OTA_OK);
	before = writes;
	assert(ota_data(&flash, &download, 0, new_image, OTA_CHUNK) == OTA_OK);
	assert(ota_begin(&flash, &download, &header) == OTA_OK);
	assert(writes == before && download.received == OTA_CHUNK);
	assert(ota_data(&flash, &download, 0, old_image, OTA_CHUNK) == OTA_BAD_OFFSET);
	assert(ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &check));
	assert(check.crc == ota_crc(old_image, IMAGE_SIZE));
	/* 复位只丢弃易失进度，重新 BEGIN 才擦除暂存区。 */
	memset(&download, 0, sizeof(download));
	upload(&download);
	before = writes;
	assert(ota_end(&flash, &download) == OTA_OK && writes == before);
	at(OTA_STAGE_ADDR, IMAGE_SIZE)[500] ^= 1;
	assert(!ota_install(&flash));
	assert(ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &check));
}

/* 遍历各写入点模拟掉电，并验证恢复可用镜像。 */
static void test_power_cuts(void)
{
	struct ota_download download = { 0 };
	struct ota_header check;
	unsigned int install_writes;
	unsigned int download_writes;

	seed_old();
	upload(&download);
	download_writes = writes;
	memcpy(snapshot, memory, sizeof(memory));
	writes = 0;
	assert(ota_install(&flash));
	install_writes = writes;
	assert(ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &check));
	assert(check.crc == ota_crc(new_image, IMAGE_SIZE));
	for (unsigned int cut = 0; cut < install_writes; cut++) {
		memcpy(memory, snapshot, sizeof(memory));
		remaining = (int)cut;
		assert(!ota_install(&flash));
		remaining = -1;
		assert(ota_install(&flash));
		assert(ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &check));
		assert(!memcmp(at(OTA_APP_ADDR, IMAGE_SIZE), new_image, IMAGE_SIZE));
		assert(!ota_image_valid(&flash, OTA_STAGE_ADDR, OTA_STAGE_META, &check));
	}
	printf("PASS: recovery after %u installation interruption points\n", install_writes);
	/* 逐次中断每个下载写操作，包括最后提交的镜像头。 */
	for (unsigned int cut = 0; cut <= download_writes; cut++) {
		struct ota_header header;
		enum ota_status status;

		seed_old();
		memset(&download, 0, sizeof(download));
		ota_make_header(&header, IMAGE_SIZE, ota_crc(new_image, IMAGE_SIZE));
		remaining = (int)cut;
		status = ota_begin(&flash, &download, &header);
		for (uint32_t offset = 0; status == OTA_OK && offset < IMAGE_SIZE; offset += OTA_CHUNK) {
			size_t size = IMAGE_SIZE - offset;

			if (size > OTA_CHUNK)
				size = OTA_CHUNK;
			status = ota_data(&flash, &download, offset, new_image + offset, size);
		}
		if (status == OTA_OK)
			ota_end(&flash, &download);
		remaining = -1;
		assert(ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &check));
		assert(check.crc == ota_crc(old_image, IMAGE_SIZE));
	}
}

/* 运行引导传输和安装恢复回归。 */
int main(void)
{
	for (unsigned int i = 0; i < IMAGE_SIZE; i++) {
		old_image[i] = (uint8_t)i;
		new_image[i] = (uint8_t)(i ^ 0x5a);
	}
	ota_put32(old_image, OTA_REQUEST_ADDR);
	ota_put32(old_image + 4, OTA_APP_ADDR + 513);
	ota_put32(new_image, OTA_REQUEST_ADDR);
	ota_put32(new_image + 4, OTA_APP_ADDR + 517);
	test_frames();
	test_transfer();
	test_power_cuts();
	puts("PASS: CRC, framing, bounds, lost ACKs, staging integrity, download interruptions");
	return 0;
}
