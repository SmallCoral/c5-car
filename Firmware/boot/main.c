/* 上电引导和蓝牙升级入口。先检查暂存与应用镜像，再选择启动或等待电脑下载；擦写与校验期间在屏幕显示进度。 */

#include <string.h>
#include "stm32c5xx.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "emm.h"
#include "flash_port.h"
#include "serial.h"

static struct ota_download download;
static struct ota_parser parser;
static struct ui_state screen = { .updating = true, .update_status = "WAITING FOR PC" };
static struct ota_header application;
static bool app_valid;
static bool lcd_ok;

/* 把已完成字节数同步到升级界面，并在耗时操作间更新心跳。 */
static void progress(uint32_t done, uint32_t total)
{
	screen.update_bytes = done;
	screen.update_total = total;
	board_heartbeat();
	if (lcd_ok)
		lcd_ok = display_poll(&screen, board_millis());
}

static const struct ota_flash flash = {
	.read = flash_port_read, .erase = flash_port_erase,
	.program = flash_port_program, .progress = progress,
};

/* 从向量表加载新应用的栈顶，恢复执行状态后跳至复位入口。 */
__attribute__((naked, noreturn)) static void enter_application(uint32_t sp __attribute__((unused)),
							    uint32_t entry __attribute__((unused)))
{
	__asm volatile ("msr msp, r0\n"
			"movs r2, #0\n"
			"msr control, r2\n"
			"isb\n"
			"cpsie i\n"
			"bx r1\n");
}

/* 关闭旧 SysTick 及中断，切换向量表，再把控制权交给应用。 */
static void jump_to_app(void)
{
	const uint32_t *vectors = (const uint32_t *)OTA_APP_ADDR;

	__disable_irq();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	for (unsigned int i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
		NVIC->ICER[i] = UINT32_MAX;
		NVIC->ICPR[i] = UINT32_MAX;
	}
	SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
	SCB->VTOR = OTA_APP_ADDR;
	__DSB();
	__ISB();
	enter_application(vectors[0], vectors[1]);
}

/* 向步进串口依次发送停转和失能，使机构进入升级所需状态。 */
static void stop_stepper(void)
{
	uint8_t bytes[6];
	size_t length = emm_stop(bytes, STEP_ADDRESS);

	serial_write(SERIAL_STEP, bytes, length);
	length = emm_enable(bytes, STEP_ADDRESS, false);
	serial_write(SERIAL_STEP, bytes, length);
}

/* 编码带原请求序号的应答，再交给蓝牙串口发送。 */
static void respond(const struct ota_frame *request, enum ota_status status)
{
	struct ota_frame response = {
		.command = request->command | 0x80U, .sequence = request->sequence,
		.length = request->command == OTA_HELLO ? 32 : 8,
	};
	uint8_t bytes[OTA_FRAME_MAX];

	ota_put32(response.payload, status);
	ota_put32(response.payload + 4, download.received);
	if (request->command == OTA_HELLO) {
		ota_put32(response.payload + 8, OTA_BOARD_ID);
		ota_put32(response.payload + 12, OTA_APP_ADDR);
		ota_put32(response.payload + 16, OTA_IMAGE_MAX);
		ota_put32(response.payload + 20, OTA_CHUNK);
		ota_put32(response.payload + 24, app_valid ? application.length : 0);
		ota_put32(response.payload + 28, app_valid ? application.crc : 0);
	}
	serial_write(SERIAL_BT, bytes, ota_encode(bytes, &response));
}

/* 按引导命令分发版本查询、分块下载、校验和启动请求。 */
static bool command(const struct ota_frame *request)
{
	struct ota_header header;
	enum ota_status status = OTA_BAD_ARGUMENT;
	bool reboot = false;

	switch (request->command) {
	case OTA_HELLO:
		if (!request->length)
			status = OTA_OK;
		break;
	case OTA_BEGIN:
		if (request->length != sizeof(header))
			break;
		memcpy(&header, request->payload, sizeof(header));
		screen.update_status = "ERASING STAGE";
		status = ota_begin(&flash, &download, &header);
		screen.update_status = status ? "BEGIN FAILED" : "RECEIVING";
		break;
	case OTA_DATA:
		if (request->length <= 4)
			break;
		status = ota_data(&flash, &download, ota_get32(request->payload),
				  request->payload + 4, request->length - 4);
		break;
	case OTA_END:
		if (request->length)
			break;
		screen.update_status = "VERIFYING";
		status = ota_end(&flash, &download);
		screen.update_status = status ? "VERIFY FAILED" : "READY TO INSTALL";
		break;
	case OTA_RUN:
		if (!request->length) {
			status = download.committed || app_valid ? OTA_OK : OTA_BAD_STATE;
			reboot = status == OTA_OK;
		}
		break;
	case OTA_ABORT:
		/* 复位时优先安装已经提交的暂存镜像，保留这份恢复副本。 */
		if (!request->length && !download.committed) {
			download.active = false;
			status = OTA_OK;
			screen.update_status = "WAITING FOR PC";
		}
		break;
	default:
		break;
	}
	respond(request, status);
	return reboot;
}

/* 上电校验已提交镜像，决定正常启动或进入蓝牙升级收包循环。 */
int main(void)
{
	volatile uint32_t *request = (volatile uint32_t *)OTA_REQUEST_ADDR;
	bool requested = request[0] == OTA_REQUEST_MAGIC && request[1] == ~OTA_REQUEST_MAGIC;
	struct ota_header stage;
	uint32_t last_byte = 0;
	uint32_t reset_at = 0;
	bool reboot = false;

	request[0] = 0;
	request[1] = 0;
	if (!board_init())
		board_panic();
	app_valid = ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &application);
	if (ota_image_valid(&flash, OTA_STAGE_ADDR, OTA_STAGE_META, &stage)) {
		if (!serial_init())
			board_panic();
		stop_stepper();
		lcd_ok = display_init();
		screen.update_status = "INSTALLING";
		if (ota_install(&flash))
			NVIC_SystemReset();
		download.header = stage;
		download.received = stage.length;
		download.committed = true;
		app_valid = ota_image_valid(&flash, OTA_APP_ADDR, OTA_APP_META, &application);
		screen.update_status = "INSTALL FAILED";
	} else if (app_valid && !requested && !(board_keys() & (1U << KEY_MODE))) {
		jump_to_app();
	} else {
		if (!serial_init())
			board_panic();
		stop_stepper();
		lcd_ok = display_init();
	}
	for (;;) {
		uint32_t now = board_millis();
		uint8_t byte;
		struct ota_frame frame;

		board_heartbeat();
		if (parser.used && now - last_byte > 1000)
			parser.used = 0;
		while (!reboot && serial_read(SERIAL_BT, &byte)) {
			last_byte = now;
			if (ota_parse(&parser, byte, &frame) && command(&frame)) {
				reboot = true;
				reset_at = now;
			}
		}
		/* 等两个 UART 移位寄存器和 HC-08 通知缓冲发送完毕。 */
		if (reboot && now - reset_at >= 300 && serial_tx_idle(SERIAL_BT) &&
		    serial_tx_idle(SERIAL_STEP))
			NVIC_SystemReset();
		progress(download.received, download.header.length);
		board_led((now / 150U) & 1U);
	}
}
