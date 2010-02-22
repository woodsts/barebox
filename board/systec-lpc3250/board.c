
#include <common.h>
#include <init.h>
#include <nand.h>
#include <partition.h>

#include <asm/armlinux.h>
#include <asm/mmu.h>

#include <mach/lpc3250.h>
#include <mach/lpc32xx_nand.h>
#include <mach/lpc32xx_clkpwr_driver.h>

#include "phy3250_board.h"

static struct memory_platform_data ram_pdata = {
	.name = "ram0",
	.flags = DEVFS_RDWR,
};

static struct device_d sdram_dev = {
	.name		= "mem",
	.map_base	= 0x80000000,
	.size		= 64 * 1024 * 1024,
	.platform_data	= &ram_pdata,
};

static struct device_d net_dev = {
	.name		= "lpc3250_net",
	.map_base	= ETHERNET_BASE,
};

static struct lpc32xx_nand_platform_data nand_pdata = {
	.flash_bbt = 1,
};

static struct device_d nand_dev = {
	.name		= "lpc3250_nand",
	.map_base	= 0x20020000,
	.size		= 4096,
	.platform_data	= &nand_pdata,
};

#ifdef CONFIG_MMU
static void systec_mmu_init(void)
{
	mmu_init();

	arm_create_section(0x80000000, 0x80000000, 128, PMD_SECT_DEF_CACHED);
	arm_create_section(0x90000000, 0x80000000, 128, PMD_SECT_DEF_UNCACHED);

	setup_dma_coherent(0x10000000);

#if TEXT_BASE & (0x100000 - 1)
#warning cannot create vector section. Adjust TEXT_BASE to a 1M boundary
#else
	arm_create_section(0x0,        TEXT_BASE,   1, PMD_SECT_DEF_UNCACHED);
#endif
	mmu_enable();
}
#else
static void systec_mmu_init(void)
{
}
#endif

static int systec_devices_init(void)
{
	systec_mmu_init();

	register_device(&sdram_dev);
	register_device(&net_dev);
	register_device(&nand_dev);

	devfs_add_partition("nand0", 0x40000, 0x20000, PARTITION_FIXED, "env_raw");
        dev_add_bb_dev("env_raw", "env0");

	armlinux_add_dram(&sdram_dev);
	armlinux_set_bootparams((void *)0x80000100);
	armlinux_set_architecture(2511); /* FIXME */

	return 0;
}

device_initcall(systec_devices_init);

static struct device_d systec_serial_device = {
	.name     = "lpc_serial",
	.map_base = UART5_BASE,
	.size     = 4096,
};

static int systec_console_init(void)
{
	CLKPWR->clkpwr_uart_clk_ctrl |= CLKPWR_UARTCLKCTRL_UART5_EN;

	register_device(&systec_serial_device);

	return 0;
}

console_initcall(systec_console_init);

