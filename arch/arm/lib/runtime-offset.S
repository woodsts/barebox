#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * Get the offset between the link address and the address
 * we are currently running at.
 */
ENTRY(get_runtime_offset)
1:	adr r0, 1b
	ldr r1, linkadr
	subs r0, r1, r0
THUMB(	subs r0, r0, #1)
	mov pc, lr

linkadr:
.word get_runtime_offset
ENDPROC(get_runtime_offset)


__ld_var_base:

.macro get_linker_var name
	.globl __ld_var_\name
	__ld_var_\name:
		ldr r0, __\name
		b 1f
	__\name: .word \name - __ld_var_base
.endm

get_linker_var _text
get_linker_var __rel_dyn_start
get_linker_var __rel_dyn_end
get_linker_var __dynsym_start
get_linker_var __dynsym_end
get_linker_var _barebox_image_size

1:
	ldr r1, =__ld_var_base
	adds r0, r0, r1
	mov pc, lr

