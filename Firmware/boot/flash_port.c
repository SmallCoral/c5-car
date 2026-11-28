/* STM32 Flash 后端，检查分区、对齐及硬件状态，在 SRAM 内完成耗时的擦写并将错误返回给安装器。 */

#include <string.h>
#include "stm32c5xx.h"
#include "stm32c5xx_ll_flash.h"
#include "board.h"
#include "flash_port.h"

static volatile bool read_ecc_error;

/* 用起点与长度检查访问是否完整落在允许的区域内，避免地址加法溢出。 */
static bool within(uint32_t address, size_t length, uint32_t base, uint32_t size)
{
	return address >= base && address - base < size && length <= size - (address - base);
}

/* 按白名单检查应用区、暂存区及两块元数据的允许范围。 */
static bool writable(uint32_t address, size_t length)
{
	return within(address, length, OTA_APP_ADDR, OTA_IMAGE_MAX) ||
	       within(address, length, OTA_STAGE_ADDR, OTA_IMAGE_MAX) ||
	       within(address, length, OTA_APP_META, sizeof(struct ota_header)) ||
	       within(address, length, OTA_STAGE_META, sizeof(struct ota_header));
}

/* 从许可的 Flash 区间读取，并将本次读取触发的 ECC 错误传回上层。 */
bool flash_port_read(uint32_t address, void *data, size_t length)
{
	if (!writable(address, length))
		return false;
	read_ecc_error = false;
	memcpy(data, (const void *)(uintptr_t)address, length);
	__DSB();
	return !read_ecc_error;
}

/* 对 ECC 双位错误置标志并清中断，其余不可恢复异常交给板级停机。 */
void NMI_Handler(void)
{
	/* Flash 四字写入中途掉电可能触发 ECC，应将镜像判无效，避免恢复流程困在 NMI 中。 */
	if (LL_FLASH_IsActiveFlag_ECCD(FLASH)) {
		read_ecc_error = true;
		LL_FLASH_ClearFlag_ECCD(FLASH);
		return;
	}
	board_panic();
}

/* 擦写银行 1 时，指令和常量都须来自 RAM。CMSIS 启动复制表负责搬运 .ramfunc 及其常量池。 */
/* 从 RAM 解锁并擦除页或编程四字，等待完成并检查错误位。 */
__attribute__((section(".ramfunc"), noinline, optimize("O2")))
static bool flash_operation(uint32_t address, const uint32_t *words)
{
	uint32_t irq = __get_PRIMASK();
	uint32_t budget = 10000000U;
	uint32_t errors;
	volatile uint32_t *destination = (volatile uint32_t *)(uintptr_t)address;

	__disable_irq();
	FLASH->KEYR = LL_FLASH_KEY1;
	FLASH->KEYR = LL_FLASH_KEY2;
	if (FLASH->CR & FLASH_CR_LOCK) {
		__set_PRIMASK(irq);
		return false;
	}
	FLASH->CCR = LL_FLASH_FLAG_ERRORS_ALL | LL_FLASH_FLAG_EOP;
	if (words) {
		FLASH->CR = FLASH_CR_PG;
		for (unsigned int i = 0; i < 4; i++)
			destination[i] = words[i];
	} else {
		uint32_t page = ((address - FLASH_BASE) % 0x80000U) / OTA_PAGE_SIZE;
		uint32_t bank = address >= OTA_STAGE_ADDR ? FLASH_CR_BKSEL : 0;

		FLASH->CR = bank | (page << FLASH_CR_PNB_Pos) | FLASH_CR_PER | FLASH_CR_STRT;
	}
	__DSB();
	while ((FLASH->SR & LL_FLASH_FLAG_STATUS_ALL) && --budget)
		__NOP();
	if (!budget) {
		/* 银行仍忙时禁止返回 Flash 执行。复位后仍可从已提交的暂存镜像恢复。 */
		SCB->AIRCR = (0x5faU << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
		__DSB();
		for (;;)
			__NOP();
	}
	errors = FLASH->SR & LL_FLASH_FLAG_ERRORS_ALL;
	FLASH->CCR = LL_FLASH_FLAG_ERRORS_ALL | LL_FLASH_FLAG_EOP;
	FLASH->CR = FLASH_CR_LOCK;
	__DSB();
	__ISB();
	__set_PRIMASK(irq);
	return !errors;
}

/* 检查 Flash 的容量、缓存及银行映射是否符合引导程序的写入前提。 */
static bool flash_ready(void)
{
	/* v1 使用固定的物理银行映射，并在复位时保持指令缓存关闭。 */
	return FLASH_SIZE == 0x100000U && !LL_FLASH_OB_GetSwapBank(FLASH) &&
	       !(ICACHE->CR & ICACHE_CR_EN) && !(FLASH->SR & LL_FLASH_FLAG_STATUS_ALL);
}

/* 校验地址、页边界和许可区域后擦除对应页。 */
bool flash_port_erase(uint32_t page)
{
	if (page % OTA_PAGE_SIZE || !writable(page, 16) || !flash_ready())
		return false;
	return flash_operation(page, NULL);
}

/* 校验目标地址及四字对齐要求，送入 RAM 中的 Flash 写操作。 */
bool flash_port_program(uint32_t address, const void *data, size_t length)
{
	const uint8_t *bytes = data;
	uint32_t words[4];
	uint8_t readback[16];

	if (!length || address % 16 || length % 16 || !writable(address, length) ||
	    !flash_ready())
		return false;
	for (size_t offset = 0; offset < length; offset += 16) {
		memcpy(words, bytes + offset, 16);
		if (!flash_operation(address + (uint32_t)offset, words) ||
		    !flash_port_read(address + (uint32_t)offset, readback, 16) ||
		    memcmp(words, readback, 16))
			return false;
	}
	return true;
}
