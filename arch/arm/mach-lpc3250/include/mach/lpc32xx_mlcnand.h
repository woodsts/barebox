/***********************************************************************
* $Id:: lpc32xx_mlcnand.h 729 2008-05-08 18:26:03Z wellsk             $
*
* Project: LPC32XX MLC NAND controller definitions
*
* Description:
*     This file contains the structure definitions and manifest
*     constants for the LPC32xx chip family component:
*         MLC NAND controller
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

#ifndef LPC32XX_MLCNAND_H
#define LPC32XX_MLCNAND_H

/**********************************************************************
* MLC NAND controller register structures
**********************************************************************/

/* MLC NAND controller module register structures */
typedef struct
{
  volatile u32 mlc_buff [0x2000]; /* MLC buffer reg */
  volatile u32 mlc_data [0x2000]; /* Start of MLC data buffer */
  volatile u32 mlc_cmd;           /* MLC command reg */
  volatile u32 mlc_addr;          /* MLC address register */
  volatile u32 mlc_enc_ecc;       /* MLC ECC encode reg */
  volatile u32 mlc_dec_ecc;       /* MLC ECC decode reg */
  volatile u32 mlc_autoenc_enc;   /* MLC ECC auto encode reg */
  volatile u32 mlc_autodec_dec;   /* MLC ECC auto decode reg */
  volatile u32 mlc_rpr;           /* MLC read parity reg */
  volatile u32 mlc_wpr;           /* MLC write parity reg */
  volatile u32 mlc_rubp;          /* MLC reset user buffer reg */
  volatile u32 mlc_robp;          /* MLC reset overhead reg */
  volatile u32 mlc_sw_wp_add_low; /* MLC write prot low reg */
  volatile u32 mlc_sw_wp_add_hi;  /* MLC write prot high reg */
  volatile u32 mlc_icr;           /* MLC config reg */
  volatile u32 mlc_time;          /* MLC timing reg */
  volatile u32 mlc_irq_mr;        /* MLC int mask reg */
  volatile u32 mlc_irq_sr;        /* MLC int status reg */
  volatile u32 mlc_reserved3;
  volatile u32 mlc_lock_pr;       /* MLC lock prot reg */
  volatile u32 mlc_isr;           /* MLC status reg */
  volatile u32 mlc_ceh;           /* MLC chip en host reg */
} MLCNAND_REGS_T;

/**********************************************************************
* mlc_autoenc_enc register definitions
**********************************************************************/
/* Auto encode enable */
#define MLC_AUTO_EN_ENABLE             _BIT(8)
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
#define MLC_SWWP_ENABLE                _BIT(3)
/* Enable large block FLASH support (2K+64) */
#define MLC_LARGE_BLK_ENABLE           _BIT(2)
/* Enable 4-word FLASH address support */
#define MLC_ADDR4_ENABLE               _BIT(1)
/* Enable 16-bit NAND data support */
#define MLC_DATA16_ENABLE              _BIT(0)

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
#define MLC_INT_DEV_RDY                _BIT(5)
/* NAND controller ready interrupt mask */
#define MLC_INT_CNTRLLR_RDY            _BIT(4)
/* NAND decode failure interrupt mask */
#define MLC_INT_DECODE_FAIL            _BIT(3)
/* NAND decode error interrupt mask */
#define MLC_INT_DECODE_ERR             _BIT(2)
/* NAND encode/decode ready interrupt mask */
#define MLC_INT_ENCDEC_RDY             _BIT(1)
/* NAND software write protect fault interrupt mask */
#define MLC_INT_SWWP_FAULT             _BIT(0)

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
#define MLC_DECODE_FAIL_STS            _BIT(6)
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
#define MLC_DECODE_ERR_DETECT_STS      _BIT(3)
/* NAND status ECC ready mask */
#define MLC_ECC_RDY_STS                _BIT(2)
/* NAND status controller ready mask */
#define MLC_CNTRLLR_RDY_STS            _BIT(1)
/* NAND status device ready mask */
#define MLC_DEV_RDY_STS                _BIT(0)

/**********************************************************************
* mlc_ceh register definitions
**********************************************************************/
/* NAND normal nCE operation mask (0 = forced nCE assertion) */
#define MLC_NORMAL_NCE                 _BIT(0)

/* Macro pointing to MLC NAND controller registers */
#define MLCNAND ((MLCNAND_REGS_T *)(MLC_BASE))

void __bare_init lpc3250_mlc_nand_load_image(void *dest);

#endif /* LPC32XX_MLCNAND_H */
