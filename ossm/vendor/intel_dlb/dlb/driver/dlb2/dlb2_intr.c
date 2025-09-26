// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include "base/dlb2_resource.h"
#include "dlb2_intr.h"
#include "dlb2_main.h"

void dlb2_wake_thread(struct dlb2_cq_intr *intr, enum dlb2_wake_reason reason)
{
	switch (reason) {
	case WAKE_CQ_INTR:
		WRITE_ONCE(intr->wake, true);
		break;
	case WAKE_PORT_DISABLED:
		WRITE_ONCE(intr->disabled, true);
		break;
	default:
		break;
	}

	wake_up_interruptible(&intr->wq_head);
}

static inline bool wake_condition(struct dlb2_cq_intr *intr,
				  struct dlb2 *dlb2,
				  struct dlb2_domain *domain)
{
	return (READ_ONCE(intr->wake) ||
		READ_ONCE(dlb2->reset_active) ||
		!READ_ONCE(domain->valid) ||
		READ_ONCE(intr->disabled));
}

struct dlb2_dequeue_qe {
	u8 rsvd0[15];
	u8 cq_gen:1;
	u8 rsvd1:7;
} __packed;

/**
 * dlb2_cq_empty() - determine whether a CQ is empty
 * @user_cq_va: User VA pointing to next CQ entry.
 * @cq_gen: Current CQ generation bit.
 *
 * Return:
 * Returns 1 if empty, 0 if non-empty, or < 0 if an error occurs.
 */
static int dlb2_cq_empty(u64 user_cq_va, u8 cq_gen)
{

	/* Check if the user_cq_va is in kernel space since this function may
	 * be used by the kernel data path. If it is, not need to call
	 * copy_from_user().
	 */
	if (virt_addr_valid(user_cq_va)) {
		struct dlb2_dequeue_qe *qe_ptr = (void *)user_cq_va;

		return qe_ptr->cq_gen != cq_gen;
	} else {
		struct dlb2_dequeue_qe qe;

		if (copy_from_user(&qe, (void __user *)user_cq_va, sizeof(qe)))
			return -EFAULT;

		return qe.cq_gen != cq_gen;
	}
}

/**
 * dlb2_is_siov_vdev() - determine whether dev is a Scalable IOV VDEV
 * @dlb2: struct dlb2 pointer.
 */
bool dlb2_is_siov_vdev(struct dlb2 *dlb2)
{
	return DLB2_IS_VF(dlb2) &&
	       dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SIOV;
}

int dlb2_block_on_cq_interrupt(struct dlb2 *dlb2,
			       struct dlb2_domain *dom,
			       int port_id,
			       bool is_ldb,
			       u64 cq_va,
			       u8 cq_gen,
			       bool arm)
{
	struct dlb2_cq_intr *intr;
	int ret;

	if (is_ldb && port_id >= DLB2_MAX_NUM_LDB_PORTS)
		return -EINVAL;
	if (!is_ldb && port_id >= DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver))
		return -EINVAL;

	if (is_ldb)
		intr = &dlb2->intr.ldb_cq_intr[port_id];
	else
		intr = &dlb2->intr.dir_cq_intr[port_id];

	/*
	 * If the user assigns more CQs to a VF resource group than there are
	 * interrupt vectors (31 per VF), then some of its CQs won't be
	 * configured for interrupts.
	 */
	if (!intr->configured || intr->domain_id != dom->id)
		return -EPERM;

	/*
	 * This function requires that only one thread process the CQ at a time.
	 * Otherwise, the wake condition could become false in the time between
	 * the ISR calling wake_up_interruptible() and the thread checking its
	 * wake condition.
	 */
	mutex_lock(&intr->mutex);

	/* Return early if the port's interrupt is disabled */
	if (READ_ONCE(intr->disabled)) {
		mutex_unlock(&intr->mutex);
		return -EACCES;
	}

	dev_dbg(dlb2->dev,
		"Thread is blocking on %s port %d's interrupt\n",
		(is_ldb) ? "LDB" : "DIR", port_id);

	/* Don't block if the CQ is non-empty */
	ret = dlb2_cq_empty(cq_va, cq_gen);
	if (ret != 1)
		goto error;

	if (arm) {
		ret = dlb2->ops->arm_cq_interrupt(dlb2,
						  dom->id,
						  port_id,
						  is_ldb);
		if (ret)
			goto error;
	}

	do {
		ret = wait_event_interruptible_timeout(intr->wq_head,
					       wake_condition(intr, dlb2, dom), 1);

		if (ret >= 0) {
			if (READ_ONCE(dlb2->reset_active) ||
			    !READ_ONCE(dom->valid))
				ret = -EINTR;
			else if (READ_ONCE(intr->disabled))
				ret = -EACCES;
		}

		WRITE_ONCE(intr->wake, false);

		/*
		 * In case of spurious CQ interrupts or timeout with a false
		 * condition, wait again. The workaround does not disarm the
		 * interrupt, so no need to re-arm it.
		 */
	} while (ret >= 0 &&
		 dlb2_cq_empty(cq_va, cq_gen));

	/* Unlike wait_event_interruptible(), wait_event_interruptible_timeout() returns
	 * > 0 on success. 0 is returned if the condition evaluates to false after the
	 * timeout is elapsed. Updating ret as a workaround to avoid changes in the
	 * calling function that expects ret = 0 on success.
	 */

	ret = (ret > 0)? 0 : ret;

	dev_dbg(dlb2->dev,
		"Thread is unblocked from %s port %d's interrupt\n",
		(is_ldb) ? "LDB" : "DIR", port_id);

error:
	mutex_unlock(&intr->mutex);

	return ret;
}
