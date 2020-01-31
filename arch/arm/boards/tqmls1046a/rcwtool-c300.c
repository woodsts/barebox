
#include <stdio.h>
#include <stdint.h>

#define BITS_PER_LONG	(sizeof(uint32_t) * 8)
#define BIT_MASK(nr)	(1UL << (31 - ((nr) % BITS_PER_LONG)))
#define BIT_WORD(nr)	((nr) / BITS_PER_LONG)

static void set_bit(int nr, uint32_t *addr)
{
	uint32_t mask = BIT_MASK(nr);
	uint32_t *p = ((uint32_t *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static void clear_bit(int nr, uint32_t *addr)
{
	uint32_t mask = BIT_MASK(nr);
	uint32_t *p = ((uint32_t *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

static uint32_t rcw[16];

static void set_val(const char *name, int start, int end, int val)
{
	int i;
	int width = end - start + 1;
	uint32_t *addr = rcw;

	printf("# (%3d:%3d) %s: 0x%x\n", start, end, name, val);

	for (i = 0; i < width; i++) {
		if (val & (1 << ((width - 1) - i)))
			set_bit(i + start, addr);
		else
			clear_bit(i + start, addr);
	}
}

static void print_rcw(uint32_t *rcw)
{
	int i;

	printf("\n");
	printf("# PBL preamble and RCW header\n");
	printf("aa55aa55 01ee0100\n");
	printf("# RCW\n");

	for (i = 0; i < 16; i++) {
		printf("%08x ", rcw[i]);
		if (!((i + 1) % 4))
			printf("\n");
	}
}

#define SYS_PLL_CFG                  "SYS_PLL_CFG                 ",   0,  1
#define SYS_PLL_RAT                  "SYS_PLL_RAT                 ",   2,  6
#define MEM_PLL_CFG                  "MEM_PLL_CFG                 ",   8,  9
#define MEM_PLL_RAT                  "MEM_PLL_RAT                 ",  10, 15
#define CGA_PLL1_CFG                 "CGA_PLL1_CFG                ",  24, 25
#define CGA_PLL1_RAT                 "CGA_PLL1_RAT                ",  26, 31
#define CGA_PLL2_CFG                 "CGA_PLL2_CFG                ",  32, 33
#define CGA_PLL2_RAT                 "CGA_PLL2_RAT                ",  34, 39
#define C1_PLL_SEL                   "C1_PLL_SEL                  ",  96, 99
#define SRDS_PRTCL_S1                "SRDS_PRTCL_S1               ", 128,143
#define SRDS_PRTCL_S2                "SRDS_PRTCL_S2               ", 144,159
#define SRDS_PLL_REF_CLK_SEL_S1_PLL1 "SRDS_PLL_REF_CLK_SEL_S1_PLL1", 160,160
#define SRDS_PLL_REF_CLK_SEL_S1_PLL2 "SRDS_PLL_REF_CLK_SEL_S1_PLL2", 161,161
#define SRDS_PLL_REF_CLK_SEL_S2_PLL1 "SRDS_PLL_REF_CLK_SEL_S2_PLL1", 162,162
#define SRDS_PLL_REF_CLK_SEL_S2_PLL2 "SRDS_PLL_REF_CLK_SEL_S2_PLL2", 163,163
#define SRDS_PLL_PD_S1               "SRDS_PLL_PD_S1              ", 168,169
#define SRDS_PLL_PD_S2               "SRDS_PLL_PD_S2              ", 170,171
#define SRDS_DIV_PEX_S1              "SRDS_DIV_PEX_S1             ", 176,177
#define SRDS_DIV_PEX_S2              "SRDS_DIV_PEX_S2             ", 178,179
#define DDR_REFCLK_SEL               "DDR_REFCLK_SEL              ", 186,187
#define SRDS_REFCLK_SEL_S1           "SRDS_REFCLK_SEL_S1          ", 188,188
#define SRDS_REFCLK_SEL_S2           "SRDS_REFCLK_SEL_S2          ", 189,189
#define DDR_FDBK_MULT                "DDR_FDBK_MULT               ", 190,191
#define PBI_SRC                      "PBI_SRC                     ", 192,195
#define BOOT_HO                      "BOOT_HO                     ", 201,201
#define SB_EN                        "SB_EN                       ", 202,202
#define IFC_MODE                     "IFC_MODE                    ", 203,211
#define HWA_CGA_M1_CLK_SEL           "HWA_CGA_M1_CLK_SEL          ", 224,226
#define DRAM_LAT                     "DRAM_LAT                    ", 230,231
#define DDR_RATE                     "DDR_RATE                    ", 232,232
#define DDR_RSV0                     "DDR_RSV0                    ", 234,234
#define SYS_PLL_SPD                  "SYS_PLL_SPD                 ", 242,242
#define MEM_PLL_SPD                  "MEM_PLL_SPD                 ", 243,243
#define CGA_PLL1_SPD                 "CGA_PLL1_SPD                ", 244,244
#define CGA_PLL2_SPD                 "CGA_PLL2_SPD                ", 245,245
#define HOST_AGT_PEX                 "HOST_AGT_PEX                ", 264,266
#define GP_INFO1                     "GP_INFO1                    ", 288,295
#define GP_INFO2                     "GP_INFO2                    ", 299,319
#define UART_EXT                     "UART_EXT                    ", 354,356
#define IRQ_EXT                      "IRQ_EXT                     ", 357,359
#define SPI_EXT                      "SPI_EXT                     ", 360,362
#define SDHC_EXT                     "SDHC_EXT                    ", 363,365
#define UART_BASE                    "UART_BASE                   ", 366,368
#define ASLEEP                       "ASLEEP                      ", 369,369
#define RTC                          "RTC                         ", 370,370
#define SDHC_BASE                    "SDHC_BASE                   ", 371,371
#define IRQ_OUT                      "IRQ_OUT                     ", 372,372
#define IRQ_BASE                     "IRQ_BASE                    ", 373,381
#define SPI_BASE                     "SPI_BASE                    ", 382,383
#define IFC_GRP_A_EXT                "IFC_GRP_A_EXT               ", 384,386
#define IFC_GRP_D_EXT                "IFC_GRP_D_EXT               ", 393,395
#define IFC_GRP_E1_EXT               "IFC_GRP_E1_EXT              ", 396,398
#define IFC_GRP_F_EXT                "IFC_GRP_F_EXT               ", 399,401
#define IFC_GRP_E1_BASE              "IFC_GRP_E1_BASE             ", 405,405
#define IFC_GRP_D_BASE               "IFC_GRP_D_BASE              ", 407,407
#define IFC_GRP_A_BASE               "IFC_GRP_A_BASE              ", 412,413
#define IFC_A_22_24                  "IFC_A_22_24                 ", 415,415
#define EC1                          "EC1                         ", 416,418
#define EC2                          "EC2                         ", 419,421
#define LVDD_VSEL                    "LVDD_VSEL                   ", 422,423
#define I2C_IPGCLK_SEL               "I2C_IPGCLK_SEL              ", 424,424
#define EM1                          "EM1                         ", 425,425
#define EM2                          "EM2                         ", 426,426
#define EMI2_DMODE                   "EMI2_DMODE                  ", 427,427
#define EMI2_CMODE                   "EMI2_CMODE                  ", 428,428
#define USB_DRVVBUS                  "USB_DRVVBUS                 ", 429,429
#define USB_PWRFAULT                 "USB_PWRFAULT                ", 430,430
#define TVDD_VSEL                    "TVDD_VSEL                   ", 433,434
#define DVDD_VSEL                    "DVDD_VSEL                   ", 435,436
#define EMI1_DMODE                   "EMI1_DMODE                  ", 438,438
#define EVDD_VSEL                    "EVDD_VSEL                   ", 439,440
#define IIC2_BASE                    "IIC2_BASE                   ", 441,443
#define EMI1_CMODE                   "EMI1_CMODE                  ", 444,444
#define IIC2_EXT                     "IIC2_EXT                    ", 445,447
#define SYSCLK_FREQ                  "SYSCLK_FREQ                 ", 472,481
#define HWA_CGA_M2_CLK_SEL           "HWA_CGA_M2_CLK_SEL          ", 509,511

int main(void)
{
	printf("# RCW values:\n");
	printf("\n");

	set_val(SYS_PLL_CFG              ,       0);
	set_val(SYS_PLL_RAT              ,       6);
	set_val(MEM_PLL_CFG              ,       0);
	set_val(MEM_PLL_RAT              ,      20);
	set_val(CGA_PLL1_CFG             ,       0);
	set_val(CGA_PLL1_RAT             ,      16);
	set_val(CGA_PLL2_CFG             ,       0);
	set_val(CGA_PLL2_RAT             ,      14);
	set_val(C1_PLL_SEL               ,       0);
	set_val(SRDS_PRTCL_S1            ,  0x1133); /* XFI, XFI, SGMII, SGMII */
	set_val(SRDS_PRTCL_S2            ,  0x8888); /* 4x PCIe */
	set_val(SRDS_PLL_REF_CLK_SEL_S1_PLL1  ,  0); /* SGMII: 100MHz */
	set_val(SRDS_PLL_REF_CLK_SEL_S1_PLL2  ,  1); /* XFI: 156.25MHz */
	set_val(SRDS_PLL_REF_CLK_SEL_S2_PLL1  ,  0); /* PCIe: 100MHz */
	set_val(SRDS_PLL_REF_CLK_SEL_S2_PLL2  ,  0); /* unused: 100MHz */
	set_val(SRDS_PLL_PD_S1           ,       0);
	set_val(SRDS_PLL_PD_S2           ,       0); /* FIXME: Could power down PLL2? */
	set_val(SRDS_DIV_PEX_S1          ,       1);
	set_val(SRDS_DIV_PEX_S2          ,       1);
	set_val(DDR_REFCLK_SEL           ,       0);
	set_val(SRDS_REFCLK_SEL_S1       ,       0);
	set_val(SRDS_REFCLK_SEL_S2       ,       0);
	set_val(DDR_FDBK_MULT            ,       2);
	set_val(PBI_SRC                  ,       4);
	set_val(BOOT_HO                  ,       0);
	set_val(SB_EN                    ,       0);
	set_val(IFC_MODE                 ,      37);
	set_val(HWA_CGA_M1_CLK_SEL       ,       6);
	set_val(DRAM_LAT                 ,       1);
	set_val(DDR_RATE                 ,       0);
	set_val(DDR_RSV0                 ,       0);
	set_val(SYS_PLL_SPD              ,       0);
	set_val(MEM_PLL_SPD              ,       0);
	set_val(CGA_PLL1_SPD             ,       0);
	set_val(CGA_PLL2_SPD             ,       0);
	set_val(HOST_AGT_PEX             ,       0);
	set_val(GP_INFO1                 ,       0);
	set_val(GP_INFO2                 ,       0);
	set_val(UART_EXT                 ,       0); /* OK */
	set_val(IRQ_EXT                  ,       0);
	set_val(SPI_EXT                  ,       0);
	set_val(SDHC_EXT                 ,       0);
	set_val(UART_BASE                ,       6); /* 6 => UART1/2 including RTX/CTS */
	set_val(ASLEEP                   ,       0);
	set_val(RTC                      ,       0);
	set_val(SDHC_BASE                ,       0); /* SDHC mode, unused on C300 */
	set_val(IRQ_OUT                  ,       1); /* reserved, must be 1 */
	set_val(IRQ_BASE                 ,       0);
	set_val(SPI_BASE                 ,       0); /* SPI mode -> FRAM */
	set_val(IFC_GRP_A_EXT            ,       1); /* 1 => QSPI_A_DATA[3],
							needed for boot flash? */
	set_val(IFC_GRP_D_EXT            ,       0);
	set_val(IFC_GRP_E1_EXT           ,       0);
	set_val(IFC_GRP_F_EXT            ,       1); /* 1 = QSPI needed for boot flash? */
	set_val(IFC_GRP_E1_BASE          ,       0);
	set_val(IFC_GRP_D_BASE           ,       0);
	set_val(IFC_GRP_A_BASE           ,       0);
	set_val(IFC_A_22_24              ,       0);
	set_val(EC1                      ,       0);
	set_val(EC2                      ,       0);
	set_val(LVDD_VSEL                ,       1);
	set_val(I2C_IPGCLK_SEL           ,       0);
	set_val(EM1                      ,       0);
	set_val(EM2                      ,       0);
	set_val(EMI2_DMODE               ,       1);
	set_val(EMI2_CMODE               ,       0);
	set_val(USB_DRVVBUS              ,       0);
	set_val(USB_PWRFAULT             ,       0);
	set_val(TVDD_VSEL                ,       1);
	set_val(DVDD_VSEL                ,       2);
	set_val(EMI1_DMODE               ,       1);
	set_val(EVDD_VSEL                ,       0);
	set_val(IIC2_BASE                ,       0); /* only choice */
	set_val(EMI1_CMODE               ,       0);
	set_val(IIC2_EXT                 ,       0); /* 0 => I2C2, i2c_clkgen_cpu */
	set_val(SYSCLK_FREQ              ,     600);
	set_val(HWA_CGA_M2_CLK_SEL       ,       1);

	print_rcw(rcw);

	return 0;
}
