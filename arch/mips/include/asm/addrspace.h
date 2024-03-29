/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 1996, 99 Ralf Baechle
 * Copyright (C) 2000, 2002  Maciej W. Rozycki
 * Copyright (C) 1990, 1999 by Silicon Graphics, Inc.
 */
#ifndef _ASM_ADDRSPACE_H
#define _ASM_ADDRSPACE_H

/*
 *  Configure language
 */
#ifdef __ASSEMBLY__
#define _ATYPE_
#define _ATYPE32_
#define _ATYPE64_
#define _CONST64_(x)	x
#else
#define _ATYPE_		__PTRDIFF_TYPE__
#define _ATYPE32_	int
#define _ATYPE64_	__s64
#ifdef CONFIG_64BIT
#define _CONST64_(x)	x ## L
#else
#define _CONST64_(x)	x ## LL
#endif
#endif

/*
 *  32-bit MIPS address spaces
 */
#ifdef __ASSEMBLY__
#define _ACAST32_
#define _ACAST64_
#else
#define _ACAST32_		(_ATYPE_)(_ATYPE32_)	/* widen if necessary */
#define _ACAST64_		(_ATYPE64_)		/* do _not_ narrow */
#endif

/*
 * Returns the kernel segment base of a given address
 */
#define KSEGX(a)		((_ACAST32_(a)) & 0xe0000000)

/*
 * Returns the physical address of a CKSEGx / XKPHYS address
 */
#define CPHYSADDR(a)		((_ACAST32_(a)) & 0x1fffffff)
#define XPHYSADDR(a)            ((_ACAST64_(a)) &			\
				 _CONST64_(0x000000ffffffffff))

/*
 * Memory segments (32bit kernel mode addresses)
 * These are the traditional names used in the 32-bit universe.
 */
#define KUSEG			0x00000000
#define KSEG0			0x80000000
#define KSEG1			0xa0000000
#define KSSEG			0xc0000000
#define KSEG3			0xe0000000

#ifdef CONFIG_64BIT

/*
 * Memory segments (64bit kernel mode addresses)
 * The compatibility segments use the full 64-bit sign extended value.  Note
 * the R8000 doesn't have them so don't reference these in generic MIPS code.
 */
#define XKUSEG			_CONST64_(0x0000000000000000)
#define XKSSEG			_CONST64_(0x4000000000000000)
#define XKPHYS			_CONST64_(0x8000000000000000)
#define XKSEG			_CONST64_(0xc000000000000000)
#define CKSEG0			_CONST64_(0xffffffff80000000)
#define CKSEG1			_CONST64_(0xffffffffa0000000)
#define CKSSEG			_CONST64_(0xffffffffc0000000)
#define CKSEG3			_CONST64_(0xffffffffe0000000)

/*
 * Cache modes for XKPHYS address conversion macros
 */
#define K_CALG_COH_EXCL1_NOL2	0
#define K_CALG_COH_SHRL1_NOL2	1
#define K_CALG_UNCACHED		2
#define K_CALG_NONCOHERENT	3
#define K_CALG_COH_EXCL		4
#define K_CALG_COH_SHAREABLE	5
#define K_CALG_NOTUSED		6
#define K_CALG_UNCACHED_ACCEL	7

/*
 * 64-bit address conversions
 */
#define PHYS_TO_XKSEG_UNCACHED(p)	PHYS_TO_XKPHYS(K_CALG_UNCACHED, (p))
#define PHYS_TO_XKSEG_CACHED(p)		PHYS_TO_XKPHYS(K_CALG_COH_SHAREABLE, (p))
#define XKPHYS_TO_PHYS(p)		((p) & TO_PHYS_MASK)
#define PHYS_TO_XKPHYS(cm, a)		(XKPHYS | (_ACAST64_(cm) << 59) | (a))

#else

/*
 * Map an address to a certain kernel segment
 */
#define KSEG0ADDR(a)		(CPHYSADDR(a) | KSEG0)
#define KSEG1ADDR(a)		(CPHYSADDR(a) | KSEG1)
#define KSSEGADDR(a)		(CPHYSADDR(a) | KSSEG)
#define KSEG3ADDR(a)		(CPHYSADDR(a) | KSEG3)

#define CKUSEG			KUSEG
#define CKSEG0			KSEG0
#define CKSEG1			KSEG1
#define CKSSEG			KSSEG
#define CKSEG3			KSEG3

#endif

#define CKSEG0ADDR(a)		(CPHYSADDR(a) | CKSEG0)
#define CKSEG1ADDR(a)		(CPHYSADDR(a) | CKSEG1)
#define CKSSEGADDR(a)		(CPHYSADDR(a) | CKSSEG)
#define CKSEG3ADDR(a)		(CPHYSADDR(a) | CKSEG3)

#endif /* _ASM_ADDRSPACE_H */
