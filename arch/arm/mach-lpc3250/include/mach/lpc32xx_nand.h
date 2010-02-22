#ifndef __MACH_LPC32XX_NAND_H__
#define __MACH_LPC32XX_NAND_H__

void __bare_init lpc3250_mlc_nand_load_image(void *dest);

struct lpc32xx_nand_platform_data {
	int	flash_bbt;
};

#endif /* __MACH_LPC32XX_NAND_H__ */
