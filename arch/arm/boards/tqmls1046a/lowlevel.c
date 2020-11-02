// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <debug_ll.h>
#include <image-metadata.h>
#include <platform_data/mmc-esdhc-imx.h>
#include <soc/fsl/fsl_ddr_sdram.h>
#include <soc/fsl/immap_lsch2.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/syscounter.h>
#include <asm/cache.h>
#include <mach/errata.h>
#include <mach/lowlevel.h>
#include <mach/xload.h>
#include <mach/layerscape.h>

static struct fsl_ddr_controller ddrc[] = {
	{
		.memctl_opts.ddrtype = SDRAM_TYPE_DDR4,
		.base = IOMEM(LSCH2_DDR_ADDR),
		.ddr_freq = LS1046A_DDR_FREQ,
		.erratum_A008511 = 1,
		.erratum_A009803 = 1,
		.erratum_A010165 = 1,
		.erratum_A009801 = 1,
		.erratum_A009942 = 1,
		.chip_selects_per_ctrl = 4,
		.fsl_ddr_config_reg = {
	.cs[0].bnds         = 0x0000007F,
	.cs[0].config       = 0x80010312,
	.cs[0].config_2     = 0x00000000,
	.cs[1].bnds         = 0x00000000,
	.cs[1].config       = 0x00000000,
	.cs[1].config_2     = 0x00000000,
	.cs[2].bnds         = 0x00000000,
	.cs[2].config       = 0x00000000,
	.cs[2].config_2     = 0x00000000,
	.cs[3].bnds         = 0x00000000,
	.cs[3].config       = 0x00000000,
	.cs[3].config_2     = 0x00000000,
	.timing_cfg_3       = 0x020F1100,
	.timing_cfg_0       = 0xF7660008,
	.timing_cfg_1       = 0xF1FC4178,
	.timing_cfg_2       = 0x00590160,
	.ddr_sdram_cfg      = 0x65000008,
	.ddr_sdram_cfg_2    = 0x00401150,
	.ddr_sdram_cfg_3    = 0x40000000,
	.ddr_sdram_mode     = 0x01030631,
	.ddr_sdram_mode_2   = 0x00100200,
	.ddr_sdram_mode_3   = 0x00000000,
	.ddr_sdram_mode_4   = 0x00000000,
	.ddr_sdram_mode_5   = 0x00000000,
	.ddr_sdram_mode_6   = 0x00000000,
	.ddr_sdram_mode_7   = 0x00000000,
	.ddr_sdram_mode_8   = 0x00000000,
	.ddr_sdram_mode_9   = 0x00000500,
	.ddr_sdram_mode_10  = 0x08800000,
	.ddr_sdram_mode_11  = 0x00000000,
	.ddr_sdram_mode_12  = 0x00000000,
	.ddr_sdram_mode_13  = 0x00000000,
	.ddr_sdram_mode_14  = 0x00000000,
	.ddr_sdram_mode_15  = 0x00000000,
	.ddr_sdram_mode_16  = 0x00000000,
	.ddr_sdram_interval = 0x0F3C079E,
	.ddr_data_init      = 0xDEADBEEF,
	.ddr_sdram_clk_cntl = 0x03000000,
	.ddr_init_addr      = 0x00000000,
	.ddr_init_ext_addr  = 0x00000000,
	.timing_cfg_4       = 0x00220002,
	.timing_cfg_5       = 0x00000000,
	.timing_cfg_6       = 0x00000000,
	.timing_cfg_7       = 0x25500000,
	.timing_cfg_8       = 0x05447A00,
	.timing_cfg_9       = 0x00000000,
	.ddr_zq_cntl        = 0x8A090705,
	.ddr_wrlvl_cntl     = 0x8605070A,
	.ddr_wrlvl_cntl_2   = 0x0A080807,
	.ddr_wrlvl_cntl_3   = 0x0706060A,
	.ddr_sr_cntr        = 0x00000000,
	.ddr_sdram_rcw_1    = 0x00000000,
	.ddr_sdram_rcw_2    = 0x00000000,
	.ddr_sdram_rcw_3    = 0x00000000,
	.ddr_cdr1           = 0x80080000,
	.ddr_cdr2           = 0x000000C0,
	.dq_map_0           = 0x00000000,
	.dq_map_1           = 0x00000000,
	.dq_map_2           = 0x00000000,
	.dq_map_3           = 0x00000000,
	.debug[28]          = 0x00700046,
		},
	},
};

extern char __dtb_fsl_tqmls1046a_mbls10xxa_start[];

static noinline __noreturn void tqmls1046a_r_entry(void)
{
	unsigned long membase = LS1046A_DDR_SDRAM_BASE;

	if (get_pc() >= membase)
		barebox_arm_entry(membase, 0x80000000 - SZ_64M,
				  __dtb_fsl_tqmls1046a_mbls10xxa_start);

	arm_cpu_lowlevel_init();
	ls1046a_init_lowlevel();

	debug_ll_init();

	udelay(500);
	putc_ll('>');

	IMD_USED_OF(fsl_tqmls1046a_mbls10xxa);

	fsl_ddr_set_memctl_regs(&tqmls1046a_ddrc[0], 0);

	ls1046a_errata_post_ddr();

	ls1046a_xload_start_image(0, 0, 0);

	pr_err("Booting failed\n");

	while (1);
}

void tqmls1046a_entry(unsigned long r0, unsigned long r1, unsigned long r2);

__noreturn void tqmls1046a_entry(unsigned long r0, unsigned long r1, unsigned long r2)
{
	relocate_to_current_adr();
	setup_c();

	tqmls1046a_r_entry();
}

extern char __dtb_fsl_tqmls1046a_arkona_c300_start[];

static struct fsl_ddr_controller arkona_c300_ddrc[] = {
	{
		.dimm_slots_per_ctrl = ARRAY_SIZE(dimm_params),
		.dimm_params = dimm_params,
		.memctl_opts.ddrtype = SDRAM_TYPE_DDR4,
		.base = IOMEM(LSCH2_DDR_ADDR),
		.ddr_freq = LS1046A_DDR_FREQ,
		.erratum_A008511 = 1,
		.erratum_A009803 = 1,
		.erratum_A010165 = 1,
		.erratum_A009801 = 1,
		.erratum_A009942 = 1,
		.chip_selects_per_ctrl = 4,
		.board_options = ddr_board_options,
		.fsl_ddr_config_reg = {
	.cs[0].bnds         = 0x000001FF,
	.cs[0].config       = 0x80010422,
	.cs[0].config_2     = 0x00000000,
	.cs[1].bnds         = 0x00000000,
	.cs[1].config       = 0x00000000,
	.cs[1].config_2     = 0x00000000,
	.cs[2].bnds         = 0x00000000,
	.cs[2].config       = 0x00000000,
	.cs[2].config_2     = 0x00000000,
	.cs[3].bnds         = 0x00000000,
	.cs[3].config       = 0x00000000,
	.cs[3].config_2     = 0x00000000,
	.timing_cfg_3       = 0x020F1100,
	.timing_cfg_0       = 0xF7660008,
	.timing_cfg_1       = 0xF1FCC178,
	.timing_cfg_2       = 0x00590160,
	.ddr_sdram_cfg      = 0x65000008,
	.ddr_sdram_cfg_2    = 0x00401150,
	.ddr_sdram_cfg_3    = 0x40000000,
	.ddr_sdram_mode     = 0x01030631,
	.ddr_sdram_mode_2   = 0x00100200,
	.ddr_sdram_mode_3   = 0x00000000,
	.ddr_sdram_mode_4   = 0x00000000,
	.ddr_sdram_mode_5   = 0x00000000,
	.ddr_sdram_mode_6   = 0x00000000,
	.ddr_sdram_mode_7   = 0x00000000,
	.ddr_sdram_mode_8   = 0x00000000,
	.ddr_sdram_mode_9   = 0x00000500,
	.ddr_sdram_mode_10  = 0x08800000,
	.ddr_sdram_mode_11  = 0x00000000,
	.ddr_sdram_mode_12  = 0x00000000,
	.ddr_sdram_mode_13  = 0x00000000,
	.ddr_sdram_mode_14  = 0x00000000,
	.ddr_sdram_mode_15  = 0x00000000,
	.ddr_sdram_mode_16  = 0x00000000,
	.ddr_sdram_interval = 0x0F3C079E,
	.ddr_data_init      = 0xDEADBEEF,
	.ddr_sdram_clk_cntl = 0x03000000,
	.ddr_init_addr      = 0x00000000,
	.ddr_init_ext_addr  = 0x00000000,
	.timing_cfg_4       = 0x00220002,
	.timing_cfg_5       = 0x00000000,
	.timing_cfg_6       = 0x00000000,
	.timing_cfg_7       = 0x25500000,
	.timing_cfg_8       = 0x05447A00,
	.timing_cfg_9       = 0x00000000,
	.ddr_zq_cntl        = 0x8A090705,
	.ddr_wrlvl_cntl     = 0x8605070A,
	.ddr_wrlvl_cntl_2   = 0x0A080807,
	.ddr_wrlvl_cntl_3   = 0x0706060A,
	.ddr_sr_cntr        = 0x00000000,
	.ddr_sdram_rcw_1    = 0x00000000,
	.ddr_sdram_rcw_2    = 0x00000000,
	.ddr_sdram_rcw_3    = 0x00000000,
	.ddr_cdr1           = 0x80080000,
	.ddr_cdr2           = 0x000000C0,
	.dq_map_0           = 0x00000000,
	.dq_map_1           = 0x00000000,
	.dq_map_2           = 0x00000000,
	.dq_map_3           = 0x00000000,
	.debug[28]          = 0x00700046,
		},
	},
};

static noinline __noreturn void arkona_c300_r_entry(void)
{
	unsigned long membase = LS1046A_DDR_SDRAM_BASE;

	if (get_pc() >= membase)
		barebox_arm_entry(membase, 0x80000000 - SZ_64M,
				  __dtb_fsl_tqmls1046a_arkona_c300_start);

	arm_cpu_lowlevel_init();
	ls1046a_init_lowlevel();

	debug_ll_init();

	udelay(500);
	putc_ll('>');

	IMD_USED_OF(fsl_tqmls1046a_arkona_c300);

	fsl_ddr_set_memctl_regs(&arkona_c300_ddrc[0], 0);

	ls1046a_errata_post_ddr();

	ls1046a_xload_start_image(0, 0, 0);

	pr_err("Booting failed\n");

	while (1);
}

void arkona_c300_entry(unsigned long r0, unsigned long r1, unsigned long r2);

__noreturn void arkona_c300_entry(unsigned long r0, unsigned long r1, unsigned long r2)
{
	relocate_to_current_adr();
	setup_c();

	arkona_c300_r_entry();
}
