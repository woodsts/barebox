/*
 * Copyright (C) 2013 Pengutronix <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <debug_ll.h>
#include <common.h>
#include <io.h>
#include <sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/cache.h>
#include <mach/imx6-regs.h>

static inline void early_uart_init(void)
{
	writel(0x00000000, MX6_UART1_BASE_ADDR + 0x80);
	writel(0x00004027, MX6_UART1_BASE_ADDR + 0x84);
	writel(0x00000704, MX6_UART1_BASE_ADDR + 0x88);
	writel(0x00000a81, MX6_UART1_BASE_ADDR + 0x90);
	writel(0x0000002b, MX6_UART1_BASE_ADDR + 0x9c);
	writel(0x00013880, MX6_UART1_BASE_ADDR + 0xb0);
	writel(0x0000047f, MX6_UART1_BASE_ADDR + 0xa4);
	writel(0x0000c34f, MX6_UART1_BASE_ADDR + 0xa8);
	writel(0x00000001, MX6_UART1_BASE_ADDR + 0x80);
}

/*
 * UART1 is used for the console and connected to:
 * - TxD (i.MX6 CPU out to host): pin M1, CSI0_DAT10/GPIO5[28]
 * - RxD (host to i.MX6 in): pin M3, CSI0_DAT11/GPIO5[29]
 * no status lines are connected
 */
static noinline void early_uart_init_at1008(void)
{
	/* (re-)enable all clocks in this early boot time */
	writel(0xffffffff, 0x020c4068);
	writel(0xffffffff, 0x020c406c);
	writel(0xffffffff, 0x020c4070);
	writel(0xffffffff, 0x020c4074);
	writel(0xffffffff, 0x020c4078);
	writel(0xffffffff, 0x020c407c);
	writel(0xffffffff, 0x020c4080);
	writel(0xffffffff, 0x020c4084);

	/*
	 * IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA10:
	 *   00011
	 *     ^^^_ UART1_TX_DATA mode
	 *   ^_____ no force
	 */
	writel(0x3, MX6_IOMUXC_BASE_ADDR + 0x280);
	/*
	 * IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA11:
	 *   00011
	 *     ^^^_ UART1_RX_DATA mode
	 *   ^_____ no force
	 */
	writel(0x3, MX6_IOMUXC_BASE_ADDR + 0x284);

	/*
	 * IOMUXC_UART1_UART_RX_DATA_SELECT_INPUT
	 *   01 FIXME
	 *   ^^_  Selecting ALT3 mode of pad CSI0_DAT11 for UART1_RX_DATA
	 */
	writel(0x1, MX6_IOMUXC_BASE_ADDR + 0x920);

	/*
	 * IOMUXC_SW_PAD_CTL_PAD_CSI0_DATA10
	 *   1 1011 0xxx 1011 0xx1
	 *                       ^_ fast slew rate (FIXME)
	 *                 ^^_^____ 40 ohm (FIXME)
	 *               ^^________ 100 MHz speed (FIXME)
	 *          ^______________ push pull output
	 *        ^________________ pull up/keeper enabled (FIXME!)
	 *       ^_________________ pull up feature selected
	 *     ^^__________________ 100k pull up selected
	 *   ^_____________________ Schmitt trigger input (FIXME)
	 */
	writel(0x0001b0b1, MX6_IOMUXC_BASE_ADDR + 0x650);
	/*
	 * IOMUXC_SW_PAD_CTL_PAD_CSI0_DATA11
	 *   1 1011 0xxx 1011 0xx1
	 *                       ^_ fast slew rate (FIXME)
	 *                 ^^_^____ 40 ohm (FIXME)
	 *               ^^________ 100 MHz speed (FIXME)
	 *          ^______________ push pull output (FIXME)
	 *        ^________________ pull up/keeper enabled
	 *       ^_________________ pull up feature selected
	 *     ^^__________________ 100k pull up selected
	 *   ^_____________________ Schmitt trigger input
	 */
	writel(0x0001b0b1, MX6_IOMUXC_BASE_ADDR + 0x654);

	early_uart_init();
}

extern char __dtb_imx6d_arkona_link4_start[];

ENTRY_FUNCTION(start_imx6d_arkona_at1008, r0, r1, r2)
{
	uint32_t fdt;

	arm_cpu_lowlevel_init();

	arm_setup_stack(0x00940000 - 8); /* use the internal SRAM for now */

	if (IS_ENABLED(CONFIG_DEBUG_LL)) {
		early_uart_init_at1008();
		putc_ll('@');
	}

	fdt = (uint32_t)__dtb_imx6d_arkona_link4_start - get_runtime_offset();

	barebox_arm_entry(0x10000000, SZ_1G, fdt);
}
