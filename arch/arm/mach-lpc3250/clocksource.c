
#include <common.h>
#include <init.h>
#include <clock.h>

#include <asm/io.h>

#include <mach/lpc3250.h>

uint64_t lpc_clocksource_read(void)
{
	return readl(&TIMER_CNTR0->tc);
}

static struct clocksource cs = {
	.read	= lpc_clocksource_read,
	.mask	= 0xffffffff,
	.shift	= 10,
};

static int clocksource_init (void)
{
	unsigned int tbaseclk;
	u32 tmp;

	/* Enable timer system clock */
	tmp = readl(&CLKPWR->clkpwr_timers_pwms_clk_ctrl_1);
	tmp |= CLKPWR_TMRPWMCLK_TIMER0_EN;
	writel(tmp, &CLKPWR->clkpwr_timers_pwms_clk_ctrl_1);

	/* Reset timer */
	writel(TIMER_CNTR_TCR_RESET, &TIMER_CNTR0->tcr);
	writel(0, &TIMER_CNTR0->tcr);
	writel(0, &TIMER_CNTR0->tc);

	/* Clear and enable match function */
	writel(TIMER_CNTR_MTCH_BIT(0), &TIMER_CNTR0->ir);

	/* Count mode is PCLK edge */
	writel(TIMER_CNTR_SET_MODE(TIMER_CNTR_CTCR_TIMER_MODE),
			&TIMER_CNTR0->ctcr);

	/* Set prescale counter value for a 1mS tick */
	tbaseclk = sys_get_rate(CLKPWR_PERIPH_CLK);

	/* Enable the timer */
	writel(TIMER_CNTR_TCR_EN, &TIMER_CNTR0->tcr);

	cs.mult = clocksource_hz2mult(tbaseclk, cs.shift);

	init_clock(&cs);

	return 0;
}

core_initcall(clocksource_init);

