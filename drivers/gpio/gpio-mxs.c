// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Freescale MXS gpio support
 */

#include <common.h>
#include <errno.h>
#include <io.h>
#include <of.h>
#include <gpio.h>
#include <init.h>
#include <stmp-device.h>
#include <linux/err.h>

struct mxs_gpio_chip {
	void __iomem *base;
	void __iomem *din;
	void __iomem *doe;
	void __iomem *dout;
	struct gpio_chip chip;
};

struct mxs_gpio_regs {
	unsigned int din;
	unsigned int doe;
	unsigned int dout;
};

static struct mxs_gpio_regs regs_mxs23 = {
	.din = 0x0600,
	.dout = 0x0500,
	.doe = 0x0700,
};

static struct mxs_gpio_regs regs_mxs28 = {
	.din = 0x0900,
	.dout = 0x0700,
	.doe = 0x0b00,
};

static void mxs_gpio_set_value(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct mxs_gpio_chip *mxsgpio = container_of(chip, struct mxs_gpio_chip, chip);

	if (value)
		writel(0x1 << gpio, mxsgpio->dout + STMP_OFFSET_REG_SET);
	else
		writel(0x1 << gpio, mxsgpio->dout + STMP_OFFSET_REG_CLR);
}

static int mxs_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	struct mxs_gpio_chip *mxsgpio = container_of(chip, struct mxs_gpio_chip, chip);

	writel(0x1 << gpio, mxsgpio->doe + STMP_OFFSET_REG_CLR);

	return 0;
}


static int mxs_gpio_direction_output(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct mxs_gpio_chip *mxsgpio = container_of(chip, struct mxs_gpio_chip, chip);

	mxs_gpio_set_value(chip, gpio, value);
	writel(0x1 << gpio, mxsgpio->doe + STMP_OFFSET_REG_SET);

	return 0;
}

static int mxs_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	struct mxs_gpio_chip *mxsgpio = container_of(chip, struct mxs_gpio_chip, chip);

	if (readl(mxsgpio->din) & (1 << gpio))
		return 1;
	else
		return 0;
}

static int mxs_get_direction(struct gpio_chip *chip, unsigned gpio)
{
	struct mxs_gpio_chip *mxsgpio = container_of(chip, struct mxs_gpio_chip, chip);

	if (readl(mxsgpio->doe) & (1 << gpio))
		return GPIOF_DIR_OUT;
	else
		return GPIOF_DIR_IN;
}

static struct gpio_ops mxs_gpio_ops = {
	.direction_input = mxs_gpio_direction_input,
	.direction_output = mxs_gpio_direction_output,
	.get = mxs_gpio_get_value,
	.set = mxs_gpio_set_value,
	.get_direction = mxs_get_direction,
};

static int mxs_gpio_probe(struct device *dev)
{
	struct mxs_gpio_chip *mxsgpio;
	const struct mxs_gpio_regs *regs;
	int id;

	regs = device_get_match_data(dev);
	if (!regs)
		return -ENODEV;

	mxsgpio = xzalloc(sizeof(*mxsgpio));
	mxsgpio->chip.ops = &mxs_gpio_ops;
	if (dev->id < 0) {
		id = of_alias_get_id(dev->of_node, "gpio");
		if (id < 0)
			return id;
		mxsgpio->base = dev_get_mem_region(dev->parent, 0);
		mxsgpio->chip.base = id * 32;
	} else {
		id = dev->id;
		mxsgpio->base = dev_get_mem_region(dev, 0);
		mxsgpio->chip.base = dev->id * 32;
	}

	if (IS_ERR(mxsgpio->base))
		return PTR_ERR(mxsgpio->base);

	mxsgpio->doe = mxsgpio->base + regs->doe + id * 0x10;
	mxsgpio->dout = mxsgpio->base + regs->dout + id * 0x10;
	mxsgpio->din = mxsgpio->base + regs->din + id * 0x10;

	mxsgpio->chip.ngpio = 32;
	mxsgpio->chip.dev = dev;
	gpiochip_add(&mxsgpio->chip);

	dev_dbg(dev, "probed gpiochip%d with base %d\n", dev->id, mxsgpio->chip.base);

	return 0;
}

static __maybe_unused struct of_device_id mxs_gpio_dt_ids[] = {
	{
		.compatible = "fsl,imx23-gpio",
		.data = &regs_mxs23,
	}, {
		.compatible = "fsl,imx28-gpio",
		.data = &regs_mxs28,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mxs_gpio_dt_ids);

static struct platform_device_id mxs_gpio_ids[] = {
	{
		.name = "imx23-gpio",
		.driver_data = (unsigned long)&regs_mxs23,
	}, {
		.name = "imx28-gpio",
		.driver_data = (unsigned long)&regs_mxs28,
	}, {
		/* sentinel */
	},
};

static struct driver mxs_gpio_driver = {
	.name = "gpio-mxs",
	.probe = mxs_gpio_probe,
	.of_compatible = DRV_OF_COMPAT(mxs_gpio_dt_ids),
	.id_table = mxs_gpio_ids,
};

postcore_platform_driver(mxs_gpio_driver);
