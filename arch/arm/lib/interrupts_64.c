/*
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/compiler.h>


int interrupt_init(void)
{
	return 0;
}

void enable_interrupts(void)
{
	return;
}

int disable_interrupts(void)
{
	return 0;
}

void show_regs(struct pt_regs *regs)
{
	int i;

	printf("ELR:     %lx\n", regs->elr);
	printf("LR:      %lx\n", regs->regs[30]);
	for (i = 0; i < 29; i += 2)
		printf("x%-2d: %016lx x%-2d: %016lx\n",
		       i, regs->regs[i], i+1, regs->regs[i+1]);
	printf("\n");
}

/*
 * do_bad_sync handles the impossible case in the Synchronous Abort vector.
 */
void do_bad_sync(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("Bad mode in \"Synchronous Abort\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_bad_irq handles the impossible case in the Irq vector.
 */
void do_bad_irq(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("Bad mode in \"Irq\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_bad_fiq handles the impossible case in the Fiq vector.
 */
void do_bad_fiq(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("Bad mode in \"Fiq\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_bad_error handles the impossible case in the Error vector.
 */
void do_bad_error(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("Bad mode in \"Error\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_sync handles the Synchronous Abort exception.
 */

/* [dbg] scan stack for values that look like kernel return addresses */
static void scan_kaddr(unsigned long base, unsigned long len, int cap, const char *tag)
{
	unsigned long i, v;
	int n = 0;

	if (base < 0x80000000UL || base + len > 0xc0000000UL) {
		printf("[dbg] %s range bad\n", tag);
		return;
	}
	printf("[dbg] scan %s 0x%lx..0x%lx\n", tag, base, base + len);
	for (i = 0; i < len; i += 8) {
		v = *(volatile unsigned long *)(base + i);
		if (v >= 0x80000000UL && v < 0x83000000UL && n < cap) {
			printf("      +0x%04lx  0x%lx\n", i, v);
			n++;
		}
	}
}

void do_sync(struct pt_regs *pt_regs, unsigned int esr)
{
	unsigned long mpidr = 0, curel = 0, spsr = 0, el;

	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(curel));
	el = (curel >> 2) & 3;
	if (el == 2)
		__asm__ volatile("mrs %0, spsr_el2" : "=r"(spsr));
	printf("[dbg] MPIDR=0x%lx CurrentEL=0x%lx (EL%ld) SPSR=0x%lx\n",
	       mpidr, curel, el, spsr);
	printf("\"Synchronous Abort\" handler, esr 0x%08x\n", esr);
	scan_kaddr(((pt_regs->regs[29]) & ~0x3fUL) - 0x2000, 0x2000, 6, "uboot-stk");
	scan_kaddr(0x821e0000UL, 0x4000, 8, "init_thread_union");
	scan_kaddr(0x82620000UL, 0x4000, 8, "boot-stk");
	show_regs(pt_regs);
	printf("[dbg] kern boot_args=%lx,%lx,%lx,%lx fdt=%lx cmd=%lx\n",
	       *(volatile unsigned long *)0x821e5000UL,
	       *(volatile unsigned long *)0x821e5008UL,
	       *(volatile unsigned long *)0x821e5010UL,
	       *(volatile unsigned long *)0x821e5018UL,
	       *(volatile unsigned long *)0x81be9370UL,
	       *(volatile unsigned long *)0x81a17d08UL);
	panic("Resetting CPU ...\n");
}

/*
 * do_irq handles the Irq exception.
 */
void do_irq(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("\"Irq\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_fiq handles the Fiq exception.
 */
void do_fiq(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("\"Fiq\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}

/*
 * do_error handles the Error exception.
 * Errors are more likely to be processor specific,
 * it is defined with weak attribute and can be redefined
 * in processor specific code.
 */
void __weak do_error(struct pt_regs *pt_regs, unsigned int esr)
{
	printf("\"Error\" handler, esr 0x%08x\n", esr);
	show_regs(pt_regs);
	panic("Resetting CPU ...\n");
}
