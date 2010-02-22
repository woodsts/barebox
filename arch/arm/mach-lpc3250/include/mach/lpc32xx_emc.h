/***********************************************************************
* $Id:: lpc32xx_emc.h 941 2008-07-24 20:39:53Z wellsk                 $
*
* Project: LPC32XX chip family External Memory Controller definitions
*
* Description:
*     This file contains the structure definitions and manifest
*     constants for the LPC32XX chip family component:
*         External Memory Controller
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

#ifndef LPC32XX_EMC_H
#define LPC32XX_EMC_H

#include "lpc32xx_chip.h"

/***********************************************************************
* External Memory Controller Module Register Structure
**********************************************************************/

/* Static chip select configuration structure */
typedef struct
{
  volatile unsigned int emcstaticconfig;
  volatile unsigned int emcstaticwaitwen;
  volatile unsigned int emcstaticwait0en;
  volatile unsigned int emcstaticwaitrd;
  volatile unsigned int emcstaticpage;
  volatile unsigned int emcstaticwr;
  volatile unsigned int emcstaticturn;
  volatile unsigned int reserved;
} EMC_STATIC_CFG;

/*  AHB control structure */
typedef struct
{
  volatile unsigned int emcahbcontrol;
  volatile unsigned int emcahbstatus;
  volatile unsigned int emcahbtimeout;
  volatile unsigned int reserved [5];
} EMC_AHB_CTRL_T;

/* External Memory Controller Module Register Structure */
typedef struct
{
  volatile unsigned int emccontrol;
  volatile unsigned int emcstatus;
  volatile unsigned int emcconfig;
  volatile unsigned int reserved1 [5];
  volatile unsigned int emcdynamiccontrol;
  volatile unsigned int emcdynamicrefresh;
  volatile unsigned int emcdynamicreadconfig;
  volatile unsigned int reserved2;
  volatile unsigned int emcdynamictrp;
  volatile unsigned int emcdynamictras;
  volatile unsigned int emcdynamictsrex;
  volatile unsigned int reserved3 [2];
  volatile unsigned int emcdynamictwr;
  volatile unsigned int emcdynamictrc;
  volatile unsigned int emcdynamictrfc;
  volatile unsigned int emcdynamictxsr;
  volatile unsigned int emcdynamictrrd;
  volatile unsigned int emcdynamictmrd;
  volatile unsigned int emcdynamictcdlr;
  volatile unsigned int reserved4 [8];
  volatile unsigned int emcstaticextendedwait;
  volatile unsigned int reserved5 [31];
  volatile unsigned int emcdynamicconfig0;
  volatile unsigned int emcdynamicrascas0;
  volatile unsigned int reserved6 [6];
  volatile unsigned int emcdynamicconfig1;
  volatile unsigned int emcdynamicrascas1;
  volatile unsigned int reserved7 [54];
  EMC_STATIC_CFG  emcstatic_regs [4];
  volatile unsigned int reserved8 [96];
  EMC_AHB_CTRL_T  emcahn_regs [5];
} EMC_REGS_T;

/***********************************************************************
* emccontrol register defines
**********************************************************************/
/* Dynamic controller low power mode bit */
#define EMC_DYN_LP_MODE            _BIT(2)
/* SDRAM controller enable bit */
#define EMC_DYN_SDRAM_CTRL_EN      _BIT(0)

/***********************************************************************
* emcstatus register defines
**********************************************************************/
/* Self-refesh mode status bit */
#define EMC_DYN_SELFRESH_MODE_BIT  _BIT(2)
/* SDRAM controller busy status bit */
#define EMC_DYN_CTRL_BUSY_BIT      _BIT(0)

/***********************************************************************
* emcconfig register defines
**********************************************************************/
/* SDRAM Big-endian mode bit */
#define EMC_DYN_ENDIAN_MODE_BIT    _BIT(0)

/***********************************************************************
* emcdynamiccontrol register defines
**********************************************************************/
/* Enter deep sleep power down mode bit */
#define EMC_DYN_DEEPDLEEP_BIT      _BIT(13)
/* SDRAM normal mode state */
#define EMC_DYN_NORMAL_MODE        0x00000000
#define EMC_DYN_CMD_MODE           0x00000080
#define EMC_DYN_PALL_MODE          0x00000100
#define EMC_DYN_NOP_MODE           0x00000180
/* Mask to get the current SDRAM mode */
#define EMC_DYN_MODE_MASK          0x00000180
/* Disable (1) or enable (0) memory clock bit */
#define EMC_DYN_DIS_MEMCLK         _BIT(5)
/* Disable (1) or enable (0) inverted memory clock bit */
#define EMC_DYN_DIS_INV_MEMCLK     _BIT(4)
/* Disable (1) or enable (0) memory clock during self-refresh bit */
#define EMC_DYN_DIS_MEMCLK_IN_SFRSH _BIT(3)
/* Enter self refresh request bit */
#define EMC_DYN_REQ_SELF_REFRESH   _BIT(2)
/* RAM clock runs continuously (1) or os stops when idle (0) bit */
#define EMC_DYN_CLK_ALWAYS_ON      _BIT(1)
/* RAM clock enables are always driven high bit */
#define EMC_DYN_CLKEN_ALWAYS_ON    _BIT(0)

/***********************************************************************
* emcdynamicrefresh register defines
**********************************************************************/
/* Macro for loading the SDRAM refresh interval in groups of 16
   clocks */
#define EMC_DYN_REFRESH_IVAL(n)    (((n) >> 4) & 0x7FF)

/***********************************************************************
* emcdynamicreadconfig register defines
**********************************************************************/
/* DDR capture polarity is postive edge of HCLK */
#define EMC_DDR_READCAP_POS_POL    _BIT(12)
/* DDR capture strategy: Clock out delay, command not delayed */
#define EMC_DDR_CLK_DLY_CMD_NODELY 0x00000000
/* DDR capture strategy: Clock out not delay, command delayed */
#define EMC_DDR_CLK_NODLY_CMD_DEL  0x00000100
/* DDR capture strategy: Clock out not delay, command delayed plus 1 clk */
#define EMC_DDR_CLK_NODLY_CMD_DEL1 0x00000200
/* DDR capture strategy: Clock out not delay, command delayed plus 2 clk */
#define EMC_DDR_CLK_NODLY_CMD_DEL2 0x00000300
/* SDRAM read polarity is postive edge of HCLK */
#define EMC_SDR_READCAP_POS_POL    _BIT(4)
/* SDRAM read strategy: Clock out delay, command not delayed */
#define EMC_SDR_CLK_DLY_CMD_NODELY 0x00000000
/* SDRAM read strategy: Clock out not delay, command delayed */
#define EMC_SDR_CLK_NODLY_CMD_DEL  0x00000001
/* SDRAM read strategy: Clock out not delay, command delayed plus 1 clk */
#define EMC_SDR_CLK_NODLY_CMD_DEL1 0x00000002
/* SDRAM read strategy: Clock out not delay, command delayed plus 2 clk */
#define EMC_SDr_CLK_NODLY_CMD_DEL2 0x00000003

/***********************************************************************
* emcdynamictrp register defines
**********************************************************************/
/* Macro for setting the precharge command period */
#define EMC_DYN_PRE_CMD_PER(n)     ((n) & 0xF)

/***********************************************************************
* emcdynamictras register defines
**********************************************************************/
/* Macro for setting the active to precharge command period */
#define EMC_DYN_ACTPRE_CMD_PER(n)  ((n) & 0xF)

/***********************************************************************
* emcdynamictsrex register defines
**********************************************************************/
/* Macro for setting the self refresh exit time */
#define EMC_DYN_SELF_RFSH_EXIT(n)  ((n) & 0x7F)

/***********************************************************************
* emcdynamictwr register defines
**********************************************************************/
/* Macro for setting the write recover time */
#define EMC_DYN_WR_RECOVERT_TIME(n) ((n) & 0xF)

/***********************************************************************
* emcdynamictrc register defines
**********************************************************************/
/* Macro for setting the active to command period */
#define EMC_DYN_ACT2CMD_PER(n)     ((n) & 0x1F)

/***********************************************************************
* emcdynamictrfc register defines
**********************************************************************/
/* Macro for setting the auto-refresh period */
#define EMC_DYN_AUTOREFRESH_PER(n) ((n) & 0x1F)

/***********************************************************************
* emcdynamictxsr register defines
**********************************************************************/
/* Macro for setting the exit self-refresh time */
#define EMC_DYN_EXIT_SRFSH_TIME(n) ((n) & 0xFF)

/***********************************************************************
* emcdynamictrrd register defines
**********************************************************************/
/* Macro for setting the active bank A to bank B latency time */
#define EMC_DYN_BANKA2BANKB_LAT(n) ((n) & 0xF)

/***********************************************************************
* emcdynamictmrd register defines
**********************************************************************/
/* Macro for setting the load mode register to active command time */
#define EMC_DYN_LM2ACT_CMD_TIME(n) ((n) & 0xF)

/***********************************************************************
* emcdynamictcdlr register defines
**********************************************************************/
/* Macro for setting the last daat-in to read command time */
#define EMC_DYN_LASTDIN_CMD_TIME(n) ((n) & 0xF)

/***********************************************************************
* emcstaticextendedwait register defines
**********************************************************************/
/* Macro for setting the extended wait time time */
#define EMC_STC_EXT_WAIT_TIME(n)   ((n) & 0x1FF)

/***********************************************************************
* emcdynamicconfig0, emcdynamicconfig1 register defines
***********************************************************************/
/* Write protect enable */
#define EMC_DYN_WR_PROTECT_EN      __BIT(20)
/* Address mapping modes mask, see documentation for mode selection */
#define EMC_DYN_ADDR_MAP_MASK      0x00007F80
/* Memory device selections */
#define EMC_DYN_DEV_SDR_SDRAM      0x00000000 /* SDR SDRAM */
#define EMC_DYN_DEV_LP_SDR_SDRAM   0x00000002 /* Low power SDR SDRAM */
#define EMC_DYN_DEV_DDR_SDRAM      0x00000004 /* DDR SDRAM */
#define EMC_DYN_DEV_LP_DDR_SDRAM   0x00000006 /* Low power DDR SDRAM */

/***********************************************************************
* emcdynamicrascas0, emcdynamicrascas1 register defines
***********************************************************************/
/* Macro for loading CAS latency in 1/2 clock cycles */
#define EMC_SET_CAS_IN_HALF_CYCLES(n) (((n) & 0xF) << 7)
/* Macro for loading RAS latency in clock cycles, n = 1 to 15 */
#define EMC_SET_RAS_IN_CYCLES(n) ((n) & 0xF)

/***********************************************************************
* emcstaticconfig register defines
***********************************************************************/
/* Static memory interface write protect bit */
#define EMC_STC_WP_BIT             _BIT(20)
/* Static memory interface extended wait enable bit */
#define EMC_STC_EXT_WAIT_EN_BIT    _BIT(8)
/* Static memory Byte lane signals enable bit */
#define EMC_STC_BLS_EN_BIT         _BIT(7)
/* Static memory select high chip select polarity enable bit */
#define EMC_STC_CS_POL_HIGH_BIT    _BIT(6)
/* Static memory page mode enable bit */
#define EMC_STC_PAGEMODE_EN_BIT    _BIT(3)
/* Static memory memory width selections */
#define EMC_STC_MEMWIDTH_8         0x00000000
#define EMC_STC_MEMWIDTH_16        0x00000001
#define EMC_STC_MEMWIDTH_32        0x00000002

/* Static memory memory chip selects */
#define EMC_STC_CS0        0x00000000
#define EMC_STC_CS1        0x00000001
#define EMC_STC_CS2        0x00000002
#define EMC_STC_CS3        0x00000003

/***********************************************************************
* emcstaticwaitwen register defines
***********************************************************************/
/* Macro for loading the chip select to wait write enable delay,
   (cs->we) n = 0 to 15 */
#define EMC_STC_WAIT_WR_DELAY(n)   ((n) & 0xF)

/***********************************************************************
* emcstaticwait0en register defines
***********************************************************************/
/* Macro for loading the chip select to output enable delay, (cs->oe)
   n = 0 to 15 */
#define EMC_STC_CS2OE_DELAY(n)     ((n) & 0xF)

/***********************************************************************
* emcstaticwaitrd register defines
***********************************************************************/
/* Macro for loading the first read delay in page mode, n = 0 to 15 */
#define EMC_STC_RDPAGEMODE_DELAY1(n) ((n) & 0x1F)

/***********************************************************************
* emcstaticpage register defines
***********************************************************************/
/* Macro for loading the 2nd, 3rd, and 4rth read delay in page
   mode, n = 0 to 15 */
#define EMC_STC_RDPAGEMODE_DELAY234(n) ((n) & 0x1F)

/***********************************************************************
* emcstaticwr register defines
***********************************************************************/
/* Macro for loading the 2nd, 3rd, and 4rth write delay in page
   mode, n = 0 to 15 */
#define EMC_STC_WRPAGEMODE_DELAY234(n) ((n) & 0x1F)

/***********************************************************************
* emcstaticturn register defines
***********************************************************************/
/* Macro for loading the bus turnaround time, n = 0 to 15 */
#define EMC_STC_BUSTURNAROUND_DELAY(n) ((n) & 0xF)

/***********************************************************************
* emcahbcontrol register defines
***********************************************************************/
/* Bit for enabling the AHB port buffer */
#define EMC_AHB_PORTBUFF_EN        0x1

/***********************************************************************
* emcahbstatus register defines
***********************************************************************/
/* AHB buffer not empty status bit mask */
#define EMC_AHB_PORTBUFF_NOT_MT    0x2

/***********************************************************************
* emcahbtimeout register defines
***********************************************************************/
/* Macro for loading the AHB timeout in clocks, n = 0 to 1023 */
#define EMC_AHB_SET_TIMEOUT(n)     ((n) & 0x1FF)

/* Macro pointing to EMC registers */
#define EMC  ((EMC_REGS_T *)(EMC_BASE))

#endif /* LPC32XX_EMC_H */
