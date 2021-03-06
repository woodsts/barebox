// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

#include <common.h>
#include <init.h>
#include <mach/imx35-regs.h>
#include <mach/imx-pll.h>
#include <mach/esdctl.h>
#include <asm/cache-l2x0.h>
#include <io.h>
#include <mach/imx-nand.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm/sections.h>
#include <asm-generic/memory_layout.h>
#include <asm/system.h>

#define IMX35_CHIP_REVISION_2_1		0x11

#define CCM_PDR0_399	0x00011000
#define CCM_PDR0_532	0x00001000

void __bare_init __naked barebox_arm_reset_vector(uint32_t r0, uint32_t r1, uint32_t r2)
{
	uint32_t r, s;
	unsigned long ccm_base = MX35_CCM_BASE_ADDR;
	unsigned long iomuxc_base = MX35_IOMUXC_BASE_ADDR;
	unsigned long esdctl_base = MX35_ESDCTL_BASE_ADDR;

	arm_cpu_lowlevel_init();

	arm_setup_stack(MX35_IRAM_BASE_ADDR + MX35_IRAM_SIZE);

	r = get_cr();
	r |= CR_Z; /* Flow prediction (Z) */
	r |= CR_U; /* unaligned accesses  */
	r |= CR_FI; /* Low Int Latency     */

	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 1":"=r"(s));
	s |= 0x7;
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 1" : : "r"(s));

	set_cr(r);

	r = 0;
	__asm__ __volatile__("mcr p15, 0, %0, c15, c2, 4" : : "r"(r));

	/*
	 * Branch predicition is now enabled.  Flush the BTAC to ensure a valid
	 * starting point.  Don't flush BTAC while it is disabled to avoid
	 * ARM1136 erratum 408023.
	 */
	__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 6" : : "r"(r));

	/* invalidate I cache and D cache */
	__asm__ __volatile__("mcr p15, 0, %0, c7, c7, 0" : : "r"(r));

	/* invalidate TLBs */
	__asm__ __volatile__("mcr p15, 0, %0, c8, c7, 0" : : "r"(r));

	/* Drain the write buffer */
	__asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" : : "r"(r));

	/* Also setup the Peripheral Port Remap register inside the core */
	r = 0x40000015; /* start from AIPS 2GB region */
	__asm__ __volatile__("mcr p15, 0, %0, c15, c2, 4" : : "r"(r));

	/*
	 * End of ARM1136 init
	 */

	writel(0x003F4208, ccm_base + MX35_CCM_CCMR);

	/* Set MPLL , arm clock and ahb clock*/
	writel(MPCTL_PARAM_532, ccm_base + MX35_CCM_MPCTL);

	writel(PPCTL_PARAM_300, ccm_base + MX35_CCM_PPCTL);

	/* Check silicon revision and use 532MHz if >=2.1 */
	r = readl(MX35_IIM_BASE_ADDR + 0x24);
	if (r >= IMX35_CHIP_REVISION_2_1)
		writel(CCM_PDR0_532, ccm_base + MX35_CCM_PDR0);
	else
		writel(CCM_PDR0_399, ccm_base + MX35_CCM_PDR0);

	r = readl(ccm_base + MX35_CCM_CGR0);
	r |= 0x3 << MX35_CCM_CGR0_EPIT1_SHIFT;
	writel(r, ccm_base + MX35_CCM_CGR0);

	r = readl(ccm_base + MX35_CCM_CGR1);
	r |= 0x3 << MX35_CCM_CGR1_FEC_SHIFT;
	r |= 0x3 << MX35_CCM_CGR1_I2C1_SHIFT;
	writel(r, ccm_base + MX35_CCM_CGR1);

	r = readl(MX35_L2CC_BASE_ADDR + L2X0_AUX_CTRL);
	r |= 0x1000;
	writel(r, MX35_L2CC_BASE_ADDR + L2X0_AUX_CTRL);

	/* Skip SDRAM initialization if we run from RAM */
	r = get_pc();
	if (r > 0x80000000 && r < 0x90000000)
		goto out;

	/* Set DDR Type to SDRAM, drive strength workaround	*
	 * 0x00000000	MDDR					*
	 * 0x00000800	3,3V SDRAM				*/

	r = 0x00000800;
	writel(r, iomuxc_base + 0x794);
	writel(r, iomuxc_base + 0x798);
	writel(r, iomuxc_base + 0x79c);
	writel(r, iomuxc_base + 0x7a0);
	writel(r, iomuxc_base + 0x7a4);

	/* MDDR init, enable mDDR*/
	writel(0x00000304, esdctl_base + IMX_ESDMISC); /* was 0x00000004 */

	/* set timing paramters */
	writel(0x0025541F, esdctl_base + IMX_ESDCFG0);
	/* select Precharge-All mode */
	writel(0x92220000, esdctl_base + IMX_ESDCTL0);
	/* Precharge-All */
	writel(0x12345678, MX35_CSD0_BASE_ADDR + 0x400);

	/* select Load-Mode-Register mode */
	writel(0xB8001000, esdctl_base + IMX_ESDCTL0);
	/* Load reg EMR2 */
	writeb(0xda, 0x84000000);
	/* Load reg EMR3 */
	writeb(0xda, 0x86000000);
	/* Load reg EMR1 -- enable DLL */
	writeb(0xda, 0x82000400);
	/* Load reg MR -- reset DLL */
	writeb(0xda, 0x80000333);

	/* select Precharge-All mode */
	writel(0x92220000, esdctl_base + IMX_ESDCTL0);
	/* Precharge-All */
	writel(0x12345678, MX35_CSD0_BASE_ADDR + 0x400);

	/* select Manual-Refresh mode */
	writel(0xA2220000, esdctl_base + IMX_ESDCTL0);
	/* Manual-Refresh 2 times */
	writel(0x87654321, MX35_CSD0_BASE_ADDR);
	writel(0x87654321, MX35_CSD0_BASE_ADDR);

	/* select Load-Mode-Register mode */
	writel(0xB2220000, esdctl_base + IMX_ESDCTL0);
	/* Load reg MR -- CL3, BL8, end DLL reset */
	writeb(0xda, 0x80000233);
	/* Load reg EMR1 -- OCD default */
	writeb(0xda, 0x82000780);
	/* Load reg EMR1 -- OCD exit */
	writeb(0xda, 0x82000400);

	/* select normal-operation mode
	 * DSIZ32-bit, BL8, COL10-bit, ROW13-bit
	 * disable PWT & PRCT
	 * disable Auto-Refresh */
	writel(0x82220080, esdctl_base + IMX_ESDCTL0);

	/* enable Auto-Refresh */
	writel(0x82228080, esdctl_base + IMX_ESDCTL0);
	/* enable Auto-Refresh */
	writel(0x00002000, esdctl_base + IMX_ESDCTL1);

	if (IS_ENABLED(CONFIG_ARCH_IMX_EXTERNAL_BOOT_NAND)) {
		/* Speed up NAND controller by adjusting the NFC divider */
		r = readl(MX35_CCM_BASE_ADDR + MX35_CCM_PDR4);
		r &= ~(0xf << 28);
		r |= 0x1 << 28;
		writel(r, MX35_CCM_BASE_ADDR + MX35_CCM_PDR4);

		imx35_barebox_boot_nand_external();
	}

out:
	imx35_barebox_entry(NULL);
}
