#include <common.h>
#include <init.h>
#include <bbu.h>
#include <asm/armlinux.h>
#include <asm/io.h>
#include <mach/common.h>

#define REG_BASE		0xf1000000

#define ADDITIONAL_MEM_BASE	0x40000000
#define ADDITIONAL_MEM_SIZE	0xb0000000

#define WIN0_BASE		0x00000000
#define WIN0_CTRL		0xffffffe0
#define WIN1_BASE		0x00000000
#define WIN1_CTRL		0xffffffe0
#define WIN2_CTRL		0xffffffe0
#define WIN3_CTRL		0xffffffe0

#define WIN_0_BASE_REG_OFFSET	0x20180
#define WIN_0_CTRL_REG_OFFSET	0x20184
#define WIN_1_BASE_REG_OFFSET	0x20188
#define WIN_1_CTRL_REG_OFFSET	0x2018c
#define WIN_2_BASE_REG_OFFSET	0x20190
#define WIN_2_CTRL_REG_OFFSET	0x20194
#define WIN_3_BASE_REG_OFFSET	0x20198
#define WIN_3_CTRL_REG_OFFSET	0x2019c

#define BRIDGE_CTRL		0x0fff0000
#define BRIDGE_BASE		0xf0000000

#define BRIDGE_CTRL_REG_OFFSET	0x20250
#define BRIDGE_BASE_REG_OFFSET	0x20254

#define SDRAM_CONF_REG_OFFSET	0x1400
#define ECC_IERR_OFFSET		19


static int at1101_phy_reset(void)
{
	void __iomem *base = mvebu_get_initial_int_reg_base();

	if (!of_machine_is_compatible("arkona,at1101"))
		return 0;

	/* set mpp5 to 1 to take phy out of reset */
#define GPIO0_BASE_ADDRESS (base + 0x18100)
	writel(readl(GPIO0_BASE_ADDRESS) | 0x20, GPIO0_BASE_ADDRESS);
	writel(readl(GPIO0_BASE_ADDRESS + 4) & ~0x20, GPIO0_BASE_ADDRESS + 4);


	bbu_register_std_file_update("spi", BBU_HANDLER_FLAG_DEFAULT,
		"/dev/m25p0.barebox", filetype_kwbimage_v1);

	return 0;
}
fs_initcall(at1101_phy_reset);

static int at1101_ram_init(void)
{
	if (!of_machine_is_compatible("arkona,at1101"))
		return 0;

	/*
	 * The vendor binary that initializes RAM doesn't program the SDRAM
	 * Address Decoding registers to match the actual available RAM. But as
	 * barebox later determines the RAM size from these, fix them up here.
	 *
	 * As we are running from RAM, we cannot just reconfigure WIN0. Instead
	 * create an identical WIN1 and then reconfigure WIN0. Finally disable
	 * WIN1 again.
	 *
	 * We also need to configure the MBUS Bridge to allow access to the
	 * MBUS device ranges with will be allocated in the range now covered
	 * by the 4G RAM window.
	 */

	printk("reconfiguring memory window... ");

	/* win 1 Control Register: en=0*/
	writel(WIN1_CTRL, REG_BASE + WIN_1_CTRL_REG_OFFSET);
	/* win 1 Base Address Register */
	writel(WIN1_BASE, REG_BASE + WIN_1_BASE_REG_OFFSET);
	/* win 1 Control Register: en=1*/
	writel(WIN1_CTRL | 1, REG_BASE + WIN_1_CTRL_REG_OFFSET);

	/* win 0 Control Register: en=0*/
	writel(WIN0_CTRL, REG_BASE + WIN_0_CTRL_REG_OFFSET);
	/* win 0 Base Address Register */
	writel(WIN0_BASE, REG_BASE + WIN_0_BASE_REG_OFFSET);
	/* win 0 Control Register: en=1*/
	writel(WIN0_CTRL | 1, REG_BASE + WIN_0_CTRL_REG_OFFSET);

	/* win 1 Control Register: en=0*/
	writel(WIN1_CTRL, REG_BASE + WIN_1_CTRL_REG_OFFSET);
	/* win 2 Control Register: en=0*/
	writel(WIN2_CTRL, REG_BASE + WIN_2_CTRL_REG_OFFSET);
	/* win 3 Control Register: en=0*/
	writel(WIN3_CTRL, REG_BASE + WIN_3_CTRL_REG_OFFSET);

	/* bridge Control Register: en=0*/
	writel(BRIDGE_CTRL, REG_BASE + BRIDGE_CTRL_REG_OFFSET);
	/* bridge Base Address Register */
	writel(BRIDGE_BASE, REG_BASE + BRIDGE_BASE_REG_OFFSET);
	/* bridge Control Register: en=1*/
	writel(BRIDGE_CTRL | 1, REG_BASE + BRIDGE_CTRL_REG_OFFSET);

	/* tell barebox about modified RAM region */
	arm_add_mem_device("ram1", ADDITIONAL_MEM_BASE, ADDITIONAL_MEM_SIZE);

	printk("done.\n");

	return 0;
}
mem_initcall(at1101_ram_init);


static int at1101_ecc_scrubbing(void)
{
	uint32_t reg;
	uint32_t i;

	if (!of_machine_is_compatible("arkona,at1101"))
		return 0;

	printk("scrubbing additional ECC RAM... ");

	/* ignore ECC errors for now */
	reg = readl(REG_BASE + SDRAM_CONF_REG_OFFSET);
	reg |= (1 << ECC_IERR_OFFSET);
	writel(reg, REG_BASE + SDRAM_CONF_REG_OFFSET);

	barrier();

	/* ECC scrubbing */
	for (i = ADDITIONAL_MEM_BASE; i < ADDITIONAL_MEM_BASE + ADDITIONAL_MEM_SIZE - 1; i += 4) {
		writel(i, i);
	}

	barrier();

	/* report ECC errors again */
	reg = readl(REG_BASE + SDRAM_CONF_REG_OFFSET);
	reg &= ~(1 << ECC_IERR_OFFSET);
	writel(reg, REG_BASE + SDRAM_CONF_REG_OFFSET);

	printk("done.\n");

	return 0;
}
coredevice_initcall(at1101_ecc_scrubbing);
