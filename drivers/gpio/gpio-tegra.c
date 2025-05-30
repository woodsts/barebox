// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2010 Erik Gilling <konkers@google.com>, Google, Inc
// SPDX-FileCopyrightText: 2013 Lucas Stach <l.stach@pengutronix.de>

#include <common.h>
#include <gpio.h>
#include <init.h>
#include <io.h>
#include <linux/err.h>

#define GPIO_BANK(x)		((x) >> 5)
#define GPIO_PORT(x)		(((x) >> 3) & 0x3)
#define GPIO_BIT(x)		((x) & 0x7)

#define GPIO_REG(x)		(GPIO_BANK(x) * config->bank_stride + \
					GPIO_PORT(x) * 4)

#define GPIO_CNF(x)		(GPIO_REG(x) + 0x00)
#define GPIO_OE(x)		(GPIO_REG(x) + 0x10)
#define GPIO_OUT(x)		(GPIO_REG(x) + 0x20)
#define GPIO_IN(x)		(GPIO_REG(x) + 0x30)
#define GPIO_INT_ENB(x)		(GPIO_REG(x) + 0x50)

#define GPIO_MSK_CNF(x)		(GPIO_REG(x) + config->upper_offset + 0x00)
#define GPIO_MSK_OE(x)		(GPIO_REG(x) + config->upper_offset + 0x10)
#define GPIO_MSK_OUT(x)		(GPIO_REG(x) + config->upper_offset + 0X20)

struct tegra_gpio_soc_config {
	u32 bank_stride;
	u32 upper_offset;
	u32 bank_count;
};

static void __iomem *gpio_base;
static const struct tegra_gpio_soc_config *config;

static inline void tegra_gpio_writel(u32 val, u32 reg)
{
	writel(val, gpio_base + reg);
}

static inline u32 tegra_gpio_readl(u32 reg)
{
	return readl(gpio_base + reg);
}

static int tegra_gpio_compose(int bank, int port, int bit)
{
	return (bank << 5) | ((port & 0x3) << 3) | (bit & 0x7);
}

static void tegra_gpio_mask_write(u32 reg, int gpio, int value)
{
	u32 val;

	val = 0x100 << GPIO_BIT(gpio);
	if (value)
		val |= 1 << GPIO_BIT(gpio);
	tegra_gpio_writel(val, reg);
}

static void tegra_gpio_enable(int gpio)
{
	tegra_gpio_mask_write(GPIO_MSK_CNF(gpio), gpio, 1);
}

static void tegra_gpio_disable(int gpio)
{
	tegra_gpio_mask_write(GPIO_MSK_CNF(gpio), gpio, 0);
}

static int tegra_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static void tegra_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	tegra_gpio_disable(offset);
}

static void tegra_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	tegra_gpio_mask_write(GPIO_MSK_OUT(offset), offset, value);
}

static int tegra_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	/* If gpio is in output mode then read from the out value */
	if ((tegra_gpio_readl(GPIO_OE(offset)) >> GPIO_BIT(offset)) & 1) {
		return (tegra_gpio_readl(GPIO_OUT(offset)) >>
				GPIO_BIT(offset)) & 0x1;
	}

	return (tegra_gpio_readl(GPIO_IN(offset)) >> GPIO_BIT(offset)) & 0x1;
}

static int tegra_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	tegra_gpio_mask_write(GPIO_MSK_OE(offset), offset, 0);
	tegra_gpio_enable(offset);
	return 0;
}

static int tegra_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int value)
{
	tegra_gpio_set(chip, offset, value);
	tegra_gpio_mask_write(GPIO_MSK_OE(offset), offset, 1);
	tegra_gpio_enable(offset);
	return 0;
}

static struct gpio_ops tegra_gpio_ops = {
	.request		= tegra_gpio_request,
	.free			= tegra_gpio_free,
	.direction_input	= tegra_gpio_direction_input,
	.direction_output	= tegra_gpio_direction_output,
	.get			= tegra_gpio_get,
	.set			= tegra_gpio_set,
};

static struct gpio_chip tegra_gpio_chip = {
	.ops	= &tegra_gpio_ops,
	.base	= 0,
};

static int tegra_gpio_probe(struct device *dev)
{
	struct resource *iores;
	int i, j;

	config = device_get_match_data(dev);
	if (!config)
		return -ENODEV;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores)) {
		dev_err(dev, "could not get memory region\n");
		return PTR_ERR(iores);
	}
	gpio_base = IOMEM(iores->start);

	for (i = 0; i < config->bank_count; i++) {
		for (j = 0; j < 4; j++) {
			int gpio = tegra_gpio_compose(i, j, 0);
			tegra_gpio_writel(0x00, GPIO_INT_ENB(gpio));
		}
	}

	tegra_gpio_chip.ngpio = config->bank_count * 32;
	tegra_gpio_chip.dev = dev;

	gpiochip_add(&tegra_gpio_chip);

	return 0;
}

static struct tegra_gpio_soc_config tegra20_gpio_config = {
	.bank_stride = 0x80,
	.upper_offset = 0x800,
	.bank_count = 7,
};

static struct tegra_gpio_soc_config tegra30_gpio_config = {
	.bank_stride = 0x100,
	.upper_offset = 0x80,
	.bank_count = 8,
};

static __maybe_unused struct of_device_id tegra_gpio_dt_ids[] = {
	{
		.compatible = "nvidia,tegra20-gpio",
		.data = &tegra20_gpio_config
	}, {
		.compatible = "nvidia,tegra30-gpio",
		.data = &tegra30_gpio_config
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tegra_gpio_dt_ids);

static struct driver tegra_gpio_driver = {
	.name		= "tegra-gpio",
	.of_compatible	= DRV_OF_COMPAT(tegra_gpio_dt_ids),
	.probe		= tegra_gpio_probe,
};

coredevice_platform_driver(tegra_gpio_driver);
