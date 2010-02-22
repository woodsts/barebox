/*
 * (C) Copyright 2007-2008
 * Stelian Pop <stelian.pop@leadtechdesign.com>
 * Lead Tech Design <www.leadtechdesign.com>
 *
 * (C) Copyright 2006 ATMEL Rousset, Lacressonniere Nicolas
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <nand.h>
#include <errno.h>
#include <init.h>
#include <malloc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <asm/io.h>
#include <mach/lpc3250.h>
#include <mach/lpc32xx_nand.h>

#define	NAND_ALE_OFFS   4
#define	NAND_CLE_OFFS   8

struct lpc3250_nand_host {
	struct mtd_info		mtd;
	struct nand_chip	nand;
	struct mtd_partition	*parts;
	struct device_d		*dev;

	unsigned long		base;
};

static void lpc3250_nand_inithw(struct lpc3250_nand_host *host)
{
	/* Enable clocks to the SLC NAND controller */
	writel(0x5, &CLKPWR->clkpwr_nand_clk_ctrl);

	/* Reset SLC NAND controller & clear ECC */
	writel(SLCCTRL_SW_RESET | SLCCTRL_ECC_CLEAR, &SLCNAND->slc_ctrl);

	/* 8-bit bus, no DMA, CE normal */
	writel(0, &SLCNAND->slc_cfg);

	/* Interrupts disabled and cleared */
	writel(0, &SLCNAND->slc_ien);
	writel(SLCSTAT_INT_TC | SLCSTAT_INT_RDY_EN, &SLCNAND->slc_icr);

	/* This is optimized for a 266MHz clock rate, but will work fine for
	 * rates lower then 266MHz
	 */
	writel(SLCTAC_WDR(3) |
		SLCTAC_WWIDTH(10) |
		SLCTAC_WHOLD(3) |
		SLCTAC_WSETUP(4) |
		SLCTAC_RDR(3) |
		SLCTAC_RWIDTH(10) |
		SLCTAC_RHOLD(3) |
		SLCTAC_RSETUP(4),
		&SLCNAND->slc_tac);
}

static void lpc3250_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;
	unsigned long io_addr_w;

	if (ctrl & NAND_CTRL_CHANGE) {
		io_addr_w = (unsigned long) this->IO_ADDR_W;
		io_addr_w &= ~(NAND_CLE_OFFS | NAND_ALE_OFFS);

		if (ctrl & NAND_CLE)
			io_addr_w |= NAND_CLE_OFFS;

		else if (ctrl & NAND_ALE)
			io_addr_w |= NAND_ALE_OFFS;

		if (ctrl & NAND_NCE)
			SLCNAND->slc_cfg |= SLCCFG_CE_LOW;
		else
			SLCNAND->slc_cfg &= ~SLCCFG_CE_LOW;

		this->IO_ADDR_W = (void *)io_addr_w;
	}

	if (cmd != NAND_CMD_NONE)
		writel(cmd, this->IO_ADDR_W);
}

static int lpc3250_nand_ready(struct mtd_info *mtd)
{
	/* Check the SLC NAND controller status */
	return SLCNAND->slc_stat & SLCSTAT_NAND_READY;
}

static u8 lpc3250_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;

	return readl(this->IO_ADDR_R);
}

static void lpc3250_write_buf(struct mtd_info *mtd, const u8 *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++)
		writel(buf[i], this->IO_ADDR_W);
}

static void lpc3250_read_buf(struct mtd_info *mtd, u8 *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readl(this->IO_ADDR_R);
}

static int lpc3250_verify_buf(struct mtd_info *mtd, const u8 *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		if (buf[i] != (u8)readl(this->IO_ADDR_R))
			return -EFAULT;
	}

	return 0;
}

static int lpc3250_nand_probe(struct device_d *dev)
{
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct lpc3250_nand_host *host;
	struct lpc32xx_nand_platform_data *pdata = dev->platform_data;
	int ret;

	/* Allocate memory for MTD device structure and private data */
	host = kzalloc(sizeof(struct lpc3250_nand_host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->dev = dev;
	host->base = dev->map_base;

	/* structures must be linked */
	chip = &host->nand;
	mtd = &host->mtd;
	mtd->priv = chip;

	/* 50 us command delay time */
	chip->chip_delay = 2000;
	chip->priv = host;

	if (pdata && pdata->flash_bbt)
		chip->options |= NAND_USE_FLASH_BBT;

	chip->IO_ADDR_R = chip->IO_ADDR_W = (void *)dev->map_base;

	chip->read_byte = lpc3250_read_byte;
	chip->write_buf = lpc3250_write_buf;
	chip->read_buf = lpc3250_read_buf;
	chip->verify_buf = lpc3250_verify_buf;

	chip->ecc.mode = NAND_ECC_SOFT;
	chip->cmd_ctrl = lpc3250_nand_hwcontrol;
	chip->dev_ready = lpc3250_nand_ready;

	lpc3250_nand_inithw(host);

	/* Scan to find existence of the device */
	ret = nand_scan(mtd, 1);
	if (ret != 0) {
		ret = -ENXIO;
		goto on_error;
	}

	ret = add_mtd_device(mtd);
	if (ret)
		goto on_error;

	return 0;

on_error:
	free(host);
	return ret;
}

static struct driver_d lpc3250_nand_driver = {
	.name  = "lpc3250_nand",
	.probe = lpc3250_nand_probe,
};

/*
 * Main initialization routine
 * @return 0 if successful; non-zero otherwise
 */
static int __init lpc3250_nand_init(void)
{
	return register_driver(&lpc3250_nand_driver);
}

device_initcall(lpc3250_nand_init);
