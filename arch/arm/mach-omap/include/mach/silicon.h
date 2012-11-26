/*
 * (C) Copyright 2008
 * Texas Instruments, <www.ti.com>
 * Nishanth Menon <x0nishan@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 *
  */
#ifndef __ASM_ARCH_OMAP_SILICON_H
#define __ASM_ARCH_OMAP_SILICON_H

/* Each platform silicon header comes here */
#ifdef CONFIG_ARCH_OMAP3
#include <mach/omap3-silicon.h>
#endif
#ifdef CONFIG_ARCH_OMAP4
#include <mach/omap4-silicon.h>
#endif

/* If Architecture specific init functions are present */
#ifndef __ASSEMBLY__
void omap3_core_init(void);
void omap3_invalidate_dcache(void);
#endif /* __ASSEMBLY__ */

#endif /* __ASM_ARCH_OMAP_SILICON_H */
