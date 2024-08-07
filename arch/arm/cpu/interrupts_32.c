// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/**
 * @file
 * @brief Interrupt Support Routines
 */

#include <common.h>
#include <abort.h>
#include <linux/sizes.h>
#include <asm/ptrace.h>
#include <asm/barebox-arm.h>
#include <asm/unwind.h>
#include <init.h>

/* Avoid missing prototype warning, called from assembly */
void do_undefined_instruction (struct pt_regs *pt_regs);
void do_software_interrupt (struct pt_regs *pt_regs);
void do_prefetch_abort (struct pt_regs *pt_regs);
void do_data_abort (struct pt_regs *pt_regs);
void do_fiq (struct pt_regs *pt_regs);
void do_irq (struct pt_regs *pt_regs);

/**
 * Display current register set content
 * @param[in] regs Guess what
 */
void show_regs (struct pt_regs *regs)
{
	unsigned long flags;
	const char *processor_modes[] = {
	"USER_26",	"FIQ_26",	"IRQ_26",	"SVC_26",
	"UK4_26",	"UK5_26",	"UK6_26",	"UK7_26",
	"UK8_26",	"UK9_26",	"UK10_26",	"UK11_26",
	"UK12_26",	"UK13_26",	"UK14_26",	"UK15_26",
	"USER_32",	"FIQ_32",	"IRQ_32",	"SVC_32",
	"UK4_32",	"UK5_32",	"UK6_32",	"ABT_32",
	"UK8_32",	"UK9_32",	"UK10_32",	"UND_32",
	"UK12_32",	"UK13_32",	"UK14_32",	"SYS_32",
	};

	flags = condition_codes (regs);

	eprintf("pc : [<%08lx>]    lr : [<%08lx>]\n"
		"sp : %08lx  ip : %08lx  fp : %08lx\n",
		instruction_pointer (regs),
		regs->ARM_lr, regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	eprintf("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9, regs->ARM_r8);
	eprintf("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6, regs->ARM_r5, regs->ARM_r4);
	eprintf("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2, regs->ARM_r1, regs->ARM_r0);
	eprintf("Flags: %c%c%c%c",
		flags & PSR_N_BIT ? 'N' : 'n',
		flags & PSR_Z_BIT ? 'Z' : 'z',
		flags & PSR_C_BIT ? 'C' : 'c', flags & PSR_V_BIT ? 'V' : 'v');
	eprintf("  IRQs %s  FIQs %s  Mode %s%s\n",
		interrupts_enabled (regs) ? "on" : "off",
		fast_interrupts_enabled (regs) ? "on" : "off",
		processor_modes[processor_mode (regs)],
		thumb_mode (regs) ? " (T)" : "");
#ifdef CONFIG_ARM_UNWIND
	unwind_backtrace(regs);
#endif
}

static void __noreturn do_exception(struct pt_regs *pt_regs)
{
	show_regs(pt_regs);

	panic_no_stacktrace("");
}

/**
 * The CPU runs into an undefined instruction. That really should not happen!
 * @param[in] pt_regs Register set content when the accident happens
 */
void do_undefined_instruction (struct pt_regs *pt_regs)
{
	eprintf("undefined instruction\n");
	do_exception(pt_regs);
}

/**
 * The CPU catches a software interrupt
 * @param[in] pt_regs Register set content when the interrupt happens
 *
 * There is no function behind this feature. So what to do else than
 * a reset?
 */
void do_software_interrupt (struct pt_regs *pt_regs)
{
	eprintf("software interrupt\n");
	do_exception(pt_regs);
}

/**
 * The CPU catches a prefetch abort. That really should not happen!
 * @param[in] pt_regs Register set content when the accident happens
 *
 * instruction fetch from an unmapped area
 */
void do_prefetch_abort (struct pt_regs *pt_regs)
{
	eprintf("prefetch abort\n");
	do_exception(pt_regs);
}

static const char *data_abort_reason(ulong far)
{
	ulong guard_page;

	if (far < PAGE_SIZE)
		return "NULL pointer dereference";

	if (IS_ENABLED(CONFIG_STACK_GUARD_PAGE)) {
		guard_page = arm_mem_guard_page_get();
		if (guard_page <= far && far < guard_page + PAGE_SIZE)
			return "stack overflow";
	}

	return "paging request";
}

/**
 * The CPU catches a data abort. That really should not happen!
 * @param[in] pt_regs Register set content when the accident happens
 *
 * data fetch from an unmapped area
 */
void do_data_abort (struct pt_regs *pt_regs)
{
	u32 far;

	asm volatile ("mrc     p15, 0, %0, c6, c0, 0" : "=r" (far) : : "cc");

	eprintf("unable to handle %s at address 0x%08x\n",
		data_abort_reason(far), far);

	do_exception(pt_regs);
}

/**
 * The CPU catches a fast interrupt request.
 * @param[in] pt_regs Register set content when the interrupt happens
 *
 * We never enable FIQs, so this should not happen
 */
void do_fiq (struct pt_regs *pt_regs)
{
	eprintf("fast interrupt request\n");
	do_exception(pt_regs);
}

/**
 * The CPU catches a regular interrupt.
 * @param[in] pt_regs Register set content when the interrupt happens
 *
 * We never enable interrupts, so this should not happen
 */
void do_irq (struct pt_regs *pt_regs)
{
	eprintf("interrupt request\n");
	do_exception(pt_regs);
}

extern volatile int arm_ignore_data_abort;
extern volatile int arm_data_abort_occurred;

void data_abort_mask(void)
{
	arm_data_abort_occurred = 0;
	arm_ignore_data_abort = 1;
}

int data_abort_unmask(void)
{
	arm_ignore_data_abort = 0;

	return arm_data_abort_occurred != 0;
}
