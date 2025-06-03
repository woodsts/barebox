// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2010 Juergen Beisert, Pengutronix
// SPDX-FileCopyrightText: 2011 Wolfram Sang, Pengutronix

#include <common.h>
#include <init.h>
#include <gpio.h>
#include <envfs.h>
#include <environment.h>
#include <mci.h>
#include <linux/err.h>
#include <asm/armlinux.h>
#include <asm/mach-types.h>
#include <asm/barebox-arm.h>
#include <mach/mxs/imx-regs.h>
#include <mach/mxs/mci.h>
#include <linux/usb/fsl_usb2.h>
#include <mach/mxs/usb.h>
#include <mach/mxs/iomux.h>

static struct mxs_mci_platform_data mci_pdata = {
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED,
	.voltages = MMC_VDD_32_33 | MMC_VDD_33_34,	/* fixed to 3.3 V */
	.f_min = 400000,
};

static const uint32_t pad_setup[] = {
	/* SD card interface */
	SSP1_DATA0 | PULLUP(1),
	SSP1_DATA1 | PULLUP(1),
	SSP1_DATA2 | PULLUP(1),
	SSP1_DATA3 | PULLUP(1),
	SSP1_SCK,
	SSP1_CMD | PULLUP(1),
	SSP1_DETECT | PULLUP(1),
};

#ifdef CONFIG_USB_GADGET_DRIVER_ARC
static struct fsl_usb2_platform_data usb_pdata = {
	.operating_mode	= FSL_USB2_DR_DEVICE,
	.phy_mode	= FSL_USB2_PHY_UTMI,
};
#endif

/**
 * Try to register an environment storage on the attached MCI card
 * @return 0 on success
 *
 * We rely on the existence of a usable SD card, already attached to
 * our system, to get something like a persistent memory for our environment.
 * If this SD card is also the boot media, we can use the second partition
 * for our environment purpose (if present!).
 */
static int register_persistent_environment(void)
{
	struct cdev *cdev;

	/*
	 * The imx23-olinuxino only has one MCI card socket.
	 * So, we expect its name as "disk0".
	 */
	cdev = cdev_open_by_name("disk0", O_RDONLY);
	if (cdev == NULL) {
		pr_err("No MCI card preset\n");
		return -ENODEV;
	}
	cdev_close(cdev);

	/* MCI card is present, also a useable partition on it? */
	cdev = cdev_open_by_name("disk0.1", O_RDONLY);
	if (cdev == NULL) {
		pr_err("No second partition available\n");
		pr_info("Please create at least a second partition with"
			" 256 kiB...512 kiB in size (your choice)\n");
		return -ENODEV;
	}

	default_environment_path_set("/dev/disk0.1");

	cdev_close(cdev);

	return PTR_ERR_OR_ZERO(cdev);
}

static int mx23_evk_devices_init(void)
{
	int i, rc;

	if (barebox_arm_machine() != MACH_TYPE_MX23EVK)
		return 0;

	/* initizalize gpios */
	for (i = 0; i < ARRAY_SIZE(pad_setup); i++)
		imx_gpio_mode(pad_setup[i]);

	armlinux_set_architecture(MACH_TYPE_MX23EVK);

	add_generic_device("mxs_mci", DEVICE_ID_DYNAMIC, NULL, IMX_SSP1_BASE,
					0x8000, IORESOURCE_MEM, &mci_pdata);

	rc = register_persistent_environment();
	if (rc != 0)
		printf("Cannot create the 'env0' persistent "
			 "environment storage (%d)\n", rc);

#ifdef CONFIG_USB_GADGET_DRIVER_ARC
	imx23_usb_phy_enable();
	add_generic_usb_ehci_device(DEVICE_ID_DYNAMIC, IMX_USB_BASE, NULL);
	add_generic_device("fsl-udc", DEVICE_ID_DYNAMIC, NULL, IMX_USB_BASE,
			   0x200, IORESOURCE_MEM, &usb_pdata);
#endif
	return 0;
}

device_initcall(mx23_evk_devices_init);

static int mx23_evk_console_init(void)
{
	if (barebox_arm_machine() != MACH_TYPE_MX23EVK)
		return 0;

	barebox_set_model("Freescale i.MX23 EVK");
	barebox_set_hostname("mx23evk");

	add_generic_device("stm_serial", 0, NULL, IMX_DBGUART_BASE, 8192,
			   IORESOURCE_MEM, NULL);

	return 0;
}

console_initcall(mx23_evk_console_init);
