#include <string.h>
#include "stm32c5xx.h"
#include "stm32c5xx_ll_usart.h"
#include "board.h"
#include "config.h"
#include "logic.h"
#include "serial.h"

#define TX_SIZE 512U
#define RX_SIZE 512U

struct uart_port {
	USART_TypeDef *uart;
	uint8_t tx[TX_SIZE];
	uint8_t rx[RX_SIZE];
	unsigned int tx_head;
	unsigned int tx_tail;
	unsigned int rx_head;
	unsigned int rx_tail;
	struct serial_stats stats;
};

/* Every thread-context access is protected by PRIMASK; ISRs never nest. */
static struct uart_port ports[SERIAL_COUNT] = {
	[SERIAL_BT] = { .uart = USART1 },
	[SERIAL_STEP] = { .uart = USART6 },
};

static bool uart_init(USART_TypeDef *uart, uint32_t baud)
{
	uint32_t start = board_millis();

	LL_USART_Disable(uart);
	LL_USART_ConfigAsyncMode(uart);
	LL_USART_SetPrescaler(uart, LL_USART_PRESCALER_DIV1);
	LL_USART_SetDataWidth(uart, LL_USART_DATAWIDTH_8_BIT);
	LL_USART_SetParity(uart, LL_USART_PARITY_NONE);
	LL_USART_SetStopBitsLength(uart, LL_USART_STOP_BIT_1);
	LL_USART_SetTransferDirection(uart, LL_USART_DIRECTION_TX_RX);
	LL_USART_SetBaudRate(uart, BOARD_CLOCK_HZ, LL_USART_PRESCALER_DIV1,
			     LL_USART_OVERSAMPLING_16, baud);
	LL_USART_Enable(uart);
	while (!LL_USART_IsActiveFlag_TEACK(uart) ||
	       !LL_USART_IsActiveFlag_REACK(uart)) {
		if (elapsed(board_millis(), start, 20))
			return false;
	}
	LL_USART_EnableIT_RXNE_RXFNE(uart);
	LL_USART_EnableIT_ERROR(uart);
	return true;
}

bool serial_init(void)
{
	if (!uart_init(USART1, BT_BAUD) || !uart_init(USART6, STEP_BAUD))
		return false;
	NVIC_SetPriority(USART1_IRQn, 2);
	NVIC_SetPriority(USART6_IRQn, 2);
	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_EnableIRQ(USART6_IRQn);
	return true;
}

bool serial_write(enum serial_port port, const uint8_t *data, size_t size)
{
	struct uart_port *p = &ports[port];
	uint32_t irq = __get_PRIMASK();
	size_t index;

	__disable_irq();
	if (size > (p->tx_tail - p->tx_head - 1U) % TX_SIZE) {
		p->stats.tx_drops++;
		__set_PRIMASK(irq);
		return false;
	}
	for (index = 0; index < size; index++) {
		p->tx[p->tx_head] = data[index];
		p->tx_head = (p->tx_head + 1) % TX_SIZE;
	}
	LL_USART_EnableIT_TXE_TXFNF(p->uart);
	__set_PRIMASK(irq);
	return true;
}

bool serial_read(enum serial_port port, uint8_t *byte)
{
	struct uart_port *p = &ports[port];
	uint32_t irq = __get_PRIMASK();
	bool available;

	__disable_irq();
	available = p->rx_head != p->rx_tail;
	if (available) {
		*byte = p->rx[p->rx_tail];
		p->rx_tail = (p->rx_tail + 1) % RX_SIZE;
	}
	__set_PRIMASK(irq);
	return available;
}

bool serial_tx_idle(enum serial_port port)
{
	struct uart_port *p = &ports[port];
	uint32_t irq = __get_PRIMASK();
	bool idle;

	__disable_irq();
	idle = p->tx_head == p->tx_tail && LL_USART_IsActiveFlag_TC(p->uart);
	__set_PRIMASK(irq);
	return idle;
}

struct serial_stats serial_stats(enum serial_port port)
{
	uint32_t irq = __get_PRIMASK();
	struct serial_stats stats;

	__disable_irq();
	stats = ports[port].stats;
	__set_PRIMASK(irq);
	return stats;
}

static void uart_irq(struct uart_port *p)
{
	USART_TypeDef *uart = p->uart;
	uint32_t errors = uart->ISR & (USART_ISR_ORE | USART_ISR_FE |
				     USART_ISR_NE | USART_ISR_PE);

	if (errors) {
		/* Error bit positions match the corresponding write-one-clear bits. */
		uart->ICR = errors;
		p->stats.errors++;
	}
	if (LL_USART_IsActiveFlag_RXNE_RXFNE(uart)) {
		uint8_t byte = LL_USART_ReceiveData8(uart);
		unsigned int next = (p->rx_head + 1) % RX_SIZE;

		p->stats.rx_bytes++;
		if (!errors && next != p->rx_tail) {
			p->rx[p->rx_head] = byte;
			p->rx_head = next;
		} else {
			p->stats.rx_drops++;
		}
	}
	if (!LL_USART_IsEnabledIT_TXE_TXFNF(uart) ||
	    !LL_USART_IsActiveFlag_TXE_TXFNF(uart))
		return;
	if (p->tx_head == p->tx_tail) {
		LL_USART_DisableIT_TXE_TXFNF(uart);
		return;
	}
	LL_USART_TransmitData8(uart, p->tx[p->tx_tail]);
	p->tx_tail = (p->tx_tail + 1) % TX_SIZE;
	p->stats.tx_bytes++;
}

void USART1_IRQHandler(void)
{
	uart_irq(&ports[SERIAL_BT]);
}

void USART6_IRQHandler(void)
{
	uart_irq(&ports[SERIAL_STEP]);
}
