/* SPDX-License-Identifier: <Insert identifier here!!!>
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_H
#define __DLB2_OSDEP_H

#define DLB2_PCI_REG_READ(addr)        ioread32(addr)
#define DLB2_PCI_REG_WRITE(reg, value) iowrite32(value, reg)

/* Read/write register 'reg' in the CSR BAR space */
#define DLB2_CSR_REG_ADDR(a, reg) ((a)->csr_kva + (reg))
#define DLB2_CSR_RD(hw, reg) \
	DLB2_PCI_REG_READ(DLB2_CSR_REG_ADDR((hw), (reg)))
#define DLB2_CSR_WR(hw, reg, value) \
	DLB2_PCI_REG_WRITE(DLB2_CSR_REG_ADDR((hw), (reg)), (value))

/* Read/write register 'reg' in the func BAR space */
#define DLB2_FUNC_REG_ADDR(a, reg) ((a)->func_kva + (reg))
#define DLB2_FUNC_RD(hw, reg) \
	DLB2_PCI_REG_READ(DLB2_FUNC_REG_ADDR((hw), (reg)))
#define DLB2_FUNC_WR(hw, reg, value) \
	DLB2_PCI_REG_WRITE(DLB2_FUNC_REG_ADDR((hw), (reg)), (value))

/* Macros that prevent the compiler from optimizing away memory accesses */
#define OS_READ_ONCE(x) READ_ONCE(x)
#define OS_WRITE_ONCE(x, y) WRITE_ONCE(x, y)

/**
 * os_udelay() - busy-wait for a number of microseconds
 * @usecs: delay duration.
 */
static inline void os_udelay(int usecs)
{
}

/**
 * os_msleep() - sleep for a number of milliseconds
 * @usecs: delay duration.
 */
static inline void os_msleep(int msecs)
{
}

/**
 * os_map_producer_port() - map a producer port into the caller's address space
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 *
 * This function maps the requested producer port memory into the caller's
 * address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void *os_map_producer_port(struct dlb2_hw *hw,
					 u8 port_id,
					 bool is_ldb)
{
}

/**
 * os_unmap_producer_port() - unmap a producer port
 * @addr: mapped producer port address
 *
 * This function undoes os_map_producer_port() by unmapping the producer port
 * memory from the caller's address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void os_unmap_producer_port(struct dlb2_hw *hw, void *addr)
{
}

/**
 * os_fence_hcw() - fence an HCW to ensure it arrives at the device
 * @hw: dlb2_hw handle for a particular device.
 * @pp_addr: producer port address
 */
static inline void os_fence_hcw(struct dlb2_hw *hw, void *pp_addr)
{
	/*
	 * To ensure outstanding HCWs reach the device, read the PP address. IA
	 * memory ordering prevents reads from passing older writes, and the
	 * mfence also ensures this.
	 */
	//Insert full memory barrier here

	//Read the address pointed to by pp_addr here
}

/**
 * os_enqueue_four_hcws() - enqueue four HCWs to DLB
 * @hw: dlb2_hw handle for a particular device.
 * @hcw: pointer to the 64B-aligned contiguous HCW memory
 * @addr: producer port address
 */
static inline void os_enqueue_four_hcws(struct dlb2_hw *hw,
					struct dlb2_hcw *hcw,
					void *addr)
{
}

/**
 * os_notify_user_space() - notify user space
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: ID of domain to notify.
 * @alert_id: alert ID.
 * @aux_alert_data: additional alert data.
 *
 * This function notifies user space of an alert (such as a hardware alarm).
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
static inline int os_notify_user_space(struct dlb2_hw *hw,
				       u32 domain_id,
				       u64 alert_id,
				       u64 aux_alert_data)
{
}

/**
 * DLB2_HW_ERR() - log an error message
 * @dlb2: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_ERR(dlb2, ...)

/**
 * DLB2_HW_DBG() - log a debug message
 * @dlb2: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_DBG(dlb2, ...)

/**
 * os_schedule_work() - launch a thread to process pending map and unmap work
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function launches a kernel thread that will run until all pending
 * map and unmap procedures are complete.
 */
static inline void os_schedule_work(struct dlb2_hw *hw)
{
}

/**
 * os_worker_active() - query whether the map/unmap worker thread is active
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether a thread (launched by
 * os_schedule_work()) is active. This function is used to determine
 * whether or not to launch a worker thread.
 */
static inline bool os_worker_active(struct dlb2_hw *hw)
{
}

#endif /*  __DLB2_OSDEP_H */
