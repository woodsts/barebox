#include <common.h>
#include <sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/hardware.h>

void __naked reset(void)
{
	common_reset();
	barebox_arm_entry(AT91_CHIPSELECT_1, SZ_128M, 0);
}
