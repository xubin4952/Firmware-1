/************************************************************************************
 * nuttx-configs/tmrfc-v1/include/board.h
 * include/arch/board/board.h
 *
  *   Copyright (C) 2013 TMR Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name TMR nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef __CONFIG_TMRFC_V1_INCLUDE_BOARD_H
#define __CONFIG_TMRFC_V1_INCLUDE_BOARD_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
# include <stdint.h>
#endif

#include "stm32_rcc.h"
#include "stm32_sdio.h"
#include "stm32.h"

/************************************************************************************
 * Definitions
 ************************************************************************************/

/* Clocking *************************************************************************/
/* The TMRFC uses a 24MHz crystal connected to the HSE.
 *
 * This is the canonical configuration:
 *   System Clock source           : PLL (HSE)
 *   SYSCLK(Hz)                    : 168000000    Determined by PLL configuration
 *   HCLK(Hz)                      : 168000000    (STM32_RCC_CFGR_HPRE)
 *   AHB Prescaler                 : 1            (STM32_RCC_CFGR_HPRE)
 *   APB1 Prescaler                : 4            (STM32_RCC_CFGR_PPRE1)
 *   APB2 Prescaler                : 2            (STM32_RCC_CFGR_PPRE2)
 *   HSE Frequency(Hz)             : 8000000      (STM32_BOARD_XTAL)
 *   PLLM                          : 8            (STM32_PLLCFG_PLLM)
 *   PLLN                          : 336          (STM32_PLLCFG_PLLN)
 *   PLLP                          : 2            (STM32_PLLCFG_PLLP)
 *   PLLQ                          : 7            (STM32_PLLCFG_PLLQ)
 *   Main regulator output voltage : Scale1 mode  Needed for high speed SYSCLK
 *   Flash Latency(WS)             : 5
 *   Prefetch Buffer               : OFF
 *   Instruction cache             : ON
 *   Data cache                    : ON
 *   Require 48MHz for USB OTG FS, : Enabled
 *   SDIO and RNG clock
 */

/* HSI - 16 MHz RC factory-trimmed
 * LSI - 32 KHz RC
 * HSE - On-board crystal frequency is 8MHz
 * LSE - not installed
 */

#define STM32_BOARD_XTAL        8000000ul

#define STM32_HSI_FREQUENCY     16000000ul
#define STM32_LSI_FREQUENCY     32000
#define STM32_HSE_FREQUENCY     STM32_BOARD_XTAL
#define STM32_LSE_FREQUENCY     32768

/* Main PLL Configuration.
 *
 * PLL source is HSE
 * PLL_VCO = (STM32_HSE_FREQUENCY / PLLM) * PLLN
 *         = (8,000,000 / 8) * 336
 *         = 336,000,000
 * SYSCLK  = PLL_VCO / PLLP
 *         = 336,000,000 / 2 = 168,000,000
 * USB OTG FS, SDIO and RNG Clock
 *         =  PLL_VCO / PLLQ
 *         = 48,000,000
 */

#define STM32_PLLCFG_PLLM       RCC_PLLCFG_PLLM(8)
#define STM32_PLLCFG_PLLN       RCC_PLLCFG_PLLN(336)
#define STM32_PLLCFG_PLLP       RCC_PLLCFG_PLLP(2)
#define STM32_PLLCFG_PLLQ       RCC_PLLCFG_PLLQ(7)

#define STM32_SYSCLK_FREQUENCY  168000000ul

/* AHB clock (HCLK) is SYSCLK (168MHz) */

#define STM32_RCC_CFGR_HPRE     RCC_CFGR_HPRE_SYSCLK  /* HCLK  = SYSCLK / 1 */
#define STM32_HCLK_FREQUENCY    STM32_SYSCLK_FREQUENCY
#define STM32_BOARD_HCLK        STM32_HCLK_FREQUENCY  /* same as above, to satisfy compiler */

/* APB1 clock (PCLK1) is HCLK/4 (42MHz) */

#define STM32_RCC_CFGR_PPRE1    RCC_CFGR_PPRE1_HCLKd4     /* PCLK1 = HCLK / 4 */
#define STM32_PCLK1_FREQUENCY   (STM32_HCLK_FREQUENCY/4)

/* Timers driven from APB1 will be twice PCLK1 */

#define STM32_APB1_TIM2_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM3_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM4_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM5_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM6_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM7_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM12_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM13_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM14_CLKIN  (2*STM32_PCLK1_FREQUENCY)

/* APB2 clock (PCLK2) is HCLK/2 (84MHz) */

#define STM32_RCC_CFGR_PPRE2    RCC_CFGR_PPRE2_HCLKd2     /* PCLK2 = HCLK / 2 */
#define STM32_PCLK2_FREQUENCY   (STM32_HCLK_FREQUENCY/2)

/* Timers driven from APB2 will be twice PCLK2 */

#define STM32_APB2_TIM1_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM8_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM9_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM10_CLKIN  (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM11_CLKIN  (2*STM32_PCLK2_FREQUENCY)

/* Timer Frequencies, if APBx is set to 1, frequency is same to APBx
 * otherwise frequency is 2xAPBx.
 * Note: TIM1,8 are on APB2, others on APB1
 */

#define STM32_TIM18_FREQUENCY   (2*STM32_PCLK2_FREQUENCY)
#define STM32_TIM27_FREQUENCY   (2*STM32_PCLK1_FREQUENCY)

/* High-resolution timer
 */
#define HRT_TIMER		1	/* use timer1 for the HRT */
#define HRT_TIMER_CHANNEL	1	/* use capture/compare channel */

/* LED definitions ******************************************************************/
/* If CONFIG_ARCH_LEDS is not defined, then the user can control the LEDs in any
 * way.  The following definitions are used to access individual LEDs.
 */

/* LED index values for use with stm32_setled() */

#define BOARD_LED1        0
#define BOARD_LED2        1
#define BOARD_LED3        2
#define BOARD_LED4        3
#define BOARD_NLEDS       4

#define BOARD_LED_BLUE    BOARD_LED1
#define BOARD_LED_RED     BOARD_LED2

/* LED bits for use with stm32_setleds() */

#define BOARD_LED1_BIT    (1 << BOARD_LED1)
#define BOARD_LED2_BIT    (1 << BOARD_LED2)
#define BOARD_LED3_BIT    (1 << BOARD_LED3)
#define BOARD_LED4_BIT    (1 << BOARD_LED4)

/* If CONFIG_ARCH_LEDs is defined, then NuttX will control the 2 LEDs on board the
 * tmrfc-v1.  The following definitions describe how NuttX controls the LEDs:
 */

#define LED_STARTED       0  /* LED1 */
#define LED_HEAPALLOCATE  1  /* LED2 */
#define LED_IRQSENABLED   2  /* LED1 */
#define LED_STACKCREATED  3  /* LED1 + LED2 */
#define LED_INIRQ         4  /* LED1 */
#define LED_SIGNAL        5  /* LED2 */
#define LED_ASSERTION     6  /* LED1 + LED2 */
#define LED_PANIC         7  /* LED1 + LED2 */

/*
 *          1. The SDIO clock (SDIOCLK = 48 MHz) is coming from a specific output
 *             of PLL (PLL48CLK). Before to start working with SDIO peripheral
 *             make sure that the PLL is well configured.
 *            
 *              The SDIO peripheral uses two clock signals:
 *              - SDIO adapter clock (SDIOCLK = 48 MHz)
 *              - APB2 bus clock (PCLK2)
 *              PCLK2 and SDIO_CK clock frequencies must respect the following condition:
 *                   Frequenc(PCLK2) >= (3 / 8 x Frequency(SDIO_CK))
 *
 *          2. Enable peripheral clock using RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE).
 *
 *          3.  According to the SDIO mode, enable the GPIO clocks using 
 *              RCC_AHB1PeriphClockCmd() function. 
 *              The I/O can be one of the following configurations:
 *                 - 1-bit data length: SDIO_CMD, SDIO_CK and D0.
 *                 - 4-bit data length: SDIO_CMD, SDIO_CK and D[3:0].
 *                 - 8-bit data length: SDIO_CMD, SDIO_CK and D[7:0].
 */

/* SDIO dividers.  Note that slower clocking is required when DMA is disabled 
 * in order to avoid RX overrun/TX underrun errors due to delayed responses
 * to service FIFOs in interrupt driven mode.  These values have not been
 * tuned!!!
 *
 */

#define DMAMAP_SDIO DMAMAP_SDIO_1

#define SDIO_INIT_CLKDIV        (118 << SDIO_CLKCR_CLKDIV_SHIFT) // SDIO_CK=SDIOCLK/(118+2) = 48000 KHz / (118+2) = 400KHz

/*
 * SDIO_CK frequency = SDIOCLK / [CLKDIV + 2]
 * DMA ON:  SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(1+2)=16 MHz
 * DMA OFF: SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(2+2)=12 MHz
 */

#ifdef CONFIG_SDIO_DMA
#  define SDIO_MMCXFR_CLKDIV    (1 << SDIO_CLKCR_CLKDIV_SHIFT)
#  define SDIO_SDXFR_CLKDIV    (1 << SDIO_CLKCR_CLKDIV_SHIFT) 
#else
#  define SDIO_MMCXFR_CLKDIV    (2 << SDIO_CLKCR_CLKDIV_SHIFT)
#  define SDIO_SDXFR_CLKDIV    (2 << SDIO_CLKCR_CLKDIV_SHIFT)
#endif

/* Alternate function pin selections ************************************************/

/*
 * UARTs.
 *
 * Note that UART5 has no optional pinout, so it is not listed here.
 */
#define GPIO_USART1_RX	GPIO_USART1_RX_2
#define GPIO_USART1_TX	GPIO_USART1_TX_2

#define GPIO_USART2_RX	GPIO_USART2_RX_1
#define GPIO_USART2_TX	GPIO_USART2_TX_1

#define GPIO_UART4_RX	GPIO_UART4_RX_1
#define GPIO_UART4_TX	GPIO_UART4_TX_1

#define GPIO_USART6_RX	GPIO_USART6_RX_1
#define GPIO_USART6_TX	GPIO_USART6_TX_1

/* UART DMA configuration for USART1/6 */
#define DMAMAP_USART1_RX DMAMAP_USART1_RX_2
#define DMAMAP_USART6_RX DMAMAP_USART6_RX_2
 
/*
 * PWM
 *
 * Four PWM outputs can be configured on pins otherwise shared with
 * USART2; two can take the flow control pins if they are not being used.
 *
 * Pins:
 *
 * CTS - PA0 - TIM2CH1
 * RTS - PA1 - TIM2CH2
 * TX  - PA2 - TIM2CH3
 * RX  - PA3 - TIM2CH4
 *
 */
#define GPIO_TIM2_CH1OUT	GPIO_TIM2_CH1OUT_1
#define GPIO_TIM2_CH2OUT	GPIO_TIM2_CH2OUT_1
#define GPIO_TIM2_CH3OUT	GPIO_TIM2_CH3OUT_1
#define GPIO_TIM2_CH4OUT	GPIO_TIM2_CH4OUT_1

/*
 * PPM :
 *
 * PPM input is handled by the HRT timer.
 */
#define HRT_PPM_CHANNEL	1	/* use capture/compare channel 1 */
#define GPIO_PPM_IN	(GPIO_ALT|GPIO_AF1|GPIO_SPEED_50MHz|GPIO_PULLUP|GPIO_PORTA|GPIO_PIN8)

/*
 * I2C
 *
 * The optional _GPIO configurations allow the I2C driver to manually
 * reset the bus to clear stuck slaves.  They match the pin configuration,
 * but are normally-high GPIOs.
 */
#define GPIO_I2C1_SCL		GPIO_I2C1_SCL_1
#define GPIO_I2C1_SDA		GPIO_I2C1_SDA_1
#define GPIO_I2C1_SCL_GPIO	(GPIO_OUTPUT|GPIO_OPENDRAIN|GPIO_SPEED_50MHz|GPIO_OUTPUT_SET|GPIO_PORTB|GPIO_PIN6)
#define GPIO_I2C1_SDA_GPIO	(GPIO_OUTPUT|GPIO_OPENDRAIN|GPIO_SPEED_50MHz|GPIO_OUTPUT_SET|GPIO_PORTB|GPIO_PIN7)

#define GPIO_I2C2_SCL		GPIO_I2C2_SCL_1
#define GPIO_I2C2_SDA		GPIO_I2C2_SDA_1
#define GPIO_I2C2_SCL_GPIO	(GPIO_OUTPUT|GPIO_OPENDRAIN|GPIO_SPEED_50MHz|GPIO_OUTPUT_SET|GPIO_PORTB|GPIO_PIN10)
#define GPIO_I2C2_SDA_GPIO	(GPIO_OUTPUT|GPIO_OPENDRAIN|GPIO_SPEED_50MHz|GPIO_OUTPUT_SET|GPIO_PORTB|GPIO_PIN11)

/*
 * I2C busses
 */
#define TMR_I2C_BUS_ONBOARD	2
#define TMR_I2C_BUS_EXPANSION	1

/*
 * Devices on the onboard bus.
 *
 * Note that these are unshifted addresses.
 */
#define TMR_I2C_OBDEV_HMC5883	0x1e
#define TMR_I2C_OBDEV_MS5611	0x76
#define TMR_I2C_OBDEV_PCA9536   0x41
#define TMR_I2C_OBDEV_PCA9533   0x62
#define TMR_I2C_OBDEV_EEPROM	NOTDEFINED

/*
 * SPI
 *
 * There are sensors on SPI3 is connected to the microSD slot.
 */
#define GPIO_SPI3_MISO	GPIO_SPI3_MISO_1
#define GPIO_SPI3_MOSI	GPIO_SPI3_MOSI_1
#define GPIO_SPI3_SCK	GPIO_SPI3_SCK_1
//#define GPIO_SPI3_NSS	GPIO_SPI3_NSS_1

/* SPI DMA configuration for SPI3 (microSD) */
#define DMACHAN_SPI3_RX DMAMAP_SPI3_RX_1
#define DMACHAN_SPI3_TX DMAMAP_SPI3_TX_2
/* XXX since we allocate the HP work stack from CCM RAM on normal system startup,
   SPI1 will never run in DMA mode - so we can just give it a random config here.
   What we really need to do is to make DMA configurable per channel, and always
   disable it for SPI1. */
#define DMACHAN_SPI1_RX DMAMAP_SPI1_RX_1
#define DMACHAN_SPI1_TX DMAMAP_SPI1_TX_2

/*
 * Use these in place of the spi_dev_e enumeration to
 * select a specific SPI device on SPI1
 */
#if 0
#define TMR_SPIDEV_GYRO		1
#define TMR_SPIDEV_ACCEL	2
#define TMR_SPIDEV_MPU		3
#endif

/*
 * Optional devices on IO's external port
 */
#if 0
#define TMR_SPIDEV_ACCEL_MAG 2
#endif

/*
 * Tone alarm output : "Will change to use timer pin in next TMR V2, No support tone alarm on TMR V1, 2013/08/13 "
 */
#if 0
#define TONE_ALARM_TIMER	3	/* timer 3 */
#define TONE_ALARM_CHANNEL	3	/* channel 3 */
#define GPIO_TONE_ALARM_IDLE	(GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_2MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTC|GPIO_PIN8)
#define GPIO_TONE_ALARM		(GPIO_ALT|GPIO_AF2|GPIO_SPEED_2MHz|GPIO_FLOAT|GPIO_PUSHPULL|GPIO_PORTC|GPIO_PIN8)
#endif

/************************************************************************************
 * Public Data
 ************************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

/************************************************************************************
 * Public Function Prototypes
 ************************************************************************************/
/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the intitialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

EXTERN void stm32_boardinitialize(void);

/************************************************************************************
 * Name:  stm32_ledinit, stm32_setled, and stm32_setleds
 *
 * Description:
 *   If CONFIG_ARCH_LEDS is defined, then NuttX will control the on-board LEDs.  If
 *   CONFIG_ARCH_LEDS is not defined, then the following interfacesare available to
 *   control the LEDs from user applications.
 *
 ************************************************************************************/

#ifndef CONFIG_ARCH_LEDS
EXTERN void stm32_ledinit(void);
EXTERN void stm32_setled(int led, bool ledon);
EXTERN void stm32_setleds(uint8_t ledset);
#endif

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif  /* __CONFIG_TMRFC_V1_INCLUDE_BOARD_H */
