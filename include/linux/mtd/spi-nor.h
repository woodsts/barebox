/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_MTD_SPI_NOR_H
#define __LINUX_MTD_SPI_NOR_H

#include <linux/bitops.h>
#include <linux/mutex.h>

/*
 * Manufacturer IDs
 *
 * The first byte returned from the flash after sending opcode SPINOR_OP_RDID.
 * Sometimes these are the same as CFI IDs, but sometimes they aren't.
 */
#define SNOR_MFR_ATMEL		CFI_MFR_ATMEL
#define SNOR_MFR_GIGADEVICE	0xc8
#define SNOR_MFR_INTEL		CFI_MFR_INTEL
#define SNOR_MFR_ST		CFI_MFR_ST	/* ST Micro */
#define SNOR_MFR_MICRON		CFI_MFR_MICRON	/* Micron */
#define SNOR_MFR_MACRONIX	CFI_MFR_MACRONIX
#define SNOR_MFR_SPANSION	CFI_MFR_AMD
#define SNOR_MFR_SST		CFI_MFR_SST
#define SNOR_MFR_WINBOND	0xef /* Also used by some Spansion */

/*
 * Note on opcode nomenclature: some opcodes have a format like
 * SPINOR_OP_FUNCTION{4,}_x_y_z. The numbers x, y, and z stand for the number
 * of I/O lines used for the opcode, address, and data (respectively). The
 * FUNCTION has an optional suffix of '4', to represent an opcode which
 * requires a 4-byte (32-bit) address.
 */

/* Flash opcodes. */
#define SPINOR_OP_WREN		0x06	/* Write enable */
#define SPINOR_OP_RDSR		0x05	/* Read status register */
#define SPINOR_OP_WRSR		0x01	/* Write status register 1 byte */
#define SPINOR_OP_RDSR2		0x3f	/* Read status register 2 */
#define SPINOR_OP_WRSR2		0x3e	/* Write status register 2 */
#define SPINOR_OP_READ		0x03	/* Read data bytes (low frequency) */
#define SPINOR_OP_READ_FAST	0x0b	/* Read data bytes (high frequency) */
#define SPINOR_OP_READ_1_1_2	0x3b	/* Read data bytes (Dual Output SPI) */
#define SPINOR_OP_READ_1_2_2	0xbb	/* Read data bytes (Dual I/O SPI) */
#define SPINOR_OP_READ_1_1_4	0x6b	/* Read data bytes (Quad Output SPI) */
#define SPINOR_OP_READ_1_4_4	0xeb	/* Read data bytes (Quad I/O SPI) */
#define SPINOR_OP_PP		0x02	/* Page program (up to 256 bytes) */
#define SPINOR_OP_PP_1_1_4	0x32	/* Quad page program */
#define SPINOR_OP_PP_1_4_4	0x38	/* Quad page program */
#define SPINOR_OP_BE_4K		0x20	/* Erase 4KiB block */
#define SPINOR_OP_BE_4K_PMC	0xd7	/* Erase 4KiB block on PMC chips */
#define SPINOR_OP_BE_32K	0x52	/* Erase 32KiB block */
#define SPINOR_OP_CHIP_ERASE	0xc7	/* Erase whole flash chip */
#define SPINOR_OP_SE		0xd8	/* Sector erase (usually 64KiB) */
#define SPINOR_OP_RDID		0x9f	/* Read JEDEC ID */
#define SPINOR_OP_RDSFDP	0x5a	/* Read SFDP */
#define SPINOR_OP_RDCR		0x35	/* Read configuration register */
#define SPINOR_OP_RDFSR		0x70	/* Read flag status register */
#define SPINOR_OP_CLFSR		0x50	/* Clear flag status register */
#define SPINOR_OP_RDEAR		0xc8	/* Read Extended Address Register */
#define SPINOR_OP_WREAR		0xc5	/* Write Extended Address Register */
#define SPINOR_OP_GBULK		0x98	/* Global Block Unlock Protection */

/* 4-byte address opcodes - used on Spansion and some Macronix flashes. */
#define SPINOR_OP_READ_4B	0x13	/* Read data bytes (low frequency) */
#define SPINOR_OP_READ_FAST_4B	0x0c	/* Read data bytes (high frequency) */
#define SPINOR_OP_READ_1_1_2_4B	0x3c	/* Read data bytes (Dual Output SPI) */
#define SPINOR_OP_READ_1_2_2_4B	0xbc	/* Read data bytes (Dual I/O SPI) */
#define SPINOR_OP_READ_1_1_4_4B	0x6c	/* Read data bytes (Quad Output SPI) */
#define SPINOR_OP_READ_1_4_4_4B	0xec	/* Read data bytes (Quad I/O SPI) */
#define SPINOR_OP_PP_4B		0x12	/* Page program (up to 256 bytes) */
#define SPINOR_OP_PP_1_1_4_4B	0x34	/* Quad page program */
#define SPINOR_OP_PP_1_4_4_4B	0x3e	/* Quad page program */
#define SPINOR_OP_BE_4K_4B	0x21	/* Erase 4KiB block */
#define SPINOR_OP_BE_32K_4B	0x5c	/* Erase 32KiB block */
#define SPINOR_OP_SE_4B		0xdc	/* Sector erase (usually 64KiB) */

/* Double Transfer Rate opcodes - defined in JEDEC JESD216B. */
#define SPINOR_OP_READ_1_1_1_DTR	0x0d
#define SPINOR_OP_READ_1_2_2_DTR	0xbd
#define SPINOR_OP_READ_1_4_4_DTR	0xed

#define SPINOR_OP_READ_1_1_1_DTR_4B	0x0e
#define SPINOR_OP_READ_1_2_2_DTR_4B	0xbe
#define SPINOR_OP_READ_1_4_4_DTR_4B	0xee

/* Used for SST flashes only. */
#define SPINOR_OP_BP		0x02	/* Byte program */
#define SPINOR_OP_WRDI		0x04	/* Write disable */
#define SPINOR_OP_AAI_WP	0xad	/* Auto address increment word program */

/* Used for Macronix and Winbond flashes. */
#define SPINOR_OP_EN4B		0xb7	/* Enter 4-byte mode */
#define SPINOR_OP_EX4B		0xe9	/* Exit 4-byte mode */

/* Used for Spansion flashes only. */
#define SPINOR_OP_BRWR		0x17	/* Bank register write */

/* Status Register bits. */
#define SR_WIP			BIT(0)	/* Write in progress */
#define SR_WEL			BIT(1)	/* Write enable latch */
/* meaning of other SR_* bits may differ between vendors */
#define SR_BP0			BIT(2)	/* Block protect 0 */
#define SR_BP1			BIT(3)	/* Block protect 1 */
#define SR_BP2			BIT(4)	/* Block protect 2 */
#define SR_SRWD			BIT(7)	/* SR write protect */

#define SR_QUAD_EN_MX		BIT(6)	/* Macronix Quad I/O */

/* Flag Status Register bits */
#define FSR_READY		BIT(7)

/* Configuration Register bits. */
#define CR_QUAD_EN_SPAN		BIT(1)	/* Spansion Quad I/O */

/* Supported SPI protocols */
#define SNOR_PROTO_INST_MASK   GENMASK(23, 16)
#define SNOR_PROTO_INST_SHIFT  16
#define SNOR_PROTO_INST(_nbits)        \
       ((((unsigned long)(_nbits)) << SNOR_PROTO_INST_SHIFT) & \
        SNOR_PROTO_INST_MASK)

#define SNOR_PROTO_ADDR_MASK   GENMASK(15, 8)
#define SNOR_PROTO_ADDR_SHIFT  8
#define SNOR_PROTO_ADDR(_nbits)        \
       ((((unsigned long)(_nbits)) << SNOR_PROTO_ADDR_SHIFT) & \
        SNOR_PROTO_ADDR_MASK)

#define SNOR_PROTO_DATA_MASK   GENMASK(7, 0)
#define SNOR_PROTO_DATA_SHIFT  0
#define SNOR_PROTO_DATA(_nbits)        \
       ((((unsigned long)(_nbits)) << SNOR_PROTO_DATA_SHIFT) & \
        SNOR_PROTO_DATA_MASK)

#define SNOR_PROTO_STR(_inst_nbits, _addr_nbits, _data_nbits)  \
       (SNOR_PROTO_INST(_inst_nbits) |                         \
        SNOR_PROTO_ADDR(_addr_nbits) |                         \
        SNOR_PROTO_DATA(_data_nbits))

#define SPI_NOR_MAX_ADDR_WIDTH	4

enum spi_nor_protocol {
       SNOR_PROTO_1_1_1 = SNOR_PROTO_STR(1, 1, 1),
       SNOR_PROTO_1_1_2 = SNOR_PROTO_STR(1, 1, 2),
       SNOR_PROTO_1_1_4 = SNOR_PROTO_STR(1, 1, 4),
       SNOR_PROTO_1_2_2 = SNOR_PROTO_STR(1, 2, 2),
       SNOR_PROTO_1_4_4 = SNOR_PROTO_STR(1, 4, 4),
       SNOR_PROTO_2_2_2 = SNOR_PROTO_STR(2, 2, 2),
       SNOR_PROTO_4_4_4 = SNOR_PROTO_STR(4, 4, 4),
};

static inline u8 spi_nor_get_protocol_inst_nbits(enum spi_nor_protocol proto)
{
       return ((unsigned long)(proto & SNOR_PROTO_INST_MASK)) >>
               SNOR_PROTO_INST_SHIFT;
}

static inline u8 spi_nor_get_protocol_addr_nbits(enum spi_nor_protocol proto)
{
       return ((unsigned long)(proto & SNOR_PROTO_ADDR_MASK)) >>
               SNOR_PROTO_ADDR_SHIFT;
}

static inline u8 spi_nor_get_protocol_data_nbits(enum spi_nor_protocol proto)
{
       return ((unsigned long)(proto & SNOR_PROTO_DATA_MASK)) >>
               SNOR_PROTO_DATA_SHIFT;
}

static inline u8 spi_nor_get_protocol_width(enum spi_nor_protocol proto)
{
       return spi_nor_get_protocol_data_nbits(proto);
}

#define SPI_NOR_MAX_CMD_SIZE	8
enum spi_nor_ops {
	SPI_NOR_OPS_READ = 0,
	SPI_NOR_OPS_WRITE,
	SPI_NOR_OPS_ERASE,
	SPI_NOR_OPS_LOCK,
	SPI_NOR_OPS_UNLOCK,
};

enum spi_nor_option_flags {
	SNOR_F_USE_FSR		= BIT(0),
};

/**
 * struct spi_nor - Structure for defining a the SPI NOR layer
 * @mtd:		point to a mtd_info structure
 * @lock:		the lock for the read/write/erase/lock/unlock operations
 * @dev:		point to a spi device, or a spi nor controller device.
 * @page_size:		the page size of the SPI NOR
 * @addr_width:		number of address bytes
 * @erase_opcode:	the opcode for erasing a sector
 * @read_opcode:	the read opcode
 * @read_dummy:		the dummy needed by the read operation
 * @program_opcode:	the program opcode
 * @sst_write_second:	used by the SST write operation
 * @flags:		flag options for the current SPI-NOR (SNOR_F_*)
 * @read_proto:		the SPI protocol for read operations
 * @write_proto:	the SPI protocol for write operations
 * @reg_proto		the SPI protocol for read_reg/write_reg/erase operations
 * @cfg:		used by the read_xfer/write_xfer
 * @cmd_buf:		used by the write_reg
 * @prepare:		[OPTIONAL] do some preparations for the
 *			read/write/erase/lock/unlock operations
 * @unprepare:		[OPTIONAL] do some post work after the
 *			read/write/erase/lock/unlock operations
 * @read_reg:		[DRIVER-SPECIFIC] read out the register
 * @write_reg:		[DRIVER-SPECIFIC] write data to the register
 * @read:		[DRIVER-SPECIFIC] read data from the SPI NOR
 * @write:		[DRIVER-SPECIFIC] write data to the SPI NOR
 * @erase:		[DRIVER-SPECIFIC] erase a sector of the SPI NOR
 *			at the offset @offs; if not provided by the driver,
 *			spi-nor will send the erase opcode via write_reg()
 * @priv:		the private data
 */
struct spi_nor {
	struct mtd_info		*mtd;
	struct mutex		lock;
	struct device		*dev;
	const struct flash_info	*info;
	u32			page_size;
	u8			addr_width;
	u8			erase_opcode;
	u8			read_opcode;
	u8			read_dummy;
	u8			program_opcode;
	enum spi_nor_protocol	read_proto;
	enum spi_nor_protocol	write_proto;
	enum spi_nor_protocol	reg_proto;
	bool			sst_write_second;
	u32			flags;
	u8			cmd_buf[SPI_NOR_MAX_CMD_SIZE];

	int (*prepare)(struct spi_nor *nor, enum spi_nor_ops ops);
	void (*unprepare)(struct spi_nor *nor, enum spi_nor_ops ops);
	int (*read_reg)(struct spi_nor *nor, u8 opcode, u8 *buf, int len);
	int (*write_reg)(struct spi_nor *nor, u8 opcode, u8 *buf, int len);

	int (*read)(struct spi_nor *nor, loff_t from,
			size_t len, size_t *retlen, u_char *read_buf);
	void (*write)(struct spi_nor *nor, loff_t to,
			size_t len, size_t *retlen, const u_char *write_buf);
	int (*erase)(struct spi_nor *nor, loff_t offs);

	void *priv;
};

/**
 * struct spi_nor_hwcaps - Structure for describing the hardware capabilies
 * supported by the SPI controller (bus master).
 * @mask:              the bitmask listing all the supported hw capabilies
 */
struct spi_nor_hwcaps {
       u32     mask;
};

/*
 *(Fast) Read capabilities.
 * MUST be ordered by priority: the higher bit position, the higher priority.
 * As a matter of performances, it is relevant to use Quad SPI protocols first,
 * then Dual SPI protocols before Fast Read and lastly (Slow) Read.
 */
#define SNOR_HWCAPS_READ_MASK          GENMASK(7, 0)
#define SNOR_HWCAPS_READ               BIT(0)
#define SNOR_HWCAPS_READ_FAST          BIT(1)

#define SNOR_HWCAPS_READ_DUAL          GENMASK(4, 2)
#define SNOR_HWCAPS_READ_1_1_2         BIT(2)
#define SNOR_HWCAPS_READ_1_2_2         BIT(3)
#define SNOR_HWCAPS_READ_2_2_2         BIT(4)

#define SNOR_HWCAPS_READ_QUAD          GENMASK(7, 5)
#define SNOR_HWCAPS_READ_1_1_4         BIT(5)
#define SNOR_HWCAPS_READ_1_4_4         BIT(6)
#define SNOR_HWCAPS_READ_4_4_4         BIT(7)

/*
 * Page Program capabilities.
 * MUST be ordered by priority: the higher bit position, the higher priority.
 * Like (Fast) Read capabilities, Quad SPI protocols are preferred to the
 * legacy SPI 1-1-1 protocol.
 * Note that Dual Page Programs are not supported because there is no existing
 * JEDEC/SFDP standard to define them. Also at this moment no SPI flash memory
 * implements such commands.
 */
#define SNOR_HWCAPS_PP_MASK    GENMASK(19, 16)
#define SNOR_HWCAPS_PP         BIT(16)

#define SNOR_HWCAPS_PP_QUAD    GENMASK(19, 17)
#define SNOR_HWCAPS_PP_1_1_4   BIT(17)
#define SNOR_HWCAPS_PP_1_4_4   BIT(18)
#define SNOR_HWCAPS_PP_4_4_4   BIT(19)

/**
 * spi_nor_scan() - scan the SPI NOR
 * @nor:	the spi_nor structure
 * @name:	the chip type name
 * @hwcaps:	the hardware capabilities supported by the controller driver
 * @use_large_blocks: prefer large blocks even if 4k blocks are supported
 *
 * The drivers can use this fuction to scan the SPI NOR.
 * In the scanning, it will try to get all the necessary information to
 * fill the mtd_info{} and the spi_nor{}.
 *
 * The chip type name can be provided through the @name parameter.
 *
 * Return: 0 for success, others for failure.
 */
int spi_nor_scan(struct spi_nor *nor, const char *name,
		 const struct spi_nor_hwcaps *hwcaps,
		 bool use_large_blocks);

#endif
