#include "stm32c5xx.h"
#include "stm32c5xx_ll_bus.h"
#include "stm32c5xx_ll_gpio.h"
#include "stm32c5xx_ll_rcc.h"
#include "stm32c5xx_ll_spi.h"
#include "stm32c5xx_ll_tim.h"
#include "board.h"
#include "config.h"
#include "logic.h"
#include "wheel_map.h"
#include "gray_map.h"

static volatile uint32_t ticks;
static volatile uint32_t heartbeat;
static volatile uint32_t wheel_started;
static volatile bool wheel_active;
static volatile bool wheel_continuous;
static volatile enum wheel_trip wheel_trip;
static volatile uint8_t wheel_fault_pins;
static uint32_t j4_compare;
static uint32_t j6_compare;
static bool j4_reverse;
static bool j6_reverse;

static void pin_mode(GPIO_TypeDef *port, unsigned int pin, uint32_t mode,
		     uint32_t pull)
{
	uint32_t mask = 1U << pin;

	LL_GPIO_SetPinPull(port, mask, pull);
	LL_GPIO_SetPinOutputType(port, mask, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinSpeed(port, mask, LL_GPIO_SPEED_FREQ_LOW);
	LL_GPIO_SetPinMode(port, mask, mode);
}

static void pin_af(GPIO_TypeDef *port, unsigned int pin, uint32_t af,
		   uint32_t pull)
{
	uint32_t mask = 1U << pin;

	if (pin < 8)
		LL_GPIO_SetAFPin_0_7(port, mask, af);
	else
		LL_GPIO_SetAFPin_8_15(port, mask, af);
	pin_mode(port, pin, LL_GPIO_MODE_ALTERNATE, pull);
	LL_GPIO_SetPinSpeed(port, mask, LL_GPIO_SPEED_FREQ_HIGH);
}

static void encoder_init(TIM_TypeDef *timer)
{
	LL_TIM_SetPrescaler(timer, 0);
	LL_TIM_SetAutoReload(timer, UINT32_MAX);
	LL_TIM_IC_SetActiveInput(timer, LL_TIM_CHANNEL_CH1,
				LL_TIM_ACTIVEINPUT_DIRECT);
	LL_TIM_IC_SetActiveInput(timer, LL_TIM_CHANNEL_CH2,
				LL_TIM_ACTIVEINPUT_DIRECT);
	LL_TIM_IC_SetFilter(timer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1_N8);
	LL_TIM_IC_SetFilter(timer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1_N8);
	LL_TIM_IC_SetPolarity(timer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
	LL_TIM_IC_SetPolarity(timer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);
	LL_TIM_SetClockSource(timer, LL_TIM_CLK_ENCODER_X4_TI12);
	LL_TIM_CC_EnableChannel(timer, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);
	LL_TIM_GenerateEvent_UPDATE(timer);
	LL_TIM_SetCounter(timer, 0);
	LL_TIM_EnableCounter(timer);
}

static void pwm_init(void)
{
	LL_TIM_SetPrescaler(TIM4, 0);
	LL_TIM_SetAutoReload(TIM4, BOARD_CLOCK_HZ / WHEEL_PWM_HZ - 1);
	LL_TIM_OC_SetMode(TIM4, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
	LL_TIM_OC_SetMode(TIM4, LL_TIM_CHANNEL_CH4, LL_TIM_OCMODE_PWM1);
	LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH1);
	LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH4);
	LL_TIM_OC_SetCompareCH1(TIM4, 0);
	LL_TIM_OC_SetCompareCH4(TIM4, 0);
	LL_TIM_CC_EnableChannel(TIM4, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH4);
	LL_TIM_GenerateEvent_UPDATE(TIM4);
	LL_TIM_EnableCounter(TIM4);
}

bool board_init(void)
{
	unsigned int pin;

	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA |
		LL_AHB2_GRP1_PERIPH_GPIOB | LL_AHB2_GRP1_PERIPH_GPIOC |
		LL_AHB2_GRP1_PERIPH_GPIOD | LL_AHB2_GRP1_PERIPH_GPIOE |
		LL_AHB2_GRP1_PERIPH_GPIOH);
	/* Set the output latch before taking ownership of the motor enable. */
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_3 | LL_GPIO_PIN_4);
	pin_mode(GPIOA, 3, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	pin_mode(GPIOA, 4, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_3 | LL_GPIO_PIN_11);
	pin_mode(GPIOC, 3, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	pin_mode(GPIOC, 11, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
	pin_mode(GPIOE, 3, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);

	/* Keep the reset clock: HSI / 3 = 48 MHz, all APB prescalers = 1. */
	LL_RCC_SetAHBPrescaler(LL_RCC_HCLK_PRESCALER_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_PRESCALER_1);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_PRESCALER_1);
	LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_PRESCALER_1);
	SystemCoreClockUpdate();
	if (SystemCoreClock != BOARD_CLOCK_HZ)
		return false;
	if (SysTick_Config(SystemCoreClock / 1000U))
		return false;
	NVIC_SetPriority(SysTick_IRQn, 0);

	pin_mode(GPIOH, 4, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	pin_mode(GPIOC, 10, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	pin_mode(GPIOD, 15, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	pin_mode(GPIOA, 8, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	pin_mode(GPIOA, 10, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	pin_mode(GPIOH, 3, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_UP);
	for (pin = 7; pin <= 14; pin++)
		pin_mode(GPIOE, pin, LL_GPIO_MODE_INPUT, LL_GPIO_PULL_NO);
	pin_mode(GPIOH, 5, LL_GPIO_MODE_ANALOG, LL_GPIO_PULL_NO);
	pin_mode(GPIOC, 0, LL_GPIO_MODE_ANALOG, LL_GPIO_PULL_NO);

	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2 |
		LL_APB1_GRP1_PERIPH_TIM5 | LL_APB1_GRP1_PERIPH_TIM4 |
		LL_APB1_GRP1_PERIPH_SPI3 | LL_APB1_GRP1_PERIPH_USART6);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
	/* The ROM loader may leave peripherals configured before a debugger Go. */
	LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM2 |
		LL_APB1_GRP1_PERIPH_TIM5 | LL_APB1_GRP1_PERIPH_TIM4 |
		LL_APB1_GRP1_PERIPH_SPI3 | LL_APB1_GRP1_PERIPH_USART6);
	LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM2 |
		LL_APB1_GRP1_PERIPH_TIM5 | LL_APB1_GRP1_PERIPH_TIM4 |
		LL_APB1_GRP1_PERIPH_SPI3 | LL_APB1_GRP1_PERIPH_USART6);
	LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1);
	LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_USART1);
	pin_af(GPIOA, 0, LL_GPIO_AF_2, LL_GPIO_PULL_NO);
	pin_af(GPIOA, 1, LL_GPIO_AF_2, LL_GPIO_PULL_NO);
	pin_af(GPIOB, 3, LL_GPIO_AF_1, LL_GPIO_PULL_NO);
	pin_af(GPIOA, 15, LL_GPIO_AF_1, LL_GPIO_PULL_NO);
	encoder_init(TIM5);
	encoder_init(TIM2);
	pwm_init();
	pin_af(GPIOC, 2, LL_GPIO_AF_2, LL_GPIO_PULL_NO);
	pin_af(GPIOB, 6, LL_GPIO_AF_2, LL_GPIO_PULL_NO);

	LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
	LL_RCC_SetUSARTClockSource(LL_RCC_USART6_CLKSOURCE_PCLK1);
	pin_af(GPIOA, 6, LL_GPIO_AF_7, LL_GPIO_PULL_NO);
	pin_af(GPIOA, 7, LL_GPIO_AF_7, LL_GPIO_PULL_UP);
	pin_af(GPIOC, 6, LL_GPIO_AF_7, LL_GPIO_PULL_NO);
	pin_af(GPIOC, 7, LL_GPIO_AF_7, LL_GPIO_PULL_UP);

	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_4);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_5);
	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0);
	pin_mode(GPIOC, 4, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	pin_mode(GPIOC, 5, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	pin_mode(GPIOB, 0, LL_GPIO_MODE_OUTPUT, LL_GPIO_PULL_NO);
	LL_RCC_SetSPIClockSource(LL_RCC_SPI3_CLKSOURCE_PCLK1);
	pin_af(GPIOB, 1, LL_GPIO_AF_4, LL_GPIO_PULL_NO);
	pin_af(GPIOB, 2, LL_GPIO_AF_7, LL_GPIO_PULL_NO);
	/* SetConfig establishes SSI before MASTER, avoiding a latched MODF. */
	LL_SPI_SetConfig(SPI3,
		LL_SPI_DATA_WIDTH_8_BIT | LL_SPI_BAUD_RATE_PRESCALER_8,
		LL_SPI_MODE_MASTER | LL_SPI_SIMPLEX_TX | LL_SPI_NSS_SOFT |
		LL_SPI_CLOCK_POLARITY_LOW | LL_SPI_CLOCK_PHASE_1_EDGE |
		LL_SPI_MSB_FIRST);
	LL_SPI_SetFIFOThreshold(SPI3, LL_SPI_FIFO_THRESHOLD_1_DATA);
	LL_SPI_EnableGPIOControl(SPI3);
	return true;
}

void board_camera_uart_init(void)
{
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5);
	LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_UART5);
	LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_UART5);
	LL_RCC_SetUARTClockSource(LL_RCC_UART5_CLKSOURCE_PCLK1);
	/* J19 pin 5 is driven by MaixCAM2. Pin 6 stays high impedance. */
	pin_af(GPIOB, 12, LL_GPIO_AF_14, LL_GPIO_PULL_UP);
}

uint32_t board_millis(void)
{
	return ticks;
}

void board_delay(uint32_t ms)
{
	uint32_t start = ticks;

	while (!elapsed(ticks, start, ms))
		__WFI();
}

void board_heartbeat(void)
{
	heartbeat = ticks;
}

void board_led(bool on)
{
	if (on)
		LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
	else
		LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
}

uint8_t board_keys(void)
{
	return (!LL_GPIO_IsInputPinSet(GPIOD, LL_GPIO_PIN_15) << KEY_MODE) |
	       (!LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_8) << KEY_NEXT) |
	       (!LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_10) << KEY_OK) |
	       (!LL_GPIO_IsInputPinSet(GPIOH, LL_GPIO_PIN_3) << KEY_START);
}

uint8_t board_gray(void)
{
	return gray_input_order((uint8_t)(LL_GPIO_ReadInputPort(GPIOE) >> 7));
}

uint8_t board_faults(void)
{
	uint8_t channels = (!LL_GPIO_IsInputPinSet(GPIOH, LL_GPIO_PIN_4)) |
			   (!LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_10) << 1);

	return wheel_faults_to_physical(channels);
}

uint8_t board_encoder_pins(void)
{
	uint8_t channels = (LL_GPIO_ReadInputPort(GPIOA) & 3U) |
		(LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_3) << 2) |
		(LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_15) << 3);

	return wheel_phases_to_physical(channels);
}

uint32_t board_encoder(unsigned int side)
{
	return LL_TIM_GetCounter(wheel_channel(side) ? TIM2 : TIM5);
}

void board_wheels_stop(void)
{
	/* Safe to call from SysTick and thread context; never re-enables anything. */
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_3);
	LL_TIM_OC_SetCompareCH4(TIM4, 0);
	LL_TIM_OC_SetCompareCH1(TIM4, 0);
	wheel_active = false;
}

static uint32_t duty_compare(int duty)
{
	unsigned int magnitude = duty < 0 ? (unsigned int)-duty : (unsigned int)duty;

	return (BOARD_CLOCK_HZ / WHEEL_PWM_HZ) * magnitude / 1000U;
}

bool board_wheels_start(int left, int right, bool continuous)
{
	uint32_t irq = __get_PRIMASK();
	struct wheel_channels channels = wheel_to_channels(left, right);

	if (left < -WHEEL_MAX_PERMILLE || left > WHEEL_MAX_PERMILLE ||
	    right < -WHEEL_MAX_PERMILLE || right > WHEEL_MAX_PERMILLE)
		return false;
	__disable_irq();
	board_wheels_stop();
	wheel_trip = WHEEL_OK;
	wheel_fault_pins = board_faults();
	if (wheel_fault_pins) {
		wheel_trip = WHEEL_FAULT;
		__set_PRIMASK(irq);
		return false;
	}
	if (channels.j4 >= 0)
		LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_3);
	else
		LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_3);
	if (channels.j6 >= 0)
		LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_11);
	else
		LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_11);
	j4_compare = duty_compare(channels.j4);
	j6_compare = duty_compare(channels.j6);
	j4_reverse = channels.j4 < 0;
	j6_reverse = channels.j6 < 0;
	wheel_started = ticks;
	heartbeat = ticks;
	wheel_continuous = continuous;
	wheel_active = true;
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_3);
	__set_PRIMASK(irq);
	return true;
}

static void wheels_check(void)
{
	if (!wheel_active)
		return;
	/* nFAULT may be low while the charge pump wakes; PWM stays zero here. */
	if (elapsed(ticks, wheel_started, WHEEL_WAKE_MS)) {
		wheel_fault_pins = board_faults();
		if (!LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_3))
			wheel_trip = WHEEL_ENABLE_LOW;
		else if (wheel_fault_pins)
			wheel_trip = WHEEL_FAULT;
	}
	if (wheel_trip == WHEEL_OK && elapsed(ticks, heartbeat, MAIN_STALL_MS))
		wheel_trip = WHEEL_STALL;
	if (wheel_trip == WHEEL_OK && !wheel_continuous &&
	    elapsed(ticks, wheel_started, TEST_DURATION_MS))
		wheel_trip = WHEEL_DEADLINE;
	if (wheel_trip != WHEEL_OK)
		board_wheels_stop();
}

void board_wheels_poll(void)
{
	uint32_t irq = __get_PRIMASK();

	__disable_irq();
	wheels_check();
	/* DRV8874 wake-up is 1 ms typical; allow 5 ms before PWM. */
	if (wheel_active && elapsed(ticks, wheel_started, WHEEL_WAKE_MS)) {
		LL_TIM_OC_SetCompareCH4(TIM4, j4_compare);
		LL_TIM_OC_SetCompareCH1(TIM4, j6_compare);
	}
	__set_PRIMASK(irq);
}

bool board_lap_set_wheels(int left, int right)
{
	uint32_t irq;
	bool ok;
	struct wheel_channels channels = wheel_to_channels(left, right);

	if (left < -LAP_RACE_MAX_PERMILLE || left > LAP_RACE_MAX_PERMILLE ||
	    right < -LAP_RACE_MAX_PERMILLE || right > LAP_RACE_MAX_PERMILLE)
		return false;
	irq = __get_PRIMASK();
	__disable_irq();
	wheels_check();
	/* Live speed changes cannot restart a tripped drive or reverse its PH. */
	ok = wheel_active && (!channels.j4 || (channels.j4 < 0) == j4_reverse) &&
	     (!channels.j6 || (channels.j6 < 0) == j6_reverse);
	if (ok) {
		j4_compare = duty_compare(channels.j4);
		j6_compare = duty_compare(channels.j6);
		if (elapsed(ticks, wheel_started, WHEEL_WAKE_MS)) {
			LL_TIM_OC_SetCompareCH4(TIM4, j4_compare);
			LL_TIM_OC_SetCompareCH1(TIM4, j6_compare);
		}
	}
	__set_PRIMASK(irq);
	return ok;
}

bool board_wheels_active(void)
{
	return wheel_active;
}

enum wheel_trip board_wheels_trip(void)
{
	return wheel_trip;
}

uint8_t board_wheels_faults(void)
{
	return wheel_fault_pins;
}

void SysTick_Handler(void)
{
	ticks++;
	wheels_check();
}

void board_lcd_reset(bool high)
{
	if (high)
		LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0);
	else
		LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0);
}

void board_lcd_backlight(bool on)
{
	if (on)
		LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_4);
	else
		LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_4);
}

bool board_lcd_write(bool data, const uint8_t *bytes, size_t length)
{
	uint32_t start = ticks;
	size_t index = 0;
	bool ok = false;

	if (!length || length > 65535U)
		return false;
	if (data)
		LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_5);
	else
		LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_5);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_4);
	LL_SPI_SetTransferSize(SPI3, (uint32_t)length);
	LL_SPI_Enable(SPI3);
	LL_SPI_StartMasterTransfer(SPI3);
	while (index < length) {
		if (elapsed(ticks, start, 20))
			goto out;
		if (LL_SPI_IsActiveFlag_TXP(SPI3))
			LL_SPI_TransmitData8(SPI3, bytes[index++]);
	}
	while (!LL_SPI_IsActiveFlag_EOT(SPI3)) {
		if (elapsed(ticks, start, 20))
			goto out;
	}
	ok = true;
out:
	LL_SPI_Disable(SPI3);
	LL_SPI_ClearFlag_EOT(SPI3);
	LL_SPI_ClearFlag_TXTF(SPI3);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_4);
	return ok;
}

_Noreturn void board_panic(void)
{
	__disable_irq();
	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_3);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_OUTPUT);
	for (;;)
		__WFI();
}

void HardFault_Handler(void)
{
	board_panic();
}

_Noreturn void Default_IRQHandler_Hook(void)
{
	board_panic();
}
