#ifndef TEST_STM32C5XX_H
#define TEST_STM32C5XX_H

#include <stdint.h>
#include <stdlib.h>

static struct { uint32_t RSR; } test_rcc;
#define RCC (&test_rcc)
#define RCC_RSR_RMVF 1U
#define __DSB() ((void)0)
#define NVIC_SystemReset() abort()

#endif
