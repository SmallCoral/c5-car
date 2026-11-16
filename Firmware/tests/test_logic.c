#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "emm.h"
#include "logic.h"
#include "serial.h"
#include "stepper.h"

static uint8_t transmitted[128];
static size_t tx_size;
static uint8_t received[128];
static size_t rx_size;
static size_t rx_pos;
static struct serial_stats stats;

bool serial_write(enum serial_port port, const uint8_t *data, size_t size)
{
	assert(port == SERIAL_STEP);
	assert(size <= sizeof(transmitted));
	memcpy(transmitted, data, size);
	tx_size = size;
	return true;
}

bool serial_read(enum serial_port port, uint8_t *byte)
{
	assert(port == SERIAL_STEP);
	if (rx_pos == rx_size)
		return false;
	*byte = received[rx_pos++];
	return true;
}

struct serial_stats serial_stats(enum serial_port port)
{
	assert(port == SERIAL_STEP);
	return stats;
}

static void inject(const uint8_t *frame, size_t length)
{
	memcpy(received, frame, length);
	rx_pos = 0;
	rx_size = length;
}

static void ack(struct stepper *step, uint8_t command, uint32_t now)
{
	const uint8_t frame[] = { 1, command, 2, 0x6b };

	inject(frame, sizeof(frame));
	stepper_poll(step, now);
}

static void test_time_keys(void)
{
	struct debounce key = { 0 };
	struct controls ctl = { 0 };

	assert(elapsed(20, UINT32_MAX - 10, 31));
	assert(!elapsed(19, UINT32_MAX - 10, 31));
	assert(!key_update(&key, true, 100));
	assert(!key_update(&key, false, 105));
	assert(!key_update(&key, true, 110));
	assert(!key_update(&key, true, 134));
	assert(key_update(&key, true, 135));
	assert(!key_update(&key, true, 900));
	controls_key(&ctl, KEY_START, 100);
	assert(!ctl.running);
	controls_key(&ctl, KEY_MODE, 101);
	controls_key(&ctl, KEY_START, UINT32_MAX - 100);
	assert(ctl.running && ctl.task == TASK_LEFT);
	controls_tick(&ctl, 100, false);
	assert(ctl.running);
	controls_tick(&ctl, 2900, false);
	assert(!ctl.running);
	controls_key(&ctl, KEY_START, 3000);
	controls_key(&ctl, KEY_OK, 3100);
	assert(!ctl.running && !ctl.reverse);
	controls_key(&ctl, KEY_OK, 3200);
	assert(ctl.reverse && controls_duty(&ctl) == -100);
	controls_key(&ctl, KEY_START, 3300);
	controls_tick(&ctl, 3301, true);
	assert(!ctl.running);
	ctl.task = TASK_STEPPER;
	controls_key(&ctl, KEY_START, 4000);
	controls_tick(&ctl, 604000, false);
	assert(ctl.running && controls_rpm(&ctl) == 30);
	controls_key(&ctl, KEY_MODE, 604001);
	assert(!ctl.running && ctl.task == TASK_STEPPER);
	controls_key(&ctl, KEY_NEXT, 604002);
	assert(controls_rpm(&ctl) == 60);
	assert(encoder_delta(3, UINT32_MAX - 2) == 6);
	assert(encoder_delta(UINT32_MAX - 2, 3) == -6);
	assert(encoder_delta(0x80000000U, 0) == INT32_MIN);
}

static void test_protocol(void)
{
	uint8_t out[8];
	const uint8_t expected[] = {
		1, 0xf6, 0, 0, 30, 10, 0, 0x6b
	};
	const uint8_t negative[] = { 1, 0x36, 1, 0, 0, 0x6b, 0x02, 0x6b };
	const uint8_t success[] = { 1, 0xf3, 2, 0x6b };
	struct emm_parser parser = { 0 };
	struct emm_reply reply;
	unsigned int i;

	assert(emm_velocity(out, 1, false, 30, 10) == 8);
	assert(!memcmp(out, expected, sizeof(expected)));
	assert(emm_enable(out, 1, false) == 6 && out[3] == 0);
	assert(emm_stop(out, 1) == 5 && out[2] == 0x98);
	assert(!emm_feed(&parser, 1, 0x99, 0, &reply));
	for (i = 0; i < sizeof(negative); i++)
		assert(emm_feed(&parser, 1, negative[i], i + 1, &reply) ==
		       (i == sizeof(negative) - 1));
	assert(reply.value == -27394 && reply.command == 0x36);
	assert(!emm_feed(&parser, 1, 1, 20, &reply));
	assert(!emm_feed(&parser, 1, 0x36, 21, &reply));
	for (i = 0; i < sizeof(success); i++)
		assert(emm_feed(&parser, 1, success[i], 60 + i, &reply) ==
		       (i == sizeof(success) - 1));
	assert(reply.status == 2);
	/* Corrupt stream cannot overrun the fixed parser buffer. */
	for (i = 0; i < 10000; i++)
		(void)emm_feed(&parser, 1, (uint8_t)(i * 37), 100 + i, &reply);
	assert(parser.used < sizeof(parser.bytes));
}

static void test_all_toggle(void)
{
	struct controls ctl = {
		.task = TASK_ALL, .level = WHEEL_DEFAULT_LEVEL
	};
	const int expected_duties[] = { 100, 200, 300, 450 };
	unsigned int index;

	assert(!ctl.running);
	assert(task_uses_wheels(ctl.task) && task_uses_stepper(ctl.task));
	assert(!task_uses_wheels(TASK_MONITOR));
	assert(!task_uses_stepper(TASK_BOTH));
	controls_key(&ctl, KEY_START, UINT32_MAX - 100);
	controls_tick(&ctl, 600000, false);
	assert(ctl.running && controls_duty(&ctl) == 450);
	assert(controls_rpm(&ctl) == 30);
	controls_key(&ctl, KEY_START, 600001);
	assert(!ctl.running && ctl.task == TASK_ALL);
	controls_tick(&ctl, 900000, false);
	assert(!ctl.running);
	controls_key(&ctl, KEY_START, 900001);
	controls_tick(&ctl, 900002, true);
	assert(!ctl.running);
	controls_tick(&ctl, 900003, false);
	assert(!ctl.running);
	for (index = 0; index < WHEEL_LEVEL_COUNT; index++) {
		controls_key(&ctl, KEY_NEXT, 900003);
		assert(controls_duty(&ctl) == expected_duties[index]);
		assert(controls_duty(&ctl) <= WHEEL_MAX_PERMILLE);
		controls_key(&ctl, KEY_OK, 900003);
		assert(controls_duty(&ctl) == -expected_duties[index]);
		controls_key(&ctl, KEY_OK, 900003);
	}
	assert(controls_rpm(&ctl) == 30);
	controls_key(&ctl, KEY_MODE, 900004);
	assert(ctl.task == TASK_MONITOR);
}

static void make_ready(struct stepper *step)
{
	memset(&stats, 0, sizeof(stats));
	rx_size = rx_pos = 0;
	stepper_init(step);
	assert(!stepper_start(step, false, 30, 0));
	stepper_poll(step, 1000);
	assert(transmitted[1] == 0xfe);
	ack(step, 0xfe, 1010);
	assert(transmitted[1] == 0xf3 && transmitted[3] == 0);
	ack(step, 0xf3, 1020);
	assert(step->phase == STEP_IDLE);
}

static void test_step_state(void)
{
	struct stepper step;
	const uint8_t reached[] = { 1, 0x3a, 3, 0x6b };
	const uint8_t stop_disabled[] = { 1, 0xfe, 0xe2, 0x6b };
	const uint8_t rpm[] = { 1, 0x35, 1, 0, 30, 0x6b };

	stepper_init(&step);
	stepper_poll(&step, 1000);
	inject(stop_disabled, sizeof(stop_disabled));
	stepper_poll(&step, 1010);
	ack(&step, 0xf3, 1020);
	assert(step.phase == STEP_IDLE && step.error == 0);

	make_ready(&step);
	assert(!stepper_start(&step, true, 0, 1100));
	assert(!stepper_start(&step, true, 91, 1100));
	assert(stepper_start(&step, true, 30, 1100));
	assert(transmitted[1] == 0xf3 && transmitted[3] == 1);
	ack(&step, 0xf3, 1110);
	assert(tx_size == 8 && transmitted[1] == 0xf6 && transmitted[2] == 1);
	ack(&step, 0xf6, 1120);
	assert(step.phase == STEP_MOVING);
	stepper_poll(&step, 1320);
	assert(transmitted[1] == 0x3a);
	inject(reached, sizeof(reached));
	stepper_poll(&step, 1330);
	assert(step.phase == STEP_MOVING);
	/* The old 3-second test deadline and position-reached flag cannot stop
	 * continuous velocity mode. Speed replies retain their direction sign.
	 */
	stepper_poll(&step, 10000);
	assert(step.phase == STEP_MOVING && transmitted[1] == 0x35);
	inject(rpm, sizeof(rpm));
	stepper_poll(&step, 10010);
	assert(step.rpm_valid && step.rpm == -30);
	stepper_stop(&step, 10020);
	ack(&step, 0xfe, 10030);
	ack(&step, 0xf3, 10040);
	assert(!stepper_running(&step));

	/* Cancellation while enable is in flight must never queue velocity. */
	make_ready(&step);
	assert(stepper_start(&step, false, 30, 1100));
	stepper_stop(&step, 1101);
	ack(&step, 0xf3, 1102);
	assert(step.phase == STEP_STOP && transmitted[1] == 0xfe);
	stepper_poll(&step, 1301);
	assert(step.phase == STEP_DISABLE && transmitted[3] == 0);
	stepper_poll(&step, 1501);
	assert(step.phase == STEP_OFFLINE && !stepper_running(&step));

	/* Missing move ACK and damaged serial reception both request a stop. */
	make_ready(&step);
	assert(stepper_start(&step, false, 30, 1100));
	ack(&step, 0xf3, 1110);
	stepper_poll(&step, 1310);
	assert(step.phase == STEP_STOP && step.error == 0xe0);
	make_ready(&step);
	assert(stepper_start(&step, false, 30, 1100));
	stats.errors++;
	stepper_poll(&step, 1110);
	assert(step.phase == STEP_STOP && step.error == 0xe3);
}

int main(void)
{
	test_time_keys();
	test_all_toggle();
	test_protocol();
	test_step_state();
	puts("PASS: debounce, task stops, timer/encoder wrap, Emm framing and failure paths");
	return 0;
}
