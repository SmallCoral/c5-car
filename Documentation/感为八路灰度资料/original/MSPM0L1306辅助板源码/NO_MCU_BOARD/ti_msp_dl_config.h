/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0L130X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0L130X

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMG0)
#define TIMER_0_INST_IRQHandler                                 TIMG0_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMG0_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                         (31999U)




/* Defines for ADC_VOLTAGE */
#define ADC_VOLTAGE_INST                                                    ADC0
#define ADC_VOLTAGE_INST_IRQHandler                              ADC0_IRQHandler
#define ADC_VOLTAGE_INST_INT_IRQN                                (ADC0_INT_IRQn)
#define ADC_VOLTAGE_ADCMEM_ADC_CH0                            DL_ADC12_MEM_IDX_0
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF           DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_VOLTAGE_ADCMEM_ADC_CH0_REF_VOLTAGE                                      -1 // VDDA cannot be determined
#define GPIO_ADC_VOLTAGE_C0_PORT                                           GPIOA
#define GPIO_ADC_VOLTAGE_C0_PIN                                   DL_GPIO_PIN_27



/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (0)
#define ADC_VOLTAGE_INST_DMA_TRIGGER                  (DMA_ADC0_EVT_GEN_BD_TRIG)



/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOA)

/* Defines for PIN_0: GPIOA.18 with pinCMx 19 on package pin 22 */
#define LED_PIN_0_PIN                                           (DL_GPIO_PIN_18)
#define LED_PIN_0_IOMUX                                          (IOMUX_PINCM19)
/* Defines for PIN_1: GPIOA.17 with pinCMx 18 on package pin 21 */
#define LED_PIN_1_PIN                                           (DL_GPIO_PIN_17)
#define LED_PIN_1_IOMUX                                          (IOMUX_PINCM18)
/* Defines for PIN_2: GPIOA.16 with pinCMx 17 on package pin 20 */
#define LED_PIN_2_PIN                                           (DL_GPIO_PIN_16)
#define LED_PIN_2_IOMUX                                          (IOMUX_PINCM17)
/* Defines for PIN_3: GPIOA.10 with pinCMx 11 on package pin 14 */
#define LED_PIN_3_PIN                                           (DL_GPIO_PIN_10)
#define LED_PIN_3_IOMUX                                          (IOMUX_PINCM11)
/* Defines for PIN_4: GPIOA.9 with pinCMx 10 on package pin 13 */
#define LED_PIN_4_PIN                                            (DL_GPIO_PIN_9)
#define LED_PIN_4_IOMUX                                          (IOMUX_PINCM10)
/* Defines for PIN_5: GPIOA.8 with pinCMx 9 on package pin 12 */
#define LED_PIN_5_PIN                                            (DL_GPIO_PIN_8)
#define LED_PIN_5_IOMUX                                           (IOMUX_PINCM9)
/* Defines for PIN_6: GPIOA.7 with pinCMx 8 on package pin 11 */
#define LED_PIN_6_PIN                                            (DL_GPIO_PIN_7)
#define LED_PIN_6_IOMUX                                           (IOMUX_PINCM8)
/* Defines for PIN_7: GPIOA.6 with pinCMx 7 on package pin 10 */
#define LED_PIN_7_PIN                                            (DL_GPIO_PIN_6)
#define LED_PIN_7_IOMUX                                           (IOMUX_PINCM7)
/* Defines for ERR: GPIOA.11 with pinCMx 12 on package pin 15 */
#define LED_ERR_PIN                                             (DL_GPIO_PIN_11)
#define LED_ERR_IOMUX                                            (IOMUX_PINCM12)
/* Defines for KEY: GPIOA.12 with pinCMx 13 on package pin 16 */
#define LED_KEY_PIN                                             (DL_GPIO_PIN_12)
#define LED_KEY_IOMUX                                            (IOMUX_PINCM13)
/* Port definition for Pin Group GRAY_IN */
#define GRAY_IN_PORT                                                     (GPIOA)

/* Defines for IN_KEY: GPIOA.22 with pinCMx 23 on package pin 26 */
// groups represented: ["Serial","GRAY_IN"]
// pins affected: ["CLK","IN_KEY"]
#define GPIO_MULTIPLE_GPIOA_INT_IRQN                            (GPIOA_INT_IRQn)
#define GPIO_MULTIPLE_GPIOA_INT_IIDX            (DL_INTERRUPT_GROUP1_IIDX_GPIOA)
#define GRAY_IN_IN_KEY_IIDX                                 (DL_GPIO_IIDX_DIO22)
#define GRAY_IN_IN_KEY_PIN                                      (DL_GPIO_PIN_22)
#define GRAY_IN_IN_KEY_IOMUX                                     (IOMUX_PINCM23)
/* Defines for IN_ERR: GPIOA.26 with pinCMx 27 on package pin 30 */
#define GRAY_IN_IN_ERR_PIN                                      (DL_GPIO_PIN_26)
#define GRAY_IN_IN_ERR_IOMUX                                     (IOMUX_PINCM27)
/* Port definition for Pin Group Gray_Address */
#define Gray_Address_PORT                                                (GPIOA)

/* Defines for P0: GPIOA.15 with pinCMx 16 on package pin 19 */
#define Gray_Address_P0_PIN                                     (DL_GPIO_PIN_15)
#define Gray_Address_P0_IOMUX                                    (IOMUX_PINCM16)
/* Defines for P1: GPIOA.14 with pinCMx 15 on package pin 18 */
#define Gray_Address_P1_PIN                                     (DL_GPIO_PIN_14)
#define Gray_Address_P1_IOMUX                                    (IOMUX_PINCM15)
/* Defines for P2: GPIOA.13 with pinCMx 14 on package pin 17 */
#define Gray_Address_P2_PIN                                     (DL_GPIO_PIN_13)
#define Gray_Address_P2_IOMUX                                    (IOMUX_PINCM14)
/* Port definition for Pin Group Serial */
#define Serial_PORT                                                      (GPIOA)

/* Defines for CLK: GPIOA.3 with pinCMx 4 on package pin 7 */
#define Serial_CLK_IIDX                                      (DL_GPIO_IIDX_DIO3)
#define Serial_CLK_PIN                                           (DL_GPIO_PIN_3)
#define Serial_CLK_IOMUX                                          (IOMUX_PINCM4)
/* Defines for DAT: GPIOA.4 with pinCMx 5 on package pin 8 */
#define Serial_DAT_PIN                                           (DL_GPIO_PIN_4)
#define Serial_DAT_IOMUX                                          (IOMUX_PINCM5)



/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_ADC_VOLTAGE_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_SYSTICK_init(void);


#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
