// SPDX-License-Identifier: BSD-3-Clause
/*
 *
 * Copyright (C) 2012 Altera Corporation <www.altera.com>
 * All rights reserved.
 */

#include <firmware.h>
#include <fpga-mgr.h>
#include <command.h>
#include <common.h>
#include <malloc.h>
#include <clock.h>
#include <fcntl.h>
#include <init.h>
#include <io.h>
#include <mach/socfpga/cyclone5-system-manager.h>
#include <mach/socfpga/cyclone5-reset-manager.h>
#include <mach/socfpga/cyclone5-regs.h>
#include <mach/socfpga/cyclone5-sdram.h>
#include <asm/fncpy.h>
#include <mmu.h>
#include <asm/cache.h>

#define FPGAMGRREGS_STAT			0x0
#define FPGAMGRREGS_CTRL			0x4
#define FPGAMGRREGS_DCLKCNT			0x8
#define FPGAMGRREGS_DCLKSTAT			0xc

#define FPGAMGRREGS_MON_GPIO_PORTA_EOI_ADDRESS	0x84c
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS	0x850

#define FPGAMGRREGS_CTRL_CFGWDTH_MASK		0x200
#define FPGAMGRREGS_CTRL_AXICFGEN_MASK		0x100
#define FPGAMGRREGS_CTRL_NCONFIGPULL_MASK	0x4
#define FPGAMGRREGS_CTRL_NCE_MASK		0x2
#define FPGAMGRREGS_CTRL_EN_MASK		0x1
#define FPGAMGRREGS_CTRL_CDRATIO_LSB		6

#define FPGAMGRREGS_STAT_MODE_MASK		0x7
#define FPGAMGRREGS_STAT_MSEL_MASK		0xf8
#define FPGAMGRREGS_STAT_MSEL_LSB		3

#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_CRC_MASK	0x8
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_ID_MASK	0x4
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK	0x2
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK	0x1

/* FPGA Mode */
#define FPGAMGRREGS_MODE_FPGAOFF	0x0
#define FPGAMGRREGS_MODE_RESETPHASE	0x1
#define FPGAMGRREGS_MODE_CFGPHASE	0x2
#define FPGAMGRREGS_MODE_INITPHASE	0x3
#define FPGAMGRREGS_MODE_USERMODE	0x4
#define FPGAMGRREGS_MODE_UNKNOWN	0x5

/* FPGA CD Ratio Value */
#define CDRATIO_x1	0x0
#define CDRATIO_x2	0x1
#define CDRATIO_x4	0x2
#define CDRATIO_x8	0x3

extern void socfpga_sdram_apply_static_cfg(void __iomem *sdrctrlgrp);
extern void socfpga_sdram_apply_static_cfg_end(void *);
extern const u32 socfpga_sdram_apply_static_cfg_sz;

/* Get the FPGA mode */
static uint32_t socfpga_fpgamgr_get_mode(struct fpgamgr *mgr)
{
	return readl(mgr->regs + FPGAMGRREGS_STAT) & FPGAMGRREGS_STAT_MODE_MASK;
}

static int socfpga_fpgamgr_dclkcnt_set(struct fpgamgr *mgr, unsigned long cnt)
{
	uint64_t start;

	/* clear any existing done status */
	if (readl(mgr->regs + FPGAMGRREGS_DCLKSTAT))
		writel(0x1, mgr->regs + FPGAMGRREGS_DCLKSTAT);

	writel(cnt, mgr->regs + FPGAMGRREGS_DCLKCNT);

	/* wait till the dclkcnt done */
	start = get_time_ns();
	while (1) {
		if (readl(mgr->regs + FPGAMGRREGS_DCLKSTAT)) {
			writel(0x1, mgr->regs + FPGAMGRREGS_DCLKSTAT);
			return 0;
		}

		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}
}

/* Start the FPGA programming by initialize the FPGA Manager */
static int socfpga_fpgamgr_program_init(struct fpgamgr *mgr)
{
	unsigned long reg;
	uint32_t ctrl = 0, ratio;
	uint64_t start;

	/* get the MSEL value */
	reg = readl(mgr->regs + FPGAMGRREGS_STAT);
	reg = ((reg & FPGAMGRREGS_STAT_MSEL_MASK) >> FPGAMGRREGS_STAT_MSEL_LSB);

	if (reg & 0x8)
		ctrl |= FPGAMGRREGS_CTRL_CFGWDTH_MASK;
	else
		ctrl &= ~FPGAMGRREGS_CTRL_CFGWDTH_MASK;

	switch (reg & 0xb) {
	case 0xa:
		ratio = CDRATIO_x8;
		break;
	case 0x2:
	case 0x9:
		ratio = CDRATIO_x4;
		break;
	case 0x1:
		ratio = CDRATIO_x2;
		break;
	case 0x8:
	case 0xb:
	default:
		ratio = CDRATIO_x1;
		break;
	}

	ctrl |= ratio << FPGAMGRREGS_CTRL_CDRATIO_LSB;

	/* clear nce bit to allow HPS configuration */
	ctrl &= ~FPGAMGRREGS_CTRL_NCE_MASK;

	/* to enable FPGA Manager drive over configuration line */
	ctrl |= FPGAMGRREGS_CTRL_EN_MASK;

	/* put FPGA into reset phase */
	ctrl |= FPGAMGRREGS_CTRL_NCONFIGPULL_MASK;

	writel(ctrl, mgr->regs + FPGAMGRREGS_CTRL);

	/* (1) wait until FPGA enter reset phase */
	start = get_time_ns();
	while (1) {
		if (socfpga_fpgamgr_get_mode(mgr) == FPGAMGRREGS_MODE_RESETPHASE)
			break;
		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}

	/* release FPGA from reset phase */
	ctrl = readl(mgr->regs + FPGAMGRREGS_CTRL);
	ctrl &= ~FPGAMGRREGS_CTRL_NCONFIGPULL_MASK;
	writel(ctrl, mgr->regs + FPGAMGRREGS_CTRL);

	/* (2) wait until FPGA enter configuration phase */
	start = get_time_ns();
	while (1) {
		if (socfpga_fpgamgr_get_mode(mgr) == FPGAMGRREGS_MODE_CFGPHASE)
			break;
		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}

	/* clear all interrupt in CB Monitor */
	writel(0xFFF, (mgr->regs + FPGAMGRREGS_MON_GPIO_PORTA_EOI_ADDRESS));

	/* enable AXI configuration */
	ctrl = readl(mgr->regs + FPGAMGRREGS_CTRL);
	ctrl |= FPGAMGRREGS_CTRL_AXICFGEN_MASK;
	writel(ctrl, mgr->regs + FPGAMGRREGS_CTRL);

	return 0;
}

/* Ensure the FPGA entering config done */
static int socfpga_fpgamgr_program_poll_cd(struct fpgamgr *mgr)
{
	unsigned long reg;
	uint32_t val;
	uint64_t start;

	/* (3) wait until full config done */
	start = get_time_ns();
	while (1) {
		reg = readl(mgr->regs + FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);

		/* config error */
		if (!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			return -EIO;

		/* config done without error */
		if ((reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			break;

		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}

	/* disable AXI configuration */
	val = readl(mgr->regs + FPGAMGRREGS_CTRL);
	val &= ~FPGAMGRREGS_CTRL_AXICFGEN_MASK;
	writel(val, mgr->regs + FPGAMGRREGS_CTRL);

	return 0;
}

/* Ensure the FPGA entering init phase */
static int socfpga_fpgamgr_program_poll_initphase(struct fpgamgr *mgr)
{
	uint64_t start;

	/* additional clocks for the CB to enter initialization phase */
	if (socfpga_fpgamgr_dclkcnt_set(mgr, 0x4) != 0)
		return -5;

	/* (4) wait until FPGA enter init phase or user mode */
	start = get_time_ns();
	while (1) {
		int mode = socfpga_fpgamgr_get_mode(mgr);

		if (mode == FPGAMGRREGS_MODE_INITPHASE ||
				mode == FPGAMGRREGS_MODE_USERMODE)
			break;

		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}

	return 0;
}

/* Ensure the FPGA entering user mode */
static int socfpga_fpgamgr_program_poll_usermode(struct fpgamgr *mgr)
{
	uint32_t val;
	uint64_t start;

	/* additional clocks for the CB to exit initialization phase */
	if (socfpga_fpgamgr_dclkcnt_set(mgr, 0x5000) != 0)
		return -7;

	/* (5) wait until FPGA enter user mode */
	start = get_time_ns();
	while (1) {
		if (socfpga_fpgamgr_get_mode(mgr) == FPGAMGRREGS_MODE_USERMODE)
			break;
		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;
	}

	/* to release FPGA Manager drive over configuration line */
	val = readl(mgr->regs + FPGAMGRREGS_CTRL);
	val &= ~FPGAMGRREGS_CTRL_EN_MASK;
	writel(val, mgr->regs + FPGAMGRREGS_CTRL);

	return 0;
}

/*
 * Using FPGA Manager to program the FPGA
 * Return 0 for sucess
 */
static int socfpga_fpgamgr_program_start(struct firmware_handler *fh)
{
	struct fpgamgr *mgr = container_of(fh, struct fpgamgr, fh);
	int status;

	/* prior programming the FPGA, all bridges need to be shut off */

	/* disable all signals from hps peripheral controller to fpga */
	writel(0, SYSMGR_FPGAINTF_MODULE);

	dev_dbg(&mgr->dev, "start programming...\n");

	/* initialize the FPGA Manager */
	status = socfpga_fpgamgr_program_init(mgr);
	if (status) {
		dev_err(&mgr->dev, "program init failed with: %pe\n",
				ERR_PTR(status));
		return status;
	}

	return 0;
}

/* Write the RBF data to FPGA Manager */
static int socfpga_fpgamgr_program_write_buf(struct firmware_handler *fh, const void *buf,
		size_t size)
{
	struct fpgamgr *mgr = container_of(fh, struct fpgamgr, fh);
	const uint32_t *buf32 = buf;

	/* write to FPGA Manager AXI data */
	while (size >= sizeof(uint32_t)) {
		writel(*buf32, mgr->regs_data);
		readl(mgr->regs + FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
		buf32++;
		size -= sizeof(uint32_t);
	}

	if (size) {
		const uint8_t *buf8 = (const uint8_t *)buf32;
		uint32_t word = 0;

		while (size--) {
			word |= *buf8;
			word <<= 8;
			buf8++;
		}

		writel(word, mgr->regs_data);
		readl(mgr->regs + FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
	}

	return 0;
}

static int socfpga_fpgamgr_program_finish(struct firmware_handler *fh)
{
	struct fpgamgr *mgr = container_of(fh, struct fpgamgr, fh);
	int status;
	void (*ocram_func)(void __iomem *ocram_base);

	/* Ensure the FPGA entering config done */
	status = socfpga_fpgamgr_program_poll_cd(mgr);
	if (status) {
		dev_err(&mgr->dev, "poll for config done failed with: %pe\n",
				ERR_PTR(status));
		return status;
	}

	dev_dbg(&mgr->dev, "waiting for init phase...\n");

	/* Ensure the FPGA entering init phase */
	status = socfpga_fpgamgr_program_poll_initphase(mgr);
	if (status) {
		dev_err(&mgr->dev, "poll for init phase failed with: %pe\n",
				ERR_PTR(status));
		return status;
	}

	dev_dbg(&mgr->dev, "waiting for user mode...\n");

	/* Ensure the FPGA entering user mode */
	status = socfpga_fpgamgr_program_poll_usermode(mgr);
	if (status) {
		dev_err(&mgr->dev, "poll for user mode with: %pe\n",
				ERR_PTR(status));
		return status;
	}

	remap_range((void *)CYCLONE5_OCRAM_ADDRESS, PAGE_SIZE, MAP_CACHED);

	dev_dbg(&mgr->dev, "Setting APPLYCFG bit...\n");

	ocram_func = fncpy((void __iomem *)CYCLONE5_OCRAM_ADDRESS,
			   &socfpga_sdram_apply_static_cfg,
			   socfpga_sdram_apply_static_cfg_sz);

	sync_caches_for_execution();

	ocram_func((void __iomem *) (CYCLONE5_SDR_ADDRESS +
				     SDR_CTRLGRP_STATICCFG_ADDRESS));

	return 0;
}

/* Get current programmed state of fpga and put in "programmed" parameter */
static int programmed_get(struct param_d *p, void *priv)
{
	struct fpgamgr *mgr = priv;
	mgr->programmed = socfpga_fpgamgr_get_mode(mgr) == FPGAMGRREGS_MODE_USERMODE;
	return 0;
}

static int socfpga_fpgamgr_probe(struct device *dev)
{
	struct resource *iores;
	struct fpgamgr *mgr;
	struct firmware_handler *fh;
	const char *alias = of_alias_get(dev->of_node);
	const char *model = NULL;
	struct param_d *p;
	int ret;

	dev_dbg(dev, "Probing FPGA firmware programmer\n");

	mgr = xzalloc(sizeof(*mgr));
	fh = &mgr->fh;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores)) {
		ret = PTR_ERR(iores);
		goto out;
	}
	mgr->regs = IOMEM(iores->start);

	iores = dev_request_mem_resource(dev, 1);
	if (IS_ERR(iores)) {
		ret = PTR_ERR(iores);
		goto out;
	}
	mgr->regs_data = IOMEM(iores->start);

	if (alias)
		fh->id = xstrdup(alias);
	else
		fh->id = xstrdup("socfpga-fpga");

	fh->open = socfpga_fpgamgr_program_start;
	fh->write = socfpga_fpgamgr_program_write_buf;
	fh->close = socfpga_fpgamgr_program_finish;
	of_property_read_string(dev->of_node, "compatible", &model);
	if (model)
		fh->model = xstrdup(model);
	fh->dev = dev;

	dev_dbg(dev, "Registering FPGA firmware programmer\n");

	mgr->dev.id = DEVICE_ID_SINGLE;
	dev_set_name(&mgr->dev, "fpga");
	mgr->dev.parent = dev;
	ret = register_device(&mgr->dev);
	if (ret)
		goto out;

	p = dev_add_param_bool(&mgr->dev, "programmed", NULL, programmed_get, &mgr->programmed, mgr);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto out_unreg;
	}

	fh->dev = &mgr->dev;
	fh->device_node = dev->of_node;

	ret = firmwaremgr_register(fh);
	if (ret != 0) {
		free(mgr);
		goto out_unreg;
	}

	return 0;
out_unreg:
	unregister_device(&mgr->dev);
out:
	free(fh->id);
	free(mgr);

	return ret;
}

static struct of_device_id socfpga_fpgamgr_id_table[] = {
	{
		.compatible = "altr,socfpga-fpga-mgr",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, socfpga_fpgamgr_id_table);

static struct driver socfpga_fpgamgr_driver = {
	.name = "socfpa-fpgamgr",
	.of_compatible = DRV_OF_COMPAT(socfpga_fpgamgr_id_table),
	.probe = socfpga_fpgamgr_probe,
};
device_platform_driver(socfpga_fpgamgr_driver);
