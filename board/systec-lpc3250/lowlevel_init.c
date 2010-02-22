#include <common.h>
#include <init.h>

#include <asm/io.h>

#include <mach/lpc3250.h>
#include <mach/lpc32xx_clkpwr_driver.h>
#include <mach/lpc32xx_nand.h>

#include "phy3250_board.h"

#define IRAM_STK_TEMP 0x0003C000

/* we emulate a udelay with a loop in early stage */
static inline void s_udelay(unsigned long usec)
{
	volatile unsigned int count;
	count = 0;
	while (count < usec * 200)
		count++;
}

static void __bare_init gpio_set_gpo_state(unsigned int set_pins, unsigned int clr_pins)
{
	writel(set_pins, &GPIO->p3_outp_set);
	writel(clr_pins, &GPIO->p3_outp_clr);
}

static void __bare_init phy3250_gpio_setup(void)
{
	/* Serial EEPROM SSEL signal - a GPIO is used instead of the SSP
	 * controlled SSEL signal
	 */
	writel(OUTP_STATE_GPIO(5), &GPIO->p2_dir_set);
	writel(OUTP_STATE_GPIO(5), &GPIO->p3_outp_set);

	/* Sets the following signals muxes :
	 * GPIO_02 / KEY_ROW6 | (ENET_MDC)      ->KEY_ROW6 | (ENET_MDC)
	 * GPIO_03 / KEY_ROW7 | (ENET_MDIO)     ->KEY_ROW7 | (ENET_MDIO)
	 * GPO_21 / U4_TX | (LCDVD[3])          ->U4_TX | (LCDVD[3])
	 * EMC_D_SEL                            ->(D16 ..D31 used)
	 * GPIO_04 / SSEL1 | (LCDVD[22])        ->SSEL1 | (LCDVD[22])
	 * GPIO_05 / SSEL0                      ->SSEL0
	 */
	writel(P2_SDRAMD19D31_GPIO | P2_GPIO05_SSEL0, &GPIO->p2_mux_clr);
	writel(P2_GPIO03_KEYROW7 | P2_GPIO02_KEYROW6 |
			    P2_GPO21_U4TX | P2_GPIO04_SSEL1, &GPIO->p2_mux_set);

	/* Sets the following signal muxes:
	 * I2S1TX_SDA / MAT3.1                  ->I2S1TX_SDA
	 * I2S1TX_CLK / MAT3.0                  ->I2S1TX_CLK
	 * I2S1TX_WS / CAP3.0                   ->I2S1TX_WS
	 * SPI2_DATIO / MOSI1 | (LCDVD[20])     ->MOSI1 | (LCDVD[20])
	 * SPI2_DATIN / MISO1 | (LCDVD[21])     ->MISO1 | (LCDVD[21])
	 * SPI2_CLK / SCK1 | (LCDVD[23])        ->SCK1 | (LCDVD[23])
	 * SPI1_DATIO / MOSI0                   ->MOSI0
	 * SPI1_DATIN / MISO0                   ->MISO0
	 * SPI1_CLK / SCK0                      ->SCK0
	 * (MS_BS) | MAT2.1 / PWM3.6            ->(MS_BS)
	 * (MS_SCLK) | MAT2.0 / PWM3.5          ->(MS_SCLK)
	 * U7_TX / MAT1.1 | (LCDVD[11])         ->MAT1.1 | (LCDVD[11])
	 * (MS_DIO3) | MAT0.3 / PWM3.4          ->(MS_DIO3)
	 * (MS_DIO2) | MAT0.2 / PWM3.3          ->(MS_DIO2)
	 * (MS_DIO1) | MAT0.1 / PWM3.2          ->(MS_DIO1)
	 * (MS_DIO0) | MAT0.0 / PWM3.1          ->(MS_DIO0)
	 */
	writel(P_SPI2DATAIO_MOSI1 |
			P_SPI2DATAIN_MISO1 | P_SPI2CLK_SCK1 |
			P_SPI1DATAIO_SSP0_MOSI | P_SPI1DATAIN_SSP0_MISO |
			P_SPI1CLK_SCK0 | P_U7TX_MAT11,
			&GPIO->p_mux_set);
	writel(P_I2STXSDA1_MAT31 | P_I2STXCLK1_MAT30 |
			P_I2STXWS1_CAP30 | P_MAT21_PWM36 |
			P_MAT20_PWM35 | P_MAT03_PWM34 | P_MAT02_PWM33 |
			P_MAT01_PWM32 | P_MAT00_PWM31,
			&GPIO->p_mux_clr);

	/* Sets the following signal muxes:
	 * GPO_02 / MAT1.0 | (LCDVD[0])         ->GPO_02
	 * GPO_06 / PWM4.3 | (LCDVD[18])        ->PWM4.3 | (LCDVD[18])
	 * GPO_08 / PWM4.2 | (LCDVD[8])         ->PWM4.2 | (LCDVD[8])
	 * GPO_09 / PWM4.1 | (LCDVD[9])         ->PWM4.1 | (LCDVD[9])
	 */
	writel(P3_GPO6_PWM43 | P3_GPO8_PWM42 | P3_GPO9_PWM41, &GPIO->p3_mux_set);

	/* on EPC 10/12/13/15/16/18 are used for I/O or PWM and not actual for:
	 * GPO_10 / PWM3.6 | (LCDPWR)           ->PWM3.6 | (LCDPWR)
	 * GPO_12 / PWM3.5 | (LCDLE)            ->PWM3.5 | (LCDLE)
	 * GPO_13 / PWM3.4 | (LCDDCLK)          ->PWM3.4 | (LCDDCLK)
	 * GPO_15 / PWM3.3 | (LCDFP)            ->PWM3.3 | (LCDFP)
	 * GPO_16 / PWM3.2 | (LCDENAB/LCDM)     ->PWM3.2 | (LCDENAB/LCDM)
	 *  GPO_18 / PWM3.1 | (LCDLP)            ->PWM3.1 | (LCDLP)
	 *
	 * GPIO->p3_mux_set = (P3_GPO6_PWM43 | P3_GPO8_PWM42 |
	 * P3_GPO9_PWM41 | P3_GPO10_PWM36 | P3_GPO12_PWM35 |
	 * P3_GPO13_PWM34 | P3_GPO15_PWM33 | P3_GPO16_PWM32 |
	 *  P3_GPO18_PWM31);
	 */
	writel(P3_GPO2_MAT10, &GPIO->p3_mux_clr);

	/* Sets the following signal muxes:
	 * P0.0 / I2S1RX_CLK                    ->I2S1RX_CLK
	 * P0.1 / I2S1RX_WS                     ->I2S1RX_WS
	 * P0.2 / I2S0RX_SDA | (LCDVD[4])       ->I2S0RX_SDA | (LCDVD[4])
	 * P0.3 / I2S0RX_CLK | (LCDVD[5])       ->I2S0RX_CLK | (LCDVD[5])
	 * P0.4 / I2S0RX_WS | (LCDVD[6])        ->I2S0RX_WS | (LCDVD[6])
	 * P0.5 / I2S0TX_SDA | (LCDVD[7])       ->I2S0TX_SDA | (LCDVD[7])
	 * P0.6 / I2S0TX_CLK | (LCDVD[12])      ->I2S0TX_CLK | (LCDVD[12])
	 * P0.7 / I2S0TX_WS | (LCDVD[13])       ->I2S0TX_WS | (LCDVD[13])
	 */
	writel(P0_ALL, &GPIO->p0_mux_set);

	/* Default mux configuation for P1 as follows:
	 * All signals  -> mapped to address lines (Clear)
	 */
	writel(P1_ALL, &GPIO->p1_mux_clr);

	/* Some GPO and GPIO states and directions needs to be setup here:
	 * GPO_20                      -> Output (watchdog enable) low
	 * GPO_19                      -> Output (NAND write protect) high
	 * GPO_17                      -> Output (deep sleep set) low
	 * GPO_11                      -> Output (deep sleep exit) low
	 * GPO_05                      -> Output (SDMMC power control) low
	 * GPO_04                      -> Output (unused) low
	 * GPO_02                      -> Output (audio reset) low
	 * GPO_01                      -> Output (LED1) low
	 * GPIO_1                      -> Input (MMC write protect)
	 * GPIO_0                      -> Input (MMC detect)
	 */
	gpio_set_gpo_state(OUTP_STATE_GPO(19),
			   (OUTP_STATE_GPO(20) | OUTP_STATE_GPO(17) |
			    OUTP_STATE_GPO(11) | OUTP_STATE_GPO(5) |
			    OUTP_STATE_GPO(4) | OUTP_STATE_GPO(2) |
			    OUTP_STATE_GPO(1)));
}

static void __bare_init clkpwr_sysclk_setup(CLKPWR_OSC_T osc, int bpval)
{
	u32 tmp;

	tmp = readl(&CLKPWR->clkpwr_sysclk_ctrl);
	tmp &= ~(CLKPWR_SYSCTRL_BP_MASK | CLKPWR_SYSCTRL_USEPLL397);
	tmp |= CLKPWR_SYSCTRL_BP_TRIG(bpval);

	switch (osc) {
	case CLKPWR_PLL397_OSC:
		tmp |= CLKPWR_SYSCTRL_USEPLL397;
		break;

	case CLKPWR_MAIN_OSC:
	default:
		break;
	}

	writel(tmp, &CLKPWR->clkpwr_sysclk_ctrl);
}

static int __bare_init hclk_is_locked(CLKPWR_PLL_T pll)
{
	int locked = 0;

	if ((readl(&CLKPWR->clkpwr_hclkpll_ctrl) & CLKPWR_HCLKPLL_PLL_STS) != 0) {
		locked = 1;
	} else if ((readl(&CLKPWR->clkpwr_hclkpll_ctrl) & CLKPWR_HCLKPLL_POWER_UP) == 0) {
		/* Simulated status */
		locked = 1;
	}

	return locked;
}

static void __bare_init phy3250_clock_setup(void)
{
	/* use main osc directly */
	/* Set bad phase timing only */
	clkpwr_sysclk_setup(CLKPWR_MAIN_OSC, 0x50);

	/* Setup the HCLK PLL for 208MHz operation, but if a configuration
	 *  can't be found, stay in direct run mode
	 * set fixed value exctraced from s1l
	 */
	writel(0x1401e, &CLKPWR->clkpwr_hclkpll_ctrl);

	/* Wait for PLL to lock */
	while (!hclk_is_locked(CLKPWR_HCLK_PLL));

	/* DDR divider is 2, PERIPH_CLK divider is 16, and HCLK divider
	 * is 2. Value is extracted from s1l
	 */
	writel(0x3d, &CLKPWR->clkpwr_hclk_div);

	/* Switch to run mode - the ARM core clock is HCLK_PLL (208MHz),
	 * HCLK is (HCLK_PLL / 2), and PERIPH_CLK is (HCLK / 16),
	 * Value is extracted from s1l
	 */
	writel(0x16, &CLKPWR->clkpwr_pwr_ctrl);
}

static void __bare_init enable_sdram_clk(void)
{
	CLKPWR->clkpwr_sdramclk_ctrl &= 0;
}

static void __bare_init sdram_adjust_timing(void)
{
	    /*  HAUPT SYSTEC HCLK=9,6ns    */
	    /* (K4S561632H-UI75 -> x2 chips -> 64MB) */

	/* all timing values used below are directly extraced from s1l */
	/* Setup precharge command delay */
	EMC->emcdynamictrp = 0x3;

	/* Setup Dynamic Memory Active to Precharge Command period */
	EMC->emcdynamictras = 0x5;

	/* Dynamic Memory Self-refresh Exit Time */
	EMC->emcdynamictsrex = 0x7;

	/* Dynamic Memory write recovery Time */
	EMC->emcdynamictwr = 0x2;

	/* Dynamic Memory Active To Active Command Period */
	EMC->emcdynamictrc = 0x7;

	/* Dynamic Memory Auto-refresh Period */
	EMC->emcdynamictrfc = 0x7;

	/* Dynamic Memory Active To Active Command Period */
	EMC->emcdynamictxsr = 0x5;

	/* Dynamic Memory Active Bank A to Active Bank B Time */
	EMC->emcdynamictrrd = 0x2;

	/* Dynamic Memory Load Mode Register To Active Command Time */
	EMC->emcdynamictmrd = EMC_DYN_LM2ACT_CMD_TIME(1);

	/* Dynamic Memory Last Data In to Read Command Time */
	EMC->emcdynamictcdlr = EMC_DYN_LASTDIN_CMD_TIME(0);

	/* Dynamic refresh */
	EMC->emcdynamicrefresh = 0x32;
}

static void __bare_init phy3250_dram_init(void)
{
	volatile unsigned int tmp, tmp32;

	/* Set HCLK delay */
	writel(0x70C000, &CLKPWR->clkpwr_sdramclk_ctrl); /* CLKPWR_SDRCLK_HCLK_DLY(0x7); */

	/* Enable normal power mode, little-endian mode, start clocks,
	   disable clock enables and self-refresh mode */
	writel(EMC_DYN_SDRAM_CTRL_EN, &EMC->emccontrol);
	writel(0, &EMC->emcconfig);

	writel(SDRAM_ADDRESS_MAP_64MB << 7, &EMC->emcdynamicconfig0);

	/* Setup CAS timing and clock edge config */
	writel(EMC_SET_CAS_IN_HALF_CYCLES(6) |
		EMC_SET_RAS_IN_CYCLES(2),
		&EMC->emcdynamicrascas0);

	writel(EMC_SDR_CLK_NODLY_CMD_DEL |
		EMC_SDR_READCAP_POS_POL |
		EMC_SDR_CLK_DLY_CMD_NODELY,
		&EMC->emcdynamicreadconfig);

	/* Setup SDRAM timing for current HCLK clock settings */
	sdram_adjust_timing();
	s_udelay(20);

	/* NOP mode with clocks running for 100uS */
	tmp = (EMC_DYN_CLK_ALWAYS_ON | EMC_DYN_CLKEN_ALWAYS_ON |
	       EMC_DYN_DIS_INV_MEMCLK);
	writel(tmp | EMC_DYN_NOP_MODE, &EMC->emcdynamiccontrol);
	s_udelay(200);

	/* Issue a precharge all command */
	writel(tmp | EMC_DYN_PALL_MODE, &EMC->emcdynamiccontrol);

	/* Issue a nop command */
	writel(tmp | EMC_DYN_NOP_MODE, &EMC->emcdynamiccontrol);

	/* Fast dynamic refresh for at least a few SDRAM clock cycles */
	writel(EMC_DYN_REFRESH_IVAL(128), &EMC->emcdynamicrefresh);
	s_udelay(20);

	/* Set correct DRAM refresh timing */
	sdram_adjust_timing();
	s_udelay(20);

	/* Issue load mode command and normal mode word */
	writel(tmp | EMC_DYN_CMD_MODE, &EMC->emcdynamiccontrol);
	tmp32 = *(volatile unsigned int *) (EMC_DYCS0_BASE +
					    DRAM_MODE_VAL_64MB);

	/* Normal DDR mode */
	writel(EMC_DYN_NORMAL_MODE | EMC_DYN_DIS_INV_MEMCLK, &EMC->emcdynamiccontrol);
}

static void __bare_init phy3250_mem_setup(void)
{
	/* Mirror IRAM at address 0x0 */
	CLKPWR->clkpwr_bootmap = CLKPWR_BOOTMAP_SEL_BIT;

	/* Enable HCLK and SDRAM bus clocks */
	enable_sdram_clk();

	/* Enable buffers in AHB ports */
	EMC->emcahn_regs[0].emcahbcontrol = EMC_AHB_PORTBUFF_EN;
	EMC->emcahn_regs[2].emcahbcontrol = EMC_AHB_PORTBUFF_EN;
	EMC->emcahn_regs[3].emcahbcontrol = EMC_AHB_PORTBUFF_EN;
	EMC->emcahn_regs[4].emcahbcontrol = EMC_AHB_PORTBUFF_EN;

	/* Enable port timeouts */
	EMC->emcahn_regs[0].emcahbtimeout = EMC_AHB_SET_TIMEOUT(100);
	EMC->emcahn_regs[2].emcahbtimeout = EMC_AHB_SET_TIMEOUT(400);
	EMC->emcahn_regs[3].emcahbtimeout = EMC_AHB_SET_TIMEOUT(400);
	EMC->emcahn_regs[4].emcahbtimeout = EMC_AHB_SET_TIMEOUT(400);

	/* Perform DRAM initialization */
	phy3250_dram_init();
}

#define CLKPWR_INTSRC_TS_P_BIT _BIT(30)
#define CLKPWR_INTSRC_RTC_BIT _BIT(24)
#define CLKPWR_INTSRC_MSTIMER_BIT _BIT(25)

static void __bare_init phy3250_misc_setup(void)
{
	/* Start Activation Polarity Register for all Sources */
	(*(volatile unsigned int *) (0x4000402C)) = 0x40000000;

	/* Disable all start enables */
	CLKPWR->clkpwr_p01_er = 0;
	CLKPWR->clkpwr_int_er = 0;
	CLKPWR->clkpwr_pin_er = 0;

	/* Set default start polarities for wakeup signals */
	CLKPWR->clkpwr_int_ap = (CLKPWR_INTSRC_TS_P_BIT |
				 CLKPWR_INTSRC_RTC_BIT |
				 CLKPWR_INTSRC_MSTIMER_BIT);
	CLKPWR->clkpwr_pin_ap = 0;

	/* Clear and active (latched) wakeup statuses */
	CLKPWR->clkpwr_int_rs = 0xFFFFFFFF;
	CLKPWR->clkpwr_pin_rs = 0xFFFFFFFF;
}

void __naked __bare_init board_init_lowlevel(void)
{
	unsigned long pc = get_program_counter();

	unsigned long stack = IRAM_STK_TEMP;
	unsigned long text_base = TEXT_BASE;

	__asm__ __volatile__(
		"mov     sp, %0\n"
		"stmia   sp, {lr}\n"
		:
		: "r" (stack)
		: "memory");

	phy3250_gpio_setup();
	phy3250_clock_setup();
	/* Skip memory setup when running from SDRAM */
	if (pc < 0x80000000 || pc > 0x90000000)
		phy3250_mem_setup();
	phy3250_misc_setup();

	if (pc < 4096) {
		lpc3250_mlc_nand_load_image((void *)text_base);

		__asm__ __volatile__(
			"ldmia   sp, {r3}\n"
			"add	pc, r3, %0\n"
			:
			: "r" (text_base)
			: "memory");
	} else {
		__asm__ __volatile__(
			"ldmia   sp, {pc}\n"
			:
			:
			: "memory");
	}
}

