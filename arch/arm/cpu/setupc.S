#include <linux/linkage.h>

.section .text.setupc

/*
 * setup_c: copy binary to link address, clear bss and
 * continue executing at new address.
 *
 * This function does not return to the address it is
 * called from, but to the same location in the copied
 * binary.
 */
ENTRY(setup_c)
	push	{r4, r5}
	mov	r5, lr
	bl	get_runtime_offset
	subs	r4, r0, #0
	beq	1f			/* skip memcpy if already at correct address */
	ldr	r0,=_text
	ldr	r2,=__bss_start
	sub	r2, r2, r0
	sub	r1, r0, r4
	bl	memcpy			/* memcpy(_text, _text - offset, __bss_start - _text) */
1:	ldr	r0, =__bss_start
	mov	r1, #0
	ldr	r2, =__bss_stop
	sub	r2, r2, r0
	bl	memset			/* clear bss */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0	/* flush icache */
	add	lr, r5, r4		/* adjust return address to new location */
	pop	{r4, r5}
	mov	pc, lr
ENDPROC(setup_c)

#ifdef CONFIG_RELOCATABLE
/*
 * void setup_c(unsigned long targetadr)
 *
 * Copy binary to targetadr, relocate code, clear bss and continue
 * executing at new address.
 */
.section .text.relocate_binary
ENTRY(relocate_binary)
					/* r0: target address */
	push	{r3, r4, r5, r6, r7}
	mov	r7, lr
	mov	r6, r0
	adr	r4,_start
	ldr	r5,=_start
	sub	r4, r4, r5

	mov	r1, r4			/* r1: from address */

	cmp	r1, r0			/* already at correct address? */
	beq	1f			/* yes, skip copy to new address */

	ldr	r2, bss_ofs
	add	r2, r2, r5		/* r2: size */

	bl	memcpy			/* copy binary */

	mov	r0,#0
	mcr	p15, 0, r0, c7, c5, 0	/* flush icache */

	ldr	r0,=1f
	add	r0, r0, r6
	mov	pc, r0			/* jump to relocated address */
1:
	bl	relocate		/* relocate binary */

	ldr	r0, bss_ofs
	ldr	r5,=_start
	add	r0, r0, r5		/* r0: bss start */

	ldr	r2, bss_stop_ofs
	ldr	r3, bss_ofs
	sub	r2, r2, r3		/* r2: bss size */

	mov	r1, #0

	bl	memset			/* clear bss */

	sub	lr, r7, r4
	add	lr, lr, r6
	pop	{r3, r4, r5, r6, r7}
	mov	pc, lr

_start:
bss_ofs:
	.word __bss_start - _start
bss_stop_ofs:
	.word __bss_stop - _start

ENDPROC(relocate_binary)
#endif
