/* 以串口替身和虚拟毫秒时钟测试按键、步进命令、故障处理与比赛状态机。 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "contest.h"
#include "emm.h"
#include "jog.h"
#include "logic.h"
#include "serial.h"
#include "stepper.h"

static uint8_t transmitted[128];
static size_t tx_size;
static uint8_t received[128];
static size_t rx_size;
static size_t rx_pos;
static struct serial_stats stats;
static unsigned int disable_count;

/* 模拟向驱动写一帧，保存字节供断言。 */
bool serial_write(enum serial_port port, const uint8_t *data, size_t size)
{
	assert(port == SERIAL_STEP);
	assert(size <= sizeof(transmitted));
	if (size == 6 && data[1] == 0xf3 && !data[3])
		disable_count++;
	memcpy(transmitted, data, size);
	tx_size = size;
	return true;
}

/* 从模拟串口取出下一字节。 */
bool serial_read(enum serial_port port, uint8_t *byte)
{
	assert(port == SERIAL_STEP);
	if (rx_pos == rx_size)
		return false;
	*byte = received[rx_pos++];
	return true;
}

/* 提供模拟的串口故障及丢字节计数。 */
struct serial_stats serial_stats(enum serial_port port)
{
	assert(port == SERIAL_STEP);
	return stats;
}

/* 把指定应答放入模拟接收队列。 */
static void inject(const uint8_t *frame, size_t length)
{
	memcpy(received, frame, length);
	rx_pos = 0;
	rx_size = length;
}

/* 构造同一时刻的多圈轴位与绝对角快照。 */
static void shaft_snapshot(int raw, uint16_t absolute)
{
	uint32_t mag = (uint32_t)(raw < 0 ? -raw : raw);
	const uint8_t reply[31] = { 1, 0x43, 31, 9, [8] = absolute >> 8, absolute & 255,
		[18] = raw < 0, mag >> 24, mag >> 16, mag >> 8, mag & 255, [29] = 3, 0x6b };
	inject(reply, sizeof(reply));
}

/* 构造与命令匹配的驱动应答。 */
static void ack(struct stepper *step, uint8_t command, uint32_t now)
{
	const uint8_t frame[] = { 1, command, 2, 0x6b };

	inject(frame, sizeof(frame));
	stepper_poll(step, now);
}

/* 核验时钟回绕与按键消抖时序。 */
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

/* 核对 Emm 编码和受损数据的重新同步。 */
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
	/* 损坏字节流不得写出定长解析缓冲区。 */
	for (i = 0; i < 10000; i++)
		(void)emm_feed(&parser, 1, (uint8_t)(i * 37), 100 + i, &reply);
	assert(parser.used < sizeof(parser.bytes));
}

/* 核对联合运行任务的启停切换。 */
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
	assert(ctl.task == TASK_GRAY);
	controls_key(&ctl, KEY_MODE, 900005);
	assert(ctl.task == TASK_LAP);
	controls_key(&ctl, KEY_MODE, 900006);
	assert(ctl.task == TASK_BALANCE && !task_uses_wheels(ctl.task));
	controls_key(&ctl, KEY_MODE, 900007);
	assert(ctl.task == TASK_MONITOR);
}

/* 将模拟驱动推进至启动所需的就绪状态。 */
static void make_ready(struct stepper *step)
{
	const uint8_t disabled[] = { 1, 0x3a, 2, 0x6b };
	memset(&stats, 0, sizeof(stats));
	rx_size = rx_pos = 0;
	stepper_init(step);
	assert(!stepper_start(step, false, 30, 0));
	stepper_poll(step, 1000);
	assert(transmitted[1] == 0xfe);
	ack(step, 0xfe, 1010);
	stepper_poll(step, 1016);
	assert(transmitted[1] == 0x3a);
	inject(disabled, sizeof(disabled));
	stepper_poll(step, 1020);
	assert(step->phase == STEP_IDLE);
}

/* 模拟步进使能、应答、取消及超时。 */
static void test_step_state(void)
{
	struct stepper step;
	const uint8_t reached[] = { 1, 0x3a, 3, 0x6b };
	const uint8_t stop_disabled[] = { 1, 0xfe, 0xe2, 0x6b };
	const uint8_t rpm[] = { 1, 0x35, 1, 0, 30, 0x6b };

	stepper_init(&step);
	unsigned int disables = disable_count;
	stepper_poll(&step, 1000);
	inject(stop_disabled, sizeof(stop_disabled));
	stepper_poll(&step, 1010);
	stepper_poll(&step, 1016);
	const uint8_t disabled[] = { 1, 0x3a, 2, 0x6b };
	inject(disabled, sizeof(disabled));
	stepper_poll(&step, 1020);
	assert(step.phase == STEP_IDLE && !step.hold && !step.error && disable_count == disables);

	stepper_init(&step);
	stepper_poll(&step, 1000);
	ack(&step, 0xfe, 1010);
	stepper_poll(&step, 1016);
	inject(reached, sizeof(reached));
	stepper_poll(&step, 1020);
	assert(step.phase == STEP_PARKED && step.hold && step.boot_parked && disable_count == disables);
	assert(!stepper_start(&step, false, 30, 1021));
	step.travel_valid = true;
	step.position_at = 1020;
	step.pulses_per_rev = 3200;
	assert(stepper_start(&step, false, 30, 1022) && !step.boot_parked);

	make_ready(&step);
	assert(!stepper_start(&step, true, 30, 1050));
	step.travel_valid = true;
	step.position_at = 1050;
	step.pulses_per_rev = 3200;
	assert(!stepper_start(&step, true, 0, 1100));
	assert(!stepper_start(&step, true, 91, 1100));
	assert(stepper_start(&step, true, 30, 1100));
	assert(transmitted[1] == 0xf3 && transmitted[3] == 1);
	ack(&step, 0xf3, 1110);
	assert(tx_size == 13 && transmitted[1] == 0xfd && transmitted[2] == 1 && transmitted[10] == 1);
	ack(&step, 0xfd, 1120);
	assert(step.phase == STEP_MOVING);
	stepper_poll(&step, 1320);
	assert(transmitted[1] == 0x3a);
	inject(reached, sizeof(reached));
	stepper_poll(&step, 1330);
	assert(step.phase == STEP_MOVING);
	/* 持续速度模式不应被旧 3 s 期限或位置到位标志停止，速度反馈还须保留方向。 */
	stepper_poll(&step, 10000);
	assert(step.phase == STEP_MOVING && transmitted[1] == 0x35);
	inject(rpm, sizeof(rpm));
	stepper_poll(&step, 10010);
	assert(step.rpm_valid && step.rpm == -30);
	stepper_stop(&step, 10020);
	ack(&step, 0xfe, 10030);
	ack(&step, 0xf3, 10040);
	assert(!stepper_running(&step));

	/* 使能应答尚未收到时取消操作，随后到达的应答也不能排入速度指令。 */
	make_ready(&step);
	assert(!stepper_start(&step, true, 30, 1050));
	step.travel_valid = true;
	step.position_at = 1050;
	step.pulses_per_rev = 3200;
	assert(stepper_start(&step, false, 30, 1100));
	stepper_stop(&step, 1101);
	ack(&step, 0xf3, 1102);
	assert(step.phase == STEP_STOP && transmitted[1] == 0xfe);
	stepper_poll(&step, 1301);
	assert(step.phase == STEP_DISABLE && transmitted[3] == 0);
	stepper_poll(&step, 1501);
	assert(step.phase == STEP_OFFLINE && !stepper_running(&step));

	/* 运动应答丢失或串口接收损坏都应触发停机。 */
	make_ready(&step);
	assert(!stepper_start(&step, true, 30, 1050));
	step.travel_valid = true;
	step.position_at = 1050;
	step.pulses_per_rev = 3200;
	assert(stepper_start(&step, false, 30, 1100));
	ack(&step, 0xf3, 1110);
	stepper_poll(&step, 1310);
	assert(step.phase == STEP_STOP && step.error == 0xe0);
	make_ready(&step);
	assert(!stepper_start(&step, true, 30, 1050));
	step.travel_valid = true;
	step.position_at = 1050;
	step.pulses_per_rev = 3200;
	assert(stepper_start(&step, false, 30, 1100));
	stats.errors++;
	stepper_poll(&step, 1110);
	assert(step.phase == STEP_STOP && step.error == 0xe3);
}

/* 在位置模式下核对定位、反馈及抱持状态。 */
static void test_position_mode(void)
{
	struct stepper step;
	uint8_t cfg[33] = { 1, 0x42, 33, 21, 25, 0, 2, 2, 0, 16 };
	const uint8_t flags[] = { 1, 0x3a, 2, 0x6b };
	const uint8_t held[] = { 1, 0x3a, 3, 0x6b };
	uint8_t move[13];

	make_ready(&step);
	inject(flags, sizeof(flags));
	stepper_poll(&step, 1025);
	assert(!stepper_latch(&step, 1030));
	stepper_request_config(&step, 1030);
	assert(transmitted[1] == 0x42);
	cfg[32] = 0x6b;
	inject(cfg, sizeof(cfg));
	stepper_poll(&step, 1040);
	assert(step.pulses_per_rev == 3200);
	unsigned int before = disable_count;
	assert(stepper_latch(&step, 1050) && step.phase == STEP_LATCH);
	assert(transmitted[1] == 0xf3 && transmitted[3] == 1);
	ack(&step, 0xf3, 1060);
	assert(step.phase == STEP_PARKED && !stepper_running(&step));
	stepper_poll(&step, 1066);
	assert(transmitted[1] == 0x3a);
	inject(held, sizeof(held));
	stepper_poll(&step, 1070);
	stepper_poll(&step, 1076);
	assert(transmitted[1] == 0x43);
	stepper_poll(&step, 1121);
	assert(step.retried && step.pending == 0x43);
	shaft_snapshot(-2048, SHAFT_LEVEL_ABS - 2048);
	stepper_poll(&step, 1130);
	assert(step.position == -2048 && step.position_at == 1130);
	assert(!stepper_hold(&step, -2000, -1000, 1140));
	assert(stepper_hold(&step, -2490, -1000, 1140));
	stepper_poll(&step, 1141);
	assert(step.phase == STEP_POSITION && transmitted[1] == 0x43);
	shaft_snapshot(-2048, SHAFT_LEVEL_ABS - 2048);
	stepper_poll(&step, 1148);
	stepper_poll(&step, 1154);
	assert(emm_position(move, 1, -2048, 3200, 15) == sizeof(move));
	assert(move[2] == 1 && move[9] == 100 && move[10] == 1);
	assert(tx_size == 13 && !memcmp(move, transmitted, sizeof(move)));
	ack(&step, 0xfd, 1162);
	step.goal = -2490;
	stepper_poll(&step, 1183);
	shaft_snapshot(-2048, SHAFT_LEVEL_ABS - 2048);
	stepper_poll(&step, 1186);
	stepper_poll(&step, 1194);
	assert(transmitted[1] == 0xfd && transmitted[9] == 121);
	ack(&step, 0xfd, 1200);
	step.goal--;
	stepper_poll(&step, 1234);
	shaft_snapshot(-2048, SHAFT_LEVEL_ABS - 2048);
	stepper_poll(&step, 1240);
	stepper_poll(&step, 1246);
	assert(step.phase == STEP_BRAKE && step.error == 0xe4 && transmitted[1] == 0xfe);
	ack(&step, 0xfd, 1248);
	assert(step.phase == STEP_BRAKE);
	ack(&step, 0xfe, 1250);
	assert(step.phase == STEP_PARKED && step.hold);
	assert(!stepper_start(&step, true, 30, 1251));
	stepper_poll(&step, 1256);
	inject(held, sizeof(held));
	stepper_poll(&step, 1260);
	assert(step.hold && disable_count == before);
	assert(stepper_hold(&step, -2490, -1000, 1261));
	stepper_poll(&step, 1270);
	stepper_poll(&step, 1315);
	stepper_poll(&step, 1516);
	assert(step.phase == STEP_BRAKE && transmitted[1] == 0xfe && disable_count == before);
	ack(&step, 0xfe, 1520);
	/* 只有显式释放命令才撤去抱持力矩。 */
	stepper_stop(&step, 1530);
	ack(&step, 0xfe, 1540);
	assert(!step.hold && transmitted[1] == 0xf3 && disable_count == before + 1);
}

/* 模拟给定方向的轴位变化并检查测向结果。 */
static void test_direction_measurement(int sign, bool ccw)
{
	struct stepper step = {
		.phase = STEP_PARKED, .hold = true, .flags = 3, .seen = true,
		.last_reply = 1000, .travel_valid = true, .pulses_per_rev = 3200, .position_valid = true,
		.position = -2048, .position_at = 1000,
	};
	unsigned int nudges = 0;
	uint32_t moved_at = 0;
	int raw = -2048;
	assert(stepper_measure_direction(&step, raw, ccw, 1001));
	for (uint32_t t = 1002; t < 2000 && !step.cw_sign; t++) {
		tx_size = 0;
		stepper_poll(&step, t);
		if (!tx_size)
			continue;
		if (transmitted[1] == 0xfd && !transmitted[10]) {
			assert(transmitted[2] == ccw && transmitted[9] == 36);
			nudges++;
			moved_at = t;
			raw += (ccw ? -sign : sign) * 730;
			continue; /* 故意丢弃相对运动的确认应答。 */
		}
		if (transmitted[1] == 0x43) {
			shaft_snapshot(raw, SHAFT_LEVEL_ABS + raw);
		} else {
			uint8_t response[] = { 1, transmitted[1], transmitted[1] == 0x3a ? 3 : 2, 0x6b };
			inject(response, sizeof(response));
		}
	}
	assert(nudges == 1 && moved_at && step.cw_sign == sign && step.phase == STEP_BRAKE);
}

/* 检查已接受目标和限速变化时的指令重发。 */
static void test_position_speed_update(void)
{
	struct stepper step = {
		.phase = STEP_POSITION, .hold = true, .flags = 3, .seen = true,
		.travel_valid = true, .pulses_per_rev = 3200, .lower = -2000, .upper = 2000,
		.goal_sent = true, .goal = 1000, .sent_goal = 1000, .sent_rpm = 30,
		.target_rpm = 8, .last_poll = 1000, .flags_at = 1000, .last_reply = 1000,
	};
	step.rx_errors = stats.errors + stats.rx_drops;
	stepper_poll(&step, 1008);
	assert(step.pending == 0xfd && step.sent_rpm == 8 && transmitted[4] == 8);
	/* ACK 丢失时重发原绝对目标与速度，即使编码器环已要求减速接近终点。 */
	step.target_rpm = 3;
	stepper_poll(&step, 1055);
	assert(step.retried && transmitted[4] == 8);
}

/* 丢弃一次读取应答，核对空闲状态下能否补读。 */
static void test_idle_read_retry(void)
{
	struct stepper step;
	uint8_t cfg[33] = { 1, 0x42, 33, 21, 25, 0, 0, 0, 0, 16 };
	cfg[32] = 0x6b;
	make_ready(&step);
	const uint8_t flags[] = { 1, 0x3a, 2, 0x6b };
	inject(flags, sizeof(flags));
	stepper_poll(&step, 1025);
	stepper_request_config(&step, 1030);
	assert(step.pending == 0x42);
	stats.errors++;
	stepper_poll(&step, 1075);
	const uint8_t config_read[] = { 1, 0x42, 0x6c, 0x6b };
	assert(step.retried && tx_size == sizeof(config_read) &&
	       !memcmp(transmitted, config_read, sizeof(config_read)));
	inject(cfg, sizeof(cfg));
	stepper_poll(&step, 1080);
	assert(step.phase == STEP_IDLE && step.pulses_per_rev == 3200 && !step.error);
	stepper_poll(&step, 1220);
	assert(step.pending == 0x35);
	stepper_poll(&step, 1265);
	assert(step.retried && transmitted[1] == 0x35);
	stepper_poll(&step, 1465);
	assert(step.phase == STEP_OFFLINE && step.error == 0xe0 && !step.pulses_per_rev);
}

/* 对照旧稳球控制器的正负转速检查线上报文。 */
static void test_f407_wire_speed(void)
{
	struct stepper step = { .phase = STEP_PARKED, .hold = true, .flags = 3, .seen = true,
		.travel_valid = true, .pulses_per_rev = 3200, .position_valid = true, .position_at = 1000, .last_reply = 1000 };
	memset(&stats, 0, sizeof(stats));
	rx_size = rx_pos = 0;
	assert(!stepper_speed_loop(&step, -1456, 1456, 1040) && step.phase == STEP_PARKED);
	step.pending = 0x3a;
	assert(!stepper_speed_loop(&step, -1456, 1456, 1000) && step.phase == STEP_PARKED);
	step.pending = 0;
	assert(stepper_speed_loop(&step, -1456, 1456, 1000));
	stepper_poll(&step, 1001);
	assert(step.pending == 0xf6 && transmitted[4] == 0);
	ack(&step, 0xf6, 1002);
	stepper_balance_rpm(&step, 24);
	stepper_poll(&step, 1003);
	assert(step.pending == 0x43);
	shaft_snapshot(0, SHAFT_LEVEL_ABS);
	stepper_poll(&step, 1004);
	stepper_poll(&step, 1005);
	uint8_t ccw[13];
	assert(emm_position(ccw, 1, -1445, 3200, 24) == 13);
	assert(tx_size == sizeof(ccw) && !memcmp(transmitted, ccw, sizeof(ccw)));
	ack(&step, 0xfd, 1006);
	stepper_balance_rpm(&step, -16);
	stepper_poll(&step, 1007);
	uint8_t cw[13];
	assert(emm_position(cw, 1, 1445, 3200, 16) == 13);
	assert(tx_size == sizeof(cw) && !memcmp(transmitted, cw, sizeof(cw)));
	ack(&step, 0xfd, 1008);
	step.position = 1440; /* 接近正向限位时，顺时针目标转速必须降至零。 */
	stepper_poll(&step, 1009);
	assert(step.pending == 0xf6 && !transmitted[4]);
	ack(&step, 0xf6, 1010);
	step.position = 0;
	stepper_poll(&step, 1014);
	assert(step.pending == 0x43);
	stepper_poll(&step, 1039);
	assert(step.phase == STEP_BRAKE && transmitted[1] == 0xfe && step.hold);
	ack(&step, 0xfe, 1040);
	assert(step.phase == STEP_PARKED);
}

/* 模拟点动按键的短按、长按及释放。 */
static void test_jog_buttons(void)
{
	struct jog_keys keys = {0};
	const uint8_t left = 1U << KEY_OK, right = 1U << KEY_NEXT;
	for (int side = 0; side < 2; side++) {
		uint8_t button = side ? right : left;
		assert(jog_key_update(&keys, button, true, UINT32_MAX - 35) == JOG_NONE);
		assert(jog_key_update(&keys, 0, true, 40) == (side ? JOG_RIGHT : JOG_LEFT));
		assert(jog_key_update(&keys, 0, true, 100) == JOG_NONE);
		assert(jog_key_update(&keys, button, true, 150) == JOG_NONE);
		assert(jog_key_update(&keys, button, true, 850) == (side ? JOG_ZERO : JOG_MAP));
		assert(jog_key_update(&keys, button, true, 2000) == JOG_NONE);
		assert(jog_key_update(&keys, 0, true, 2050) == JOG_NONE);
	}
	assert(jog_key_update(&keys, left | right, true, 3000) == JOG_NONE);
	assert(jog_key_update(&keys, right, true, 3200) == JOG_NONE);
	assert(jog_key_update(&keys, 0, true, 3260) == JOG_NONE);
	/* 用于停止自动控制的按键，在松开时不能变成点动命令。 */
	assert(jog_key_update(&keys, left, false, 4000) == JOG_NONE);
	assert(jog_key_update(&keys, 0, true, 4050) == JOG_NONE);
	assert(jog_cal_key(&keys, left, 4100) == JOG_LEFT);
	assert(jog_cal_key(&keys, left, 4549) == JOG_NONE);
	assert(jog_cal_key(&keys, left, 4550) == JOG_LEFT);
	assert(jog_cal_key(&keys, left, 4670) == JOG_LEFT);
	assert(jog_cal_key(&keys, left | right, 4671) == JOG_NONE);
	assert(jog_cal_key(&keys, right, 4900) == JOG_NONE);
	assert(jog_cal_key(&keys, 0, 4901) == JOG_NONE);
	assert(jog_cal_key(&keys, right, 4910) == JOG_RIGHT);
	assert(jog_cal_key(&keys, 0, 4950) == JOG_NONE);
}

/* 核对一次点击到最终抱持的轴位变化。 */
static void test_jog_motion(void)
{
	struct shaft_jog jog = {0};
	struct stepper step = { .phase = STEP_IDLE, .seen = true, .flags = 2,
		.travel_valid = true, .pulses_per_rev = 3200, .position_valid = true, .position = 1247,
		.position_at = 1000, .last_reply = 1000 };
	memset(&stats, 0, sizeof(stats));
	rx_size = rx_pos = 0;
	int actual = step.position;
	int expected_pulses = 61; /* 取距离实测水平位置最近的微步。 */
	unsigned int disables = disable_count;
	uint32_t now = 1050;
	const int directions[] = { -1, 1, 1, -1 };
	for (unsigned int i = 0; i < sizeof(directions) / sizeof(directions[0]); i++) {
		assert(jog_press(&jog, &step, directions[i], now));
		expected_pulses += directions[i] * 16;
		bool sent_absolute = false;
		for (uint32_t start = now; now - start <= 1600 && jog_busy(&jog); now++) {
			if (step.pending == 0x43) {
				shaft_snapshot(actual, SHAFT_LEVEL_ABS + actual);
			} else if (step.pending == 0x3a) {
				const uint8_t flags[] = { 1, 0x3a, 3, 0x6b };
				inject(flags, sizeof(flags));
			} else if (step.pending) {
				uint8_t reply[] = { 1, step.pending, 2, 0x6b };
				assert(step.pending == 0xf3 || step.pending == 0xfd);
				if (step.pending == 0xfd) {
					assert(transmitted[2] == 0 && transmitted[10] == 1 && tx_size == 13);
					assert(!transmitted[6] && !transmitted[7] && !transmitted[8] &&
					       transmitted[9] == expected_pulses);
					sent_absolute = true;
					actual = (expected_pulses * 65536 + 1600) / 3200;
				}
				inject(reply, sizeof(reply));
			}
			stepper_poll(&step, now);
			jog_poll(&jog, &step, now);
		}
		assert(sent_absolute && jog.state == JOG_HELD && step.phase == STEP_PARKED);
		assert(jog.origin == 1247 && step.position == jog.target && step.hold);
	}
	assert(!jog.steps && step.position == 1249 && disable_count == disables);
	int32_t previous = jog.target;
	jog.upper = previous + 100;
	assert(!jog_press(&jog, &step, 1, now) && jog.limited);
	assert(jog.steps == 0 && jog.target == previous && step.phase == STEP_PARKED);
	assert(jog_press(&jog, &step, -1, now) && !jog.limited);
	step.error = 0xe3;
	jog_poll(&jog, &step, ++now);
	assert(jog.state == JOG_FAILED && !jog.referenced && step.hold);
}

/* 模拟 ACK 丢失与外力移轴，检查重试及重新建立参考。 */
static void test_jog_retry_and_rebase(void)
{
	struct shaft_jog jog = { .state = JOG_MOVING, .referenced = true, .origin = -1024,
		.lower = -8000, .upper = 8000 };
	struct stepper step = { .phase = STEP_POSITION, .hold = true, .seen = true,
		.travel_valid = true, .pulses_per_rev = 3200, .position_valid = true, .position = -1024, .flags = 3,
		.last_reply = 1000, .last_poll = 1010, .flags_at = 1010, .position_at = 1000,
		.lower = -8000, .upper = 8000, .target_rpm = 15 };
	memset(&stats, 0, sizeof(stats));
	rx_size = rx_pos = 0;
	assert(jog_press(&jog, &step, 1, 1010));
	stepper_poll(&step, 1010);
	assert(step.pending == 0xfd && transmitted[2] == 1 && transmitted[9] == 34 && transmitted[10] == 1);
	assert(jog_press(&jog, &step, 1, 1020));
	assert(jog.steps == 2 && step.goal != step.sent_goal);
	stepper_poll(&step, 1055); /* 模拟 −34 目标的 ACK 丢失，只允许重发同一绝对目标。 */
	assert(step.retried && transmitted[2] == 1 && transmitted[9] == 34 && transmitted[10] == 1);
	ack(&step, 0xfd, 1056);
	assert(jog_press(&jog, &step, 1, 1057) && jog_press(&jog, &step, 1, 1058));
	uint8_t move[13];
	assert(emm_position(move, 1, step.goal, 3200, 15) == 13 && !move[2] && move[9] == 14);
	assert(jog_press(&jog, &step, -1, 1059));
	assert(emm_position(move, 1, step.goal, 3200, 15) == 13 && move[2] && move[9] == 2);

	/* 已抱停的轴被外力移动后，仅将最新一次点击加到新位置上。 */
	jog.state = JOG_HELD;
	step.phase = STEP_PARKED;
	step.position = jog.target;
	assert(jog_press(&jog, &step, -1, 1100));
	step.position = 4096;
	step.position_at = 1105;
	jog_poll(&jog, &step, 1106);
	assert(jog.state == JOG_MOVING && jog.origin == 4096 && jog.steps == -1);
	assert(emm_position(move, 1, step.goal, 3200, 15) == 13 && !move[2] && move[9] == 184);
}

/* 在两个驱动原点下核验相同的实物行程。 */
static void test_calibrated_travel(void)
{
	/* 模拟 Emm 使用不同多圈原点，实际两端限位应一致。 */
	const int levels[] = { -80313, 0, 795 };
	const int offsets[] = { SHAFT_HIGH_OFFSET, SHAFT_LOW_OFFSET, 0 };
	for (unsigned int i = 0; i < 3; i++) {
		for (unsigned int j = 0; j < 3; j++) {
			const int raw = levels[i] + offsets[j];
			struct stepper s = { .phase = STEP_PARKED, .hold = true, .seen = true,
				.pending = 0x43, .pulses_per_rev = 3200, .rx_errors = stats.errors + stats.rx_drops };
			shaft_snapshot(raw, SHAFT_LEVEL_ABS + offsets[j]);
			stepper_poll(&s, 1000);
			assert(s.travel_valid && s.shaft_level == levels[i] && s.position == raw);
			assert(stepper_hold(&s, levels[i] - 65536, levels[i] + 65536, 1000));
			assert(s.lower == levels[i] + SHAFT_HIGH_OFFSET && s.upper == levels[i] + SHAFT_LOW_OFFSET);
			s.last_poll = s.flags_at = 1000;
			s.goal = j == 1 ? s.upper : s.lower;
			stepper_poll(&s, 1006);
			assert(s.pending == 0xfd && transmitted[10] == 1);
			int pulses = transmitted[8] * 256 + transmitted[9];
			int target = (transmitted[2] ? -pulses : pulses) * 65536 / 3200;
			assert(target > s.lower && target < s.upper);
			/* 运动途中检测到驱动原点变化时，先制动再考虑下一次定位。 */
			s.pending = 0x43;
			shaft_snapshot(raw + 7500, SHAFT_LEVEL_ABS + offsets[j]);
			stepper_poll(&s, 1010);
			assert(s.phase == STEP_BRAKE && s.pending == 0xfe);
		}
	}
	struct stepper s = { .phase = STEP_PARKED, .hold = true, .pending = 0x43, .pulses_per_rev = 3200,
		.rx_errors = stats.errors + stats.rx_drops };
	shaft_snapshot(0, SHAFT_LEVEL_ABS + SHAFT_LOW_OFFSET + 1);
	stepper_poll(&s, 1000);
	assert(!s.travel_valid && !stepper_hold(&s, -1000, 1000, 1000));
	assert(!stepper_speed_loop(&s, -1000, 1000, 1000));
	assert(!stepper_measure_direction(&s, 0, false, 1000));
	s.phase = STEP_IDLE;
	assert(!stepper_start(&s, false, 30, 1000));
}

/* 按菜单与实测序列验证第一题的三次停稳及左点保持。 */
static void test_contest(void)
{
	struct contest_menu m = { .page = MENU_HOME };
	enum key_id back = KEY_START, confirm = KEY_MODE; /* 板级按键映射中 PH3 接 SW5，PD15 接 SW2。 */
	for (unsigned int i = 0; i < EVENT_COUNT; i++) {
		assert(contest_key(&m, back) == MENU_NONE && m.page == MENU_HOME);
		assert((unsigned int)m.item == i);
		assert(contest_key(&m, confirm) == MENU_NONE && m.page == MENU_DETAIL);
		if (m.item == EVENT_LAP)
			assert(contest_key(&m, KEY_NEXT) == MENU_LAP_SPEED && m.page == MENU_DETAIL);
		assert(contest_key(&m, confirm) == MENU_START_EVENT);
		assert(contest_key(&m, back) == MENU_NONE && m.page == MENU_HOME);
		contest_key(&m, KEY_NEXT);
	}
	assert(m.item == EVENT_POSITION);
	assert(contest_lap_notice(LAP_RUNNING, 29999, CONTEST_LAP_LIMIT_MS) == NOTICE_NONE);
	assert(contest_lap_notice(LAP_RUNNING, 30000, CONTEST_LAP_LIMIT_MS) == NOTICE_TIMEOUT);
	assert(contest_lap_notice(LAP_DONE, 30000, CONTEST_LAP_LIMIT_MS) == NOTICE_FINISHED);
	assert(contest_lap_notice(LAP_DONE, 30001, CONTEST_LAP_LIMIT_MS) == NOTICE_TIMEOUT);
	assert(contest_lap_notice(LAP_ENCODER, 4000, CONTEST_LAP_LIMIT_MS) == NOTICE_DRIVE);
	assert(contest_lap_notice(LAP_DRIVE, 10, CONTEST_LAP_LIMIT_MS) == NOTICE_DRIVE);
	assert(contest_lap_notice(LAP_NO_MARKER, 20000, CONTEST_LAP_LIMIT_MS) == NOTICE_LINE);
	assert(contest_lap_notice(LAP_CANCELLED, 2000, CONTEST_LAP_LIMIT_MS) == NOTICE_STOPPED);
	m.page = MENU_DETAIL;
	m.notice = NOTICE_TRAVEL;
	assert(contest_key(&m, confirm) == MENU_RELEVEL && m.page == MENU_DETAIL);
	assert(contest_ball_mm(160) == 50 && contest_ball_mm(400) == -50 && !contest_ball_mm(282));
	struct position_run r;
	uint32_t start = UINT32_MAX - 200;
	position_begin(&r, 282, 65530, start);
	assert(r.leg == LEVEL_SHAFT && !r.goal_cd);
	assert(position_update(&r, BALL_TRACKED, 282, 65531, start + 10, start + 10) == NOTICE_NONE && !r.goal_cd);
	position_check_level(&r, true, start + 12);
	position_check_level(&r, true, start + 12);
	position_check_level(&r, false, start + 17);
	position_check_level(&r, true, start + 20);
	position_check_level(&r, true, start + 94);
	assert(r.leg == LEVEL_SHAFT);
	for (unsigned int dt = 25; dt <= 6400; dt += 25) {
		if (dt == 100)
			position_check_level(&r, true, start + dt);
		int x = r.leg == MOVE_RIGHT ? 160 : r.leg == MOVE_LEFT || r.leg == HOLD_LEFT ? 400 : 282;
		enum position_leg before = r.leg;
		assert(position_update(&r, BALL_TRACKED, x, (uint16_t)(65530 + dt / 25), start + dt, start + dt) == NOTICE_NONE);
		if (r.leg != before) {
			assert(start + dt - r.steady_at >= (before == MOVE_CENTER ? 300U : 400U));
		}
		if (dt == 25)
			assert(r.leg == LEVEL_SHAFT && !r.goal_cd);
		if (dt == 375)
			assert(r.leg == MOVE_CENTER);
	}
	assert(r.leg == HOLD_LEFT && r.finished_ms >= 1000 && r.finished_ms <= 6400);
	r = (struct position_run){ .leg = MOVE_RIGHT, .mm = 50 };
	for (int ms = 25; ms < 475; ms += 25) {
		assert(position_update(&r, ms != 275 ? BALL_TRACKED : BALL_MISSING, 160, ms / 25, ms, ms) == NOTICE_NONE);
		assert(r.leg == MOVE_RIGHT);
	}
	assert(position_update(&r, BALL_TRACKED, 160, 19, 475, 475) == NOTICE_NONE && r.leg == MOVE_LEFT);
	r = (struct position_run){ .leg = MOVE_RIGHT, .mm = 50 };
	for (int ms = 25; ms <= 725; ms += 25) {
		assert(position_update(&r, ms != 275 && ms != 300 ? BALL_TRACKED : BALL_MISSING, 160, ms / 25, ms, ms) == NOTICE_NONE);
		assert(r.leg == (ms == 725 ? MOVE_LEFT : MOVE_RIGHT));
	}
	r = (struct position_run){ .leg = MOVE_RIGHT, .mm = 50 };
	for (int i = 1; i <= 30; i++) {
		assert(position_update(&r, i % 4 != 0 ? BALL_TRACKED : BALL_MISSING, 160, i, i * 33, i * 33) == NOTICE_NONE);
		assert(r.leg == MOVE_RIGHT);
	}
	r = (struct position_run){ .leg = MOVE_RIGHT, .mm = 47, .have_sample = true };
	for (int ms = 50; ms <= 650; ms += 50) {
		int x = ms <= 100 ? 167 : ms <= 350 ? 160 : 155; /* 模拟右侧 +47、+50、+52 mm 三个位置。 */
		assert(position_update(&r, BALL_TRACKED, x, ms / 50, ms, ms) == NOTICE_NONE);
		if (ms < 650)
			assert(r.leg == MOVE_RIGHT);
	}
	assert(r.leg == MOVE_LEFT && r.steady_at == 250);
	r = (struct position_run){ .leg = MOVE_LEFT, .mm = -50, .have_sample = true };
	for (int ms = 25; ms < 775; ms += 25) {
		/* 移动 6 mm 虽仍在目标容差内，但已超出停稳窄带，须重新计时。 */
		int x = ms == 350 ? 386 : 400;
		assert(position_update(&r, BALL_TRACKED, x, ms / 25, ms, ms) == NOTICE_NONE && r.leg == MOVE_LEFT);
	}
	for (int ms = 775; ms <= 1000; ms += 25)
		assert(position_update(&r, BALL_TRACKED, 400, ms / 25, ms, ms) == NOTICE_NONE);
	assert(r.leg == HOLD_LEFT && r.finished_ms >= 775);
	r = (struct position_run){ .leg = HOLD_LEFT, .mm = -50, .have_sample = true, .goal_cd = -910,
				   .trim_milli_cd = -110000, .sample_at = 1000 };
	assert(position_update(&r, BALL_MISSING, 399, 2, 1550, 1550) == NOTICE_NONE && r.goal_cd == -910);
	assert(position_update(&r, BALL_TRACKED, 400, 24, 1690, 1690) == NOTICE_NONE && r.goal_cd == -910);
	assert(position_update(&r, BALL_MISSING, 400, 35, 2590, 2590) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_MISSING, 402, 240, 9600, 9600) == NOTICE_NONE && r.goal_cd == -910);
	assert(position_update(&r, BALL_TRACKED, 160, 241, 9633, 9633) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 387, 242, 9666, 9666) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 387, 242, 9666, 9670) == NOTICE_NONE && r.goal_cd == -910);
	assert(position_update(&r, BALL_MISSING, 387, 243, 9700, 9700) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 386, 244, 9733, 9733) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 386, 245, 9766, 9766) == NOTICE_NONE && !r.waiting_vision);
	assert(r.mm == -44 && r.goal_cd < -910 && r.leg == HOLD_LEFT);
	assert(position_update(&r, BALL_OFFLINE, 386, 245, 9766, 10666) == NOTICE_VISION);
	r = (struct position_run){ .leg = MOVE_LEFT, .mm = -47, .have_sample = true, .sample_at = 5300 };
	for (int i = 1; i <= 8; i++)
		assert(position_update(&r, BALL_TRACKED, 393, i, 5300 + 25*i, 5300 + 25*i) == NOTICE_NONE);
	int parked_goal = r.goal_cd;
	assert(position_update(&r, BALL_MISSING, 391, 24, 6015, 6015) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_MISSING, 391, 200, 12500, 12500) == NOTICE_NONE);
	assert(r.leg == MOVE_LEFT && !r.finished_ms && !r.active_bands && r.goal_cd == parked_goal);
	for (int i = 1; i <= 17; i++) {
		assert(position_update(&r, BALL_TRACKED, 393, 200+i, 12500+25*i, 12500+25*i) == NOTICE_NONE);
		assert(!r.finished_ms && r.leg == MOVE_LEFT);
	}
	assert(position_update(&r, BALL_TRACKED, 393, 218, 12950, 12950) == NOTICE_NONE);
	assert(r.finished_ms == 12950 && r.leg == HOLD_LEFT);
	r = (struct position_run){ .leg = HOLD_LEFT, .mm = -58, .have_sample = true, .finished_ms = 5904 };
	for (int i = 1; i <= 160; i++)
		assert(position_update(&r, BALL_TRACKED, 419, i, 25 * i, 25 * i) == NOTICE_NONE);
	assert(r.trim_milli_cd == 250000 && r.goal_cd == -310 && r.finished_ms == 5904);
	assert(position_update(&r, BALL_TRACKED, 395, 161, 4025, 4025) == NOTICE_NONE);
	assert(r.velocity > 0 && r.goal_cd == -1600);
	position_begin(&r, 282, 4, 0);
	assert(position_update(&r, BALL_TRACKED, 282, 5, 25, 25) == NOTICE_NONE && !r.goal_cd);
	position_check_level(&r, true, 30);
	position_check_level(&r, true, 110);
	assert(position_update(&r, BALL_TRACKED, 282, 6, 110, 110) == NOTICE_NONE && r.leg == MOVE_CENTER && r.goal_cd == -300);
	assert(position_update(&r, BALL_MISSING, 160, 7, 240, 240) == NOTICE_NONE && r.goal_cd == 0);
	assert(position_update(&r, BALL_MISSING, 160, 8, 620, 620) == NOTICE_NONE && r.waiting_vision);
	assert(!r.goal_cd && !r.active_bands);
	assert(position_update(&r, BALL_UNCERTAIN, 270, 10, 700, 700) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 247, 13, 801, 801) == NOTICE_NONE && r.waiting_vision);
	assert(position_update(&r, BALL_TRACKED, 238, 14, 834, 834) == NOTICE_NONE && !r.waiting_vision);
	assert(r.mm == contest_ball_mm(238) && r.velocity > 0 && r.leg == MOVE_CENTER);
	assert(position_update(&r, BALL_OFFLINE, 238, 14, 834, 1400) == NOTICE_VISION);
	/* 超 8 s 的慢速运行仍执行三段完整停留，保留原起点时间并进入持续左点保持。 */
	position_begin(&r, 282, 0, 0);
	position_check_level(&r, true, 0);
	position_check_level(&r, true, 100);
	for (int i = 1; i <= 500; i++) {
		int x = i < 325 ? 215 : r.leg == MOVE_CENTER ? 282 : r.leg == MOVE_RIGHT ? 160 : 400;
		assert(position_update(&r, BALL_TRACKED, x, i, i * 25, i * 25) == NOTICE_NONE);
		if (i == 321)
			assert(r.leg == MOVE_CENTER && !r.finished_ms);
	}
	assert(r.leg == HOLD_LEFT && r.finished_ms > 8000 && r.finished_ms < 12500);
	/* 实车记录 45682 中，向 O 移动时实测一直为 −44 mm。一旦观测到球滚动，应撤掉额外的起动倾角。 */
	r = (struct position_run){ .leg = MOVE_CENTER, .mm = -44, .have_sample = true };
	for (int i = 1; i <= 90; i++) {
		assert(position_update(&r, BALL_TRACKED, 386, i, i * 30, i * 30) == NOTICE_NONE);
		if (i == 10)
			assert(r.goal_cd == 800);
	}
	assert(r.goal_cd == 1600 && r.leg == MOVE_CENTER);
	assert(position_update(&r, BALL_TRACKED, 377, 91, 2730, 2730) == NOTICE_NONE);
	assert(!r.still_ms && r.goal_cd < 800);
	position_begin(&r, -1, 1, 0);
	position_check_level(&r, true, 120);
	assert(position_update(&r, BALL_MISSING, 282, 2, 180, 180) == NOTICE_NONE);
	position_check_level(&r, true, 200);
	assert(position_update(&r, BALL_TRACKED, 400, 3, 250, 250) == NOTICE_NONE && r.leg == MOVE_CENTER && r.goal_cd > 0);
	position_begin(&r, 49, 1, 0);
	position_check_level(&r, true, 10);
	position_check_level(&r, true, 85);
	assert(position_update(&r, BALL_TRACKED, 49, 2, 100, 100) == NOTICE_NONE && r.leg == MOVE_CENTER && r.goal_cd < 0);
	r = (struct position_run){ .leg = MOVE_LEFT, .mm = -31 };
	for (int i = 1; i <= 70; i++)
		assert(position_update(&r, BALL_TRACKED, 355, i, i * 25, i * 25) == NOTICE_NONE);
	assert(r.leg == MOVE_LEFT && r.trim_milli_cd > -900000 && r.goal_cd == -1600);
	int saturated_trim = r.trim_milli_cd;
	assert(position_update(&r, BALL_TRACKED, 355, 71, 1775, 1775) == NOTICE_NONE && r.trim_milli_cd == saturated_trim);
	assert(position_update(&r, BALL_MISSING, 355, 71, 1900, 1900) == NOTICE_NONE && !r.goal_cd && !r.trim_milli_cd);
}

/* 在原地稳球期间改变目标，检查定点阶段与再次获取球位。 */
static void test_offset_positions(void)
{
	const int left = contest_ball_mm(RULER_X_LEFT), right = contest_ball_mm(RULER_X_RIGHT);
	assert(left == -106 && right == 95);
	struct contest_menu m = { .page = MENU_DETAIL, .item = EVENT_OFFSET };
	for (int i = 0; i >= left; i--)
		contest_key(&m, KEY_OK);
	assert(m.target_mm == left);
	m.page = MENU_RUN;
	for (int mm = left; mm <= right; mm++) {
		assert(m.target_mm == mm);
		contest_key(&m, KEY_NEXT);
	}
	assert(m.target_mm == right);
	m.offset_driving = true;
	assert(contest_key(&m, KEY_OK) == MENU_NONE && m.target_mm == right);

	const int locations[] = { RULER_X_LEFT, 385, RULER_X_O, 203, RULER_X_RIGHT };
	struct position_run r;
	position_begin_target(&r, left, RULER_X_LEFT, 0, 0);
	position_check_level(&r, true, 1);
	position_check_level(&r, true, 76);
	uint16_t seq = 0;
	uint32_t now = 100;
	for (unsigned int i = 0; i < sizeof(locations) / sizeof(locations[0]); i++) {
		int x = locations[i], target = contest_ball_mm(x);
		position_retarget(&r, target);
		for (int sample = 0; sample < 70; sample++) {
			now += 25;
			assert(position_update(&r, BALL_TRACKED, x, ++seq, now, now) == NOTICE_NONE);
		}
		assert(r.leg == HOLD_TARGET && r.mm == target && r.target_mm == target && !r.start);
	}
	/* 失测后允许确认球已移动，同时保留原先选定的固定目标。 */
	now += 600;
	assert(position_update(&r, BALL_MISSING, 0, ++seq, now, now) == NOTICE_NONE && r.waiting_vision);
	now += 25;
	position_update(&r, BALL_TRACKED, RULER_X_LEFT, ++seq, now, now);
	assert(r.waiting_vision);
	now += 25;
	position_update(&r, BALL_TRACKED, RULER_X_LEFT, ++seq, now, now);
	assert(!r.waiting_vision && r.leg == MOVE_TARGET && r.target_mm == right && r.goal_cd > 0);
	position_retarget(&r, left);
	now += 25;
	position_update(&r, BALL_TRACKED, RULER_X_O, ++seq, now, now);
	assert(r.leg == MOVE_TARGET && r.target_mm == left && r.goal_cd < 0);
}

/* 依次执行协议、控制及题目状态机回归。 */
int main(void)
{
	test_contest();
	test_offset_positions();
	test_calibrated_travel();
	test_jog_buttons();
	test_jog_motion();
	test_jog_retry_and_rebase();
	test_idle_read_retry();
	test_f407_wire_speed();
	test_time_keys();
	test_all_toggle();
	test_protocol();
	test_step_state();
	test_position_mode();
	test_position_speed_update();
	test_direction_measurement(1, false);
	test_direction_measurement(-1, false);
	test_direction_measurement(1, true);
	test_direction_measurement(-1, true);
	puts("PASS: debounce, task stops, timer/encoder wrap, Emm framing and failure paths");
	return 0;
}
