
#include <common.h>
#include <init.h>
#include <sizes.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/pgtable.h>
#include <asm/cache.h>

#include "common.h"

void relocate(void)
{
	uint32_t offset;
	uint32_t *dstart, *dend, *dynsym;
	uint32_t *dynend;

	/* Get offset between linked address and runtime address */
	offset = get_runtime_offset();

	dstart = (void *)(ld_var(__rel_dyn_start) - offset);
	dend = (void *)(ld_var(__rel_dyn_end) - offset);

	dynsym = (void *)(ld_var(__dynsym_start) - offset);
	dynend = (void *)(ld_var(__dynsym_end) - offset);

	while (dstart < dend) {
		uint32_t *fixup = (uint32_t *)(*dstart - offset);
		uint32_t type = *(dstart + 1);

		if ((type & 0xff) == 0x17) {
			*fixup = *fixup - offset;
		} else {
			int index = type >> 8;
			uint32_t r = dynsym[index * 4 + 1];

			*fixup = *fixup + r - offset;
		}
		dstart += 2;
	}

	flush_icache();
}
