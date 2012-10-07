/*
 * start-arm.c
 *
 * Copyright (c) 2010 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <init.h>
#include <sizes.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/cache.h>
#include <memory.h>
#include <sizes.h>

#include "common.h"

static uint32_t __barebox_arm_boarddata;

unsigned long arm_stack_top;

/*
 * the board specific lowlevel init code can pass a pointer or
 * data value to barebox_arm_entry() and pick it up later with
 * this function.
 */
uint32_t barebox_arm_boarddata(void)
{
	return __barebox_arm_boarddata;
}

static noinline void __start(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	unsigned long malloc_start, malloc_end;

#ifdef CONFIG_RELOCATABLE
	relocate_binary(membase + memsize - SZ_2M);
#else
	setup_c();
#endif

	if ((unsigned long)_text > membase + memsize ||
			(unsigned long) _text < membase)
		/*
		 * barebox is either outside SDRAM or in another
		 * memory bank, so we can use the whole bank for
		 * malloc.
		 */
		malloc_end = membase + memsize - 1;
	else
		malloc_end = (unsigned long)_text - 1;

	if (MALLOC_SIZE > 0) {
		/*
		 * malloc size has been configured in Kconfig. Use it, but
		 * check if it's outside the region we are provided.
		 */
		malloc_start = malloc_end - MALLOC_SIZE + 1;
		if (malloc_start < membase)
			malloc_start = membase;
	} else {
		/*
		 * No memory size given in Kconfig, try to make the best out of the
		 * available space.
		 *
		 * We need space for malloc and space to put the kernel outside the
		 * malloc space later. Divide available memory into two parts.
		 */
		malloc_start = (membase + malloc_end) / 2;

		/*
		 * Limit space we do not use for malloc to 16MB
		 */
		if (malloc_start - membase > SZ_16M)
			malloc_start = membase + SZ_16M;
	}

	mem_malloc_init((void *)malloc_start,
			(void *)malloc_end);

	arm_stack_top = membase + memsize;

	__barebox_arm_boarddata = boarddata;

	start_barebox();
}

#ifndef CONFIG_PBL_IMAGE

void __naked __section(.text_entry) start(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	barebox_arm_head();
}

/*
 * Main ARM entry point in the uncompressed image. Call this with the memory
 * region you can spare for barebox. This doesn't necessarily have to be the
 * full SDRAM. The currently running binary can be inside or outside of this
 * region. TEXT_BASE can be inside or outside of this region. boarddata will
 * be preserved and can be accessed later with barebox_arm_boarddata().
 */
void __naked barebox_arm_entry(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	arm_setup_stack(membase + memsize - 8);

	__start(membase, memsize, boarddata);
}

#else

/*
 * First function in the uncompressed image. We get here from
 * the pbl. The stack already has been set up by the pbl.
 */
void __naked __section(.text_entry) start(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	__start(membase, memsize, boarddata);
}
#endif
