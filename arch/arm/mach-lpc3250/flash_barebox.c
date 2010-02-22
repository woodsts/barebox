#include <common.h>
#include <command.h>
#include <malloc.h>
#include <init.h>
#include <fs.h>

#include <linux/mtd/nand.h>

#include <asm/io.h>

#include <mach/lpc3250.h>
#include <mach/lpc32xx_nand.h>

struct mlcnand_regs {
	u32 mlc_buff [0x2000]; /* MLC buffer reg */
	u32 mlc_data [0x2000]; /* Start of MLC data buffer */
	u32 mlc_cmd;           /* MLC command reg */
	u32 mlc_addr;          /* MLC address register */
	u32 mlc_enc_ecc;       /* MLC ECC encode reg */
	u32 mlc_dec_ecc;       /* MLC ECC decode reg */
	u32 mlc_autoenc_enc;   /* MLC ECC auto encode reg */
	u32 mlc_autodec_dec;   /* MLC ECC auto decode reg */
	u32 mlc_rpr;           /* MLC read parity reg */
	u32 mlc_wpr;           /* MLC write parity reg */
	u32 mlc_rubp;          /* MLC reset user buffer reg */
	u32 mlc_robp;          /* MLC reset overhead reg */
	u32 mlc_sw_wp_add_low; /* MLC write prot low reg */
	u32 mlc_sw_wp_add_hi;  /* MLC write prot high reg */
	u32 mlc_icr;           /* MLC config reg */
	u32 mlc_time;          /* MLC timing reg */
	u32 mlc_irq_mr;        /* MLC int mask reg */
	u32 mlc_irq_sr;        /* MLC int status reg */
	u32 mlc_reserved3;
	u32 mlc_lock_pr;       /* MLC lock prot reg */
	u32 mlc_isr;           /* MLC status reg */
	u32 mlc_ceh;           /* MLC chip en host reg */
};

/**********************************************************************
* mlc_autoenc_enc register definitions
**********************************************************************/
/* Auto encode enable */
#define MLC_AUTO_EN_ENABLE             (1 << 8)
/* Auto program macro */
#define MLC_AUTO_PROGRAM_CMD(n)        ((n) & 0xFF)

/**********************************************************************
* mlc_sw_wp_add_low register definitions
**********************************************************************/
/* Macro for loading the software write protection low and high
   bound address */
#define MLC_LOAD_LOHI_SWP_ADDR(n)      ((n) & 0x007FFFFF)

/**********************************************************************
* mlc_icr register definitions
**********************************************************************/
/* Software write protection enable */
#define MLC_SWWP_ENABLE                (1 << 3)
/* Enable large block FLASH support (2K+64) */
#define MLC_LARGE_BLK_ENABLE           (1 << 2)
/* Enable 4-word FLASH address support */
#define MLC_ADDR4_ENABLE               (1 << 1)
/* Enable 16-bit NAND data support */
#define MLC_DATA16_ENABLE              (1 << 0)

/**********************************************************************
* mlc_time register definitions
**********************************************************************/
/* Macro for loading the nCE low to dout valid (tCEA) time in clocks */
#define MLC_LOAD_TCEA(n)               (((n) & 0x3) << 24)
/* Macro for loading the read/Write high to busy (tWB/tRB) time in clocks */
#define MLC_LOAD_TWBTRB(n)             (((n) & 0x1F) << 19)
/* Macro for loading the read high to high impedance (tRHZ) time in clocks */
#define MLC_LOAD_TRHZ(n)               (((n) & 0x7) << 16)
/* Macro for loading the read high hold time (tREH) time in clocks */
#define MLC_LOAD_TREH(n)               (((n) & 0xF) << 12)
/* Macro for loading the read pulse width (tRP) time in clocks */
#define MLC_LOAD_TRP(n)                (((n) & 0xF) << 8)
/* Macro for loading the write high hold time (tWH) time in clocks */
#define MLC_LOAD_TWH(n)                (((n) & 0xF) << 4)
/* Macro for loading the write pulse width (tWP) time in clocks */
#define MLC_LOAD_TWP(n)                (((n) & 0xF) << 0)

/**********************************************************************
* mlc_irq_mr and mlc_irq_sr register definitions
**********************************************************************/
/* NAND device ready interrupt mask */
#define MLC_INT_DEV_RDY                (1 << 5)
/* NAND controller ready interrupt mask */
#define MLC_INT_CNTRLLR_RDY            (1 << 4)
/* NAND decode failure interrupt mask */
#define MLC_INT_DECODE_FAIL            (1 << 3)
/* NAND decode error interrupt mask */
#define MLC_INT_DECODE_ERR             (1 << 2)
/* NAND encode/decode ready interrupt mask */
#define MLC_INT_ENCDEC_RDY             (1 << 1)
/* NAND software write protect fault interrupt mask */
#define MLC_INT_SWWP_FAULT             (1 << 0)

/**********************************************************************
* mlc_lock_pr register definitions
**********************************************************************/
/* Value for unlocking the mlc_sw_wp_add_low, mlc_sw_wp_add_hi,
   mlc_icr, TBB (WP_REG?), and mlc_time registers for a single access */
#define MLC_UNLOCK_REG_VALUE           0x0000A25E

/**********************************************************************
* mlc_isr register definitions
**********************************************************************/
/* NAND status decode failure mask */
#define MLC_DECODE_FAIL_STS            (1 << 6)
/* NAND status R/S symbol error mask word */
#define MLC_DECODE_ERRMASK_STS         0x00000030
/* NAND status R/S symbol error - one symbol error detected value */
#define MLC_DECODE_FAIL_1S_STS         0x00000000
/* NAND status R/S symbol error - 2 symbol errors detected value */
#define MLC_DECODE_FAIL_2S_STS         0x00000010
/* NAND status R/S symbol error - 3 symbol errors detected value */
#define MLC_DECODE_FAIL_3S_STS         0x00000020
/* NAND status R/S symbol error - 4 symbol errors detected value */
#define MLC_DECODE_FAIL_4S_STS         0x00000030
/* NAND status error detected mask */
#define MLC_DECODE_ERR_DETECT_STS      (1 << 3)
/* NAND status ECC ready mask */
#define MLC_ECC_RDY_STS                (1 << 2)
/* NAND status controller ready mask */
#define MLC_CNTRLLR_RDY_STS            (1 << 1)
/* NAND status device ready mask */
#define MLC_DEV_RDY_STS                (1 << 0)

/**********************************************************************
* mlc_ceh register definitions
**********************************************************************/
/* NAND normal nCE operation mask (0 = forced nCE assertion) */
#define MLC_NORMAL_NCE                 _BIT(0)

/* Macro pointing to MLC NAND controller registers */
#define MLCNAND ((struct mlcnand_regs *)(MLC_BASE))

extern unsigned long __bss_start;

static void __bare_init mlc_addr(u8 addr)
{
	writel(addr, &MLCNAND->mlc_addr);
}

static void __bare_init mlc_wait_ready(void)
{
	while ((readl(&MLCNAND->mlc_isr) & MLC_DEV_RDY_STS) == 0) ;
}

static void __bare_init mlc_cmd(u8 cmd)
{
	writel(cmd, &MLCNAND->mlc_cmd);
}

static u8 __bare_init mlc_get_status(void)
{
	mlc_cmd(NAND_CMD_STATUS);
	mlc_wait_ready();

	return readl(&MLCNAND->mlc_data[0]);
}

static void __bare_init mlc_write_addr(int block, int page)
{
	u32 nandaddr;

	/* Block  Page  Index */
	/* 31..13 12..8 7..0  */

	nandaddr = (page & 0x1F) << 8;
	nandaddr = nandaddr | ((block & 0xFFF) << 13);

	/* Write block and page address */
	mlc_addr((nandaddr >> 0)  & 0xFF);
	mlc_addr((nandaddr >> 8)  & 0xFF);
	mlc_addr((nandaddr >> 16) & 0xFF);
	if (4 == 4) { /* FIXME: addr cycles */
		mlc_addr((nandaddr >> 24) & 0xFF);
	}
}

static int mlc_write_page(int block, int page, u8 *buff, u8 *extrabuff)
{
	int bytes = 0, towrite;
	u8 status;
	volatile u32 tmp;

	/*
	 * The sequence must be modified follows:
	 * 1. Write Serial Input command (0x80) to Command register.
	 * 2. Write page address data to Address register.
	 * 3. Write Start Encode register.
	 * 4. Write 518 bytes of NAND data.
	 * 5. Write MLC NAND Write Parity register.
	 * 6. Read Status register.18
	 * 7. Wait controller Ready status bit set.
	 * 8. Write Auto Program command to Command register.
	 */

	/* Force nCE for the entire cycle */
	writel(MLC_NORMAL_NCE, &MLCNAND->mlc_ceh);

	/* Issue page write1 command */
	mlc_cmd(NAND_CMD_READ0);
	mlc_cmd(NAND_CMD_SEQIN);

	/* Write address */
	mlc_write_addr(block, page);

	/* Start encode */
	writel(1, &MLCNAND->mlc_enc_ecc);

	/* Write 512 bytes of data */
	towrite = 512;
	while (towrite > 0) {
		tmp = *buff;
		buff++;
		tmp |= (*buff << 8);
		buff++;
		tmp |= (*buff << 16);
		buff++;
		tmp |= (*buff << 24);
		buff++;
		writel(tmp, &MLCNAND->mlc_data[0]);
		towrite = towrite - 4;
	}
	bytes = bytes + 512;

	/* Write 8 dummy bytes */
	if (!extrabuff) {
		writel(0xffffffff, &MLCNAND->mlc_data[0]);
		writew(0xffff, &MLCNAND->mlc_data[0]);
	} else {
		tmp = (*extrabuff | (*(extrabuff + 1) << 8) |
		(*(extrabuff + 2) << 16) | (*(extrabuff + 3) << 24));
		writel(tmp, &MLCNAND->mlc_data[0]);
		extrabuff += 4;
		tmp = (*extrabuff | (*(extrabuff + 1) << 8));
		writew(tmp, &MLCNAND->mlc_data[0]);
	}

	/* Write NAND parity */
	writel(1, &MLCNAND->mlc_wpr);

	/* Wait for controller ready */
	while ((readl(&MLCNAND->mlc_isr) & MLC_CNTRLLR_RDY_STS) == 0);

	/* Issue page write2 command */
	mlc_cmd(NAND_CMD_PAGEPROG);

	/* Deassert nCE */
	writel(0, &MLCNAND->mlc_ceh);

	/* Wait for device ready */
	mlc_wait_ready();

	/* Read status */
	status = mlc_get_status();
	if ((status & 0x1) != 0) {
		/* Program was not good */
		bytes = 0;
	}

	return bytes;
}

static int __bare_init mlc_read_page(int block, int page, u8 *buff, u8 *extrabuff)
{
	int bytes = 0, toread;
	volatile u32 tmp;
	volatile u16 tmp16;

	/*
	 * 1. Write Read Mode (1) command (0x00) to Command register.
	 * 2. Write Read Start command (0x30) to Command register.
	 * 3. Write address data to Address register.
	 * 4. Read controllers Status register.
	 * 5. Wait until NAND Ready status bit set.
	 * 6. Write Start Decode register.
	 * 7. Read 518 NAND data bytes.
	 * 8. Write Read Parity register.
	 * 9. Read Status register.10
	 * 10. Wait until ECC Ready status bit set.
	 * 11. Check error detection/correction status.11
	 * 12. If error was detected, read 518/528 bytes from serial Data Buffer.
	 */

	/* Force nCE for the entire cycle */
	writel(MLC_NORMAL_NCE, &MLCNAND->mlc_ceh);

	/* Issue page read1 command */
	mlc_cmd(NAND_CMD_READ0);

	/* Write address */
	mlc_write_addr(block, page);

	/* Wait for ready */
	mlc_wait_ready();

	/* Start decode */
	writel(1, &MLCNAND->mlc_dec_ecc);

	/* Read data */
	toread = 512;
	while (toread > 0) {
		tmp = readl(&MLCNAND->mlc_data[0]);
		*buff = ((tmp >> 0) & 0xFF);
		buff++;
		*buff = ((tmp >> 8) & 0xFF);
		buff++;
		*buff = ((tmp >> 16) & 0xFF);
		buff++;
		*buff = ((tmp >> 24) & 0xFF);
		buff++;
		toread = toread - 4;
	}
	bytes = bytes + 512;

	/* Read 6 dummy bytes */
	if (extrabuff == NULL) {
		tmp = readl(&MLCNAND->mlc_data[0]);
		tmp16 = readw(&MLCNAND->mlc_data[0]);
	} else {
		tmp = readl(&MLCNAND->mlc_data[0]);
		*extrabuff = ((tmp >> 0) & 0xFF);
		extrabuff++;
		*extrabuff = ((tmp >> 8) & 0xFF);
		extrabuff++;
		*extrabuff = ((tmp >> 16) & 0xFF);
		extrabuff++;
		*extrabuff = ((tmp >> 24) & 0xFF);
		extrabuff++;
		tmp16 = readw(&MLCNAND->mlc_data[0]);
		*extrabuff = ((tmp >> 0) & 0xFF);
		extrabuff++;
		*extrabuff = ((tmp >> 8) & 0xFF);
	}

	/* Write read parity register */
	writel(1, &MLCNAND->mlc_rpr);

	/* Wait for ECC ready */
	while ((readl(&MLCNAND->mlc_isr) & MLC_ECC_RDY_STS) == 0);

	/* Deassert nCE */
	writel(0, &MLCNAND->mlc_ceh);

	/* Wait for device ready */
	mlc_wait_ready();

	return bytes;
}

static int nand_erase_block(int block)
{
	u8 status;

	/* Issue block erase1 command */
	mlc_cmd(NAND_CMD_ERASE1);

	/* Write block and page address */
	mlc_addr(((block << 5) & 0x00E0));
	mlc_addr(((block >> 3) & 0x00FF));
	if (4 == 4)
		mlc_addr(((block >> 11) & 0x0003));

	/* Issue page erase2 command */
	mlc_cmd(NAND_CMD_ERASE2);

	/* Wait for ready */
	mlc_wait_ready();

	/* Read status */
	status = mlc_get_status();
	if ((status & 0x1) == 0) {
		/* Erase was good */
		return 0;
	}

	return -1;
}

static void __bare_init mlc_init(void)
{
	writel(0x2, &CLKPWR->clkpwr_nand_clk_ctrl);

	/* Configure controller for no software write protection, x8 bus
	   width, large block device, and 4 address words */
	writel(MLC_UNLOCK_REG_VALUE, &MLCNAND->mlc_lock_pr);
	writel(MLC_LARGE_BLK_ENABLE | MLC_ADDR4_ENABLE, &MLCNAND->mlc_icr);

	/* Make sure MLC interrupts are disabled */
	writel(0, &MLCNAND->mlc_irq_mr);

	/* Normal chip enable operation */
	writel(MLC_NORMAL_NCE, &MLCNAND->mlc_ceh);

	/* Setup NAND timing */
	writel(MLC_UNLOCK_REG_VALUE, &MLCNAND->mlc_lock_pr);

	writel(0x03ffffff, &MLCNAND->mlc_time);

	mlc_cmd(NAND_CMD_RESET);
	mlc_wait_ready();

	/* Reset buffer pointer */
	writel(1, &MLCNAND->mlc_rubp);

}

#define NAND_PAGE_SIZE	512
#define PAGES_PER_BLOCK	32

void __bare_init lpc3250_mlc_nand_load_image(void *dest)
{
	int size = (unsigned int)&__bss_start - TEXT_BASE;
	int page = 1, block = 0, i;
	void *buf = (void *)TEXT_BASE;

	size = (size + NAND_PAGE_SIZE - 1) & ~(NAND_PAGE_SIZE - 1);

	mlc_init();

	for (i = 0; i < size; i += NAND_PAGE_SIZE) {
		mlc_read_page(block, page, buf, NULL);
		buf += NAND_PAGE_SIZE;
		page++;
		if (!(page % PAGES_PER_BLOCK)) {
			block++;
			page = 0;
		}
	}
}

static int
do_flash_barebox(struct command *cmdtp, int argc, char *argv[])
{
	u32 tmp;
	unsigned int imagesize, page, block, i;
	void *buf;
	u32 *first_page;
	u8 extrabuf[16];
	void *image;
	int ofs;

	if (argc < 2) {
		barebox_cmd_usage(cmdtp);
		return 1;
	}

	image = read_file(argv[1], &imagesize);
	if (!image)
		return 1;

	imagesize = (imagesize + NAND_PAGE_SIZE - 1) & ~(NAND_PAGE_SIZE - 1);

	printf("flashing '%s'\n", argv[1]);

	mlc_init();

	/* Read the device ID */
	mlc_cmd(NAND_CMD_READID);
	mlc_addr(0);
	tmp = MLCNAND->mlc_data[0];

	i = 0;
	ofs = 0;
	do {
		nand_erase_block(i);
		i += 1;
		ofs += PAGES_PER_BLOCK * NAND_PAGE_SIZE;
	} while (ofs < imagesize);

	buf = xzalloc(512);
	first_page = buf;
	first_page[0] = 0xd2;
	first_page[1] = 0x2d;
	first_page[2] = 0xd2;
	first_page[3] = 0x2d;
	first_page[4] = 0x13;
	first_page[5] = 0xec;
	first_page[6] = 0x13;
	first_page[7] = 0xec;
	first_page[8] = 0x13;
	first_page[9] = 0xec;
	first_page[10] = 0x13;
	first_page[11] = 0xec;
	first_page[12] = 0xaa;

	memset(extrabuf, 0xff, 16);
	extrabuf[4] = 0xfe;
	mlc_write_page(0, 0, buf, extrabuf);

	free(buf);

	buf = (void *)image;
	page = 1;
	block = 0;
	for (i = 0; i < imagesize; i += NAND_PAGE_SIZE) {
		mlc_write_page(block, page, buf, NULL);
		buf += NAND_PAGE_SIZE;
		page++;
		if (!(page % PAGES_PER_BLOCK)) {
			block++;
			page = 0;
		}
	}

	free(image);

	writel(0x5, &CLKPWR->clkpwr_nand_clk_ctrl);

	printf("done\n");

	return 0;
}

static const __maybe_unused char cmd_flash_barebox_help[] =
"Usage: flash_barebox <file>\n"
"This copies <file> to NAND flash. <file> should be a regular barebox.bin\n"
"file which can then be booted from NAND\n";

BAREBOX_CMD_START(flash_barebox)
	.cmd = do_flash_barebox,
	.usage = "flash barebox file to nand",
	BAREBOX_CMD_HELP(cmd_flash_barebox_help)
BAREBOX_CMD_END
