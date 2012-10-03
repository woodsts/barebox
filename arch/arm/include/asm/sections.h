#ifndef __ASM_SECTIONS_H
#define __ASM_SECTIONS_H

#include <asm-generic/sections.h>

#define ld_var(name) ({ \
	unsigned long __ld_var_##name(void); \
	__ld_var_##name(); \
})

#endif /* __ASM_SECTIONS_H */
