#include <common.h>
#include <init.h>
#include <sizes.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/at91sam9_sdramc.h>

void __naked __bare_init reset(void)
{
	common_reset();

	at91sam926x_entry(AT91_SDRAM_BASE, at91_get_sdram_size());
}
