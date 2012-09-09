/*
 * Copyright (C) 2007 Sascha Hauer, Pengutronix
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

#include <common.h>
#include <init.h>
#include <sizes.h>
#include <environment.h>
#include <asm/armlinux.h>
#include <mach/generic.h>

extern void *barebox_reserve_pointer;

enum soc_type {
	SOC_UNKNOWN,
	SOC_IMX1,
	SOC_IMX21,
	SOC_IMX25,
	SOC_IMX27,
	SOC_IMX31,
	SOC_IMX35,
	SOC_IMX51,
	SOC_IMX53,
	SOC_IMX6,
};

struct of_soc_compat {
	char *name;
	enum soc_type soc;
	int (*init)(void);
};

struct of_soc_compat imx_compat[] = {
	{
		.name = "fsl,imx1",
		.soc = SOC_IMX1,
		.init = imx1_init,
	},{
		.name = "fsl,imx21",
		.soc = SOC_IMX21,
		.init = imx21_init,
	},{
		.name = "fsl,imx25",
		.soc = SOC_IMX25,
		.init = imx25_init,
	},{
		.name = "fsl,imx27",
		.soc = SOC_IMX27,
		.init = imx27_init,
	},{
		.name = "fsl,imx31",
		.soc = SOC_IMX31,
		.init = imx31_init,
	},{
		.name = "fsl,imx35",
		.soc = SOC_IMX35,
		.init = imx35_init,
	},{
		.name = "fsl,imx51",
		.soc = SOC_IMX51,
		.init = imx51_init,
	},{
		.name = "fsl,imx53",
		.soc = SOC_IMX53,
		.init = imx53_init,
	},{
		.name = "fsl,imx6q",
		.soc = SOC_IMX6,
		.init = imx6_init,
	},
};

enum soc_type soc_type;

enum soc_type of_soctype(struct of_soc_compat *arr, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		struct of_soc_compat *compat = &arr[i];
		if (of_machine_is_compatible(compat->name)) {
			compat->init();
			return compat->soc;
		}
	}

	return SOC_UNKNOWN;
}


static int probe_dt(void)
{
	of_parse_dtb(barebox_reserve_pointer);

	soc_type = of_soctype(imx_compat, ARRAY_SIZE(imx_compat));

	of_probe();

	return 0;
}
core_initcall(probe_dt);
