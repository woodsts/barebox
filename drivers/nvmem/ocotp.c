// SPDX-License-Identifier: GPL-2.0-only
/*
 * ocotp.c - i.MX6 ocotp fusebox driver
 *
 * Provide an interface for programming and sensing the information that are
 * stored in on-chip fuse elements. This functionality is part of the IC
 * Identification Module (IIM), which is present on some i.MX CPUs.
 *
 * Copyright (c) 2010 Baruch Siach <baruch@tkos.co.il>,
 * 	Orex Computed Radiography
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <common.h>
#include <deep-probe.h>
#include <driver.h>
#include <malloc.h>
#include <xfuncs.h>
#include <errno.h>
#include <init.h>
#include <net.h>
#include <io.h>
#include <of.h>
#include <clock.h>
#include <linux/regmap.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <machine_id.h>
#ifdef CONFIG_ARCH_IMX
#include <mach/imx/ocotp.h>
#include <mach/imx/ocotp-fusemap.h>
#else
#include <mach/mxs/ocotp.h>
#include <mach/mxs/ocotp-fusemap.h>
#endif
#include <soc/imx8m/featctrl.h>
#include <linux/nvmem-provider.h>

/*
 * a single MAC address reference has the form
 * <&phandle regoffset>
 */
#define MAC_ADDRESS_PROPLEN	(2 * sizeof(__be32))

/* OCOTP Registers offsets */
#define OCOTP_CTRL			0x00
#define OCOTP_CTRL_SET			0x04
#define OCOTP_CTRL_CLR			0x08
#define OCOTP_TIMING			0x10
#define OCOTP_DATA			0x20
#define OCOTP_READ_CTRL			0x30
#define OCOTP_READ_FUSE_DATA		0x40
#define OCOTP_SW_STICKY			0x50

#define MX7_OCOTP_DATA0			0x20
#define MX7_OCOTP_DATA1			0x30
#define MX7_OCOTP_DATA2			0x40
#define MX7_OCOTP_DATA3			0x50
#define MX7_OCOTP_READ_CTRL		0x60
#define MX7_OCOTP_READ_FUSE_DATA0	0x70
#define MX7_OCOTP_READ_FUSE_DATA1	0x80
#define MX7_OCOTP_READ_FUSE_DATA2	0x90
#define MX7_OCOTP_READ_FUSE_DATA3	0xA0

#define DEF_FSOURCE			1001	/* > 1000 ns */
#define DEF_STROBE_PROG			10000	/* IPG clocks */

/* OCOTP Registers bits and masks */
#define OCOTP_CTRL_ADDR			GENMASK(7, 0)
#define OCOTP_CTRL_BUSY			BIT(8)
#define OCOTP_CTRL_ERROR		BIT(9)
#define OCOTP_CTRL_RELOAD_SHADOWS	BIT(10)
#define OCOTP_CTRL_WR_UNLOCK		GENMASK(31, 16)
#define OCOTP_CTRL_WR_UNLOCK_KEY	0x3E77

/*
 * i.MX8MP OCOTP CTRL has a different layout. See RM Rev.1 06/2021
 * Section 6.3.5.1.2.4
 */
#define OCOTP_CTRL_ADDR_8MP		GENMASK(8, 0)
#define OCOTP_CTRL_BUSY_8MP		BIT(9)
#define OCOTP_CTRL_ERROR_8MP		BIT(10)
#define OCOTP_CTRL_RELOAD_SHADOWS_8MP	BIT(11)
#define OCOTP_CTRL_WR_UNLOCK_8MP	GENMASK(31, 16)

#define OCOTP_TIMING_STROBE_READ	GENMASK(21, 16)
#define OCOTP_TIMING_RELAX		GENMASK(15, 12)
#define OCOTP_TIMING_STROBE_PROG	GENMASK(11, 0)
#define OCOTP_TIMING_WAIT		GENMASK(27, 22)

#define OCOTP_SW_STICKY_SRK_REVOKE_LOCK		BIT(1)
#define OCOTP_SW_STICKY_FIELD_RETURN_LOCK	BIT(2)

#define OCOTP_READ_CTRL_READ_FUSE	BIT(1)

#define OCOTP_OFFSET_TO_ADDR(o) (OCOTP_OFFSET_TO_INDEX(o) * 4)

/* Other definitions */
#define IMX6_OTP_DATA_ERROR_VAL		0xBADABADA
#define TIMING_STROBE_PROG_US		10
#define TIMING_STROBE_READ_NS		37
#define TIMING_RELAX_NS			17
#define MAC_OFFSET_0			(0x22 * 4)
#define IMX6UL_MAC_OFFSET_1		(0x23 * 4)
#define MAC_OFFSET_1			(0x24 * 4)
#define IMX8MP_MAC_OFFSET_1		(0x25 * 4)
#define MAX_MAC_OFFSETS			2
#define MAC_BYTES			8
#define UNIQUE_ID_NUM			2

#define field_prep(_mask, _val) (((_val) << (ffs(_mask) - 1)) & (_mask))

enum imx_ocotp_format_mac_direction {
	OCOTP_HW_TO_MAC,
	OCOTP_MAC_TO_HW,
};

struct ocotp_ctrl_reg {
	u32 bm_addr;
	u32 bm_busy;
	u32 bm_error;
	u32 bm_reload_shadows;
	u32 bm_wr_unlock;
};

const struct ocotp_ctrl_reg ocotp_ctrl_reg_default = {
	.bm_addr = OCOTP_CTRL_ADDR,
	.bm_busy = OCOTP_CTRL_BUSY,
	.bm_error = OCOTP_CTRL_ERROR,
	.bm_reload_shadows = OCOTP_CTRL_RELOAD_SHADOWS,
	.bm_wr_unlock = OCOTP_CTRL_WR_UNLOCK,
};

const struct ocotp_ctrl_reg ocotp_ctrl_reg_8mp = {
	.bm_addr = OCOTP_CTRL_ADDR_8MP,
	.bm_busy = OCOTP_CTRL_BUSY_8MP,
	.bm_error = OCOTP_CTRL_ERROR_8MP,
	.bm_reload_shadows = OCOTP_CTRL_RELOAD_SHADOWS_8MP,
	.bm_wr_unlock = OCOTP_CTRL_WR_UNLOCK_8MP,
};

struct ocotp_priv;

struct imx_ocotp_data {
	int nregs;
	u32 (*addr_to_offset)(u32 addr);
	void (*format_mac)(u8 *dst, const u8 *src,
			   enum imx_ocotp_format_mac_direction dir);
	int (*set_timing)(struct ocotp_priv *priv);
	int (*fuse_read)(struct ocotp_priv *priv, u32 addr, u32 *pdata);
	int (*fuse_blow)(struct ocotp_priv *priv, u32 addr, u32 value);
	bool (*srk_revoke_locked)(struct ocotp_priv *priv);
	void (*lock_srk_revoke)(struct ocotp_priv *priv);
	bool (*field_return_locked)(struct ocotp_priv *priv);
	u8  mac_offsets[MAX_MAC_OFFSETS];
	u8  mac_offsets_num;
	struct imx8m_featctrl_data *feat;
	const struct ocotp_ctrl_reg *ctrl;
};

struct ocotp_priv_ethaddr {
	char value[MAC_BYTES];
	struct regmap *map;
	u8 offset;
	const struct imx_ocotp_data *data;
};

struct ocotp_priv {
	struct regmap *map;
	void __iomem *base;
	struct clk *clk;
	struct device dev;
	int permanent_write_enable;
	int sense_enable;
	struct ocotp_priv_ethaddr ethaddr[MAX_MAC_OFFSETS];
	struct regmap_config map_config;
	const struct imx_ocotp_data *data;
	int  mac_offset_idx;
};

static struct ocotp_priv *imx_ocotp;

static int imx6_ocotp_set_timing(struct ocotp_priv *priv)
{
	u32 clk_rate;
	u32 relax, strobe_read, strobe_prog;
	u32 timing;

	/*
	 * Note: there are minimum timings required to ensure an OTP fuse burns
	 * correctly that are independent of the ipg_clk. Those values are not
	 * formally documented anywhere however, working from the minimum
	 * timings given in u-boot we can say:
	 *
	 * - Minimum STROBE_PROG time is 10 microseconds. Intuitively 10
	 *   microseconds feels about right as representative of a minimum time
	 *   to physically burn out a fuse.
	 *
	 * - Minimum STROBE_READ i.e. the time to wait post OTP fuse burn before
	 *   performing another read is 37 nanoseconds
	 *
	 * - Minimum RELAX timing is 17 nanoseconds. This final RELAX minimum
	 *   timing is not entirely clear the documentation says "This
	 *   count value specifies the time to add to all default timing
	 *   parameters other than the Tpgm and Trd. It is given in number
	 *   of ipg_clk periods." where Tpgm and Trd refer to STROBE_PROG
	 *   and STROBE_READ respectively. What the other timing parameters
	 *   are though, is not specified. Experience shows a zero RELAX
	 *   value will mess up a re-load of the shadow registers post OTP
	 *   burn.
	 */
	clk_rate = clk_get_rate(priv->clk);

	relax = DIV_ROUND_UP(clk_rate * TIMING_RELAX_NS, 1000000000) - 1;

	strobe_read = DIV_ROUND_UP(clk_rate * TIMING_STROBE_READ_NS,
				   1000000000);
	strobe_read += 2 * (relax + 1) - 1;
	strobe_prog = DIV_ROUND_CLOSEST(clk_rate * TIMING_STROBE_PROG_US,
					1000000);
	strobe_prog += 2 * (relax + 1) - 1;

	timing = readl(priv->base + OCOTP_TIMING) & OCOTP_TIMING_WAIT;
	timing |= FIELD_PREP(OCOTP_TIMING_RELAX, relax);
	timing |= FIELD_PREP(OCOTP_TIMING_STROBE_READ, strobe_read);
	timing |= FIELD_PREP(OCOTP_TIMING_STROBE_PROG, strobe_prog);

	writel(timing, priv->base + OCOTP_TIMING);

	return 0;
}

static int imx7_ocotp_set_timing(struct ocotp_priv *priv)
{
	unsigned long clk_rate;
	u64 fsource, strobe_prog;
	u32 timing;

	clk_rate = clk_get_rate(priv->clk);

	fsource = DIV_ROUND_UP_ULL((u64)clk_rate * DEF_FSOURCE,
				   NSEC_PER_SEC) + 1;
	strobe_prog = DIV_ROUND_CLOSEST_ULL((u64)clk_rate * DEF_STROBE_PROG,
					    NSEC_PER_SEC) + 1;

	timing = strobe_prog & 0x00000FFF;
	timing |= (fsource << 12) & 0x000FF000;

	writel(timing, priv->base + OCOTP_TIMING);

	return 0;
}

static int imx6_ocotp_wait_busy(struct ocotp_priv *priv, u32 flags)
{
	uint64_t start = get_time_ns();
	u32 bm_ctrl_busy = priv->data->ctrl->bm_busy;

	while (readl(priv->base + OCOTP_CTRL) & (bm_ctrl_busy | flags))
		if (is_timeout(start, MSECOND))
			return -ETIMEDOUT;

	return 0;
}

static int imx6_ocotp_prepare(struct ocotp_priv *priv)
{
	int ret;

	ret = priv->data->set_timing(priv);
	if (ret)
		return ret;

	ret = imx6_ocotp_wait_busy(priv, 0);
	if (ret)
		return ret;

	return 0;
}

static bool imx8m_srk_revoke_locked(struct ocotp_priv *priv)
{
	return readl(priv->base + OCOTP_SW_STICKY) & OCOTP_SW_STICKY_SRK_REVOKE_LOCK;
}

static void imx8m_lock_srk_revoke(struct ocotp_priv *priv)
{
	u32 val;

	val = readl(priv->base + OCOTP_SW_STICKY);
	val |= OCOTP_SW_STICKY_SRK_REVOKE_LOCK;
	writel(val, priv->base + OCOTP_SW_STICKY);
}

static bool imx8m_field_return_locked(struct ocotp_priv *priv)
{
	return readl(priv->base + OCOTP_SW_STICKY) & OCOTP_SW_STICKY_FIELD_RETURN_LOCK;
}

static int imx6_fuse_read_addr(struct ocotp_priv *priv, u32 addr, u32 *pdata)
{
	const u32 bm_ctrl_error = priv->data->ctrl->bm_error;
	const u32 bm_ctrl_addr = priv->data->ctrl->bm_addr;
	const u32 bm_ctrl_wr_unlock = priv->data->ctrl->bm_wr_unlock;
	u32 ctrl_reg;
	int ret;

	writel(bm_ctrl_error, priv->base + OCOTP_CTRL_CLR);

	ctrl_reg = readl(priv->base + OCOTP_CTRL);
	ctrl_reg &= ~bm_ctrl_addr;
	ctrl_reg &= ~bm_ctrl_wr_unlock;
	ctrl_reg |= field_prep(bm_ctrl_addr, addr);
	writel(ctrl_reg, priv->base + OCOTP_CTRL);

	writel(OCOTP_READ_CTRL_READ_FUSE, priv->base + OCOTP_READ_CTRL);
	ret = imx6_ocotp_wait_busy(priv, 0);
	if (ret)
		return ret;

	if (readl(priv->base + OCOTP_CTRL) & bm_ctrl_error)
		*pdata = 0xbadabada;
	else
		*pdata = readl(priv->base + OCOTP_READ_FUSE_DATA);

	return 0;
}

static int imx7_fuse_read_addr(struct ocotp_priv *priv, u32 index, u32 *pdata)
{
	const u32 bm_ctrl_error = priv->data->ctrl->bm_error;
	const u32 bm_ctrl_addr = priv->data->ctrl->bm_addr;
	const u32 bm_ctrl_wr_unlock = priv->data->ctrl->bm_wr_unlock;
	u32 ctrl_reg;
	u32 bank_addr;
	u16 word;
	int ret;

	word = index & 0x3;
	bank_addr = index >> 2;

	writel(bm_ctrl_error, priv->base + OCOTP_CTRL_CLR);

	ctrl_reg = readl(priv->base + OCOTP_CTRL);
	ctrl_reg &= ~bm_ctrl_addr;
	ctrl_reg &= ~bm_ctrl_wr_unlock;
	ctrl_reg |= field_prep(bm_ctrl_addr, bank_addr);
	writel(ctrl_reg, priv->base + OCOTP_CTRL);

	writel(OCOTP_READ_CTRL_READ_FUSE, priv->base + MX7_OCOTP_READ_CTRL);
	ret = imx6_ocotp_wait_busy(priv, 0);
	if (ret)
		return ret;

	if (readl(priv->base + OCOTP_CTRL) & bm_ctrl_error)
		*pdata = 0xbadabada;
	else
		switch (word) {
			case 0:
				*pdata = readl(priv->base + MX7_OCOTP_READ_FUSE_DATA0);
				break;
			case 1:
				*pdata = readl(priv->base + MX7_OCOTP_READ_FUSE_DATA1);
				break;
			case 2:
				*pdata = readl(priv->base + MX7_OCOTP_READ_FUSE_DATA2);
				break;
			case 3:
				*pdata = readl(priv->base + MX7_OCOTP_READ_FUSE_DATA2);
				break;
		}

	return 0;
}

static int imx6_ocotp_read_one_u32(struct ocotp_priv *priv, u32 index, u32 *pdata)
{
	int ret;

	ret = imx6_ocotp_prepare(priv);
	if (ret) {
		dev_err(&priv->dev, "failed to prepare read fuse 0x%08x\n",
				index);
		return ret;
	}

	ret = priv->data->fuse_read(priv, index, pdata);
	if (ret) {
		dev_err(&priv->dev, "failed to read fuse 0x%08x\n", index);
		return ret;
	}

	return 0;
}

static int imx_ocotp_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct ocotp_priv *priv = ctx;
	u32 index;
	int ret;

	index = reg >> 2;

	if (priv->sense_enable) {
		ret = imx6_ocotp_read_one_u32(priv, index, val);
		if (ret)
			return ret;
	} else {
		*(u32 *)val = readl(priv->base +
				    priv->data->addr_to_offset(index));
	}

	return 0;
}

static void imx_ocotp_clear_unlock(struct ocotp_priv *priv, u32 index)
{
	const u32 bm_ctrl_error = priv->data->ctrl->bm_error;
	const u32 bm_ctrl_addr = priv->data->ctrl->bm_addr;
	const u32 bm_ctrl_wr_unlock = priv->data->ctrl->bm_wr_unlock;
	u32 ctrl_reg;

	writel(bm_ctrl_error, priv->base + OCOTP_CTRL_CLR);

	/* Control register */
	ctrl_reg = readl(priv->base + OCOTP_CTRL);
	ctrl_reg &= ~bm_ctrl_addr;
	ctrl_reg |= field_prep(bm_ctrl_addr, index);
	ctrl_reg |= field_prep(bm_ctrl_wr_unlock, OCOTP_CTRL_WR_UNLOCK_KEY);
	writel(ctrl_reg, priv->base + OCOTP_CTRL);
}

static int imx6_fuse_blow_addr(struct ocotp_priv *priv, u32 index, u32 value)
{
	const u32 bm_ctrl_error = priv->data->ctrl->bm_error;
	int ret;

	imx_ocotp_clear_unlock(priv, index);

	writel(bm_ctrl_error, priv->base + OCOTP_CTRL_CLR);

	writel(value, priv->base + OCOTP_DATA);
	ret = imx6_ocotp_wait_busy(priv, 0);
	if (ret)
		return ret;

	/* Write postamble */
	udelay(2000);
	return 0;
}

static int imx7_fuse_blow_addr(struct ocotp_priv *priv, u32 index, u32 value)
{
	int ret;
	int word;
	int bank_addr;

	bank_addr = index >> 2;
	word = index & 0x3;

	imx_ocotp_clear_unlock(priv, bank_addr);

	switch(word) {
		case 0:
			writel(0, priv->base + MX7_OCOTP_DATA1);
			writel(0, priv->base + MX7_OCOTP_DATA2);
			writel(0, priv->base + MX7_OCOTP_DATA3);
			writel(value, priv->base + MX7_OCOTP_DATA0);
			break;
		case 1:
			writel(value, priv->base + MX7_OCOTP_DATA1);
			writel(0, priv->base + MX7_OCOTP_DATA2);
			writel(0, priv->base + MX7_OCOTP_DATA3);
			writel(0, priv->base + MX7_OCOTP_DATA0);
			break;
		case 2:
			writel(value, priv->base + MX7_OCOTP_DATA2);
			writel(0, priv->base + MX7_OCOTP_DATA3);
			writel(0, priv->base + MX7_OCOTP_DATA1);
			writel(0, priv->base + MX7_OCOTP_DATA0);
			break;
		case 3:
			writel(value, priv->base + MX7_OCOTP_DATA3);
			writel(0, priv->base + MX7_OCOTP_DATA1);
			writel(0, priv->base + MX7_OCOTP_DATA2);
			writel(0, priv->base + MX7_OCOTP_DATA0);
			break;
	}

	ret = imx6_ocotp_wait_busy(priv, 0);
	if (ret)
		return ret;

	/* Write postamble */
	udelay(2000);
	return 0;
}

static int imx6_ocotp_reload_shadow(struct ocotp_priv *priv)
{
	const u32 bm_ctrl_reload_shadows = priv->data->ctrl->bm_reload_shadows;

	dev_info(&priv->dev, "reloading shadow registers...\n");
	writel(bm_ctrl_reload_shadows, priv->base + OCOTP_CTRL_SET);
	udelay(1);

	return imx6_ocotp_wait_busy(priv, bm_ctrl_reload_shadows);
}

static int imx6_ocotp_blow_one_u32(struct ocotp_priv *priv, u32 index, u32 data,
			    u32 *pfused_value)
{
	const u32 bm_ctrl_error = priv->data->ctrl->bm_error;
	int ret;

	ret = imx6_ocotp_prepare(priv);
	if (ret) {
		dev_err(&priv->dev, "prepare to write failed\n");
		return ret;
	}

	ret = priv->data->fuse_blow(priv, index, data);
	if (ret) {
		dev_err(&priv->dev, "fuse blow failed\n");
		return ret;
	}

	if (readl(priv->base + OCOTP_CTRL) & bm_ctrl_error) {
		dev_err(&priv->dev, "bad write status\n");
		return -EFAULT;
	}

	ret = imx6_ocotp_read_one_u32(priv, index, pfused_value);

	return ret;
}

static int imx_ocotp_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct ocotp_priv *priv = ctx;
	int index;
	u32 pfuse;
	int ret;

	index = reg >> 2;

	if (priv->permanent_write_enable) {
		ret = imx6_ocotp_blow_one_u32(priv, index, val, &pfuse);
		if (ret < 0)
			return ret;
	} else {
		writel(val, priv->base +
		       priv->data->addr_to_offset(index));
	}

	if (priv->permanent_write_enable)
		imx6_ocotp_reload_shadow(priv);

	return 0;
}

static void imx_ocotp_field_decode(uint32_t field, unsigned *word,
				 unsigned *bit, unsigned *mask)
{
	unsigned width;

	*word = FIELD_GET(OCOTP_WORD_MASK, field) * 4;
	*bit = FIELD_GET(OCOTP_BIT_MASK, field);
	width = FIELD_GET(OCOTP_WIDTH_MASK, field);
	*mask = GENMASK(width, 0);
}

static int imx_ocotp_ensure_probed(void);

int imx_ocotp_read_field(uint32_t field, unsigned *value)
{
	unsigned word, bit, mask, val;
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	imx_ocotp_field_decode(field, &word, &bit, &mask);

	ret = imx_ocotp_reg_read(imx_ocotp, word, &val);
	if (ret)
		return ret;

	val >>= bit;
	val &= mask;

	dev_dbg(&imx_ocotp->dev, "%s: word: 0x%x bit: %d mask: 0x%x val: 0x%x\n",
		__func__, word, bit, mask, val);

	*value = val;

	return 0;
}

int imx_ocotp_write_field(uint32_t field, unsigned value)
{
	unsigned word, bit, mask;
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	imx_ocotp_field_decode(field, &word, &bit, &mask);

	value &= mask;
	value <<= bit;

	ret = imx_ocotp_reg_write(imx_ocotp, word, value);
	if (ret)
		return ret;

	dev_dbg(&imx_ocotp->dev, "%s: word: 0x%x bit: %d mask: 0x%x val: 0x%x\n",
		__func__, word, bit, mask, value);

	return 0;
}

int imx_ocotp_permanent_write(int enable)
{
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	imx_ocotp->permanent_write_enable = enable;

	return 0;
}

int imx_ocotp_sense_enable(bool enable)
{
	bool old_value;
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	old_value = imx_ocotp->sense_enable;
	imx_ocotp->sense_enable = enable;
	return old_value;
}

int imx_ocotp_srk_revoke_locked(void)
{
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	if (imx_ocotp->data->srk_revoke_locked)
		return imx_ocotp->data->srk_revoke_locked(imx_ocotp);

	return -ENOSYS;
}

int imx_ocotp_lock_srk_revoke(void)
{
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	if (imx_ocotp->data->lock_srk_revoke) {
		imx_ocotp->data->lock_srk_revoke(imx_ocotp);
		return 0;
	}

	return -ENOSYS;
}

int imx_ocotp_field_return_locked(void)
{
	int ret;

	ret = imx_ocotp_ensure_probed();
	if (ret)
		return ret;

	if (imx_ocotp->data->field_return_locked)
		return imx_ocotp->data->field_return_locked(imx_ocotp);

	return -ENOSYS;
}

static void imx_ocotp_format_mac(u8 *dst, const u8 *src,
				 enum imx_ocotp_format_mac_direction dir)
{
	/*
	 * This transformation is symmetic, so we don't care about the
	 * value of 'dir'.
	 */
	dst[5] = src[0];
	dst[4] = src[1];
	dst[3] = src[2];
	dst[2] = src[3];
	dst[1] = src[4];
	dst[0] = src[5];
}

static void vf610_ocotp_format_mac(u8 *dst, const u8 *src,
				   enum imx_ocotp_format_mac_direction dir)
{
	switch (dir) {
	case OCOTP_HW_TO_MAC:
		dst[1] = src[0];
		dst[0] = src[1];
		dst[5] = src[4];
		dst[4] = src[5];
		dst[3] = src[6];
		dst[2] = src[7];
		break;
	case OCOTP_MAC_TO_HW:
		dst[0] = src[1];
		dst[1] = src[0];
		dst[4] = src[5];
		dst[5] = src[4];
		dst[6] = src[3];
		dst[7] = src[2];
		break;
	}
}

static int imx_ocotp_read_mac(const struct imx_ocotp_data *data,
			      struct regmap *map, unsigned int offset,
			      u8 mac[])
{
	u8 buf[MAC_BYTES];
	int ret;

	ret = regmap_bulk_read(map, offset, buf, MAC_BYTES / 4);

	if (ret < 0)
		return ret;

	if (offset != IMX6UL_MAC_OFFSET_1 && offset != IMX8MP_MAC_OFFSET_1)
		data->format_mac(mac, buf, OCOTP_HW_TO_MAC);
	else
		data->format_mac(mac, buf + 2, OCOTP_HW_TO_MAC);

	return 0;
}

static int imx_ocotp_get_mac(struct param_d *param, void *priv)
{
	struct ocotp_priv_ethaddr *ethaddr = priv;

	return imx_ocotp_read_mac(ethaddr->data, ethaddr->map, ethaddr->offset,
				  ethaddr->value);
}

static int imx_ocotp_set_mac(struct param_d *param, void *priv)
{
	char buf[MAC_BYTES];
	struct ocotp_priv_ethaddr *ethaddr = priv;
	int ret;

	ret = regmap_bulk_read(ethaddr->map, ethaddr->offset, buf, MAC_BYTES / 4);
	if (ret < 0)
		return ret;

	if (ethaddr->offset != IMX6UL_MAC_OFFSET_1 &&
	    ethaddr->offset != IMX8MP_MAC_OFFSET_1)
		ethaddr->data->format_mac(buf, ethaddr->value,
					  OCOTP_MAC_TO_HW);
	else
		ethaddr->data->format_mac(buf + 2, ethaddr->value,
					  OCOTP_MAC_TO_HW);

	return regmap_bulk_write(ethaddr->map, ethaddr->offset,
				 buf, MAC_BYTES / 4);
}

static struct regmap_bus imx_ocotp_regmap_bus = {
	.reg_write = imx_ocotp_reg_write,
	.reg_read = imx_ocotp_reg_read,
};

static int imx_ocotp_cell_pp(void *context, const char *id, unsigned int offset,
			     void *data, size_t bytes)
{
	/* Deal with some post processing of nvmem cell data */
	if (id && !strcmp(id, "mac-address")) {
		u8 *buf = data;
		int i;

		for (i = 0; i < bytes/2; i++)
			swap(buf[i], buf[bytes - i - 1]);
	}

	return 0;
}

static int imx_ocotp_init_dt(struct ocotp_priv *priv)
{
	char mac[MAC_BYTES];
	const __be32 *prop;
	struct device_node *node = priv->dev.parent->of_node;
	u32 tester3, tester4;
	int ret, len = 0;

	if (!node)
		return 0;

	prop = of_get_property(node, "barebox,provide-mac-address", &len);

	for (; len >= MAC_ADDRESS_PROPLEN; len -= MAC_ADDRESS_PROPLEN) {
		struct device_node *rnode;
		uint32_t phandle, offset;

		phandle = be32_to_cpup(prop++);

		rnode = of_find_node_by_phandle(phandle);
		offset = be32_to_cpup(prop++);

		if (imx_ocotp_read_mac(priv->data, priv->map,
				       OCOTP_OFFSET_TO_ADDR(offset),
				       mac))
			continue;

		of_eth_register_ethaddr(rnode, mac);
	}

	if (!of_property_read_bool(node, "barebox,feature-controller"))
		return 0;

	ret = regmap_read(priv->map, OCOTP_OFFSET_TO_ADDR(0x440), &tester3);
	if (ret != 0)
		return ret;

	ret = regmap_read(priv->map, OCOTP_OFFSET_TO_ADDR(0x450), &tester4);
	if (ret != 0)
		return ret;

	return imx8m_feat_ctrl_init(priv->dev.parent, tester3, tester4, priv->data->feat);
}

static void imx_ocotp_set_unique_machine_id(void)
{
	uint32_t unique_id_parts[UNIQUE_ID_NUM];
	int i;

	for (i = 0; i < UNIQUE_ID_NUM; i++)
		if (imx_ocotp_read_field(OCOTP_UNIQUE_ID(i),
					 &unique_id_parts[i]))
			return;

	machine_id_set_hashable(unique_id_parts, sizeof(unique_id_parts));
}

static int imx_ocotp_probe(struct device *dev)
{
	struct resource *iores;
	struct ocotp_priv *priv;
	int ret = 0;
	const struct imx_ocotp_data *data;
	struct nvmem_device *nvmem;

	data = device_get_match_data(dev);
	if (!data)
		return -ENODEV;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	priv = xzalloc(sizeof(*priv));

	priv->data      = data;
	priv->base	= IOMEM(iores->start);
	priv->clk	= clk_get(dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	dev_set_name(&priv->dev, "ocotp");
	priv->dev.parent = dev;
	register_device(&priv->dev);

	priv->map_config.reg_bits = 32;
	priv->map_config.val_bits = 32;
	priv->map_config.reg_stride = 4;
	priv->map_config.max_register = priv->map_config.reg_stride * (data->nregs - 1);

	priv->map = regmap_init(dev, &imx_ocotp_regmap_bus, priv, &priv->map_config);
	if (IS_ERR(priv->map))
		return PTR_ERR(priv->map);

	nvmem = nvmem_regmap_register_with_pp(priv->map, "imx-ocotp",
					      imx_ocotp_cell_pp);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	imx_ocotp = priv;

	if (IS_ENABLED(CONFIG_IMX_OCOTP_WRITE)) {
		dev_add_param_bool(&(priv->dev), "permanent_write_enable",
				NULL, NULL, &priv->permanent_write_enable, NULL);
	}

	if (IS_ENABLED(CONFIG_NET)) {
		int i;
		struct ocotp_priv_ethaddr *ethaddr;

		for (i = 0; i < priv->data->mac_offsets_num; i++) {
			ethaddr = &priv->ethaddr[i];
			ethaddr->map = priv->map;
			ethaddr->offset = priv->data->mac_offsets[i];
			ethaddr->data = data;

			dev_add_param_mac(&priv->dev, xasprintf("mac_addr%d", i),
					  imx_ocotp_set_mac, imx_ocotp_get_mac,
					  ethaddr->value, ethaddr);
		}

		/*
		 * Alias to mac_addr0 for backwards compatibility
		 */
		ethaddr = &priv->ethaddr[0];
		dev_add_param_mac(&priv->dev, "mac_addr",
				  imx_ocotp_set_mac, imx_ocotp_get_mac,
				  ethaddr->value, ethaddr);
	}

	if (IS_ENABLED(CONFIG_MACHINE_ID))
		imx_ocotp_set_unique_machine_id();

	ret = imx_ocotp_init_dt(priv);
	if (ret)
		dev_warn(dev, "feature controller registration failed: %pe\n",
			 ERR_PTR(ret));

	dev_add_param_bool(&(priv->dev), "sense_enable", NULL, NULL, &priv->sense_enable, priv);
	return 0;
}

static u32 imx6sl_addr_to_offset(u32 addr)
{
	return OCOTP_SHADOW_OFFSET + addr * OCOTP_SHADOW_SPACING;
}

static u32 imx6q_addr_to_offset(u32 addr)
{
	u32 addendum = 0;

	if (addr > 0x2F) {
		/*
		 * If we are reading past Bank 5, take into account a
		 * 0x100 bytes wide gap between Bank 5 and Bank 6
		 */
		addendum += 0x100;
	}


	return imx6sl_addr_to_offset(addr) + addendum;
}

static u32 vf610_addr_to_offset(u32 addr)
{
	if (addr == 0x04)
		return 0x450;
	else
		return imx6q_addr_to_offset(addr);
}

static struct imx_ocotp_data imx6q_ocotp_data = {
	.nregs = 128,
	.addr_to_offset = imx6q_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { MAC_OFFSET_0 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx_ocotp_data imx6sl_ocotp_data = {
	.nregs = 64,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { MAC_OFFSET_0 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx_ocotp_data imx6ul_ocotp_data = {
	.nregs = 144,
	.addr_to_offset = imx6q_addr_to_offset,
	.mac_offsets_num = 2,
	.mac_offsets = { MAC_OFFSET_0, IMX6UL_MAC_OFFSET_1 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx_ocotp_data imx6ull_ocotp_data = {
	.nregs = 80,
	.addr_to_offset = imx6q_addr_to_offset,
	.mac_offsets_num = 2,
	.mac_offsets = { MAC_OFFSET_0, IMX6UL_MAC_OFFSET_1 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx_ocotp_data vf610_ocotp_data = {
	.nregs = 128,
	.addr_to_offset = vf610_addr_to_offset,
	.mac_offsets_num = 2,
	.mac_offsets = { MAC_OFFSET_0, MAC_OFFSET_1 },
	.format_mac = vf610_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx8m_featctrl_data imx8mp_featctrl_data = {
	.tester3.cpu_bitmask = 0xc0000,
	.tester3.vpu_bitmask = 0x43000000,
	.tester4.npu_bitmask = 0x8,
	.tester4.gpu_bitmask = 0xc0,
	.tester4.mipi_dsi_bitmask = 0x60000,
	.tester4.lvds_bitmask = 0x180000,
	.tester4.isp_bitmask = 0x3,
	.tester4.dsp_bitmask = 0x10,
};

static struct imx_ocotp_data imx8mp_ocotp_data = {
	.nregs = 384,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 2,
	.mac_offsets = { 0x90, 0x94 },
	.format_mac = imx_ocotp_format_mac,
	.feat = &imx8mp_featctrl_data,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.srk_revoke_locked = imx8m_srk_revoke_locked,
	.lock_srk_revoke = imx8m_lock_srk_revoke,
	.field_return_locked = imx8m_field_return_locked,
	.ctrl = &ocotp_ctrl_reg_8mp,
};

static struct imx_ocotp_data imx8mq_ocotp_data = {
	.nregs = 256,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { 0x90 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx8m_featctrl_data imx8mm_featctrl_data = {
	.tester4.vpu_bitmask = 0x1c0000,
	.tester4.cpu_bitmask = 0x3,
};

static struct imx_ocotp_data imx8mm_ocotp_data = {
	.nregs = 256,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { 0x90 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.srk_revoke_locked = imx8m_srk_revoke_locked,
	.lock_srk_revoke = imx8m_lock_srk_revoke,
	.field_return_locked = imx8m_field_return_locked,
	.feat = &imx8mm_featctrl_data,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx8m_featctrl_data imx8mn_featctrl_data = {
	.tester4.gpu_bitmask = 0x1000000,
	.tester4.cpu_bitmask = 0x3,
};

static struct imx_ocotp_data imx8mn_ocotp_data = {
	.nregs = 256,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { 0x90 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx6_ocotp_set_timing,
	.fuse_blow = imx6_fuse_blow_addr,
	.fuse_read = imx6_fuse_read_addr,
	.srk_revoke_locked = imx8m_srk_revoke_locked,
	.lock_srk_revoke = imx8m_lock_srk_revoke,
	.field_return_locked = imx8m_field_return_locked,
	.feat = &imx8mn_featctrl_data,
	.ctrl = &ocotp_ctrl_reg_default,
};

static struct imx_ocotp_data imx7d_ocotp_data = {
	.nregs = 64,
	.addr_to_offset = imx6sl_addr_to_offset,
	.mac_offsets_num = 1,
	.mac_offsets = { MAC_OFFSET_0, IMX6UL_MAC_OFFSET_1 },
	.format_mac = imx_ocotp_format_mac,
	.set_timing = imx7_ocotp_set_timing,
	.fuse_blow = imx7_fuse_blow_addr,
	.fuse_read = imx7_fuse_read_addr,
	.ctrl = &ocotp_ctrl_reg_default,
};

static __maybe_unused struct of_device_id imx_ocotp_dt_ids[] = {
	{
		.compatible = "fsl,imx6q-ocotp",
		.data = &imx6q_ocotp_data,
	}, {
		.compatible = "fsl,imx6sx-ocotp",
		.data = &imx6q_ocotp_data,
	}, {
		.compatible = "fsl,imx6sl-ocotp",
		.data = &imx6sl_ocotp_data,
	}, {
		.compatible = "fsl,imx6ul-ocotp",
		.data = &imx6ul_ocotp_data,
	}, {
		.compatible = "fsl,imx6ull-ocotp",
		.data = &imx6ull_ocotp_data,
	}, {
		.compatible = "fsl,imx7d-ocotp",
		.data = &imx7d_ocotp_data,
	}, {
		.compatible = "fsl,imx8mp-ocotp",
		.data = &imx8mp_ocotp_data,
	}, {
		.compatible = "fsl,imx8mq-ocotp",
		.data = &imx8mq_ocotp_data,
	}, {
		.compatible = "fsl,imx8mm-ocotp",
		.data = &imx8mm_ocotp_data,
	}, {
		.compatible = "fsl,imx8mn-ocotp",
		.data = &imx8mn_ocotp_data,
	}, {
		.compatible = "fsl,vf610-ocotp",
		.data = &vf610_ocotp_data,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, imx_ocotp_dt_ids);

static int imx_ocotp_ensure_probed(void)
{
	if (!imx_ocotp && deep_probe_is_supported()) {
		int ret;

		ret = of_devices_ensure_probed_by_dev_id(imx_ocotp_dt_ids);
		if (ret)
			return ret;
	}

	return imx_ocotp ? 0 : -EPROBE_DEFER;
}

static struct driver imx_ocotp_driver = {
	.name	= "imx_ocotp",
	.probe	= imx_ocotp_probe,
	.of_compatible = DRV_OF_COMPAT(imx_ocotp_dt_ids),
};
postcore_platform_driver(imx_ocotp_driver);
