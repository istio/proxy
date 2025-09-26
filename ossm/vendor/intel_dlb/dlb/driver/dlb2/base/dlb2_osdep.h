/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_H
#define __DLB2_OSDEP_H

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/byteorder.h>
#include "uapi/linux/dlb2_user.h"

#include "../dlb2_main.h"
#include "dlb2_resource.h"

/* Read/write register 'reg' in the CSR BAR space */
#define DLB2_CSR_REG_ADDR(a, reg)   ((a)->csr_kva + (reg))
#define DLB2_CSR_RD(hw, reg)	    ioread32(DLB2_CSR_REG_ADDR((hw), (reg)))
#define DLB2_CSR_WR(hw, reg, value) iowrite32((value), \
					      DLB2_CSR_REG_ADDR((hw), (reg)))

/* Read/write register 'reg' in the func BAR space */
#define DLB2_FUNC_REG_ADDR(a, reg)   ((a)->func_kva + (reg))
#define DLB2_FUNC_RD(hw, reg)	     ioread32(DLB2_FUNC_REG_ADDR((hw), (reg)))
#define DLB2_FUNC_WR(hw, reg, value) iowrite32((value), \
					       DLB2_FUNC_REG_ADDR((hw), (reg)))

/* Macros that prevent the compiler from optimizing away memory accesses */
#define OS_READ_ONCE(x) READ_ONCE(x)
#define OS_WRITE_ONCE(x, y) WRITE_ONCE(x, y)

/**
 * os_udelay() - busy-wait for a number of microseconds
 * @usecs: delay duration.
 */
static inline void os_udelay(int usecs)
{
	udelay(usecs);
}

/**
 * os_msleep() - sleep for a number of milliseconds
 * @msecs: delay duration (ms).
 */
static inline void os_msleep(int msecs)
{
	msleep(msecs);
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
static inline void __iomem *os_map_producer_port(struct dlb2_hw *hw,
						 u8 port_id,
						 bool is_ldb)
{
	unsigned long size;
	uintptr_t address;
	struct dlb2 *dlb2;

	dlb2 = container_of(hw, struct dlb2, hw);

	address = (uintptr_t)dlb2->hw.func_kva;

	if (is_ldb) {
		size = DLB2_LDB_PP_STRIDE;
		address += DLB2_DRV_LDB_PP_BASE + size * port_id;
	} else {
		size = DLB2_DIR_PP_STRIDE;
		address += DLB2_DRV_DIR_PP_BASE + size * port_id;
	}

	return (void __iomem *)address;
}

static inline void __iomem *os_map_producer_port_maskable(struct dlb2_hw *hw,
						 u8 port_id,
						 bool is_ldb)
{
	unsigned long size;
	uintptr_t address;
	struct dlb2 *dlb2;

	dlb2 = container_of(hw, struct dlb2, hw);

	address = (uintptr_t)dlb2->hw.func_kva;

	if (is_ldb) {
		size = DLB2_LDB_PP_STRIDE;
		address += DLB2_LDB_PP_BASE + size * port_id;
	} else {
		size = DLB2_DIR_PP_STRIDE;
		address += DLB2_DIR_PP_BASE + size * port_id;
	}

	return (void __iomem *)address;
}

/**
 * os_unmap_producer_port() - unmap a producer port
 * @hw: dlb2_hw handle for a particular device.
 * @addr: mapped producer port address
 *
 * This function undoes os_map_producer_port() by unmapping the producer port
 * memory from the caller's address space.
 *
 * Return:
 * Returns the base address at which the PP memory was mapped, else NULL.
 */
static inline void os_unmap_producer_port(struct dlb2_hw *hw,
					  void __iomem *addr)
{
	return;
}

/**
 * os_fence_hcw() - fence an HCW to ensure it arrives at the device
 * @hw: dlb2_hw handle for a particular device.
 * @pp_addr: producer port address
 */
static inline void os_fence_hcw(struct dlb2_hw *hw, void __iomem *pp_addr)
{
	/*
	 * To ensure outstanding HCWs reach the device before subsequent device
	 * accesses, fence them.
	 */
	mb();
}

/**
 * os_enqueue_four_hcws() - enqueue four HCWs to DLB
 * @hw: dlb2_hw handle for a particular device.
 * @hcw: pointer to the 64B-aligned contiguous HCW memory
 * @addr: producer port address
 */
static inline void os_enqueue_four_hcws(struct dlb2_hw *hw,
					struct dlb2_hcw *hcw,
					void __iomem *addr)
{
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);

	dlb2->enqueue_four(hcw, addr);
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
	struct dlb2_domain *domain;
	struct dlb2 *dlb2;

	dlb2 = container_of(hw, struct dlb2, hw);

	if (domain_id >= DLB2_MAX_NUM_DOMAINS) {
		dev_err(dlb2->dev,
			"[%s()] Internal error\n", __func__);
		return -EINVAL;
	}

	if (hw->domains[domain_id].id.vdev_owned) {
		WARN_ON(hw->domains[domain_id].id.vdev_owned);
		return -EINVAL;
	}

	domain = dlb2->sched_domains[domain_id];

	if (!domain) {
		dev_err(dlb2->dev,
			"[%s()] Internal error\n", __func__);
		return -EINVAL;
	}

	return dlb2_write_domain_alert(domain, alert_id, aux_alert_data);
}

/**
 * DLB2_HW_ERR() - log an error message
 * @hw: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_ERR(hw, ...) do {		  \
	struct dlb2 *dlb2;			  \
	dlb2 = container_of(hw, struct dlb2, hw); \
	dev_err(dlb2->dev, __VA_ARGS__);	  \
} while (0)

/**
 * DLB2_HW_DBG() - log a debug message
 * @dlb2: dlb2_hw handle for a particular device.
 * @...: variable string args.
 */
#define DLB2_HW_DBG(hw, ...) do {		  \
	struct dlb2 *dlb2;			  \
	dlb2 = container_of(hw, struct dlb2, hw); \
	dev_dbg(dlb2->dev, __VA_ARGS__);	  \
} while (0)

/*** Workqueue scheduling functions ***/

/*
 * The workqueue callback runs until it completes all outstanding QID->CQ
 * map and unmap requests. To prevent deadlock, this function gives other
 * threads a chance to grab the resource mutex and configure hardware.
 */
static inline void dlb2_complete_queue_map_unmap(struct work_struct *work)
{
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(work, struct dlb2, work);

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_finish_unmap_qid_procedures(&dlb2->hw);
	ret += dlb2_finish_map_qid_procedures(&dlb2->hw);

	if (ret != 0)
		/*
		 * Relinquish the CPU so the application can process its CQs,
		 * so this function doesn't deadlock.
		 */
		schedule_work(&dlb2->work);
	else
		dlb2->worker_launched = false;

	mutex_unlock(&dlb2->resource_mutex);
}

/**
 * os_schedule_work() - launch a thread to process pending map and unmap work
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function launches a kernel thread that will run until all pending
 * map and unmap procedures are complete.
 */
static inline void os_schedule_work(struct dlb2_hw *hw)
{
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);

	schedule_work(&dlb2->work);

	dlb2->worker_launched = true;
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
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);

	return dlb2->worker_launched;
}

#endif /*  __DLB2_OSDEP_H */
