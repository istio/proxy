/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OPS_DP_H
#define __DLB2_OPS_DP_H

#include <linux/version.h>
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#include <linux/objtool.h>
#else
#if defined(RHEL_RELEASE_CODE) 
#if (RHEL_RELEASE_VERSION(8, 5) <= RHEL_RELEASE_CODE)
#include <linux/objtool.h>
#else
#include <linux/frame.h>
#endif
#else
#include <linux/frame.h>
#endif
#endif

#include <asm/cpu.h>
#include <asm/fpu/api.h>

/* CPU feature enumeration macros */
#define CPUID_DIRSTR_BIT 27
#define CPUID_DIRSTR64B_BIT 28

static inline bool movdir64b_supported(void)
{
	int eax, ebx, ecx, edx;

	asm volatile("mov $7, %%eax\t\n"
		     "mov $0, %%ecx\t\n"
		     "cpuid\t\n"
		     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx));

	return ecx & (1 << CPUID_DIRSTR64B_BIT);
}

/**
 * movntdq_asm() - execute a movntdq instruction
 * @addr: mapped producer port address
 * @data0: least-significant 8B to move
 * @data1: most-significant 8B to move
 *
 * This function executes movntdq, moving @data0 and @data1 into the address
 * @addr.
 */
static inline void movntdq_asm(long long __iomem *addr,
			       long long data0,
			       long long data1)
{
#ifdef CONFIG_AS_SSE2
	__asm__ __volatile__("movq %1, %%xmm0\n"
			     "movhps %2, %%xmm0\n"
			     "movntdq %%xmm0, %0"
			     : "=m" (*addr) : "r" (data0), "m" (data1));
#endif
}

static inline void dlb2_movntdq(void *qe4, void __iomem *pp_addr)
{
	/* Move entire 64B cache line of QEs, 128 bits (16B) at a time. */
	long long *_qe  = (long long *)qe4;

	kernel_fpu_begin();
	movntdq_asm(pp_addr, _qe[0], _qe[1]);
	/* (see comment below) */
	wmb();
	movntdq_asm(pp_addr, _qe[2], _qe[3]);
	/* (see comment below) */
	wmb();
	movntdq_asm(pp_addr, _qe[4], _qe[5]);
	/* (see comment below) */
	wmb();
	movntdq_asm(pp_addr, _qe[6], _qe[7]);
	kernel_fpu_end();
	/* movntdq requires an sfence between writes to the PP MMIO address */
	wmb();
}

static inline void dlb2_movdir64b(void *qe4, void __iomem *pp_addr)
{
	/* TODO: Change to proper assembly when compiler support available */
	asm volatile(".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
		     :
		     : "a" (pp_addr), "d" (qe4));
}

/*
 * objtool's instruction decoder doesn't recognize the hard-coded machine
 * instructions for movdir64b, which causes it to emit "undefined stack state"
 * and "falls through" warnings. For now, ignore the functions.
 */
STACK_FRAME_NON_STANDARD(dlb2_movdir64b);

#endif /* __DLB2_OPS_DP_H */
