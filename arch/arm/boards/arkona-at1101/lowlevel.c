/*
 * Copyright (C) 2015 Pengutronix, Uwe Kleine-König <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm/io.h>
#include <mach/lowlevel.h>
#include <mach/common.h>

extern char __dtb_armada_xp_at1101_bb_start[];

ENTRY_FUNCTION(start_arkona_at1101, r0, r1, r2)
{
	void *fdt;
	uint32_t reg;
	void __iomem *base = mvebu_get_initial_int_reg_base();

	arm_cpu_lowlevel_init();

	/* enable global WD RSTOUTn */
#define WD_RSTOUT_ADDRESS (base + 0x20704)
	reg = readl(WD_RSTOUT_ADDRESS);
	reg |= 1 << 8;
	writel(reg, WD_RSTOUT_ADDRESS);
#define TIMER_BASE_ADDRESS (base + 0x20300)
	/* init the watchdog counter (reset value ~85 seconds at 25MHz) */
	writel(0x0, TIMER_BASE_ADDRESS + 0x30);
	writel(0x7fffffff, TIMER_BASE_ADDRESS + 0x34);
	reg = readl(TIMER_BASE_ADDRESS);
	reg &= ~(1 << 9); /* GlobalWDTimerAuto */
	reg |= 1 << 8; /* GlobalWDTimerEn */
	reg |= 1 << 10; /* GlobalWDTimer25MhzEn */
	writel(reg, TIMER_BASE_ADDRESS);

	/* enable L2 parity and ECC */
#define L2_AUX_CONTRL_ADDRESS (base + 0x8104)
	reg = readl(L2_AUX_CONTRL_ADDRESS);
	reg |= 3 << 20;
	writel(reg, L2_AUX_CONTRL_ADDRESS);

	/* We have 4GB RAM, which would confuse the memory size detection when
	 * booting as second stage. As we would need to handle all the enabled
	 * MBUS windows in that case (which is too complext for the lowlevel
	 * code), we just hardcode the available memory size. The upper memory
	 * will be initialized in board.c.
	 */
#define MVEBU_BRIDGE_REG_BASE     0x20000
#define DEVICE_INTERNAL_BASE_ADDR (MVEBU_BRIDGE_REG_BASE + 0x80)
	writel(MVEBU_REMAP_INT_REG_BASE, base + DEVICE_INTERNAL_BASE_ADDR);

	fdt = __dtb_armada_xp_at1101_bb_start +	get_runtime_offset();

	barebox_arm_entry(0, 0x40000000, fdt);
}
