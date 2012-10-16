/*
 * esdctl.c - i.MX sdram controller functions
 *
 * Copyright (c) 2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * based on Linux devicetree support
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <common.h>
#include <io.h>
#include <sizes.h>
#include <asm/barebox-arm.h>
#include <mach/esdctl.h>
#include <mach/imx1-regs.h>
#include <mach/imx21-regs.h>
#include <mach/imx25-regs.h>
#include <mach/imx27-regs.h>
#include <mach/imx31-regs.h>
#include <mach/imx35-regs.h>
#include <mach/imx51-regs.h>
#include <mach/imx53-regs.h>

static inline unsigned long imx_v1_sdram_size(void __iomem *esdctlbase, int num)
{
	void __iomem *esdctl = esdctlbase + (num ? 4 : 0);
	u32 ctlval = readl(esdctl);
	unsigned long size;
	int rows, cols, width = 2, banks = 4;

	if (!(ctlval & ESDCTL0_SDE))
		/* SDRAM controller disabled, so no RAM here */
		return 0;

	rows = ((ctlval >> 24) & 0x3) + 11;
	cols = ((ctlval >> 20) & 0x3) + 8;

	if (ctlval & (1 << 17))
		width = 4;

	size = (1 << cols) * (1 << rows) * banks * width;

	if (size > SZ_64M)
		size = SZ_64M;

	return size;
}

static inline unsigned long imx_v2_sdram_size(void __iomem *esdctlbase, int num)
{
	void __iomem *esdctl = esdctlbase + (num ? IMX_ESDCTL1 : IMX_ESDCTL0);
	u32 ctlval = readl(esdctl);
	unsigned long size;
	int rows, cols, width = 2, banks = 4;

	if (!(ctlval & ESDCTL0_SDE))
		/* SDRAM controller disabled, so no RAM here */
		return 0;

	rows = ((ctlval >> 24) & 0x7) + 11;
	cols = ((ctlval >> 20) & 0x3) + 8;

	if ((ctlval & ESDCTL0_DSIZ_MASK) == ESDCTL0_DSIZ_31_0)
		width = 4;

	size = (1 << cols) * (1 << rows) * banks * width;

	if (size > SZ_256M)
		size = SZ_256M;

	return size;
}

static inline unsigned long imx_v3_sdram_size(void __iomem *esdctlbase, int num)
{
	unsigned long size;

	size = imx_v2_sdram_size(esdctlbase, num);

	if (readl(esdctlbase + IMX_ESDMISC) & (1 << 6))
		size *= 2;

	if (size > SZ_256M)
		size = SZ_256M;

	return size;
}

#define IMX_ESDCTL_V4_ESDCTL	0x0
#define ESDCTL_V4_ESDCTL_DSIZ		(1 << 16)

#define IMX_ESDCTL_V4_ESDMISC	0x18
#define ESDCTL_V4_ESDMISC_DDR_4_BANK	(1 << 5)
#define ESDCTL_V4_ESDMISC_ONECS		(1 << 20)

#define ESDCTL_V4_ESDCTL_SDE_0	1 /* FIXME */
#define ESDCTL_V4_ESDMISC_BI	1 /* FIXME */

static inline unsigned long imx_v4_sdram_size(void __iomem *esdctlbase)
{
	u32 ctlval = readl(esdctlbase + IMX_ESDCTL_V4_ESDCTL);
	u32 esdmisc = readl(esdctlbase + IMX_ESDCTL_V4_ESDMISC);
	unsigned long size;
	int rows, cols, width = 2, banks = 8;

	if (!(ctlval & ESDCTL_V4_ESDCTL_SDE_0))
		return 0;

	rows = ((ctlval >> 24) & 0x7) + 11;
	switch ((ctlval >> 20) & 0x7) {
	case 0:
		cols = 9;
		break;
	case 1:
		cols = 10;
		break;
	case 2:
		cols = 11;
		break;
	case 3:
		cols = 8;
		break;
	case 4:
		cols = 12;
		break;
	default:
		cols = 0;
		break;
	}

	if (ctlval & ESDCTL_V4_ESDCTL_DSIZ)
		width = 4;

	if (esdmisc & ESDCTL_V4_ESDMISC_DDR_4_BANK)
		banks = 4;

	size = (1 << cols) * (1 << rows) * banks * width;

	/* second cs not enabled, return size */
	if (!(ctlval & ESDCTL_V4_ESDCTL_SDE_0))
		return size;

	/* one 2GiB cs, return size */
	if (esdmisc & ESDCTL_V4_ESDMISC_ONECS)
		return size;

	/* interleaved, double size */
	if (esdmisc & ESDCTL_V4_ESDMISC_BI)
		return size * 2;

	/* two areas with hole, return first area size */
	if (size < SZ_1G)
		return size;

	/* both cs, 2 * 1GiB */
	return size * 2;
}

/*
 * The i.MX SoCs usually have two SDRAM chipselects. The following
 * SoC specific functions return:
 *
 * - cs0 disabled, cs1 disabled: 0
 * - cs0 enabled, cs1 disabled: SDRAM size for cs0
 * - cs0 disabled, c1 enabled: 0 (currently assumed that no hardware does this)
 * - cs0 enabled, cs1 enabled: The largest continuous region, that is, cs0 + cs1
 *                             if cs0 is taking the whole address space.
 */
void __naked imx1_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = 0x08000000;

	size = imx_v1_sdram_size((void *)MX1_SDRAMC_BASE_ADDR, 0);
	if (size == SZ_64M)
		size += imx_v1_sdram_size((void *)MX1_SDRAMC_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx25_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX25_CSD0_BASE_ADDR;

	size = imx_v2_sdram_size((void *)MX25_ESDCTL_BASE_ADDR, 0);
	if (size == SZ_256M)
		size += imx_v2_sdram_size((void *)MX25_ESDCTL_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx27_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX27_CSD0_BASE_ADDR;

	size = imx_v2_sdram_size((void *)MX27_ESDCTL_BASE_ADDR, 0);
	if (size == SZ_256M)
		size += imx_v2_sdram_size((void *)MX27_ESDCTL_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx31_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX31_CSD0_BASE_ADDR;

	size = imx_v2_sdram_size((void *)MX31_ESDCTL_BASE_ADDR, 0);
	if (size == SZ_256M)
		size += imx_v2_sdram_size((void *)MX31_ESDCTL_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx35_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX35_CSD0_BASE_ADDR;

	size = imx_v2_sdram_size((void *)MX35_ESDCTL_BASE_ADDR, 0);
	if (size == SZ_256M)
		size += imx_v2_sdram_size((void *)MX35_ESDCTL_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx51_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX51_CSD0_BASE_ADDR;

	size = imx_v3_sdram_size((void *)MX51_ESDCTL_BASE_ADDR, 0);
	if (size == SZ_256M)
		size += imx_v3_sdram_size((void *)MX51_ESDCTL_BASE_ADDR, 1);

	barebox_arm_entry(base, size, boarddata);
}

void __naked imx53_barebox_entry(uint32_t boarddata)
{
	unsigned long base;
	unsigned long size;

	base = MX53_CSD0_BASE_ADDR;

	size = imx_v4_sdram_size((void *)MX53_ESDCTL_BASE_ADDR);

	barebox_arm_entry(base, size, boarddata);
}
