/*
 * start-pbl.c
 *
 * Copyright (c) 2010-2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 * Copyright (c) 2012 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <common.h>
#include <init.h>
#include <sizes.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/pgtable.h>
#include <asm/cache.h>

#include "mmu.h"

unsigned long free_mem_ptr;
unsigned long free_mem_end_ptr;

/*
 * First instructions in the pbl image
 */
void __naked __section(.text_head_entry) pbl_start(void)
{
	barebox_arm_head();
}

extern void *input_data;
extern void *input_data_end;

#define STATIC static

#ifdef CONFIG_IMAGE_COMPRESSION_LZO
#include "../../../lib/decompress_unlzo.c"
#endif

#ifdef CONFIG_IMAGE_COMPRESSION_GZIP
#include "../../../lib/decompress_inflate.c"
#endif

static unsigned long *ttb;

static void create_sections(unsigned long addr, int size_m, unsigned int flags)
{
	int i;

	addr >>= 20;

	for (i = size_m; i > 0; i--, addr++)
		ttb[addr] = (addr << 20) | flags;
}

static void map_cachable(unsigned long start, unsigned long size)
{
	start &= ~(SZ_1M - 1);
	size = (size + (SZ_1M - 1)) & ~(SZ_1M - 1);

	create_sections(start, size >> 20, PMD_SECT_AP_WRITE |
			PMD_SECT_AP_READ | PMD_TYPE_SECT | PMD_SECT_WB);
}

static void mmu_enable(unsigned long membase, unsigned long memsize)
{
	int i;

	arm_set_cache_functions();

	/* Set the ttb register */
	asm volatile ("mcr  p15,0,%0,c2,c0,0" : : "r"(ttb) /*:*/);

	/* Set the Domain Access Control Register */
	i = 0x3;
	asm volatile ("mcr  p15,0,%0,c3,c0,0" : : "r"(i) /*:*/);

	create_sections(0, 4096, PMD_SECT_AP_WRITE |
			PMD_SECT_AP_READ | PMD_TYPE_SECT);

	map_cachable(membase, memsize);

	__mmu_cache_on();
}

static void mmu_disable(void)
{
	__mmu_cache_flush();
	__mmu_cache_off();
}

static void noinline errorfn(char *error)
{
	while (1);
}

static noinline void __barebox_arm_entry(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	void (*barebox)(uint32_t, uint32_t, uint32_t);
	uint32_t offset;
	uint32_t pg_start, pg_end, pg_len;
	int use_mmu = IS_ENABLED(CONFIG_MMU);

	/* Get offset between linked address and runtime address */
	offset = get_runtime_offset();

	pg_start = (uint32_t)&input_data - offset;
	pg_end = (uint32_t)&input_data_end - offset;
	pg_len = pg_end - pg_start;

	if (offset && (IS_ENABLED(CONFIG_PBL_FORCE_PIGGYDATA_COPY) ||
				region_overlap(pg_start, pg_len, TEXT_BASE, pg_len * 4))) {
		/*
		 * copy piggydata binary to its link address
		 */
		memcpy(&input_data, (void *)pg_start, pg_len);
		pg_start = (uint32_t)&input_data;
	}

	setup_c();

	/* set 128 KiB at the end of the MALLOC_BASE for early malloc */
	free_mem_ptr = membase + memsize - SZ_256K;
	free_mem_end_ptr = free_mem_ptr + SZ_128K;

	ttb = (void *)((free_mem_ptr - 0x4000) & ~0x3fff);

	if (use_mmu)
		mmu_enable(membase, memsize);

	decompress((void *)pg_start,
			pg_len,
			NULL, NULL,
			(void *)TEXT_BASE, NULL, errorfn);

	if (use_mmu)
		mmu_disable();

	flush_icache();

	if (IS_ENABLED(CONFIG_THUMB2_BAREBOX))
		barebox = (void *)(TEXT_BASE + 1);
	else
		barebox = (void *)TEXT_BASE;

	barebox(membase, memsize, boarddata);
}

/*
 * Main ARM entry point in the compressed image. Call this with the memory
 * region you can spare for barebox. This doesn't necessarily have to be the
 * full SDRAM. The currently running binary can be inside or outside of this
 * region. TEXT_BASE can be inside or outside of this region. boarddata will
 * be preserved and can be accessed later with barebox_arm_boarddata().
 */
void __naked barebox_arm_entry(uint32_t membase, uint32_t memsize,
		uint32_t boarddata)
{
	arm_setup_stack(membase + memsize - 8);

	__barebox_arm_entry(membase, memsize, boarddata);
}
