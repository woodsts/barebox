#include <common.h>
#include <sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/silicon.h>

void __naked reset(void)
{
	omap3_invalidate_dcache();

	common_reset();

	barebox_arm_entry(0x80000000, SZ_128M, 0);
}
