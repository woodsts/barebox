/***********************************************************************
 * $Id:: lpc32xx_chip.h 941 2008-07-24 20:39:53Z wellsk                $
 *
 * Project: LPC32XX family chip definitions
 *
 * Description:
 *     This file contains chip specific information such as the
 *     physical addresses defines for the LPC32xx registers, clock
 *     frequencies, and other chip information.
 *
 * Notes:
 *
 ***********************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 **********************************************************************/
#ifndef LPC32XX_CHIP_H
#define LPC32XX_CHIP_H

/***********************************************************************
 * AHB BASES
 **********************************************************************/
#define SSP0_BASE       0x20084000 /* SSP0 registers base */
#define SSP1_BASE       0x2008C000 /* SSP registers base */
#define I2S0_BASE       0x20094000
#define I2S1_BASE       0x2009C000
#define MLC_BASE		0x200A8000	/*MLC NAND Flash registers base*/
#define LCD_BASE		0x31040000	/*LCD controller register base*/
#define ETHERNET_BASE	0x31060000	/*Ethernet ctrl register base*/
#define EMC_BASE        0x31080000  /*EMC registers base*/
#define SLC_BASE		0x20020000	/*SLC NAND Flash registers base*/
#define SPI1_BASE		0x20088000	/*SPI1 registers base*/
#define SPI2_BASE		0x20090000	/*SPI2 registers base*/
#define SD_BASE			0x20098000	/*SD card interface registers base*/
#define DMA_BASE		0x31000000	/*DMA controller registers base*/
#define USB_BASE		0x31020000	/*USB interface registers base*/
#define USBH_BASE		0x31020000	/*USB Host registers base*/
#define USB_OTG_BASE	0x31020000	/*USB OTG registers base*/
#define OTG_I2C_BASE	0x31020300	/*USB I2C registers base*/
#define ETB_CFG_BASE	0x310C0000	/*ETB configuration registers base*/
#define ETB_DATA_BASE	0x310E0000	/*ETB data base*/

/***********************************************************************
 * FAB BASES
 **********************************************************************/
#define TIMER0_BASE     0x40044000
#define TIMER1_BASE     0x4004C000
#define TIMER2_BASE     0x40058000
#define TIMER3_BASE     0x40060000
#define CLK_PM_BASE		0x40004000	/* System control registers base*/
#define MIC_BASE		0x40008000	/* Master int ctrl registers base*/
#define SIC1_BASE		0x4000C000	/* Slave1 int ctrl registers base*/
#define SIC2_BASE		0x40010000	/* Slave2 int ctrl registers base*/
#define HS_UART1_BASE	0x40014000	/* High speed UART 1 regs base*/
#define HS_UART2_BASE	0x40018000	/* High speed UART 2 regs base*/
#define HS_UART7_BASE	0x4001C000	/* High speed UART 7 regs base*/
#define RTC_BASE		0x40024000	/* RTC registers base*/
#define RTC_RAM_BASE	0x40024080	/* RTC internal SRAM base*/
#define GPIO_BASE		0x40028000	/* GPIO registers base*/
#define PWM3_BASE		0x4002C000	/* PWM3 registers base*/
#define PWM4_BASE		0x40030000	/* PWM4 registers base*/
#define MSTIM_BASE		0x40034000	/* Millisecond timer regs base*/
#define HSTIM_BASE		0x40038000	/* High speed timer regs base*/
#define WDTIM_BASE		0x4003C000	/* Watchdog timer registers base*/
#define DEBUG_CTRL_BASE	0x40040000	/* Debug control registers base*/
#define ADC_BASE		0x40048000	/* ADC registers base*/
#define KSCAN_BASE		0x40050000	/* Keyboard Scan registers base*/
#define UART_CTRL_BASE	0x40054000	/* general UART ctrl regs base*/
#define PWM1_BASE		0x4005C000	/* PWM1 registers base*/
#define PWM2_BASE		0x4005C004	/* PWM1 registers base*/

/***********************************************************************
 * APB BASES
 **********************************************************************/

#define UART3_BASE		0x40080000	/* UART 3 registers base*/
#define UART4_BASE		0x40088000	/* UART 4 registers base*/
#define UART5_BASE		0x40090000	/* UART 5 registers base*/
#define UART6_BASE		0x40098000	/* UART 6 registers base*/
#define I2C1_BASE		0x400A0000	/* I2C1 registers base*/
#define I2C2_BASE		0x400A8000	/* I2C2 registers base*/

/***********************************************************************
 * Internal memory Bases
 **********************************************************************/
#define IRAM_BASE       0x08000000  /* Internal RAM address */
#define IROM_BASE       0x0C000000  /* Internal ROM address */

/***********************************************************************
 * External Static Memory Bank Address Space Bases
 **********************************************************************/
#define EMC_CS0_BASE	0xE0000000
#define EMC_CS1_BASE	0xE1000000
#define EMC_CS2_BASE	0xE2000000
#define EMC_CS3_BASE	0xE3000000

/***********************************************************************
 * External SDRAM Memory Bank Address Space Bases
 **********************************************************************/
#define EMC_DYCS0_BASE	0x80000000  /* SDRAM DYCS0 base address */
#define EMC_DYCS1_BASE	0xA0000000  /* SDRAM DYCS1 base address */

/***********************************************************************
 * Clock and crystal information
 **********************************************************************/
#ifndef MAIN_OSC_FREQ
#define MAIN_OSC_FREQ 13000000
#endif
#define CLOCK_OSC_FREQ 32768

#endif /* LPC32XX_CHIP_H */
