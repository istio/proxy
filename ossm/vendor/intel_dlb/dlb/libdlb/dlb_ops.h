/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef __DLB_OPS_H__
#define __DLB_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* CPU feature enumeration macros */
#define CPUID_DIRSTR_BIT 27
#define CPUID_DIRSTR64B_BIT 28
#define CPUID_UMWAIT_BIT 5

static bool movdir64b_supported(void)
{
    unsigned int rax, rbx, rcx, rdx;

    asm volatile("cpuid\t\n"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx)
                 : "a" (7), "c" (0));

    return rcx & (1 << CPUID_DIRSTR64B_BIT);
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
static inline void movntdq_asm(uint64_t *addr,
                               long long data0,
                               long long data1)
{
        asm volatile("movq %1, %%xmm0\n"
                     "movhps %2, %%xmm0\n"
                     "movntdq %%xmm0, %0"
                     : "=m" (*addr) : "r" (data0), "m" (data1));
}

static void dlb_movntdq(struct dlb_enqueue_qe *qe4, uint64_t *pp_addr)
{
    /* Move entire 64B cache line of QEs, 128 bits (16B) at a time. */
    long long *_qe  = (long long *)qe4;
    movntdq_asm(pp_addr, _qe[0], _qe[1]);
    /* (see comment below) */
    _mm_sfence();
    movntdq_asm(pp_addr, _qe[2], _qe[3]);
    /* (see comment below) */
    _mm_sfence();
    movntdq_asm(pp_addr, _qe[4], _qe[5]);
    /* (see comment below) */
    _mm_sfence();
    movntdq_asm(pp_addr, _qe[6], _qe[7]);
    /* movntdq requires an sfence between writes to the PP MMIO address */
    _mm_sfence();
}

static void dlb_movdir64b(struct dlb_enqueue_qe *qe4, uint64_t *pp_addr)
{
    /* TODO: Change to proper assembly when compiler support available */
    asm volatile(".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
                 :
                 : "a" (pp_addr), "d" (qe4));
}

/* Faster wakeup, smaller power savings */
#define DLB_UMWAIT_CTRL_STATE_CO1 1
/* Slower wakeup, larger power savings */
#define DLB_UMWAIT_CTRL_STATE_CO2 0

static bool umwait_supported(void)
{
    unsigned int rax, rbx, rcx, rdx;

    asm volatile("cpuid\t\n"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx)
                 : "a" (7), "c" (0));

    return rcx & (1 << CPUID_UMWAIT_BIT);
}

static inline void dlb_umonitor(volatile void *addr)
{
	/* TODO: Change to proper assembly when compiler support available */
	asm volatile(".byte 0xf3, 0x0f, 0xae, 0xf7\t\n"
			:
			: "D" (addr));
}

static inline void dlb_umwait(int state, uint64_t timeout)
{
	uint32_t eax = timeout & UINT32_MAX;
	uint32_t edx = timeout >> 32;

	/* TODO: Change to proper assembly when compiler support available */
	asm volatile(".byte 0xf2, 0x0f, 0xae, 0xf7\t\n"
			:
			: "D" (state),  "a" (eax), "d" (edx));
}

static unsigned int cpuid_max(void)
{
    unsigned int rax, rbx, rcx, rdx;

    asm volatile("cpuid\t\n"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx)
                 : "a" (0));

    return rax;
}

static unsigned int cpuid_tsc_freq(void)
{
    unsigned int rax, rbx, rcx, rdx;

    asm volatile("cpuid\t\n"
                 : "=a" (rax), "=b" (rbx), "=c" (rcx), "=d" (rdx)
                 : "a" (0x15));

    return (rbx && rcx) ? rcx * (rbx / rax) : 0;
}

static inline void delay_ns_block(uint64_t start_time, uint64_t nsec) {
    uint64_t curr_time;
    struct timespec tv;

    curr_time = start_time;

    while ((curr_time - start_time) < nsec) {
        if (clock_gettime(CLOCK_MONOTONIC, &tv))
	    break;

	curr_time = tv.tv_sec * NS_PER_S + tv.tv_nsec;
    }
}
#ifdef __cplusplus
}
#endif

#endif /* __DLB_OPS_H__ */
