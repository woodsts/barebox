/*
 * Copyright (C) 2013 Pengutronix <kernel@pengutronix.de>
 *
 * based on arch/arm/boards/freescale-mx6-sabresd/board.c
 * Copyright (C) 2013 Hubert Feurstein <h.feurstein@gmail.com>
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
 */

#include <common.h>
#include <init.h>
#include <gpio.h>
#include <linux/phy.h>
#include <sizes.h>
#include <globalvar.h>
#include <mach/imx6.h>
#include <mach/iomux-mx6.h>
#include <mach/bbu.h>

/*
 * Arkona AT1006: baseboard
 *
 * - CPU module #1: Arkona AT1008 (GPIO1[11] 10k pull down)
 * - CPU module #2: Arkona AT1008 (GPIO1[11] open)
 *
 * Arkona AT1008: CPU Module
 * - i.MX6D
 * - PMIC
 *   - LTC3676-1
 * - DDR3 SDRAM
 *   - four devices of type MT41J128M16HA
 *   - 16 M x 16 bit x 8 banks = 256 MiB per device
 *   - 1GiB full memory
 *   - speed grade FIXME
 * - debug UART connector (TxD = AT1008 out = GPIO5[28], RxD = AT1008 in = GPIO5[29])
 * - µSD card slot
 * - MRAM via SPI
 *   - EverSpin MR25H10
 * - NOR flash
 *   - Micron M25P32-VMW6G
 */

#define AT1006_USB1_VBUS_ENABLE IMX_GPIO_NR(3, 31)
#define AT1006_USB1_OC IMX_GPIO_NR(3, 30)
#define AT1006_SPI_NOR_SELECT IMX_GPIO_NR(1, 17)
#define AT1006_SPI_MRAM_SELECT IMX_GPIO_NR(5, 25)

static iomux_v3_cfg_t at1006_pads_gpio[] = {
	MX6Q_PAD_EIM_D31__GPIO_3_31,
	MX6Q_PAD_EIM_D30__GPIO_3_30,
};

static void at1006_detect_cpu_slot(void)
{
	u32 reg, pad, mux;

	/*
	 * to detect the slot we setup the GPIO1/11 to input mode and enable
	 * the internal 100 k pull up. If we read back a high level, we are
	 * in slot J9. In the case of a low level we are in slot J7
	 */
	pad = readl(MX6_IOMUXC_BASE_ADDR + 0x740);
	writel(MX6_PAD_CTL_HYS | MX6_PAD_CTL_PUS_100K_UP | MX6_PAD_CTL_PUE | MX6_PAD_CTL_PKE | MX6_PAD_CTL_SPEED_LOW, MX6_IOMUXC_BASE_ADDR + 0x740);

	mux = readl(MX6_IOMUXC_BASE_ADDR + 0x358);
	writel((1 << 4) | 5, MX6_IOMUXC_BASE_ADDR + 0x358); /* GPIO mode */

	reg = readl(MX6_GPIO1_BASE_ADDR + 0x04);
	writel(reg & ~(1 << 11), MX6_GPIO1_BASE_ADDR + 0x04);

	reg = !!(readl(MX6_GPIO1_BASE_ADDR + 0x08) & (1 << 11));

	/* restore the settings */
	writel(mux, MX6_IOMUXC_BASE_ADDR + 0x358);
	writel(pad, MX6_IOMUXC_BASE_ADDR + 0x740);

	if (reg) {
		globalvar_add_simple("module.slot", "J9");
		barebox_set_hostname("Link4@J9");
	} else {
		globalvar_add_simple("module.slot", "J7");
		barebox_set_hostname("Link4@J7");
	}
}

/*
 * GPIO3/31 must be '1' to enable the 5 V power
 * at the upper USB A at J6/J8
 */
static void at1006_enable_usbh1(void)
{
	mxc_iomux_v3_setup_multiple_pads(at1006_pads_gpio, ARRAY_SIZE(at1006_pads_gpio));
	gpio_direction_output(AT1006_USB1_VBUS_ENABLE, 0);
	gpio_direction_input(AT1006_USB1_OC);
	gpio_direction_output(AT1006_USB1_VBUS_ENABLE, 1); /* enable right now */
}

static void at1006_enable_spi(void)
{
	gpio_direction_output(AT1006_SPI_NOR_SELECT, 1);
	gpio_direction_output(AT1006_SPI_MRAM_SELECT, 1);
}

static int at1006_core_init(void)
{
	if (!of_machine_is_compatible("arkona,link4"))
		return 0;

	imx6_init_lowlevel();

	at1006_detect_cpu_slot();
	at1006_enable_usbh1();
	at1006_enable_spi();

	of_device_enable_path("/chosen/environment-spi");

	return 0;
}
postcore_initcall(at1006_core_init);

static int at1006_env_init(void)
{
	imx6_bbu_internal_spi_i2c_register_handler("spiflash", "/dev/spiflash.barebox",
		BBU_HANDLER_FLAG_DEFAULT,
		NULL, 0, /* expect a bootable image including a DCD in its head */
		0x10000000); /* app_dest, beginning of the SDRAM */

	return 0;
}
late_initcall(at1006_env_init);
