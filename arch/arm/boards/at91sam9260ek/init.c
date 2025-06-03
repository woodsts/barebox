// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2007 Sascha Hauer, Pengutronix

#include <common.h>
#include <init.h>
#include <envfs.h>
#include <environment.h>
#include <asm/armlinux.h>
#include <asm/mach-types.h>
#include <nand.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/rawnand.h>
#include <linux/sizes.h>
#include <mach/at91/board.h>
#include <mach/at91/at91sam9_smc.h>
#include <gpio.h>
#include <mach/at91/iomux.h>
#include <mach/at91/at91_rstc.h>
#include <linux/clk.h>

/*
 * board revision encoding
 * bit 0:
 *	0 => 1 mmc
 *	1 => 2 mmcs (board from revision C)
 */
#define HAVE_2MMC	(1 << 0)
static void ek_set_board_type(void)
{
	if (machine_is_at91sam9g20ek()) {
		armlinux_set_architecture(MACH_TYPE_AT91SAM9G20EK);
		if (IS_ENABLED(CONFIG_AT91_HAVE_2MMC))
			armlinux_set_revision(HAVE_2MMC);
	} else {
		armlinux_set_architecture(MACH_TYPE_AT91SAM9260EK);
	}
}

static struct atmel_nand_data nand_pdata = {
	.ale		= 21,
	.cle		= 22,
	.det_pin	= -EINVAL,
	.rdy_pin	= AT91_PIN_PC13,
	.enable_pin	= AT91_PIN_PC14,
	.ecc_mode	= NAND_ECC_SOFT,
	.on_flash_bbt	= 1,
};

static struct sam9_smc_config ek_9260_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 1,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 3,
	.nrd_pulse		= 3,
	.ncs_write_pulse	= 3,
	.nwe_pulse		= 3,

	.read_cycle		= 5,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE |
				  AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 2,
};

static struct sam9_smc_config ek_9g20_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 2,
	.ncs_write_setup	= 0,
	.nwe_setup		= 2,

	.ncs_read_pulse		= 4,
	.nrd_pulse		= 4,
	.ncs_write_pulse	= 4,
	.nwe_pulse		= 4,

	.read_cycle		= 7,
	.write_cycle		= 7,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE |
				  AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 3,
};

static void ek_add_device_nand(void)
{
	struct sam9_smc_config *smc;

	if (machine_is_at91sam9g20ek())
		smc = &ek_9g20_nand_smc_config;
	else
		smc = &ek_9260_nand_smc_config;

	/* setup bus-width (8 or 16) */
	if (IS_ENABLED(CONFIG_MTD_NAND_ATMEL_BUSWIDTH_16)) {
		nand_pdata.bus_width_16 = 1;
		smc->mode |= AT91_SMC_DBW_16;
	} else {
		smc->mode |= AT91_SMC_DBW_8;
	}

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(0, 3, smc);

	at91_add_device_nand(&nand_pdata);
}

static struct macb_platform_data macb_pdata = {
	.phy_interface = PHY_INTERFACE_MODE_RMII,
	.phy_addr = 0,
};

static void at91sam9260ek_phy_reset(void)
{
	at91_set_gpio_input(AT91_PIN_PA14, 0);
	at91_set_gpio_input(AT91_PIN_PA15, 0);
	at91_set_gpio_input(AT91_PIN_PA17, 0);
	at91_set_gpio_input(AT91_PIN_PA25, 0);
	at91_set_gpio_input(AT91_PIN_PA26, 0);
	at91_set_gpio_input(AT91_PIN_PA28, 0);

	at91sam_phy_reset(IOMEM(AT91SAM9260_BASE_RSTC));
}

/*
 * MCI (SD/MMC)
 */
static struct atmel_mci_platform_data __initdata ek_mci_data = {
	.bus_width	= 4,
	.slot_b		= 1,
	.detect_pin	= -EINVAL,
	.wp_pin		= -EINVAL,
};

static void ek_usb_add_device_mci(void)
{
	if (!IS_ENABLED(CONFIG_MCI_ATMEL))
		return;

	if (machine_is_at91sam9g20ek())
		ek_mci_data.detect_pin = AT91_PIN_PC9;

	at91_add_device_mci(0, &ek_mci_data);
}

/*
 * USB Host port
 */
static struct at91_usbh_data __initdata ek_usbh_data = {
	.ports		= 2,
	.vbus_pin	= { -EINVAL, -EINVAL },
};

/*
 * USB Device port
 */
static struct at91_udc_data __initdata ek_udc_data = {
	.vbus_pin	= AT91_PIN_PC5,
	.pullup_pin	= -EINVAL,		/* pull-up driven by UDC */
};

struct gpio_led leds[] = {
	{
		.gpio	= AT91_PIN_PA6,
		.active_low	= 1,
		.led	= {
			.name = "ds5",
		},
	}, {
		.gpio	= AT91_PIN_PA9,
		.led	= {
			.name = "ds3",
		},
	},
};

static void __init ek_add_led(void)
{
	int i;

	if (IS_ENABLED(CONFIG_AT91_HAVE_2MMC)) {
		leds[0].gpio = AT91_PIN_PB8;
		leds[1].gpio = AT91_PIN_PB9;
	}

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		at91_set_gpio_output(leds[i].gpio, leds[i].active_low);
		led_gpio_register(&leds[i]);
	}
	led_set_trigger(LED_TRIGGER_HEARTBEAT, &leds[1].led);
}

static int at91sam9260ek_mem_init(void)
{
	at91_add_device_sdram(0);

	return 0;
}
mem_initcall(at91sam9260ek_mem_init);

static void __init ek_add_device_buttons(void)
{
	at91_set_gpio_input(AT91_PIN_PA30, 1);	/* btn3 */
	at91_set_deglitch(AT91_PIN_PA30, 1);
	export_env_ull("dfu_button", AT91_PIN_PA30);
	at91_set_gpio_input(AT91_PIN_PA31, 1);	/* btn4 */
	at91_set_deglitch(AT91_PIN_PA31, 1);
	export_env_ull("btn4", AT91_PIN_PA31);
}

static int at91sam9260ek_devices_init(void)
{
	ek_add_device_nand();
	at91sam9260ek_phy_reset();
	at91_add_device_eth(0, &macb_pdata);
	at91_add_device_usbh_ohci(&ek_usbh_data);
	at91_add_device_udc(&ek_udc_data);
	ek_usb_add_device_mci();
	ek_add_device_buttons();
	ek_add_led();

	ek_set_board_type();

	devfs_add_partition("nand0", 0x00000, SZ_128K, DEVFS_PARTITION_FIXED, "at91bootstrap_raw");
	dev_add_bb_dev("at91bootstrap_raw", "at91bootstrap");
	devfs_add_partition("nand0", SZ_128K, SZ_256K, DEVFS_PARTITION_FIXED, "self_raw");
	dev_add_bb_dev("self_raw", "self0");
	devfs_add_partition("nand0", SZ_256K + SZ_128K, SZ_128K, DEVFS_PARTITION_FIXED, "env_raw");
	dev_add_bb_dev("env_raw", "env0");
	devfs_add_partition("nand0", SZ_512K, SZ_128K, DEVFS_PARTITION_FIXED, "env_raw1");
	dev_add_bb_dev("env_raw1", "env1");
	default_environment_path_set("/dev/env0");

	if (IS_ENABLED(CONFIG_DEFAULT_ENVIRONMENT_GENERIC))
		defaultenv_append_directory(defaultenv_at91sam9260ek);

	return 0;
}
device_initcall(at91sam9260ek_devices_init);

static int at91sam9260ek_console_init(void)
{
	if (machine_is_at91sam9g20ek()) {
		barebox_set_model("Atmel at91sam9g20-ek");
		barebox_set_hostname("at91sam9g20-ek");
	} else {
		barebox_set_model("Atmel at91sam9260-ek");
		barebox_set_hostname("at91sam9260-ek");
	}

	at91_register_uart(0, 0);
	return 0;
}
console_initcall(at91sam9260ek_console_init);

static int at91sam9260ek_main_clock(void)
{
	at91_set_main_clock(18432000);
	return 0;
}
pure_initcall(at91sam9260ek_main_clock);
