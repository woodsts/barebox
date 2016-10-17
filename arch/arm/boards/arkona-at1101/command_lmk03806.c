/*
 * Copyright (C) 2013 Pengutronix <kernel@pengutronix.de>
 *                    and arkona technologies GmbH
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
#include <gpio.h>
#ifdef CONFIG_MACH_ARKONA_1006
#include <mach/imx6.h>
#endif
#include <linux/types.h>
#include <fs.h>
#include <malloc.h>
#include <command.h>
#include <libfile.h>
#include <crc.h>

/* please be aware: the GPIO pins must be already multiplexed and configured */

/* the lmk03806 command reads in a file with the following content:
 *
 * Word#    Usage
 * -------------------
 *   1    size of file in bytes
 *   2    command#1
 *   3    command#2
 *   .
 *   .
 *   n    command#n
 *  n+1   CRC over 1...n (CRC32 with seed of '0')
 *
 * note: all multi byte values are expected in little endian
 */

struct gpio_set {
	unsigned le; /* GPIO# for the 'word latch enable' signal */
	unsigned clk; /* GPIO# for the clock signal (fallig edge: bit change, risig edge: bit latch) */
	unsigned data; /* GPIO# for the data bit */
	unsigned ftest; /* FIXME sole input GPIO# */
};

#ifdef CONFIG_MACH_ARKONA_1006
/* note: both clock generators share the same DATA pin */
static const struct gpio_set clock_devs[2] = {
	{
		/* GPIO set to access the clock generator U8 */
		.le = IMX_GPIO_NR(7, 1),
		.clk = IMX_GPIO_NR(6, 7),
		.data = IMX_GPIO_NR(1, 11),
		.ftest = IMX_GPIO_NR(6, 18), /* not used yet */
	}, {
		/* GPIO set to access the clock generator U11 */
		.le = IMX_GPIO_NR(7, 0),
		.clk = IMX_GPIO_NR(7, 10),
		.data = IMX_GPIO_NR(1, 11),
		.ftest = IMX_GPIO_NR(6, 17), /* not used yet */
	},
};
#elif defined(CONFIG_MACH_ARKONA_AT1101)
static const struct gpio_set clock_devs[2] = {
	{
		/* GPIO set to access the clock generator U26 */
		.le = 58,
		.clk = 55,
		.data = 57,
		.ftest = 60, /* not used yet */
	}, {
		/* GPIO set to access the clock generator U27 */
		.le = 59,
		.clk = 56,
		.data = 57,
		.ftest = 61, /* not used yet */
	},
};
#endif

/* the rising edge samples the data bit into the clock device */
static void lmk03806_transmitt_bit(bool data_bit, const struct gpio_set *gpio)
{
	gpio_set_value(gpio->data, data_bit);
	/* data setup time of tDCS=25 ns required */
        gpio_set_value(gpio->clk, 1); /* latch data */
	/* data hold time of tCDH=8 ns required */
        gpio_set_value(gpio->clk, 0);
}

/* transmitt a word, MSB first */
static void lmk03806_transmitt_word(uint32_t word, const struct gpio_set *gpio)
{
	unsigned u;

	u = 32;
	while (u != 0) {
		lmk03806_transmitt_bit(!!(word & (1 << 31)), gpio);
		word <<= 1;
		u--;
	}

	/* setup time of tCES=25 ns required */
	gpio_set_value(gpio->le, 1); /* latch the word */
	/* tEWH=25 ns required */
	gpio_set_value(gpio->le, 0); /* ready for next word */
	/* hold time of tECS=25 ns required */
}

static void lmk03806_transmitt_list(const uint32_t *list, unsigned cnt, const struct gpio_set *gpio)
{
	while (cnt) {
		lmk03806_transmitt_word(*list, gpio);
		cnt--;
		list++;
	}

	/*
	 * the data sheet tells us, after the transfer all
	 * signal lines should be held at low level
	 */
	gpio_set_value(gpio->clk, 0);
	gpio_set_value(gpio->data, 0);
}

/* last word entry in the file is a CRC over the whole previous bytes */
static int lmk03806_check_crc(const uint32_t *list, size_t size)
{
	uint32_t crc;
	size_t idx = size / sizeof(uint32_t);

	crc = crc32(0, list, size - sizeof(uint32_t));
	if (crc != list[idx - 1]) {
		printf("File has a CRC error: %08X versus %08X **\n", crc, list[idx - 1]);
		return 1;
	}

	return 0;
}

static int lmk03806_check_integrity(const uint32_t *list, size_t size)
{
	if (size & (sizeof(uint32_t) - 1)) {
		printf("Size of file must be a multiple of 4 bytes\n");
		return 1;
	}

	if (size < 12) {
		printf("Size of file is too short. At least 12 bytes are required\n");
		return 1;
	}

	if (list[0] != size) {
		printf("Size of file does not match header entry\n");
		return 1;
	}

	return lmk03806_check_crc(list, size);
}

static int do_lmk03806(int argc, char *argv[])
{
	uint32_t *list;
	size_t dev, size;
	int rc;
	char *ep;

	if (argc < 3) {
		printf("Wrong usage, refer help\n");
		return 1;
	}

	/* first parameter must be a number */
	dev = simple_strtoul(argv[1], &ep, 0);
	if (argv[1] == ep) {
		printf("Illegal index\n");
		return 1;
	}

	if (dev >= ARRAY_SIZE(clock_devs)) {
		printf("Invalid index (%u), refer help\n", dev);
		return 1;
	}

	list = read_file(argv[2], &size);
	if (list == NULL) {
		printf("Unable to read %s\n", argv[2]);
		return 1;
	}

	rc = lmk03806_check_integrity(list, size);
	if (rc != 0) {
		free(list);
		return 1;
	}

	/* transmitt data from the second word excluding the CRC word at the end */
	lmk03806_transmitt_list(&list[1], (size / sizeof(uint32_t)) - 2, &clock_devs[dev]);

	free(list);
	return 0;
}

BAREBOX_CMD_HELP_START(lmk03806)
	BAREBOX_CMD_HELP_OPT("<index>", "clock generator index ('0'=U8, '1'=U11)\n")
	BAREBOX_CMD_HELP_OPT("<filename>", "file to read the configure values from\n")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(lmk03806)
	.cmd = do_lmk03806,
	BAREBOX_CMD_DESC("configure LMK03806 clock generator")
	BAREBOX_CMD_OPTS("lmk03806 <idx> <filename>")
BAREBOX_CMD_END

static void lmk03806_init_one_bus(const struct gpio_set *gpio)
{
	/* from the datasheet: all signal's default value should be low */
	gpio_direction_output(gpio->le, 0);
	gpio_direction_output(gpio->clk, 0);
	gpio_direction_output(gpio->data, 0);
	gpio_direction_input(gpio->ftest);
}

/* configure the GPIOs used for microwire purpose */
void at1006_enable_clock_gpios(void)
{
	lmk03806_init_one_bus(&clock_devs[0]);
	lmk03806_init_one_bus(&clock_devs[1]);
}
