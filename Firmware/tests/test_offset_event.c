/* Exercise the real foreground loop with physical key timings, camera packets,
 * drive replies and independent wheel encoders. Ball positions are scripted. */
#include <assert.h>
#include "ota.h"
static uint32_t handover_words[2];
#undef JLINK_HOLD_ADDR
#define JLINK_HOLD_ADDR ((uintptr_t)handover_words)
#define main firmware_main
#include "../src/main.c"
#undef main

enum scenario { COMPLETE, VISION_LOSS, WHEEL_TRIP, LINE_LOSS, SHAFT_FAULT, MANUAL_STOP, FINISH_VISION_LOSS };
static enum scenario scenario;
static uint32_t ms, encoder, lap_at, stopped_at;
static bool dump_uart;
static bool wheels_on, moving_correction, parked_correction;
static int shaft_raw, locked_target;
static uint8_t drive_rx[33], camera_rx[BALL_FRAME_SIZE];
static size_t drive_pos, drive_len, camera_pos = BALL_FRAME_SIZE;

bool board_init(void) { return true; }
bool serial_init(void) { return true; }
bool serial_camera_init(void) { return true; }
bool serial_tx_idle(enum serial_port port) { (void)port; return true; }
struct serial_stats serial_stats(enum serial_port port) { (void)port; return (struct serial_stats){0}; }
uint32_t board_millis(void) { return ms; }
void board_wheels_poll(void) {}
uint8_t board_encoder_pins(void) { return 0; }
uint8_t board_faults(void) { return 0; }
uint8_t board_wheels_faults(void) { return 0; }
enum wheel_trip board_wheels_trip(void) { return WHEEL_OK; }
_Noreturn void board_panic(void) { abort(); }
bool display_init(void) { return true; }
uint32_t display_last_ball(struct ball_view *view) { *view = (struct ball_view){0}; return 0; }

static bool held(uint32_t start, uint32_t duration)
{
	return ms >= start && ms - start < duration;
}

uint8_t board_keys(void)
{
	if (scenario == COMPLETE && stopped_at && held(stopped_at + 1200, 100))
		return 1U << KEY_MODE;
	/* Visit offset from home by going up twice, then enter and select +1. */
	if (held(100, 60) || held(220, 60))
		return 1U << KEY_OK;
	if (held(350, 600) || held(1900, 700) || held(5400, 100) || held(6000, 700))
		return 1U << KEY_MODE;
	if (held(1100, 60) || held(2800, 1650) || held(6800, 550))
		return 1U << KEY_NEXT;
	if (held(4600, 60))
		return 1U << KEY_OK;
	if (scenario == MANUAL_STOP && held(7500, 100))
		return 1U << KEY_START;
	return 0;
}

void board_heartbeat(void)
{
	if (scenario == WHEEL_TRIP && ms == 7500)
		wheels_on = false;
	if (ms % 25)
		return;
	unsigned int seq = ms / 25;
	/* A persistent displaced ball during travel must change the shaft command. */
	int x = RULER_X_O;
	if (ms >= 6600) {
		int frames = (int)(ms - 6600) / 25;
		x -= frames < 20 ? frames * 5 : 100;
	}
	if (stopped_at && ms - stopped_at >= 150) {
		int frames = (int)(ms - stopped_at - 150) / 25;
		x += frames < 20 ? frames * 8 : 160;
	}
	bool measured = !held(4850, 675) && !(scenario == VISION_LOSS && ms >= 7500) &&
			!(scenario == FINISH_VISION_LOSS && stopped_at && ms - stopped_at >= 800);
	uint8_t packet[BALL_FRAME_SIZE] = { 0xaa, 0x55, 2, measured ? 3 : 0,
		(uint8_t)seq, (uint8_t)(seq >> 8), (uint8_t)x, (uint8_t)(x >> 8), [15] = 0x17 };
	for (unsigned int i = 2; i < 14; ++i) {
		packet[14] ^= packet[i];
		for (unsigned int b = 0; b < 8; ++b)
			packet[14] = (uint8_t)((packet[14] << 1) ^ ((packet[14] & 0x80) ? 7 : 0));
	}
	memcpy(camera_rx, packet, sizeof(packet));
	camera_pos = 0;
}

bool serial_read(enum serial_port port, uint8_t *byte)
{
	if (port == SERIAL_CAMERA && camera_pos < sizeof(camera_rx))
		*byte = camera_rx[camera_pos++];
	else if (port == SERIAL_STEP && drive_pos < drive_len)
		*byte = drive_rx[drive_pos++];
	else
		return false;
	return true;
}

bool serial_write(enum serial_port port, const uint8_t *data, size_t size)
{
	if (port == SERIAL_BT && dump_uart)
		fwrite(data, 1, size, stdout);
	if (port != SERIAL_STEP)
		return true;
	assert(size >= 3);
	uint8_t cmd = data[1];
	memset(drive_rx, 0, sizeof(drive_rx));
	drive_pos = 0;
	drive_len = 4;
	drive_rx[0] = STEP_ADDRESS;
	drive_rx[1] = cmd;
	drive_rx[2] = 2;
	switch (cmd) {
	case 0xfd: {
		assert(size == 13 && data[10] == 1);
		unsigned int pulses = (unsigned int)data[8] * 256 + data[9];
		shaft_raw = (int)((pulses * 65536 + 1600) / 3200) * (data[2] ? -1 : 1);
		if (ms >= 6625 && wheels_on && shaft_raw < 0)
			moving_correction = true;
		if (stopped_at && !wheels_on && shaft_raw > 0)
			parked_correction = true;
		break;
	}
	case 0x42:
		drive_len = 33;
		drive_rx[2] = 33;
		drive_rx[3] = 21;
		drive_rx[4] = 25;
		drive_rx[9] = 16;
		break;
	case 0x43: {
		uint16_t absolute = (uint16_t)(SHAFT_LEVEL_ABS + shaft_raw);
		unsigned int magnitude = (unsigned int)abs(shaft_raw);
		drive_len = 31;
		drive_rx[2] = 31;
		drive_rx[3] = 9;
		drive_rx[8] = (uint8_t)(absolute >> 8);
		drive_rx[9] = (uint8_t)absolute;
		drive_rx[18] = shaft_raw < 0;
		drive_rx[21] = (uint8_t)(magnitude >> 8);
		drive_rx[22] = (uint8_t)magnitude;
		drive_rx[29] = scenario == SHAFT_FAULT && ms >= 7500 ? 7 : 3;
		break;
	}
	case 0x3a:
		drive_rx[2] = 3;
		break;
	default:
		assert(cmd == 0xfe);
	}
	drive_rx[drive_len - 1] = 0x6b;
	return true;
}

uint8_t board_gray(void) { return scenario == LINE_LOSS && ms >= 7500 ? 0xff : 0xe7; }
uint32_t board_encoder(unsigned int side) { (void)side; return encoder; }
bool board_wheels_start(int left, int right, bool continuous)
{
	assert(continuous && left == 1 && right == -1);
	assert(ms == 6025 && !lap_at && menu.offset_driving && offset_ready(ms));
	assert(menu.target_mm == 26 && position.target_mm == menu.target_mm);
	assert(lap.carry_ball && lap.level == LAP_OFFSET_LEVEL && controls.lap_level == LAP_DEFAULT_LEVEL);
	assert(lap.finish_counts == LAP_OFFSET_DISTANCE_COUNTS);
	locked_target = menu.target_mm;
	lap_at = ms;
	wheels_on = true;
	return true;
}
bool board_lap_set_wheels(int left, int right)
{
	assert(wheels_on && left >= 0 && right <= 0);
	return true;
}
void board_wheels_stop(void) { wheels_on = false; }
bool board_wheels_active(void) { return wheels_on; }

bool display_poll(const struct ui_state *s, uint32_t now)
{
	if (now == 950)
		assert(s->menu.page == MENU_DETAIL && s->menu.item == EVENT_OFFSET && !s->running);
	if (now == 1500)
		assert(s->menu.target_mm == 1 && !wheels_on && !s->running);
	if (now == 2650)
		assert(s->menu.page == MENU_RUN && !s->menu.offset_driving && s->leg == HOLD_TARGET);
	if (now == 4400)
		assert(s->running && s->menu.target_mm == 27 && position.target_mm == 27 && step.goal > 0);
	if (now == 4750)
		assert(s->running && s->menu.target_mm == 26 && position.target_mm == 26 && !lap_at);
	if (now == 5425)
		assert(!s->offset_ready && s->waiting_vision && !lap_at);
	if (now == 5800)
		assert(s->offset_ready && !s->waiting_vision && !lap_at);
	if (now < 6025)
		assert(!wheels_on && !lap_at);
	if (lap_at && s->running) {
		assert(wheels_on && position.start == 1925 && position.target_mm == locked_target);
		assert(position.driving && abs(position.trim_milli_cd) <= 200000);
		assert(s->event_ms == now - lap_at && s->task == TASK_LAP);
		if (now - lap_at == 500)
			assert(lap.base_target >= 350 && lap.base_target <= 400);
		if (lap_progress(&lap) == LAP_OFFSET_DISTANCE_COUNTS - 50)
			assert(lap.base_target <= 460);
	}
	if (s->menu.page == MENU_RESULT || (lap_at && s->menu.page == MENU_DETAIL)) {
		/* Leave the result page on screen long enough to capture post-stop telemetry. */
		if (!stopped_at)
			stopped_at = now;
		if (scenario == COMPLETE || scenario == FINISH_VISION_LOSS) {
			/* The 25 ms synthetic camera continues after stopping. Those frames
			 * must never change the lap extrema or the exact travel sample count. */
			assert(offset_range.start == lap_at && offset_range.target_mm == locked_target);
			assert(offset_range.samples == (stopped_at - 1) / 25 - (lap_at - 1) / 25);
			assert(!offset_range.missing && offset_range.max_gap <= 25);
			assert(offset_range.min_mm == -26 && offset_range.max_mm == contest_ball_mm(RULER_X_O - 100) - 26);
			assert(offset_range.outside && offset_range.last_at < stopped_at - lap_at);
			assert(!s->running && !wheels_on && lap.status == LAP_DONE);
			assert(s->event_ms == stopped_at - lap_at && lap_progress(&lap) == LAP_OFFSET_DISTANCE_COUNTS);
			if (s->menu.page == MENU_RESULT && s->menu.notice == NOTICE_FINISHED) {
				assert(holding_finish && step.phase == STEP_POSITION);
				return true;
			}
			assert(parked_correction && !holding_finish);
			if (scenario == COMPLETE)
				assert(s->menu.page == MENU_DETAIL && s->menu.notice == NOTICE_NONE);
			else
				assert(s->menu.page == MENU_RESULT && s->menu.notice == NOTICE_VISION);
		}
		if (dump_uart && now - stopped_at < 1000)
			return true;
		fprintf(stderr, "end %u ms, lap %d, notice %d, start %u, step %d, error %u, correction %d, hold %d\n",
			(unsigned)now, lap.status, s->menu.notice, (unsigned)lap_at, step.phase, step.error,
			moving_correction, step.hold);
		assert(!s->running && !wheels_on && moving_correction && step.hold);
		assert(!holding_finish);
		if (scenario == VISION_LOSS)
			assert(s->menu.notice == NOTICE_VISION && stopped_at == 7975 && lap.status == LAP_CANCELLED);
		if (scenario == WHEEL_TRIP)
			assert(s->menu.notice == NOTICE_DRIVE && lap.status == LAP_DRIVE);
		if (scenario == LINE_LOSS)
			assert(s->menu.notice == NOTICE_LINE && lap.status == LAP_NO_LINE);
		if (scenario == SHAFT_FAULT)
			assert(s->menu.notice == NOTICE_DRIVE && lap.status == LAP_CANCELLED);
		if (scenario == MANUAL_STOP)
			assert(s->menu.notice == NOTICE_STOPPED && lap.status == LAP_CANCELLED);
		exit(0);
	}
	return true;
}

void board_led(bool on)
{
	(void)on;
	if (wheels_on)
		encoder++;
	assert(++ms < 50000);
}

int main(int argc, char **argv)
{
	const char *names[] = { "complete", "vision_loss", "wheel_trip", "line_loss", "shaft_fault", "manual_stop",
				"finish_vision_loss" };
	assert(argc == 2 || (argc == 3 && !strcmp(argv[2], "--dump-uart")));
	dump_uart = argc == 3;
	for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (!strcmp(argv[1], names[i])) {
			scenario = (enum scenario)i;
			controls.gray_white = true;
			return firmware_main();
		}
	}
	return 1;
}
