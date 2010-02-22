/***********************************************************************
 * $Id:: phy3250_board.h 1271 2008-10-29 22:12:11Z wellsk              $
 *
 * Project: Phytec 3250 board definitions
 *
 * Description:
 *     This file contains board specific information such as the
 *     chip select wait states, and other board specific information.
 *
 ***********************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 **********************************************************************/

#ifndef PHY3250_BOARD_H
#define PHY3250_BOARD_H

#include <mach/lpc32xx_chip.h>
#include <mach/lpc32xx_emc.h>

/***********************************************************************
 * SDRAM configuration
 **********************************************************************/

/* Structure used to store programming and timing information for each
   SDRAM device configuration */
typedef struct
{
	unsigned int dyncfgword;    /* Programmed value for emcdynamicconfig0 */
	unsigned int modeword;      /* Mode word for devices */
	unsigned int emodeword;     /* Extended mode word for devices */
	unsigned int cas;           /* CAS half clocks */
	unsigned int ras;           /* RAS clocks */
	unsigned int tdynref;
	unsigned int trp;
	unsigned int tras;
	unsigned int tsrex;
	unsigned int twr;
	unsigned int trc;
	unsigned int trfc;
	unsigned int txsr;
	unsigned int trrd;
	unsigned int tmrd;
	unsigned int tcdlr;
} PHY_DRAM_CFG_T;
extern PHY_DRAM_CFG_T dram_cfg [1];

/* SDRAM address map configuration values
   MT48H4M16LFB4-8 IT (x2 chips -> 16MB)
   MT48H8M16LFB4-8 IT (x2 chips -> 32MB)
   MT48H16M16LFBF-8 IT (x2 chips -> 64MB)
   MT48H32M16LFBF-8 IT (x2 chips -> 128MB) */
#define SDRAM_ADDRESS_MAP_16MB	0xA5
#define SDRAM_ADDRESS_MAP_32MB	0xA9
#define SDRAM_ADDRESS_MAP_64MB  0xAD
#define SDRAM_ADDRESS_MAP_128MB 0xB1

/* A read starts with the bank and row on the SDRAM address lines.
   This is when the SDRAM reads the address lines to configure the
   mode and extended mode registers. Since the address lines are
   multiplex for BANK, ROW, COLUMN, the address is sent in two parts
   (BANK + ROW followed by BANK + COLUMN). The 32-bit address
   supplied to the SDRAM controller when performing a read or write
   is composed like:

   [31............0]
   [bank_bits,row_bits,col_bits]

   The actual bit placement of the bank, row, and column depends on
   the memory density. The mapping is as follows for the
   phyCORE-LPC3250:

    16MB: [23]bank_1, [22]bank_0, [21:10]row_bits, [9:2]col_bits
    32MB: [24]bank_0, [23]bank_1, [22:11]row_bits, [10:2]col_bits
    64MB: [25]bank_1, [24]bank_0, [23:11]row_bits, [10:2]col_bits
   128MB: [26]bank_0, [25]bank_1, [24:12]row_bits, [11:2]col_bits

   The ROW and COLUMN are transmitted on LPC3250 address pins 12:0
   while the bank bits are transmitted on address pins 14 and 13
   (bank_bit_1 and bank_bit_0).

   Writing to the mode and extended mode registers involves setting
   the bank bits and row bits (see SDRAM datasheet for details).
   Below are macros to translate a value into the correct position
   in the 32-bit address for ROW and BANK. */
#define SDRAM_MAP_ROW_16MB(x)           (x << 10)
#define SDRAM_MAP_ROW_32MB(x)           (x << 11)
#define SDRAM_MAP_ROW_64MB(x)           (x << 11)
#define SDRAM_MAP_ROW_128MB(x)          (x << 12)

#define SDRAM_MAP_BANK_16MB(BA1,BA0)    ((BA1 << 23) | (BA0 << 22))
#define SDRAM_MAP_BANK_32MB(BA1,BA0)    ((BA1 << 23) | (BA0 << 24))
#define SDRAM_MAP_BANK_64MB(BA1,BA0)    ((BA1 << 25) | (BA0 << 24))
#define SDRAM_MAP_BANK_128MB(BA1,BA0)   ((BA1 << 25) | (BA0 << 26))

/* A reference to the mapping of the address pins to mode and
   extended mode register bit definitions is provided (see Micron
   SDRAM datasheet for details):

   --MODE REGISTER--
   BA1,BA0 	= mode or extended mode select -- [0,0] for mode register
   A11,A10 	= reserved
   A9		= write burst mode
   A8,A7	= operating mode
   A6...A4	= CAS latency
   A3		= burst type
   A2...A0	= burst length

   --EXTENDED MODE REGISTER --
   BA1,BA0	= mode or extended mode select -- [1,0] for extended
              mode register
   A11...A7	= must be set to 0
   A6,A5	= driver strength
   A4,A3	= maximum case temperature (setting these bits has no
              effect for Micron SDRAM due to on-die temp sensor
              used instead)
   A2...A0	= self refresh coverage (i.e., four banks, two banks,
              one bank, etc..)

   Compose the addresses required to set the mode & extended mode
   registers for various SDRAM densities. For the mode registers we
   only set the cas to 3 and leave the other settings as default in
   mode & extended mode regs. Notice the EMODE reg definitions set
   the bank mapping to [1,0] to select the extended mode register. */

#define DRAM_MODE_VAL_16MB  (SDRAM_MAP_BANK_16MB(0,0) | \
                             SDRAM_MAP_ROW_16MB(0x3 << 4)) 	/* cas = 3 */
#define DRAM_MODE_VAL_32MB  (SDRAM_MAP_BANK_32MB(0,0) | \
                             SDRAM_MAP_ROW_32MB(0x3 << 4)) 	/* cas = 3 */
#define DRAM_MODE_VAL_64MB  (SDRAM_MAP_BANK_64MB(0,0) | \
                             SDRAM_MAP_ROW_64MB(0x3 << 4)) 	/* cas = 3 */
#define DRAM_MODE_VAL_128MB (SDRAM_MAP_BANK_128MB(0,0) | \
                             SDRAM_MAP_ROW_128MB(0x3 << 4))	/* cas = 3 */
#define DRAM_EMODE_VAL_16MB  (SDRAM_MAP_BANK_16MB(1,0))
#define DRAM_EMODE_VAL_32MB  (SDRAM_MAP_BANK_32MB(1,0))
#define DRAM_EMODE_VAL_64MB  (SDRAM_MAP_BANK_64MB(1,0))
#define DRAM_EMODE_VAL_128MB (SDRAM_MAP_BANK_128MB(1,0))

#endif /* PHY3250_BOARD_H */
