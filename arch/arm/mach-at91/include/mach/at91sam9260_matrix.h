/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2007 Atmel Corporation */

/*
 * arch/arm/mach-at91/include/mach/at91sam9260_matrix.h
 *
 * Memory Controllers (MATRIX, EBI) - System peripherals registers.
 * Based on AT91SAM9260 datasheet revision B.
 */

#ifndef AT91SAM9260_MATRIX_H
#define AT91SAM9260_MATRIX_H

#define AT91SAM9260_MATRIX_MCFG0	(0x00)	/* Master Configuration Register 0 */
#define AT91SAM9260_MATRIX_MCFG1	(0x04)	/* Master Configuration Register 1 */
#define AT91SAM9260_MATRIX_MCFG2	(0x08)	/* Master Configuration Register 2 */
#define AT91SAM9260_MATRIX_MCFG3	(0x0C)	/* Master Configuration Register 3 */
#define AT91SAM9260_MATRIX_MCFG4	(0x10)	/* Master Configuration Register 4 */
#define AT91SAM9260_MATRIX_MCFG5	(0x14)	/* Master Configuration Register 5 */
#define		AT91SAM9260_MATRIX_ULBT		(7 << 0)	/* Undefined Length Burst Type */
#define			AT91SAM9260_MATRIX_ULBT_INFINITE	(0 << 0)
#define			AT91SAM9260_MATRIX_ULBT_SINGLE		(1 << 0)
#define			AT91SAM9260_MATRIX_ULBT_FOUR		(2 << 0)
#define			AT91SAM9260_MATRIX_ULBT_EIGHT		(3 << 0)
#define			AT91SAM9260_MATRIX_ULBT_SIXTEEN	(4 << 0)

#define AT91SAM9260_MATRIX_SCFG0	(0x40)	/* Slave Configuration Register 0 */
#define AT91SAM9260_MATRIX_SCFG1	(0x44)	/* Slave Configuration Register 1 */
#define AT91SAM9260_MATRIX_SCFG2	(0x48)	/* Slave Configuration Register 2 */
#define AT91SAM9260_MATRIX_SCFG3	(0x4C)	/* Slave Configuration Register 3 */
#define AT91SAM9260_MATRIX_SCFG4	(0x50)	/* Slave Configuration Register 4 */
#define		AT91SAM9260_MATRIX_SLOT_CYCLE		(0xff <<  0)	/* Maximum Number of Allowed Cycles for a Burst */
#define		AT91SAM9260_MATRIX_DEFMSTR_TYPE	(3    << 16)	/* Default Master Type */
#define			AT91SAM9260_MATRIX_DEFMSTR_TYPE_NONE	(0 << 16)
#define			AT91SAM9260_MATRIX_DEFMSTR_TYPE_LAST	(1 << 16)
#define			AT91SAM9260_MATRIX_DEFMSTR_TYPE_FIXED	(2 << 16)
#define		AT91SAM9260_MATRIX_FIXED_DEFMSTR	(7    << 18)	/* Fixed Index of Default Master */
#define		AT91SAM9260_MATRIX_ARBT		(3    << 24)	/* Arbitration Type */
#define			AT91SAM9260_MATRIX_ARBT_ROUND_ROBIN	(0 << 24)
#define			AT91SAM9260_MATRIX_ARBT_FIXED_PRIORITY	(1 << 24)

#define AT91SAM9260_MATRIX_PRAS0	(0x80)	/* Priority Register A for Slave 0 */
#define AT91SAM9260_MATRIX_PRAS1	(0x88)	/* Priority Register A for Slave 1 */
#define AT91SAM9260_MATRIX_PRAS2	(0x90)	/* Priority Register A for Slave 2 */
#define AT91SAM9260_MATRIX_PRAS3	(0x98)	/* Priority Register A for Slave 3 */
#define AT91SAM9260_MATRIX_PRAS4	(0xA0)	/* Priority Register A for Slave 4 */
#define		AT91SAM9260_MATRIX_M0PR		(3 << 0)	/* Master 0 Priority */
#define		AT91SAM9260_MATRIX_M1PR		(3 << 4)	/* Master 1 Priority */
#define		AT91SAM9260_MATRIX_M2PR		(3 << 8)	/* Master 2 Priority */
#define		AT91SAM9260_MATRIX_M3PR		(3 << 12)	/* Master 3 Priority */
#define		AT91SAM9260_MATRIX_M4PR		(3 << 16)	/* Master 4 Priority */
#define		AT91SAM9260_MATRIX_M5PR		(3 << 20)	/* Master 5 Priority */

#define AT91SAM9260_MATRIX_MRCR	(0x100)	/* Master Remap Control Register */
#define		AT91SAM9260_MATRIX_RCB0		(1 << 0)	/* Remap Command for AHB Master 0 (ARM926EJ-S Instruction Master) */
#define		AT91SAM9260_MATRIX_RCB1		(1 << 1)	/* Remap Command for AHB Master 1 (ARM926EJ-S Data Master) */

#define AT91SAM9260_MATRIX_EBICSA	(0x11C)	/* EBI Chip Select Assignment Register */
#define		AT91SAM9260_MATRIX_CS1A		(1 << 1)	/* Chip Select 1 Assignment */
#define			AT91SAM9260_MATRIX_CS1A_SMC		(0 << 1)
#define			AT91SAM9260_MATRIX_CS1A_SDRAMC		(1 << 1)
#define		AT91SAM9260_MATRIX_CS3A		(1 << 3)	/* Chip Select 3 Assignment */
#define			AT91SAM9260_MATRIX_CS3A_SMC		(0 << 3)
#define			AT91SAM9260_MATRIX_CS3A_SMC_SMARTMEDIA	(1 << 3)
#define		AT91SAM9260_MATRIX_CS4A		(1 << 4)	/* Chip Select 4 Assignment */
#define			AT91SAM9260_MATRIX_CS4A_SMC		(0 << 4)
#define			AT91SAM9260_MATRIX_CS4A_SMC_CF1	(1 << 4)
#define		AT91SAM9260_MATRIX_CS5A		(1 << 5)	/* Chip Select 5 Assignment */
#define			AT91SAM9260_MATRIX_CS5A_SMC		(0 << 5)
#define			AT91SAM9260_MATRIX_CS5A_SMC_CF2	(1 << 5)
#define		AT91SAM9260_MATRIX_DBPUC		(1 << 8)	/* Data Bus Pull-up Configuration */
#define		AT91SAM9260_MATRIX_VDDIOMSEL		(1 << 16)	/* Memory voltage selection */
#define			AT91SAM9260_MATRIX_VDDIOMSEL_1_8V	(0 << 16)
#define			AT91SAM9260_MATRIX_VDDIOMSEL_3_3V	(1 << 16)

#endif
