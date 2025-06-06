// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Sascha Hauer, kernel@pengutronix.de
 */

#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/bitfield.h>
#include <linux/completion.h>

#define DRIVER_NAME "mxc_nand"

/* Addresses for NFC registers */
#define NFC_V1_V2_BUF_SIZE		(host->regs + 0x00)
#define NFC_V1_V2_BUF_ADDR		(host->regs + 0x04)
#define NFC_V1_V2_FLASH_ADDR		(host->regs + 0x06)
#define NFC_V1_V2_FLASH_CMD		(host->regs + 0x08)
#define NFC_V1_V2_CONFIG		(host->regs + 0x0a)
#define NFC_V1_V2_ECC_STATUS_RESULT	(host->regs + 0x0c)
#define NFC_V1_V2_RSLTMAIN_AREA		(host->regs + 0x0e)
#define NFC_V21_RSLTSPARE_AREA		(host->regs + 0x10)
#define NFC_V1_V2_WRPROT		(host->regs + 0x12)
#define NFC_V1_UNLOCKSTART_BLKADDR	(host->regs + 0x14)
#define NFC_V1_UNLOCKEND_BLKADDR	(host->regs + 0x16)
#define NFC_V21_UNLOCKSTART_BLKADDR0	(host->regs + 0x20)
#define NFC_V21_UNLOCKSTART_BLKADDR1	(host->regs + 0x24)
#define NFC_V21_UNLOCKSTART_BLKADDR2	(host->regs + 0x28)
#define NFC_V21_UNLOCKSTART_BLKADDR3	(host->regs + 0x2c)
#define NFC_V21_UNLOCKEND_BLKADDR0	(host->regs + 0x22)
#define NFC_V21_UNLOCKEND_BLKADDR1	(host->regs + 0x26)
#define NFC_V21_UNLOCKEND_BLKADDR2	(host->regs + 0x2a)
#define NFC_V21_UNLOCKEND_BLKADDR3	(host->regs + 0x2e)
#define NFC_V1_V2_NF_WRPRST		(host->regs + 0x18)
#define NFC_V1_V2_CONFIG1		(host->regs + 0x1a)
#define NFC_V1_V2_CONFIG2		(host->regs + 0x1c)

#define NFC_V1_V2_ECC_STATUS_RESULT_ERM GENMASK(3, 2)

#define NFC_V2_CONFIG1_ECC_MODE_4	(1 << 0)
#define NFC_V1_V2_CONFIG1_SP_EN		(1 << 2)
#define NFC_V1_V2_CONFIG1_ECC_EN	(1 << 3)
#define NFC_V1_V2_CONFIG1_INT_MSK	(1 << 4)
#define NFC_V1_V2_CONFIG1_BIG		(1 << 5)
#define NFC_V1_V2_CONFIG1_RST		(1 << 6)
#define NFC_V1_V2_CONFIG1_CE		(1 << 7)
#define NFC_V2_CONFIG1_ONE_CYCLE	(1 << 8)
#define NFC_V2_CONFIG1_PPB(x)		(((x) & 0x3) << 9)
#define NFC_V2_CONFIG1_FP_INT		(1 << 11)

#define NFC_V1_V2_CONFIG2_INT		(1 << 15)

/*
 * Operation modes for the NFC. Valid for v1, v2 and v3
 * type controllers.
 */
#define NFC_CMD				(1 << 0)
#define NFC_ADDR			(1 << 1)
#define NFC_INPUT			(1 << 2)
#define NFC_OUTPUT			(1 << 3)
#define NFC_ID				(1 << 4)
#define NFC_STATUS			(1 << 5)

#define NFC_V3_FLASH_CMD		(host->regs_axi + 0x00)
#define NFC_V3_FLASH_ADDR0		(host->regs_axi + 0x04)

#define NFC_V3_CONFIG1			(host->regs_axi + 0x34)
#define NFC_V3_CONFIG1_SP_EN		(1 << 0)
#define NFC_V3_CONFIG1_RBA(x)		(((x) & 0x7 ) << 4)

#define NFC_V3_ECC_STATUS_RESULT	(host->regs_axi + 0x38)

#define NFC_V3_LAUNCH			(host->regs_axi + 0x40)

#define NFC_V3_WRPROT			(host->regs_ip + 0x0)
#define NFC_V3_WRPROT_LOCK_TIGHT	(1 << 0)
#define NFC_V3_WRPROT_LOCK		(1 << 1)
#define NFC_V3_WRPROT_UNLOCK		(1 << 2)
#define NFC_V3_WRPROT_BLS_UNLOCK	(2 << 6)

#define NFC_V3_WRPROT_UNLOCK_BLK_ADD0   (host->regs_ip + 0x04)

#define NFC_V3_CONFIG2			(host->regs_ip + 0x24)
#define NFC_V3_CONFIG2_PS_512			(0 << 0)
#define NFC_V3_CONFIG2_PS_2048			(1 << 0)
#define NFC_V3_CONFIG2_PS_4096			(2 << 0)
#define NFC_V3_CONFIG2_ONE_CYCLE		(1 << 2)
#define NFC_V3_CONFIG2_ECC_EN			(1 << 3)
#define NFC_V3_CONFIG2_2CMD_PHASES		(1 << 4)
#define NFC_V3_CONFIG2_NUM_ADDR_PHASE0		(1 << 5)
#define NFC_V3_CONFIG2_ECC_MODE_8		(1 << 6)
#define NFC_V3_CONFIG2_PPB(x, shift)		(((x) & 0x3) << shift)
#define NFC_V3_CONFIG2_NUM_ADDR_PHASE1(x)	(((x) & 0x3) << 12)
#define NFC_V3_CONFIG2_INT_MSK			(1 << 15)
#define NFC_V3_CONFIG2_ST_CMD(x)		(((x) & 0xff) << 24)
#define NFC_V3_CONFIG2_SPAS(x)			(((x) & 0xff) << 16)

#define NFC_V3_CONFIG3				(host->regs_ip + 0x28)
#define NFC_V3_CONFIG3_ADD_OP(x)		(((x) & 0x3) << 0)
#define NFC_V3_CONFIG3_FW8			(1 << 3)
#define NFC_V3_CONFIG3_SBB(x)			(((x) & 0x7) << 8)
#define NFC_V3_CONFIG3_NUM_OF_DEVICES(x)	(((x) & 0x7) << 12)
#define NFC_V3_CONFIG3_RBB_MODE			(1 << 15)
#define NFC_V3_CONFIG3_NO_SDMA			(1 << 20)

#define NFC_V3_IPC			(host->regs_ip + 0x2C)
#define NFC_V3_IPC_CREQ			(1 << 0)
#define NFC_V3_IPC_INT			(1 << 31)

#define NFC_V3_DELAY_LINE		(host->regs_ip + 0x34)

struct mxc_nand_host;

struct mxc_nand_devtype_data {
	void (*preset)(struct mtd_info *);
	int (*read_page)(struct nand_chip *chip);
	void (*send_cmd)(struct mxc_nand_host *, uint16_t, int);
	void (*send_addr)(struct mxc_nand_host *, uint16_t, int);
	void (*send_page)(struct mtd_info *, unsigned int);
	void (*send_read_id)(struct mxc_nand_host *);
	uint16_t (*get_dev_status)(struct mxc_nand_host *);
	int (*check_int)(struct mxc_nand_host *);
	void (*irq_control)(struct mxc_nand_host *, int);
	u32 (*get_ecc_status)(struct nand_chip *);
	const struct mtd_ooblayout_ops *ooblayout;
	void (*select_chip)(struct nand_chip *chip, int cs);
	int (*setup_interface)(struct nand_chip *chip, int csline,
			       const struct nand_interface_config *conf);
	void (*enable_hwecc)(struct nand_chip *chip, bool enable);

	/*
	 * On i.MX21 the CONFIG2:INT bit cannot be read if interrupts are masked
	 * (CONFIG1:INT_MSK is set). To handle this the driver uses
	 * enable_irq/disable_irq_nosync instead of CONFIG1:INT_MSK
	 */
	int irqpending_quirk;
	int needs_ip;

	size_t regs_offset;
	size_t spare0_offset;
	size_t axi_offset;

	int spare_len;
	int eccbytes;
	int eccsize;
	int ppb_shift;
};

struct mxc_nand_host {
	struct nand_chip	nand;
	struct device		*dev;

	void __iomem		*spare0;
	void __iomem		*main_area0;

	void __iomem		*base;
	void __iomem		*regs;
	void __iomem		*regs_axi;
	void __iomem		*regs_ip;
	int			status_request;
	struct clk		*clk;
	int			clk_act;
	int			irq;
	int			eccsize;
	int			used_oobsize;
	int			active_cs;
	unsigned int		ecc_stats_v1;

	struct completion	op_completion;

	void			*data_buf;

	const struct mxc_nand_devtype_data *devtype_data;
};

static void memcpy32_fromio(void *trg, const void __iomem  *src, size_t size)
{
	int i;
	u32 *t = trg;
	const __iomem u32 *s = src;

	for (i = 0; i < (size >> 2); i++)
		*t++ = __raw_readl(s++);
}

static void memcpy16_fromio(void *trg, const void __iomem  *src, size_t size)
{
	int i;
	u16 *t = trg;
	const __iomem u16 *s = src;

	/* We assume that src (IO) is always 32bit aligned */
	if (PTR_ALIGN(trg, 4) == trg && IS_ALIGNED(size, 4)) {
		memcpy32_fromio(trg, src, size);
		return;
	}

	for (i = 0; i < (size >> 1); i++)
		*t++ = __raw_readw(s++);
}

static inline void memcpy32_toio(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < (size >> 2); i++)
		__raw_writel(*s++, t++);
}

static void memcpy16_toio(void __iomem *trg, const void *src, int size)
{
	int i;
	__iomem u16 *t = trg;
	const u16 *s = src;

	/* We assume that trg (IO) is always 32bit aligned */
	if (PTR_ALIGN(src, 4) == src && IS_ALIGNED(size, 4)) {
		memcpy32_toio(trg, src, size);
		return;
	}

	for (i = 0; i < (size >> 1); i++)
		__raw_writew(*s++, t++);
}

/*
 * The controller splits a page into data chunks of 512 bytes + partial oob.
 * There are writesize / 512 such chunks, the size of the partial oob parts is
 * oobsize / #chunks rounded down to a multiple of 2. The last oob chunk then
 * contains additionally the byte lost by rounding (if any).
 * This function handles the needed shuffling between host->data_buf (which
 * holds a page in natural order, i.e. writesize bytes data + oobsize bytes
 * spare) and the NFC buffer.
 */
static void copy_spare(struct mtd_info *mtd, bool bfrom, void *buf)
{
	struct nand_chip *this = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(this);
	u16 i, oob_chunk_size;
	u16 num_chunks = mtd->writesize / 512;

	u8 *d = buf;
	u8 __iomem *s = host->spare0;
	u16 sparebuf_size = host->devtype_data->spare_len;

	/* size of oob chunk for all but possibly the last one */
	oob_chunk_size = (host->used_oobsize / num_chunks) & ~1;

	if (bfrom) {
		for (i = 0; i < num_chunks - 1; i++)
			memcpy16_fromio(d + i * oob_chunk_size,
					s + i * sparebuf_size,
					oob_chunk_size);

		/* the last chunk */
		memcpy16_fromio(d + i * oob_chunk_size,
				s + i * sparebuf_size,
				host->used_oobsize - i * oob_chunk_size);
	} else {
		for (i = 0; i < num_chunks - 1; i++)
			memcpy16_toio(&s[i * sparebuf_size],
				      &d[i * oob_chunk_size],
				      oob_chunk_size);

		/* the last chunk */
		memcpy16_toio(&s[i * sparebuf_size],
			      &d[i * oob_chunk_size],
			      host->used_oobsize - i * oob_chunk_size);
	}
}

static int check_int_v3(struct mxc_nand_host *host)
{
	uint32_t tmp;

	tmp = readl(NFC_V3_IPC);
	if (!(tmp & NFC_V3_IPC_INT))
		return 0;

	tmp &= ~NFC_V3_IPC_INT;
	writel(tmp, NFC_V3_IPC);

	return 1;
}

static int check_int_v1_v2(struct mxc_nand_host *host)
{
	uint32_t tmp;

	tmp = readw(NFC_V1_V2_CONFIG2);
	if (!(tmp & NFC_V1_V2_CONFIG2_INT))
		return 0;

	if (!host->devtype_data->irqpending_quirk)
		writew(tmp & ~NFC_V1_V2_CONFIG2_INT, NFC_V1_V2_CONFIG2);

	return 1;
}

static void irq_control_v1_v2(struct mxc_nand_host *host, int activate)
{
	uint16_t tmp;

	tmp = readw(NFC_V1_V2_CONFIG1);

	if (activate)
		tmp &= ~NFC_V1_V2_CONFIG1_INT_MSK;
	else
		tmp |= NFC_V1_V2_CONFIG1_INT_MSK;

	writew(tmp, NFC_V1_V2_CONFIG1);
}

static void irq_control_v3(struct mxc_nand_host *host, int activate)
{
	uint32_t tmp;

	tmp = readl(NFC_V3_CONFIG2);

	if (activate)
		tmp &= ~NFC_V3_CONFIG2_INT_MSK;
	else
		tmp |= NFC_V3_CONFIG2_INT_MSK;

	writel(tmp, NFC_V3_CONFIG2);
}

static u32 get_ecc_status_v1(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	unsigned int ecc_stats, max_bitflips = 0;
	int no_subpages, i;

	no_subpages = mtd->writesize >> 9;

	ecc_stats = host->ecc_stats_v1;

	for (i = 0; i < no_subpages; i++) {
		switch (ecc_stats & 0x3) {
		case 0:
		default:
			break;
		case 1:
			mtd->ecc_stats.corrected++;
			max_bitflips = 1;
			break;
		case 2:
			mtd->ecc_stats.failed++;
			break;
		}

		ecc_stats >>= 2;
	}

	return max_bitflips;
}

static u32 get_ecc_status_v2_v3(struct nand_chip *chip, unsigned int ecc_stat)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	u8 ecc_bit_mask, err_limit;
	unsigned int max_bitflips = 0;
	int no_subpages, err;

	ecc_bit_mask = (host->eccsize == 4) ? 0x7 : 0xf;
	err_limit = (host->eccsize == 4) ? 0x4 : 0x8;

	no_subpages = mtd->writesize >> 9;

	do {
		err = ecc_stat & ecc_bit_mask;
		if (err > err_limit) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += err;
			max_bitflips = max_t(unsigned int, max_bitflips, err);
		}

		ecc_stat >>= 4;
	} while (--no_subpages);

	return max_bitflips;
}

static u32 get_ecc_status_v2(struct nand_chip *chip)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);

	u32 ecc_stat = readl(NFC_V1_V2_ECC_STATUS_RESULT);

	return get_ecc_status_v2_v3(chip, ecc_stat);
}

static u32 get_ecc_status_v3(struct nand_chip *chip)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);

	u32 ecc_stat = readl(NFC_V3_ECC_STATUS_RESULT);

	return get_ecc_status_v2_v3(chip, ecc_stat);
}

/* This function polls the NANDFC to wait for the basic operation to
 * complete by checking the INT bit of config2 register.
 */
static int wait_op_done(struct mxc_nand_host *host, int useirq)
{
	int ret = 0;
	int max_retries = 8000;
	int done;

	/*
	 * If operation is already complete, don't bother to setup an irq or a
	 * loop.
	 */
	if (host->devtype_data->check_int(host))
		return 0;

	do {
		udelay(1);

		done = host->devtype_data->check_int(host);
		if (done)
			break;

	} while (--max_retries);

	if (!done) {
		dev_dbg(host->dev, "timeout polling for completion\n");
		ret = -ETIMEDOUT;
	}

	WARN_ONCE(ret < 0, "timeout! useirq=%d\n", useirq);

	return ret;
}

static void send_cmd_v3(struct mxc_nand_host *host, uint16_t cmd, int useirq)
{
	/* fill command */
	writel(cmd, NFC_V3_FLASH_CMD);

	/* send out command */
	writel(NFC_CMD, NFC_V3_LAUNCH);

	/* Wait for operation to complete */
	wait_op_done(host, useirq);
}

/* This function issues the specified command to the NAND device and
 * waits for completion. */
static void send_cmd_v1_v2(struct mxc_nand_host *host, uint16_t cmd, int useirq)
{
	dev_dbg(host->dev, "send_cmd(host, 0x%x, %d)\n", cmd, useirq);

	writew(cmd, NFC_V1_V2_FLASH_CMD);
	writew(NFC_CMD, NFC_V1_V2_CONFIG2);

	if (host->devtype_data->irqpending_quirk && (cmd == NAND_CMD_RESET)) {
		int max_retries = 100;
		/* Reset completion is indicated by NFC_CONFIG2 */
		/* being set to 0 */
		while (max_retries-- > 0) {
			if (readw(NFC_V1_V2_CONFIG2) == 0) {
				break;
			}
			udelay(1);
		}
		if (max_retries < 0)
			dev_dbg(host->dev, "%s: RESET failed\n", __func__);
	} else {
		/* Wait for operation to complete */
		wait_op_done(host, useirq);
	}
}

static void send_addr_v3(struct mxc_nand_host *host, uint16_t addr, int islast)
{
	/* fill address */
	writel(addr, NFC_V3_FLASH_ADDR0);

	/* send out address */
	writel(NFC_ADDR, NFC_V3_LAUNCH);

	wait_op_done(host, 0);
}

/* This function sends an address (or partial address) to the
 * NAND device. The address is used to select the source/destination for
 * a NAND command. */
static void send_addr_v1_v2(struct mxc_nand_host *host, uint16_t addr, int islast)
{
	dev_dbg(host->dev, "send_addr(host, 0x%x %d)\n", addr, islast);

	writew(addr, NFC_V1_V2_FLASH_ADDR);
	writew(NFC_ADDR, NFC_V1_V2_CONFIG2);

	/* Wait for operation to complete */
	wait_op_done(host, islast);
}

static void send_page_v3(struct mtd_info *mtd, unsigned int ops)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);
	uint32_t tmp;

	tmp = readl(NFC_V3_CONFIG1);
	tmp &= ~(7 << 4);
	writel(tmp, NFC_V3_CONFIG1);

	/* transfer data from NFC ram to nand */
	writel(ops, NFC_V3_LAUNCH);

	wait_op_done(host, false);
}

static void send_page_v2(struct mtd_info *mtd, unsigned int ops)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);

	/* NANDFC buffer 0 is used for page read/write */
	writew(host->active_cs << 4, NFC_V1_V2_BUF_ADDR);

	writew(ops, NFC_V1_V2_CONFIG2);

	/* Wait for operation to complete */
	wait_op_done(host, true);
}

static void send_page_v1(struct mtd_info *mtd, unsigned int ops)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);
	int bufs, i;

	if (mtd->writesize > 512)
		bufs = 4;
	else
		bufs = 1;

	for (i = 0; i < bufs; i++) {

		/* NANDFC buffer 0 is used for page read/write */
		writew((host->active_cs << 4) | i, NFC_V1_V2_BUF_ADDR);

		writew(ops, NFC_V1_V2_CONFIG2);

		/* Wait for operation to complete */
		wait_op_done(host, true);
	}
}

static void send_read_id_v3(struct mxc_nand_host *host)
{
	/* Read ID into main buffer */
	writel(NFC_ID, NFC_V3_LAUNCH);

	wait_op_done(host, true);

	memcpy32_fromio(host->data_buf, host->main_area0, 16);
}

/* Request the NANDFC to perform a read of the NAND device ID. */
static void send_read_id_v1_v2(struct mxc_nand_host *host)
{
	/* NANDFC buffer 0 is used for device ID output */
	writew(host->active_cs << 4, NFC_V1_V2_BUF_ADDR);

	writew(NFC_ID, NFC_V1_V2_CONFIG2);

	/* Wait for operation to complete */
	wait_op_done(host, true);

	memcpy32_fromio(host->data_buf, host->main_area0, 16);
}

static uint16_t get_dev_status_v3(struct mxc_nand_host *host)
{
	writew(NFC_STATUS, NFC_V3_LAUNCH);
	wait_op_done(host, true);

	return readl(NFC_V3_CONFIG1) >> 16;
}

/* This function requests the NANDFC to perform a read of the
 * NAND device status and returns the current status. */
static uint16_t get_dev_status_v1_v2(struct mxc_nand_host *host)
{
	void __iomem *main_buf = host->main_area0;
	uint32_t store;
	uint16_t ret;

	writew(host->active_cs << 4, NFC_V1_V2_BUF_ADDR);

	/*
	 * The device status is stored in main_area0. To
	 * prevent corruption of the buffer save the value
	 * and restore it afterwards.
	 */
	store = readl(main_buf);

	writew(NFC_STATUS, NFC_V1_V2_CONFIG2);
	wait_op_done(host, true);

	ret = readw(main_buf);

	writel(store, main_buf);

	return ret;
}

static void mxc_nand_enable_hwecc_v1_v2(struct nand_chip *chip, bool enable)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	uint16_t config1;

	if (chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_HOST)
		return;

	config1 = readw(NFC_V1_V2_CONFIG1);

	if (enable)
		config1 |= NFC_V1_V2_CONFIG1_ECC_EN;
	else
		config1 &= ~NFC_V1_V2_CONFIG1_ECC_EN;

	writew(config1, NFC_V1_V2_CONFIG1);
}

static void mxc_nand_enable_hwecc_v3(struct nand_chip *chip, bool enable)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	uint32_t config2;

	if (chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_HOST)
		return;

	config2 = readl(NFC_V3_CONFIG2);

	if (enable)
		config2 |= NFC_V3_CONFIG2_ECC_EN;
	else
		config2 &= ~NFC_V3_CONFIG2_ECC_EN;

	writel(config2, NFC_V3_CONFIG2);
}

static int mxc_nand_read_page_v1(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	int no_subpages;
	int i;
	unsigned int ecc_stats = 0;

	if (mtd->writesize)
		no_subpages = mtd->writesize >> 9;
	else
		/* READ PARAMETER PAGE is called when mtd->writesize is not yet set */
		no_subpages = 1;

	for (i = 0; i < no_subpages; i++) {
		/* NANDFC buffer 0 is used for page read/write */
		writew((host->active_cs << 4) | i, NFC_V1_V2_BUF_ADDR);

		writew(NFC_OUTPUT, NFC_V1_V2_CONFIG2);

		/* Wait for operation to complete */
		wait_op_done(host, true);

		ecc_stats |= FIELD_GET(NFC_V1_V2_ECC_STATUS_RESULT_ERM,
				       readw(NFC_V1_V2_ECC_STATUS_RESULT)) << i * 2;
	}

	host->ecc_stats_v1 = ecc_stats;

	return 0;
}

static int mxc_nand_read_page_v2_v3(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);

	host->devtype_data->send_page(mtd, NFC_OUTPUT);

	return 0;
}

static int mxc_nand_read_page(struct nand_chip *chip, uint8_t *buf,
			      int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	int ret;

	host->devtype_data->enable_hwecc(chip, true);

	ret = nand_read_page_op(chip, page, 0, buf, mtd->writesize);

	host->devtype_data->enable_hwecc(chip, false);

	if (ret)
		return ret;

	if (oob_required)
		copy_spare(mtd, true, chip->oob_poi);

	return host->devtype_data->get_ecc_status(chip);
}

static int mxc_nand_read_page_raw(struct nand_chip *chip, uint8_t *buf,
				  int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret;

	ret = nand_read_page_op(chip, page, 0, buf, mtd->writesize);
	if (ret)
		return ret;

	if (oob_required)
		copy_spare(mtd, true, chip->oob_poi);

	return 0;
}

static int mxc_nand_read_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	int ret;

	ret = nand_read_page_op(chip, page, 0, host->data_buf, mtd->writesize);
	if (ret)
		return ret;

	copy_spare(mtd, true, chip->oob_poi);

	return 0;
}

static int mxc_nand_write_page_ecc(struct nand_chip *chip, const uint8_t *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	int ret;

	copy_spare(mtd, false, chip->oob_poi);

	host->devtype_data->enable_hwecc(chip, true);

	ret = nand_prog_page_op(chip, page, 0, buf, mtd->writesize);

	host->devtype_data->enable_hwecc(chip, false);

	return ret;
}

static int mxc_nand_write_page_raw(struct nand_chip *chip, const uint8_t *buf,
				   int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	copy_spare(mtd, false, chip->oob_poi);

	return nand_prog_page_op(chip, page, 0, buf, mtd->writesize);
}

static int mxc_nand_write_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);

	memset(host->data_buf, 0xff, mtd->writesize);
	copy_spare(mtd, false, chip->oob_poi);

	return nand_prog_page_op(chip, page, 0, host->data_buf, mtd->writesize);
}

/* This function is used by upper layer for select and
 * deselect of the NAND chip */
static void mxc_nand_select_chip_v1_v3(struct nand_chip *nand_chip, int chip)
{
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);

	if (chip == -1) {
		/* Disable the NFC clock */
		if (host->clk_act) {
			clk_disable_unprepare(host->clk);
			host->clk_act = 0;
		}
		return;
	}

	if (!host->clk_act) {
		/* Enable the NFC clock */
		clk_prepare_enable(host->clk);
		host->clk_act = 1;
	}
}

static void mxc_nand_select_chip_v2(struct nand_chip *nand_chip, int chip)
{
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);

	if (chip == -1) {
		/* Disable the NFC clock */
		if (host->clk_act) {
			clk_disable_unprepare(host->clk);
			host->clk_act = 0;
		}
		return;
	}

	if (!host->clk_act) {
		/* Enable the NFC clock */
		clk_prepare_enable(host->clk);
		host->clk_act = 1;
	}

	host->active_cs = chip;
	writew(host->active_cs << 4, NFC_V1_V2_BUF_ADDR);
}

#define MXC_V1_ECCBYTES		5

static int mxc_v1_ooblayout_ecc(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);

	if (section >= nand_chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * 16) + 6;
	oobregion->length = MXC_V1_ECCBYTES;

	return 0;
}

static int mxc_v1_ooblayout_free(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);

	if (section > nand_chip->ecc.steps)
		return -ERANGE;

	if (!section) {
		if (mtd->writesize <= 512) {
			oobregion->offset = 0;
			oobregion->length = 5;
		} else {
			oobregion->offset = 2;
			oobregion->length = 4;
		}
	} else {
		oobregion->offset = ((section - 1) * 16) + MXC_V1_ECCBYTES + 6;
		if (section < nand_chip->ecc.steps)
			oobregion->length = (section * 16) + 6 -
					    oobregion->offset;
		else
			oobregion->length = mtd->oobsize - oobregion->offset;
	}

	return 0;
}

static const struct mtd_ooblayout_ops mxc_v1_ooblayout_ops = {
	.ecc = mxc_v1_ooblayout_ecc,
	.free = mxc_v1_ooblayout_free,
};

static int mxc_v2_ooblayout_ecc(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	int stepsize = nand_chip->ecc.bytes == 9 ? 16 : 26;

	if (section >= nand_chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * stepsize) + 7;
	oobregion->length = nand_chip->ecc.bytes;

	return 0;
}

static int mxc_v2_ooblayout_free(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	int stepsize = nand_chip->ecc.bytes == 9 ? 16 : 26;

	if (section >= nand_chip->ecc.steps)
		return -ERANGE;

	if (!section) {
		if (mtd->writesize <= 512) {
			oobregion->offset = 0;
			oobregion->length = 5;
		} else {
			oobregion->offset = 2;
			oobregion->length = 4;
		}
	} else {
		oobregion->offset = section * stepsize;
		oobregion->length = 7;
	}

	return 0;
}

static const struct mtd_ooblayout_ops mxc_v2_ooblayout_ops = {
	.ecc = mxc_v2_ooblayout_ecc,
	.free = mxc_v2_ooblayout_free,
};

/*
 * v2 and v3 type controllers can do 4bit or 8bit ecc depending
 * on how much oob the nand chip has. For 8bit ecc we need at least
 * 26 bytes of oob data per 512 byte block.
 */
static int get_eccsize(struct mtd_info *mtd)
{
	int oobbytes_per_512 = 0;

	oobbytes_per_512 = mtd->oobsize * 512 / mtd->writesize;

	if (oobbytes_per_512 < 26)
		return 4;
	else
		return 8;
}

static void preset_v1(struct mtd_info *mtd)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);
	uint16_t config1 = 0;

	if (nand_chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST &&
	    mtd->writesize)
		config1 |= NFC_V1_V2_CONFIG1_ECC_EN;

	if (!host->devtype_data->irqpending_quirk)
		config1 |= NFC_V1_V2_CONFIG1_INT_MSK;

	host->eccsize = 1;

	writew(config1, NFC_V1_V2_CONFIG1);
	/* preset operation */

	/* Unlock the internal RAM Buffer */
	writew(0x2, NFC_V1_V2_CONFIG);

	/* Blocks to be unlocked */
	writew(0x0, NFC_V1_UNLOCKSTART_BLKADDR);
	writew(0xffff, NFC_V1_UNLOCKEND_BLKADDR);

	/* Unlock Block Command for given address range */
	writew(0x4, NFC_V1_V2_WRPROT);
}

static int mxc_nand_v2_setup_interface(struct nand_chip *chip, int csline,
				       const struct nand_interface_config *conf)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	int tRC_min_ns, tRC_ps, ret;
	unsigned long rate, rate_round;
	const struct nand_sdr_timings *timings;
	u16 config1;

	timings = nand_get_sdr_timings(conf);
	if (IS_ERR(timings))
		return -ENOTSUPP;

	config1 = readw(NFC_V1_V2_CONFIG1);

	tRC_min_ns = timings->tRC_min / 1000;
	rate = 1000000000 / tRC_min_ns;

	/*
	 * For tRC < 30ns we have to use EDO mode. In this case the controller
	 * does one access per clock cycle. Otherwise the controller does one
	 * access in two clock cycles, thus we have to double the rate to the
	 * controller.
	 */
	if (tRC_min_ns < 30) {
		rate_round = clk_round_rate(host->clk, rate);
		config1 |= NFC_V2_CONFIG1_ONE_CYCLE;
		tRC_ps = 1000000000 / (rate_round / 1000);
	} else {
		rate *= 2;
		rate_round = clk_round_rate(host->clk, rate);
		config1 &= ~NFC_V2_CONFIG1_ONE_CYCLE;
		tRC_ps = 1000000000 / (rate_round / 1000 / 2);
	}

	/*
	 * The timing values compared against are from the i.MX25 Automotive
	 * datasheet, Table 50. NFC Timing Parameters
	 */
	if (timings->tCLS_min > tRC_ps - 1000 ||
	    timings->tCLH_min > tRC_ps - 2000 ||
	    timings->tCS_min > tRC_ps - 1000 ||
	    timings->tCH_min > tRC_ps - 2000 ||
	    timings->tWP_min > tRC_ps - 1500 ||
	    timings->tALS_min > tRC_ps ||
	    timings->tALH_min > tRC_ps - 3000 ||
	    timings->tDS_min > tRC_ps ||
	    timings->tDH_min > tRC_ps - 5000 ||
	    timings->tWC_min > 2 * tRC_ps ||
	    timings->tWH_min > tRC_ps - 2500 ||
	    timings->tRR_min > 6 * tRC_ps ||
	    timings->tRP_min > 3 * tRC_ps / 2 ||
	    timings->tRC_min > 2 * tRC_ps ||
	    timings->tREH_min > (tRC_ps / 2) - 2500) {
		dev_dbg(host->dev, "Timing out of bounds\n");
		return -EINVAL;
	}

	if (csline == NAND_DATA_IFACE_CHECK_ONLY)
		return 0;

	ret = clk_set_rate(host->clk, rate);
	if (ret)
		return ret;

	writew(config1, NFC_V1_V2_CONFIG1);

	dev_dbg(host->dev, "Setting rate to %ldHz, %s mode\n", rate_round,
		config1 & NFC_V2_CONFIG1_ONE_CYCLE ? "One cycle (EDO)" :
		"normal");

	return 0;
}

static void preset_v2(struct mtd_info *mtd)
{
	struct nand_chip *nand_chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(nand_chip);
	uint16_t config1 = 0;

	config1 |= NFC_V2_CONFIG1_FP_INT;

	if (!host->devtype_data->irqpending_quirk)
		config1 |= NFC_V1_V2_CONFIG1_INT_MSK;

	if (mtd->writesize) {
		uint16_t pages_per_block = mtd->erasesize / mtd->writesize;

		if (nand_chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST)
			config1 |= NFC_V1_V2_CONFIG1_ECC_EN;

		host->eccsize = get_eccsize(mtd);
		if (host->eccsize == 4)
			config1 |= NFC_V2_CONFIG1_ECC_MODE_4;

		config1 |= NFC_V2_CONFIG1_PPB(ffs(pages_per_block) - 6);
	} else {
		host->eccsize = 1;
	}

	writew(config1, NFC_V1_V2_CONFIG1);
	/* preset operation */

	/* spare area size in 16-bit half-words */
	writew(mtd->oobsize / 2, NFC_V21_RSLTSPARE_AREA);

	/* Unlock the internal RAM Buffer */
	writew(0x2, NFC_V1_V2_CONFIG);

	/* Blocks to be unlocked */
	writew(0x0, NFC_V21_UNLOCKSTART_BLKADDR0);
	writew(0x0, NFC_V21_UNLOCKSTART_BLKADDR1);
	writew(0x0, NFC_V21_UNLOCKSTART_BLKADDR2);
	writew(0x0, NFC_V21_UNLOCKSTART_BLKADDR3);
	writew(0xffff, NFC_V21_UNLOCKEND_BLKADDR0);
	writew(0xffff, NFC_V21_UNLOCKEND_BLKADDR1);
	writew(0xffff, NFC_V21_UNLOCKEND_BLKADDR2);
	writew(0xffff, NFC_V21_UNLOCKEND_BLKADDR3);

	/* Unlock Block Command for given address range */
	writew(0x4, NFC_V1_V2_WRPROT);
}

static void preset_v3(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	uint32_t config2, config3;
	int i, addr_phases;

	writel(NFC_V3_CONFIG1_RBA(0), NFC_V3_CONFIG1);
	writel(NFC_V3_IPC_CREQ, NFC_V3_IPC);

	/* Unlock the internal RAM Buffer */
	writel(NFC_V3_WRPROT_BLS_UNLOCK | NFC_V3_WRPROT_UNLOCK,
			NFC_V3_WRPROT);

	/* Blocks to be unlocked */
	for (i = 0; i < NAND_MAX_CHIPS; i++)
		writel(0xffff << 16, NFC_V3_WRPROT_UNLOCK_BLK_ADD0 + (i << 2));

	writel(0, NFC_V3_IPC);

	config2 = NFC_V3_CONFIG2_ONE_CYCLE |
		NFC_V3_CONFIG2_2CMD_PHASES |
		NFC_V3_CONFIG2_SPAS(mtd->oobsize >> 1) |
		NFC_V3_CONFIG2_ST_CMD(0x70) |
		NFC_V3_CONFIG2_INT_MSK |
		NFC_V3_CONFIG2_NUM_ADDR_PHASE0;

	addr_phases = fls(chip->pagemask) >> 3;

	if (mtd->writesize == 2048) {
		config2 |= NFC_V3_CONFIG2_PS_2048;
		config2 |= NFC_V3_CONFIG2_NUM_ADDR_PHASE1(addr_phases);
	} else if (mtd->writesize == 4096) {
		config2 |= NFC_V3_CONFIG2_PS_4096;
		config2 |= NFC_V3_CONFIG2_NUM_ADDR_PHASE1(addr_phases);
	} else {
		config2 |= NFC_V3_CONFIG2_PS_512;
		config2 |= NFC_V3_CONFIG2_NUM_ADDR_PHASE1(addr_phases - 1);
	}

	if (mtd->writesize) {
		if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST)
			config2 |= NFC_V3_CONFIG2_ECC_EN;

		config2 |= NFC_V3_CONFIG2_PPB(
				ffs(mtd->erasesize / mtd->writesize) - 6,
				host->devtype_data->ppb_shift);
		host->eccsize = get_eccsize(mtd);
		if (host->eccsize == 8)
			config2 |= NFC_V3_CONFIG2_ECC_MODE_8;
	}

	writel(config2, NFC_V3_CONFIG2);

	config3 = NFC_V3_CONFIG3_NUM_OF_DEVICES(0) |
			NFC_V3_CONFIG3_NO_SDMA |
			NFC_V3_CONFIG3_RBB_MODE |
			NFC_V3_CONFIG3_SBB(6) | /* Reset default */
			NFC_V3_CONFIG3_ADD_OP(0);

	if (!(chip->options & NAND_BUSWIDTH_16))
		config3 |= NFC_V3_CONFIG3_FW8;

	writel(config3, NFC_V3_CONFIG3);

	writel(0, NFC_V3_DELAY_LINE);
}

/*
 * The generic flash bbt descriptors overlap with our ecc
 * hardware, so define some i.MX specific ones.
 */
static uint8_t bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};

/* v1 + irqpending_quirk: i.MX21 */
static const struct mxc_nand_devtype_data imx21_nand_devtype_data = {
	.preset = preset_v1,
	.read_page = mxc_nand_read_page_v1,
	.send_cmd = send_cmd_v1_v2,
	.send_addr = send_addr_v1_v2,
	.send_page = send_page_v1,
	.send_read_id = send_read_id_v1_v2,
	.get_dev_status = get_dev_status_v1_v2,
	.check_int = check_int_v1_v2,
	.irq_control = irq_control_v1_v2,
	.get_ecc_status = get_ecc_status_v1,
	.ooblayout = &mxc_v1_ooblayout_ops,
	.select_chip = mxc_nand_select_chip_v1_v3,
	.enable_hwecc = mxc_nand_enable_hwecc_v1_v2,
	.irqpending_quirk = 1,
	.needs_ip = 0,
	.regs_offset = 0xe00,
	.spare0_offset = 0x800,
	.spare_len = 16,
	.eccbytes = 3,
	.eccsize = 1,
};

/* v1 + !irqpending_quirk: i.MX27, i.MX31 */
static const struct mxc_nand_devtype_data imx27_nand_devtype_data = {
	.preset = preset_v1,
	.read_page = mxc_nand_read_page_v1,
	.send_cmd = send_cmd_v1_v2,
	.send_addr = send_addr_v1_v2,
	.send_page = send_page_v1,
	.send_read_id = send_read_id_v1_v2,
	.get_dev_status = get_dev_status_v1_v2,
	.check_int = check_int_v1_v2,
	.irq_control = irq_control_v1_v2,
	.get_ecc_status = get_ecc_status_v1,
	.ooblayout = &mxc_v1_ooblayout_ops,
	.select_chip = mxc_nand_select_chip_v1_v3,
	.enable_hwecc = mxc_nand_enable_hwecc_v1_v2,
	.irqpending_quirk = 0,
	.needs_ip = 0,
	.regs_offset = 0xe00,
	.spare0_offset = 0x800,
	.axi_offset = 0,
	.spare_len = 16,
	.eccbytes = 3,
	.eccsize = 1,
};

/* v21: i.MX25, i.MX35 */
static const struct mxc_nand_devtype_data imx25_nand_devtype_data = {
	.preset = preset_v2,
	.read_page = mxc_nand_read_page_v2_v3,
	.send_cmd = send_cmd_v1_v2,
	.send_addr = send_addr_v1_v2,
	.send_page = send_page_v2,
	.send_read_id = send_read_id_v1_v2,
	.get_dev_status = get_dev_status_v1_v2,
	.check_int = check_int_v1_v2,
	.irq_control = irq_control_v1_v2,
	.get_ecc_status = get_ecc_status_v2,
	.ooblayout = &mxc_v2_ooblayout_ops,
	.select_chip = mxc_nand_select_chip_v2,
	.setup_interface = mxc_nand_v2_setup_interface,
	.enable_hwecc = mxc_nand_enable_hwecc_v1_v2,
	.irqpending_quirk = 0,
	.needs_ip = 0,
	.regs_offset = 0x1e00,
	.spare0_offset = 0x1000,
	.axi_offset = 0,
	.spare_len = 64,
	.eccbytes = 9,
	.eccsize = 0,
};

/* v3.2a: i.MX51 */
static const struct mxc_nand_devtype_data imx51_nand_devtype_data = {
	.preset = preset_v3,
	.read_page = mxc_nand_read_page_v2_v3,
	.send_cmd = send_cmd_v3,
	.send_addr = send_addr_v3,
	.send_page = send_page_v3,
	.send_read_id = send_read_id_v3,
	.get_dev_status = get_dev_status_v3,
	.check_int = check_int_v3,
	.irq_control = irq_control_v3,
	.get_ecc_status = get_ecc_status_v3,
	.ooblayout = &mxc_v2_ooblayout_ops,
	.select_chip = mxc_nand_select_chip_v1_v3,
	.enable_hwecc = mxc_nand_enable_hwecc_v3,
	.irqpending_quirk = 0,
	.needs_ip = 1,
	.regs_offset = 0,
	.spare0_offset = 0x1000,
	.axi_offset = 0x1e00,
	.spare_len = 64,
	.eccbytes = 0,
	.eccsize = 0,
	.ppb_shift = 7,
};

/* v3.2b: i.MX53 */
static const struct mxc_nand_devtype_data imx53_nand_devtype_data = {
	.preset = preset_v3,
	.read_page = mxc_nand_read_page_v2_v3,
	.send_cmd = send_cmd_v3,
	.send_addr = send_addr_v3,
	.send_page = send_page_v3,
	.send_read_id = send_read_id_v3,
	.get_dev_status = get_dev_status_v3,
	.check_int = check_int_v3,
	.irq_control = irq_control_v3,
	.get_ecc_status = get_ecc_status_v3,
	.ooblayout = &mxc_v2_ooblayout_ops,
	.select_chip = mxc_nand_select_chip_v1_v3,
	.enable_hwecc = mxc_nand_enable_hwecc_v3,
	.irqpending_quirk = 0,
	.needs_ip = 1,
	.regs_offset = 0,
	.spare0_offset = 0x1000,
	.axi_offset = 0x1e00,
	.spare_len = 64,
	.eccbytes = 0,
	.eccsize = 0,
	.ppb_shift = 8,
};

static inline int is_imx21_nfc(struct mxc_nand_host *host)
{
	return host->devtype_data == &imx21_nand_devtype_data;
}

static inline int is_imx27_nfc(struct mxc_nand_host *host)
{
	return host->devtype_data == &imx27_nand_devtype_data;
}

static inline int is_imx25_nfc(struct mxc_nand_host *host)
{
	return host->devtype_data == &imx25_nand_devtype_data;
}

static const struct of_device_id mxcnd_dt_ids[] = {
	{ .compatible = "fsl,imx21-nand", .data = &imx21_nand_devtype_data, },
	{ .compatible = "fsl,imx27-nand", .data = &imx27_nand_devtype_data, },
	{ .compatible = "fsl,imx25-nand", .data = &imx25_nand_devtype_data, },
	{ .compatible = "fsl,imx51-nand", .data = &imx51_nand_devtype_data, },
	{ .compatible = "fsl,imx53-nand", .data = &imx53_nand_devtype_data, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxcnd_dt_ids);

static int mxcnd_attach_chip(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	struct device *dev = mtd->dev.parent;

	chip->ecc.bytes = host->devtype_data->eccbytes;
	host->eccsize = host->devtype_data->eccsize;
	chip->ecc.size = 512;

	switch (chip->ecc.engine_type) {
	case NAND_ECC_ENGINE_TYPE_ON_HOST:
		mtd_set_ooblayout(mtd, host->devtype_data->ooblayout);
		chip->ecc.read_page = mxc_nand_read_page;
		chip->ecc.read_page_raw = mxc_nand_read_page_raw;
		chip->ecc.read_oob = mxc_nand_read_oob;
		chip->ecc.write_page = mxc_nand_write_page_ecc;
		chip->ecc.write_page_raw = mxc_nand_write_page_raw;
		chip->ecc.write_oob = mxc_nand_write_oob;
		break;

	case NAND_ECC_ENGINE_TYPE_SOFT:
		chip->ecc.write_page_raw = nand_monolithic_write_page_raw;
		chip->ecc.read_page_raw = nand_monolithic_read_page_raw;
		break;

	default:
		return -EINVAL;
	}

	if (chip->bbt_options & NAND_BBT_USE_FLASH) {
		chip->bbt_td = &bbt_main_descr;
		chip->bbt_md = &bbt_mirror_descr;
	}

	/* Allocate the right size buffer now */
	devm_kfree(dev, (void *)host->data_buf);
	host->data_buf = devm_kzalloc(dev, mtd->writesize + mtd->oobsize,
				      GFP_KERNEL);
	if (!host->data_buf)
		return -ENOMEM;

	/* Call preset again, with correct writesize chip time */
	host->devtype_data->preset(mtd);

	if (!chip->ecc.bytes) {
		if (host->eccsize == 8)
			chip->ecc.bytes = 18;
		else if (host->eccsize == 4)
			chip->ecc.bytes = 9;
	}

	/*
	 * Experimentation shows that i.MX NFC can only handle up to 218 oob
	 * bytes. Limit used_oobsize to 218 so as to not confuse copy_spare()
	 * into copying invalid data to/from the spare IO buffer, as this
	 * might cause ECC data corruption when doing sub-page write to a
	 * partially written page.
	 */
	host->used_oobsize = min(mtd->oobsize, 218U);

	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST) {
		if (is_imx21_nfc(host) || is_imx27_nfc(host))
			chip->ecc.strength = 1;
		else
			chip->ecc.strength = (host->eccsize == 4) ? 4 : 8;
	}

	return 0;
}

static int mxcnd_setup_interface(struct nand_chip *chip, int chipnr,
				 const struct nand_interface_config *conf)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);

	return host->devtype_data->setup_interface(chip, chipnr, conf);
}

static void memff16_toio(void *buf, int n)
{
	__iomem u16 *t = buf;
	int i;

	for (i = 0; i < (n >> 1); i++)
		__raw_writew(0xffff, t++);
}

static void copy_page_to_sram(struct mtd_info *mtd, const void *buf, int buf_len)
{
	struct nand_chip *this = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(this);
	unsigned int no_subpages = mtd->writesize / 512;
	int oob_per_subpage, i;

	oob_per_subpage = (mtd->oobsize / no_subpages) & ~1;

	/*
	 * During a page write the i.MX NAND controller will read 512b from
	 * main_area0 SRAM, then oob_per_subpage bytes from spare0 SRAM, then
	 * 512b from main_area1 SRAM and so on until the full page is written.
	 * For software ECC we want to have a 1:1 mapping between the raw page
	 * data on the NAND chip and the view of the NAND core. This is
	 * necessary to make the NAND_CMD_RNDOUT read the data it expects.
	 * To accomplish this we have to write the data in the order the controller
	 * reads it. This is reversed in copy_page_from_sram() below.
	 *
	 * buf_len can either be the full page including the OOB or user data only.
	 * When it's user data only make sure that we fill up the rest of the
	 * SRAM with 0xff.
	 */
	for (i = 0; i < no_subpages; i++) {
		int now = min(buf_len, 512);

		if (now)
			memcpy16_toio(host->main_area0 + i * 512, buf, now);

		if (now < 512)
			memff16_toio(host->main_area0 + i * 512 + now, 512 - now);

		buf += 512;
		buf_len -= now;

		now = min(buf_len, oob_per_subpage);
		if (now)
			memcpy16_toio(host->spare0 + i * host->devtype_data->spare_len,
				      buf, now);

		if (now < oob_per_subpage)
			memff16_toio(host->spare0 + i * host->devtype_data->spare_len + now,
				     oob_per_subpage - now);

		buf += oob_per_subpage;
		buf_len -= now;
	}
}

static void copy_page_from_sram(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd_to_nand(mtd);
	struct mxc_nand_host *host = nand_get_controller_data(this);
	void *buf = host->data_buf;
	unsigned int no_subpages = mtd->writesize / 512;
	int oob_per_subpage, i;

	/* mtd->writesize is not set during ident scanning */
	if (!no_subpages)
		no_subpages = 1;

	oob_per_subpage = (mtd->oobsize / no_subpages) & ~1;

	for (i = 0; i < no_subpages; i++) {
		memcpy16_fromio(buf, host->main_area0 + i * 512, 512);
		buf += 512;

		memcpy16_fromio(buf, host->spare0 + i * host->devtype_data->spare_len,
				oob_per_subpage);
		buf += oob_per_subpage;
	}
}

static int mxcnd_do_exec_op(struct nand_chip *chip,
			    const struct nand_subop *op)
{
	struct mxc_nand_host *host = nand_get_controller_data(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	int i, j, buf_len;
	void *buf_read = NULL;
	const void *buf_write = NULL;
	const struct nand_op_instr *instr;
	bool readid = false;
	bool statusreq = false;

	for (i = 0; i < op->ninstrs; i++) {
		instr = &op->instrs[i];

		switch (instr->type) {
		case NAND_OP_WAITRDY_INSTR:
			/* NFC handles R/B internally, nothing to do here */
			break;
		case NAND_OP_CMD_INSTR:
			host->devtype_data->send_cmd(host, instr->ctx.cmd.opcode, true);

			if (instr->ctx.cmd.opcode == NAND_CMD_READID)
				readid = true;
			if (instr->ctx.cmd.opcode == NAND_CMD_STATUS)
				statusreq = true;

			break;
		case NAND_OP_ADDR_INSTR:
			for (j = 0; j < instr->ctx.addr.naddrs; j++) {
				bool islast = j == instr->ctx.addr.naddrs - 1;
				host->devtype_data->send_addr(host, instr->ctx.addr.addrs[j], islast);
			}
			break;
		case NAND_OP_DATA_OUT_INSTR:
			buf_write = instr->ctx.data.buf.out;
			buf_len = instr->ctx.data.len;

			if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST)
				memcpy32_toio(host->main_area0, buf_write, buf_len);
			else
				copy_page_to_sram(mtd, buf_write, buf_len);

			host->devtype_data->send_page(mtd, NFC_INPUT);

			break;
		case NAND_OP_DATA_IN_INSTR:

			buf_read = instr->ctx.data.buf.in;
			buf_len = instr->ctx.data.len;

			if (readid) {
				host->devtype_data->send_read_id(host);
				readid = false;

				memcpy32_fromio(host->data_buf, host->main_area0, buf_len * 2);

				if (chip->options & NAND_BUSWIDTH_16) {
					u8 *bufr = buf_read;
					u16 *bufw = host->data_buf;
					for (j = 0; j < buf_len; j++)
						bufr[j] = bufw[j];
				} else {
					memcpy(buf_read, host->data_buf, buf_len);
				}
				break;
			}

			if (statusreq) {
				*(u8*)buf_read = host->devtype_data->get_dev_status(host);
				statusreq = false;
				break;
			}

			host->devtype_data->read_page(chip);

			if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST) {
				if (IS_ALIGNED(buf_len, 4)) {
					memcpy32_fromio(buf_read, host->main_area0, buf_len);
				} else {
					memcpy32_fromio(host->data_buf, host->main_area0, mtd->writesize);
					memcpy(buf_read, host->data_buf, buf_len);
				}
			} else {
				copy_page_from_sram(mtd);
				memcpy(buf_read, host->data_buf, buf_len);
			}

			break;
		}
	}

	return 0;
}

#define MAX_DATA_SIZE  (4096 + 512)

static const struct nand_op_parser mxcnd_op_parser = NAND_OP_PARSER(
	NAND_OP_PARSER_PATTERN(mxcnd_do_exec_op,
			       NAND_OP_PARSER_PAT_CMD_ELEM(false),
			       NAND_OP_PARSER_PAT_ADDR_ELEM(true, 7),
			       NAND_OP_PARSER_PAT_CMD_ELEM(true),
			       NAND_OP_PARSER_PAT_WAITRDY_ELEM(true),
			       NAND_OP_PARSER_PAT_DATA_IN_ELEM(true, MAX_DATA_SIZE)),
	NAND_OP_PARSER_PATTERN(mxcnd_do_exec_op,
			       NAND_OP_PARSER_PAT_CMD_ELEM(false),
			       NAND_OP_PARSER_PAT_ADDR_ELEM(false, 7),
			       NAND_OP_PARSER_PAT_DATA_OUT_ELEM(false, MAX_DATA_SIZE),
			       NAND_OP_PARSER_PAT_CMD_ELEM(false),
			       NAND_OP_PARSER_PAT_WAITRDY_ELEM(true)),
	NAND_OP_PARSER_PATTERN(mxcnd_do_exec_op,
			       NAND_OP_PARSER_PAT_CMD_ELEM(false),
			       NAND_OP_PARSER_PAT_ADDR_ELEM(false, 7),
			       NAND_OP_PARSER_PAT_DATA_OUT_ELEM(false, MAX_DATA_SIZE),
			       NAND_OP_PARSER_PAT_CMD_ELEM(true),
			       NAND_OP_PARSER_PAT_WAITRDY_ELEM(true)),
	);

static int mxcnd_exec_op(struct nand_chip *chip,
			 const struct nand_operation *op, bool check_only)
{
	return nand_op_parser_exec_op(chip, &mxcnd_op_parser,
				      op, check_only);
}

static const struct nand_controller_ops mxcnd_controller_ops = {
	.attach_chip = mxcnd_attach_chip,
	.setup_interface = mxcnd_setup_interface,
	.exec_op = mxcnd_exec_op,
};

/*
 * The i.MX NAND controller has the problem that it handles the
 * data in chunks of 512 bytes. It doesn't treat 2k NAND chips as
 * 2048 byte data + 64 OOB, but instead:
 *
 * 512b data + 16b OOB +
 * 512b data + 16b OOB +
 * 512b data + 16b OOB +
 * 512b data + 16b OOB
 *
 * So the mapping between original NAND addressing (as intended by the chip
 * vendor) and interpretation when accessed via the i.MX NAND controller is as
 * follows:
 *
 *       original       |        i.MX
 * ---------------------+---------------------
 * data 0x0000 - 0x0200 | data 0x0000 - 0x0200
 * data 0x0200 - 0x0210 | oob  0x0000 - 0x0010
 * data 0x0210 - 0x0410 | data 0x0200 - 0x0400
 * data 0x0410 - 0x0420 | oob  0x0010 - 0x0020
 * data 0x0420 - 0x0620 | data 0x0400 - 0x0600
 * data 0x0620 - 0x0630 | oob  0x0020 - 0x0030
 * data 0x0630 - 0x0800 | data 0x0600 - 0x07d0
 * oob  0x0000 - 0x0030 | data 0x07d0 - 0x0800
 * oob  0x0030 - 0x0040 | oob  0x0030 - 0x0040
 *
 * This means that the factory provided bad block marker ends up
 * in the page data at offset 2000 = 0x7d0 instead of in the OOB data.
 *
 * If the NAND driver detects that no flash BBT is present on a 2k NAND
 * chip it will create one automatically in the assumption that the NAND is
 * pristine (that is completely erased with only vendor BBMs in the OOB) to
 * preserve factory bad block information.
 *
 * From this point on we can forget about the BBMs and rely completely
 * on the flash BBT.
 */
static int checkbad(struct mtd_info *mtd, loff_t ofs)
{
	int ret;
	uint8_t buf[mtd->writesize + mtd->oobsize];
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_RAW,
		.ooboffs = 0,
		.datbuf = buf,
		.len = mtd->writesize,
		.oobbuf = buf + mtd->writesize,
		.ooblen = mtd->oobsize,
	};

	ret = mtd_read_oob(mtd, ofs, &ops);
	if (ret < 0) {
		dev_err(mtd->dev.parent, "Failed to read page at 0x%08x\n",
			(unsigned int)ofs);
		return ret;
	}

	/*
	 * Automatically adding a BBT based on factory BBTs is only
	 * sensible if the NAND is pristine. Abort if the first page
	 * looks like a bootloader or UBI block.
	 */
	if (is_barebox_arm_head(buf)) {
		dev_err(mtd->dev.parent,
			"Flash seems to contain a barebox image, refusing to create BBT\n");
		return -EINVAL;
	}

	if (!memcmp(buf, "UBI#", 4)) {
		dev_err(mtd->dev.parent,
			"Flash seems to contain a UBI, refusing to create BBT\n");
		return -EINVAL;
	}

	if (buf[2000] != 0xff)
		/* block considered bad */
		return 1;

	/* block considered good */
	return 0;
}

static int imxnd_create_bbt(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int len, i, numblocks, ret;
	uint8_t *bbt;

	numblocks = mtd->size >> chip->bbt_erase_shift;

	/*
	 * Allocate memory (2bit per block = 1 byte per 4 blocks) and clear the
	 * memory bad block table
	 */
	len = (numblocks + 3) >> 2;
	bbt = kzalloc(len, GFP_KERNEL);
	if (!bbt)
		return -ENOMEM;

	for (i = 0; i < numblocks; ++i) {
		loff_t ofs = i << chip->bbt_erase_shift;

		ret = checkbad(mtd, ofs);
		if (ret < 0)
			goto out;

		if (ret) {
			bbt[i >> 2] |= 0x03 << (2 * (i & 0x3));
			dev_info(mtd->dev.parent, "Bad eraseblock %d at 0x%08x\n",
				 i, (unsigned int)ofs);
		}
	}

	chip->bbt_td->options |= NAND_BBT_CREATE;
	chip->bbt_md->options |= NAND_BBT_CREATE;

	free(chip->bbt);
	chip->bbt = bbt;

	ret = nand_update_bbt(chip, 0);
	if (ret)
		return ret;

	ret = nand_create_bbt(chip);
	if (ret)
		return ret;

	ret = 0;
out:
	free(bbt);

	return ret;
}

static int mxcnd_probe(struct device *dev)
{
	struct nand_chip *this;
	struct mtd_info *mtd;
	struct mxc_nand_host *host;
	struct resource *iores;
	const struct mxc_nand_devtype_data* devtype;
	int err = 0;

	devtype = device_get_match_data(dev);
	if (!devtype)
		return -ENODEV;

	/* Allocate memory for MTD device structure and private data */
	host = devm_kzalloc(dev, sizeof(struct mxc_nand_host),
			GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	/* allocate a temporary buffer for the nand_scan_ident() */
	host->data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!host->data_buf)
		return -ENOMEM;

	host->dev = dev;
	/* structures must be linked */
	this = &host->nand;
	mtd = nand_to_mtd(this);
	mtd->dev.parent = dev;
	mtd->name = DRIVER_NAME;

	/* 50 us command delay time */
	this->legacy.chip_delay = 5;

	nand_set_controller_data(this, host);
	nand_set_flash_node(this, dev->of_node);

	host->clk = clk_get(dev, NULL);
	if (IS_ERR(host->clk))
		return PTR_ERR(host->clk);

	host->devtype_data = devtype;

	if (!host->devtype_data->setup_interface)
		this->options |= NAND_KEEP_TIMINGS;

	if (host->devtype_data->needs_ip) {
		iores = dev_request_mem_resource(dev, 0);
		if (IS_ERR(iores))
			return PTR_ERR(iores);
		host->regs_ip = IOMEM(iores->start);

		iores = dev_request_mem_resource(dev, 1);
		if (IS_ERR(iores))
			return PTR_ERR(iores);
		host->base = IOMEM(iores->start);
	} else {
		iores = dev_request_mem_resource(dev, 0);
		if (IS_ERR(iores))
			return PTR_ERR(iores);
		host->base = IOMEM(iores->start);
	}

	if (IS_ERR(host->base))
		return PTR_ERR(host->base);

	host->main_area0 = host->base;

	if (host->devtype_data->regs_offset)
		host->regs = host->base + host->devtype_data->regs_offset;
	host->spare0 = host->base + host->devtype_data->spare0_offset;
	if (host->devtype_data->axi_offset)
		host->regs_axi = host->base + host->devtype_data->axi_offset;

	this->legacy.select_chip = host->devtype_data->select_chip;

	init_completion(&host->op_completion);

	err = clk_prepare_enable(host->clk);
	if (err)
		return err;
	host->clk_act = 1;

	/* Scan the NAND device */
	this->legacy.dummy_controller.ops = &mxcnd_controller_ops;
	err = nand_scan(this, is_imx25_nfc(host) ? 4 : 1);
	if (err)
		goto escan;

	this->options &= ~NAND_SUBPAGE_READ;

	if ((this->bbt_options & NAND_BBT_USE_FLASH) &&
	    this->bbt_td->pages[0] == -1 && this->bbt_md->pages[0] == -1) {
		dev_info(dev, "no BBT found. creating one\n");
		err = imxnd_create_bbt(this);
		if (err)
			dev_warn(dev, "Failed to create bbt: %pe\n",
				 ERR_PTR(err));
		err = 0;
	}

	/* Register the partitions */
	err = add_mtd_nand_device(mtd, "nand");
	if (err)
		goto cleanup_nand;

	dev->priv = host;

	return 0;

cleanup_nand:
	nand_cleanup(this);
escan:
	if (host->clk_act)
		clk_disable_unprepare(host->clk);

	return err;
}

static struct driver mxcnd_driver = {
	.name  = DRIVER_NAME,
	.probe = mxcnd_probe,
	.of_compatible = DRV_OF_COMPAT(mxcnd_dt_ids),
};
device_platform_driver(mxcnd_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC NAND MTD driver");
MODULE_LICENSE("GPL");
