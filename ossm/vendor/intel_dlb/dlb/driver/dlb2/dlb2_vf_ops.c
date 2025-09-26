// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/eventfd.h>

#include "base/dlb2_mbox.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_resource.h"
#include "dlb2_dp_ops.h"
#include "dlb2_intr.h"
#include "dlb2_main.h"

/***********************************/
/****** Mailbox communication ******/
/***********************************/

#define DLB2_MBOX_TOUT	10

static enum dlb2_mbox_error_code
dlb2_mbox_error_to_errno(int ret)
{
	switch (ret) {
	case DLB2_MBOX_SUCCESS: return 0;
	case DLB2_MBOX_EFAULT: return -EFAULT;
	case DLB2_MBOX_EPERM: return -EPERM;
	case DLB2_MBOX_ETIMEDOUT: return -ETIMEDOUT;
	case DLB2_MBOX_EINVAL: /* fallthrough */
	default: return -EINVAL;
	}
}

static int
dlb2_send_sync_mbox_cmd(struct dlb2 *dlb2,
			void *data,
			int len,
			int timeout_s)
{
	struct dlb2_mbox_req_hdr *req = data;
	int ret, retry_cnt, cmd_if_ver;
	int cmd = req->type;

	if (len > VF_VF2PF_MAILBOX_BYTES) {
		dev_err(dlb2->dev,
			"Internal error: VF mbox message too large\n");
		return -1;
	}

	if (cmd >= ARRAY_SIZE(dlb2_mbox_cmd_version)) {
		dev_err(dlb2->dev,
			"Internal error: add VF mbox interface version for cmd"
			" %d\n", cmd);
		return -1;
	}

	cmd_if_ver = dlb2_mbox_cmd_version[cmd];
	if (dlb2->vf_id_state.pf_interface_version < cmd_if_ver) {
		dev_err(dlb2->dev,
			"MBOX cmd %s (version: %d) unsupported by PF driver"
			"(version: %d)\n", dlb2_mbox_cmd_type_strings[cmd],
			cmd_if_ver, dlb2->vf_id_state.pf_interface_version);
		return -ENOTSUPP;
	}

	ret = dlb2_vf_write_pf_mbox_req(&dlb2->hw, data, len);
	if (ret)
		return ret;

	dlb2_send_async_vdev_to_pf_msg(&dlb2->hw);

	/* Timeout after timeout_s seconds of inactivity */
	retry_cnt = 1000 * timeout_s;
	do {
		if (dlb2_vdev_to_pf_complete(&dlb2->hw))
			break;
		usleep_range(1000, 1001);
	} while (--retry_cnt);

	if (!retry_cnt) {
		dev_err(dlb2->dev,
			"VF driver timed out waiting for mbox response\n");
		return -1;
	}

	return 0;
}

static int
dlb2_vf_mbox_dev_reset(struct dlb2 *dlb2)
{
	struct dlb2_mbox_dev_reset_cmd_resp resp;
	struct dlb2_mbox_dev_reset_cmd_req req;
	int ret;

	req.hdr.type = DLB2_MBOX_CMD_DEV_RESET;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret) {
		mutex_unlock(&dlb2->resource_mutex);
		return ret;
	}

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"VF reset failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));
	}

	mutex_unlock(&dlb2->resource_mutex);

	return dlb2_mbox_error_to_errno(resp.error_code);
}

/********************************/
/****** PCI BAR management ******/
/********************************/

static void
dlb2_vf_unmap_pci_bar_space(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	pci_iounmap(pdev, dlb2->hw.func_kva);
}

static int
dlb2_vf_map_pci_bar_space(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	enum dlb2_virt_mode mode;

	dlb2->hw.func_kva = pci_iomap(pdev, DLB2_FUNC_BAR, 0);
	dlb2->hw.func_phys_addr = pci_resource_start(pdev, DLB2_FUNC_BAR);

	if (!dlb2->hw.func_kva) {
		dev_err(&pdev->dev, "Cannot iomap BAR 0 (size %llu)\n",
			pci_resource_len(pdev, 0));

		return -EIO;
	}

	/*
	 * Before the driver can use its mailbox, it needs to identify whether
	 * its device is a VF (SR-IOV) or VDEV (Scalable IOV), because the
	 * mailbox interface differs slightly among the two. Detect by looking
	 * for an MSI-X capability (Scalable IOV only).
	 */
	mode = (pdev->msix_cap) ? DLB2_VIRT_SIOV : DLB2_VIRT_SRIOV;

	dlb2_hw_set_virt_mode(&dlb2->hw, mode);

	return 0;
}

/**********************************/
/****** Interrupt management ******/
/**********************************/

/*
 * Claim any unclaimed CQ interrupts from the primary VF. We use the primary's
 * *_cq_intr[] structure, vs. the auxiliary's copy of that structure, because
 * if the aux VFs are unbound, their memory will be lost and any blocked
 * threads in the primary's waitqueues could access their freed memory.
 */
static void
dlb2_vf_claim_cq_interrupts(struct dlb2 *dlb2)
{
	struct dlb2 *primary_vf;
	int i, cnt;
	u8 nvecs;

	dlb2->intr.num_cq_intrs = 0;
	primary_vf = dlb2->vf_id_state.primary_vf;

	if (!primary_vf)
		return;

	nvecs = DLB2_VF_NUM_CQ_INTERRUPT_VECTORS;
	cnt = 0;

	for (i = 0; i < primary_vf->intr.num_ldb_ports; i++) {
		if (primary_vf->intr.ldb_cq_intr_owner[i])
			continue;

		primary_vf->intr.ldb_cq_intr_owner[i] = dlb2;

		dlb2->intr.msi_map[cnt].port_id = i;
		dlb2->intr.msi_map[cnt].is_ldb = true;
		cnt++;

		dlb2->intr.num_cq_intrs++;

		if (dlb2->intr.num_cq_intrs == nvecs)
			return;
	}

	for (i = 0; i <  primary_vf->intr.num_dir_ports; i++) {
		if (primary_vf->intr.dir_cq_intr_owner[i])
			continue;

		primary_vf->intr.dir_cq_intr_owner[i] = dlb2;

		dlb2->intr.msi_map[cnt].port_id = i;
		dlb2->intr.msi_map[cnt].is_ldb = false;
		cnt++;

		dlb2->intr.num_cq_intrs++;

		if (dlb2->intr.num_cq_intrs == nvecs)
			return;
	}
}

static void
dlb2_vf_unclaim_cq_interrupts(struct dlb2 *dlb2)
{
	struct dlb2 *primary_vf;
	int i;

	primary_vf = dlb2->vf_id_state.primary_vf;

	if (!primary_vf)
		return;

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		if (primary_vf->intr.ldb_cq_intr_owner[i] != dlb2)
			continue;

		primary_vf->intr.ldb_cq_intr_owner[i] = NULL;
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++) {
		if (primary_vf->intr.dir_cq_intr_owner[i] != dlb2)
			continue;

		primary_vf->intr.dir_cq_intr_owner[i] = NULL;
	}
}

static void
dlb2_vf_mbox_cmd_alarm_fn(struct dlb2 *dlb2, void *data)
{
	struct dlb2_mbox_vf_alert_cmd_req *req = data;

	if (os_notify_user_space(&dlb2->hw,
				 req->domain_id,
				 req->alert_id,
				 req->aux_alert_data))
		dev_err(dlb2->dev,
			"[%s()] Internal error: failed to notify user-space\n",
			__func__);

	/* No response needed beyond ACKing the interrupt. */
}

static void
dlb2_vf_mbox_cmd_notification_fn(struct dlb2 *dlb2, void *data)
{
	struct dlb2_mbox_vf_notification_cmd_req *req = data;

	/* If the VF is auxiliary, it has no resources affected by PF reset */
	if (dlb2->vf_id_state.is_auxiliary_vf)
		return;

	/*
	 * When the PF is reset, it notifies every registered VF driver
	 * immediately prior to the reset.
	 *
	 * The pre-reset notification gives the VF an opportunity to notify its
	 * users to shutdown. The PF driver will not proceed with the reset
	 * until either all VF-owned domains are reset (and all the PF's users
	 * quiesce), or the PF driver's reset wait timeout expires.
	 */
	if (req->notification == DLB2_MBOX_VF_NOTIFICATION_PRE_RESET) {
		dev_warn(dlb2->dev,
			 "PF is being reset. To continue using the device, reload the driver.\n");

		/*
		 * Before the reset occurs, wake up all active users and block
		 * them from continuing to access the device.
		 */
		mutex_lock(&dlb2->resource_mutex);

		/* Block any new device files from being opened */
		dlb2->reset_active = true;

		/*
		 * Stop existing applications from continuing to use the device
		 * by blocking kernel driver interfaces and waking any threads
		 * on wait queues.
		 */
		dlb2_stop_users(dlb2);

		/*
		 * Unmap any MMIO mappings that could be used to access the
		 * device during the FLR
		 */
		dlb2_unmap_all_mappings(dlb2);

		/*
		 * Release resource_mutex, allowing users to clean up their
		 * port and domain files. reset_active will remain true until
		 * the driver is reloaded.
		 */
		mutex_unlock(&dlb2->resource_mutex);
	}

	/* No response needed beyond ACKing the interrupt. */
}

static void
dlb2_vf_mbox_cmd_in_use_fn(struct dlb2 *dlb2, void *data)
{
	struct dlb2_mbox_vf_in_use_cmd_resp resp;

	/* If the VF is auxiliary, the PF shouldn't send it an in-use request */
	if (dlb2->vf_id_state.is_auxiliary_vf) {
		dev_err(dlb2->dev,
			"Internal error: VF in-use request sent to auxiliary vf %d\n",
			dlb2->vf_id_state.vf_id);
		return;
	}

	resp.in_use = dlb2_in_use(dlb2);
	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	dlb2_vf_write_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));
}

static void (*vf_mbox_fn_table[])(struct dlb2 *dlb2, void *data) = {
	dlb2_vf_mbox_cmd_alarm_fn,
	dlb2_vf_mbox_cmd_notification_fn,
	dlb2_vf_mbox_cmd_in_use_fn,
};

/*
 * If an mbox request handler acquires the resource mutex, deadlock can occur.
 * For example:
 * - The PF driver grabs its resource mutex and issues a mailbox request to VF
 *   N, then waits for a response.
 * - At the same time, VF N grabs its resource mutex and issues a mailbox
 *   request, then waits for a response.
 *
 * In this scenario, both the PF and VF's mailbox handlers will block
 * attempting to grab their respective resource mutex.
 *
 * We avoid this deadlock by deferring the execution of VF handlers that
 * acquire the resource mutex until after ACKing the interrupt, which allows
 * the PF to release its resource mutex. This is possible because those VF
 * handlers don't send any response data to the PF, which must be sent prior to
 * ACKing the interrupt.
 *
 * In fact, we defer any handler that doesn't send a response, including those
 * that don't acquire the resource mutex. Handlers that respond to the PF
 * cannot be deferred.
 */
static const bool deferred_mbox_hdlrs[] = {
	[DLB2_MBOX_VF_CMD_DOMAIN_ALERT] = true,
	[DLB2_MBOX_VF_CMD_NOTIFICATION] = true,
	[DLB2_MBOX_VF_CMD_IN_USE] = false,
};

static void
dlb2_vf_handle_pf_req(struct dlb2 *dlb2)
{
	u8 data[DLB2_PF2VF_REQ_BYTES];
	bool deferred;

	dlb2_vf_read_pf_mbox_req(&dlb2->hw, &data, sizeof(data));

	deferred = deferred_mbox_hdlrs[DLB2_MBOX_CMD_TYPE(data)];

	dev_dbg(dlb2->dev,
		"[%s()] pf request received: %s\n",
		__func__, dlb2_mbox_vf_cmd_type_strings[DLB2_MBOX_CMD_TYPE(data)]);

	if (!deferred)
		vf_mbox_fn_table[DLB2_MBOX_CMD_TYPE(data)](dlb2, data);

	dlb2_ack_pf_mbox_int(&dlb2->hw);

	if (deferred)
		vf_mbox_fn_table[DLB2_MBOX_CMD_TYPE(data)](dlb2, data);
}

static irqreturn_t
dlb2_vf_intr_handler(int irq, void *hdlr_ptr)
{
	struct dlb2 *dlb2 = hdlr_ptr;
	u32 interrupts, mask, ack;
	struct dlb2 *primary_vf;
	unsigned int vector;
	int i;

	primary_vf = dlb2->vf_id_state.primary_vf;

	vector = irq - dlb2->intr.base_vector;

	mask = dlb2->intr.num_vectors - 1;

	interrupts = dlb2_read_vf_intr_status(&dlb2->hw);

	ack = 0;

	for (i = 0; i < DLB2_VF_TOTAL_NUM_INTERRUPT_VECTORS; i++) {
		if ((i & mask) == vector && (interrupts & (1 << i)))
			ack |= (1 << i);
	}

	dlb2_ack_vf_intr_status(&dlb2->hw, ack);

	for (i = 0; i < DLB2_VF_TOTAL_NUM_INTERRUPT_VECTORS; i++) {
		struct dlb2_cq_intr *intr;
		int port_id;

		if ((i & mask) != vector || !(interrupts & (1 << i)))
			continue;

		if (i == DLB2_VF_MBOX_VECTOR_ID) {
			dlb2_vf_handle_pf_req(dlb2);
			continue;
		}

		port_id = dlb2->intr.msi_map[i].port_id;

		if (dlb2->intr.msi_map[i].is_ldb) {
			/* For epoll implementation */
			if (primary_vf->ldb_port[port_id].efd_ctx)
				DLB2_EVENTFD_SIGNAL(primary_vf->ldb_port[port_id].efd_ctx);
			else {
				intr = &primary_vf->intr.ldb_cq_intr[port_id];
				dlb2_wake_thread(intr, WAKE_CQ_INTR);
			}
		} else {
			if (primary_vf->dir_port[port_id].efd_ctx)
				DLB2_EVENTFD_SIGNAL(primary_vf->dir_port[port_id].efd_ctx);
			else {
				intr = &primary_vf->intr.dir_cq_intr[port_id];
				dlb2_wake_thread(intr, WAKE_CQ_INTR);
			}
		}
	}

	dlb2_ack_vf_msi_intr(&dlb2->hw, 1 << vector);

	return IRQ_HANDLED;
}

static irqreturn_t
dlb2_vdev_intr_handler(int irq, void *hdlr_ptr)
{
	struct dlb2 *dlb2 = hdlr_ptr;
	unsigned int vector;

	vector = irq - dlb2->intr.base_vector;

	if (vector == DLB2_INT_NON_CQ) {
		dlb2_vf_handle_pf_req(dlb2);
	} else {
		struct dlb2_cq_intr *intr;
		int port_id, idx;

		idx = vector - 1;
		port_id = dlb2->intr.msi_map[idx].port_id;

		if (dlb2->intr.msi_map[idx].is_ldb) {
			/* For epoll implementation */
			if (dlb2->ldb_port[port_id].efd_ctx)
				DLB2_EVENTFD_SIGNAL(dlb2->ldb_port[port_id].efd_ctx);
			else {
				intr = &dlb2->intr.ldb_cq_intr[port_id];
				dlb2_wake_thread(intr, WAKE_CQ_INTR);
			}
		} else {
			if (dlb2->dir_port[port_id].efd_ctx)
				DLB2_EVENTFD_SIGNAL(dlb2->dir_port[port_id].efd_ctx);
			else {
				intr = &dlb2->intr.dir_cq_intr[port_id];
				dlb2_wake_thread(intr, WAKE_CQ_INTR);
			}
		}
	}

	return IRQ_HANDLED;
}

static void
dlb2_vf_get_cq_interrupt_name(struct dlb2 *dlb2, int vector)
{
	int port_id;
	bool is_ldb;

	port_id = dlb2->intr.msi_map[vector].port_id;
	is_ldb = dlb2->intr.msi_map[vector].is_ldb;

	snprintf(dlb2->intr.msi_map[vector].name,
		 sizeof(dlb2->intr.msi_map[vector].name) - 1,
		 "dlb2_%s_cq_%d", is_ldb ? "ldb" : "dir", port_id);
}

static int
dlb2_vf_init_interrupt_handlers(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int i, ret;

	/* Request CQ interrupt vectors */
	for (i = 0; i < dlb2->intr.num_vectors - 1; i++) {
		/*
		 * We allocate IRQ vectors in power-of-2 units but may have
		 * non-power-of-2 CQs to service. Don't register more handlers
		 * than are needed.
		 */
		if (i == dlb2->intr.num_cq_intrs)
			break;

		dlb2_vf_get_cq_interrupt_name(dlb2, i);

		ret = request_threaded_irq(pci_irq_vector(pdev, i),
					   NULL,
					   dlb2_vf_intr_handler,
					   IRQF_ONESHOT,
					   dlb2->intr.msi_map[i].name,
					   dlb2);
		if (ret)
			return ret;

		dlb2->intr.isr_registered[i] = true;
	}

	/* Request the mailbox interrupt vector */
	i = dlb2->intr.num_vectors - 1;

	ret = request_threaded_irq(pci_irq_vector(pdev, i),
				   NULL,
				   dlb2_vf_intr_handler,
				   IRQF_ONESHOT,
				   "dlb2_pf_to_vf_mbox",
				   dlb2);
	if (ret)
		return ret;

	dlb2->intr.isr_registered[i] = true;

	return 0;
}

static int
dlb2_vdev_init_interrupt_handlers(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int i, ret;

	/* Request the mailbox interrupt vector */
	i = DLB2_INT_NON_CQ;

	ret = request_threaded_irq(pci_irq_vector(pdev, i),
				   NULL,
				   dlb2_vdev_intr_handler,
				   IRQF_ONESHOT,
				   "dlb2_pf_to_vf_mbox",
				   dlb2);
	if (ret)
		return ret;

	dlb2->intr.isr_registered[i] = true;

	i++;

	/* Request CQ interrupt vectors */
	for (; i < dlb2->intr.num_vectors; i++) {
		int cq_idx = i - 1;
		char *name;

		name = dlb2->intr.msi_map[cq_idx].name;

		dlb2_vf_get_cq_interrupt_name(dlb2, cq_idx);

		ret = request_threaded_irq(pci_irq_vector(pdev, i),
					   NULL,
					   dlb2_vdev_intr_handler,
					   IRQF_ONESHOT,
					   name,
					   dlb2);
		if (ret)
			return ret;

		dlb2->intr.isr_registered[i] = true;
	}

	return 0;
}

static int
dlb2_vf_init_sriov_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int ret, req_size;
	int num_cq_intrs;

	/*
	 * Claim a batch of CQs from the primary VF for assigning to MSI
	 * vectors (if the primary VF has been probed).
	 */
	dlb2_vf_claim_cq_interrupts(dlb2);

	/*
	 * Request IRQ vectors. The request size depends on the number of CQs
	 * this VF claimed -- it will attempt to take enough for a 1:1 mapping,
	 * else it falls back to a single vector.
	 */
	num_cq_intrs = dlb2->intr.num_cq_intrs;

	if ((num_cq_intrs + DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS) > 16)
		req_size = 32;
	else if ((num_cq_intrs + DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS) > 8)
		req_size = 16;
	else if ((num_cq_intrs + DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS) > 4)
		req_size = 8;
	else if ((num_cq_intrs + DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS) > 2)
		req_size = 4;
	else if ((num_cq_intrs + DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS) > 1)
		req_size = 2;
	else
		req_size = 1;

	ret = pci_alloc_irq_vectors(pdev, req_size, req_size, PCI_IRQ_MSI);
	if (ret < 0) {
		ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
		if (ret < 0)
			return ret;
	}

	dlb2->intr.num_vectors = ret;
	dlb2->intr.base_vector = pci_irq_vector(pdev, 0);

	return 0;
}

static int
dlb2_vf_init_siov_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int ret, nvec, i;

	dlb2->intr.num_cq_intrs = 0;

	for (i = 0; i < dlb2->intr.num_ldb_ports; i++) {
		dlb2->intr.ldb_cq_intr_owner[i] = dlb2;
		dlb2->intr.msi_map[i].port_id = i;
		dlb2->intr.msi_map[i].is_ldb = true;
	}

	for (i = 0; i < dlb2->intr.num_dir_ports; i++) {
		int idx = dlb2->intr.num_ldb_ports + i;

		dlb2->intr.dir_cq_intr_owner[i] = dlb2;
		dlb2->intr.msi_map[idx].port_id = i;
		dlb2->intr.msi_map[idx].is_ldb = false;
	}

	nvec = pci_msix_vec_count(pdev);
	if (nvec < 0)
		return nvec;

	ret = pci_alloc_irq_vectors(pdev, nvec, nvec, PCI_IRQ_MSIX);
	if (ret < 0) {
		dev_err(dlb2->dev,
			"Error: unable to allocate %d MSI-X vectors.\n",
			nvec);
		return ret;
	}

	dlb2->intr.num_vectors = ret;
	dlb2->intr.base_vector = pci_irq_vector(pdev, 0);

	dlb2->intr.num_cq_intrs = ret - 1;

	return 0;
}

static void
dlb2_vf_free_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int i;

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		dlb2_vf_unclaim_cq_interrupts(dlb2);

	for (i = 0; i < dlb2->intr.num_vectors; i++) {
		if (dlb2->intr.isr_registered[i])
			free_irq(pci_irq_vector(pdev, i), dlb2);
	}

	pci_free_irq_vectors(pdev);
}

static int
dlb2_vf_init_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int ret, i;

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		ret = dlb2_vf_init_sriov_interrupts(dlb2, pdev);
	else
		ret = dlb2_vf_init_siov_interrupts(dlb2, pdev);

	if (ret)
		return ret;

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		ret = dlb2_vf_init_interrupt_handlers(dlb2, pdev);
	else
		ret = dlb2_vdev_init_interrupt_handlers(dlb2, pdev);

	if (ret) {
		dlb2_vf_free_interrupts(dlb2, pdev);
		return ret;
	}

	/*
	 * Initialize per-CQ interrupt structures, such as wait queues that
	 * threads will wait on until the CQ's interrupt fires.
	 */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		init_waitqueue_head(&dlb2->intr.ldb_cq_intr[i].wq_head);
		mutex_init(&dlb2->intr.ldb_cq_intr[i].mutex);
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++) {
		init_waitqueue_head(&dlb2->intr.dir_cq_intr[i].wq_head);
		mutex_init(&dlb2->intr.dir_cq_intr[i].mutex);
	}

	return 0;
}

static void
dlb2_vf_reinit_interrupts(struct dlb2 *dlb2)
{
}

static int
dlb2_vf_enable_ldb_cq_interrupts(struct dlb2 *dlb2,
				 int domain_id,
				 int id,
				 u16 thresh)
{
	struct dlb2_mbox_enable_ldb_port_intr_cmd_resp resp;
	struct dlb2_mbox_enable_ldb_port_intr_cmd_req req;
	struct dlb2 *owner;
	int ret;
	int i;

	/*
	 * If no owner was registered, dlb2->intr...configured remains false,
	 * and any attempts to block on the CQ interrupt will fail. This will
	 * only happen if the VF doesn't have enough auxiliary VFs to service
	 * its CQ interrupts.
	 */
	owner = dlb2->intr.ldb_cq_intr_owner[id];
	if (!owner) {
		dev_dbg(dlb2->dev,
			"[%s()] LDB port %d has no interrupt owner\n",
			__func__, id);
		return 0;
	}

	for (i = 0; i <= owner->intr.num_cq_intrs; i++) {
		if (owner->intr.msi_map[i].port_id == id &&
		    owner->intr.msi_map[i].is_ldb)
			break;
	}

	dlb2->intr.ldb_cq_intr[id].disabled = false;
	dlb2->intr.ldb_cq_intr[id].configured = true;
	dlb2->intr.ldb_cq_intr[id].domain_id = domain_id;

	req.hdr.type = DLB2_MBOX_CMD_ENABLE_LDB_PORT_INTR;
	req.port_id = id;
	req.vector = i;
	req.owner_vf = owner->vf_id_state.vf_id;
	req.thresh = thresh;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"LDB CQ interrupt enable failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));
	}

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_enable_dir_cq_interrupts(struct dlb2 *dlb2,
				 int domain_id,
				 int id,
				 u16 thresh)
{
	struct dlb2_mbox_enable_dir_port_intr_cmd_resp resp;
	struct dlb2_mbox_enable_dir_port_intr_cmd_req req;
	struct dlb2 *owner;
	int ret;
	int i;

	/*
	 * If no owner was registered, dlb2->intr...configured remains false,
	 * and any attempts to block on the CQ interrupt will fail. This will
	 * only happen if the VF doesn't have enough auxiliary VFs to service
	 * its CQ interrupts.
	 */
	owner = dlb2->intr.dir_cq_intr_owner[id];
	if (!owner) {
		dev_dbg(dlb2->dev,
			"[%s()] DIR port %d has no interrupt owner\n",
			__func__, id);
		return 0;
	}

	for (i = 0; i <= owner->intr.num_cq_intrs; i++) {
		if (owner->intr.msi_map[i].port_id == id &&
		    !owner->intr.msi_map[i].is_ldb)
			break;
	}

	dlb2->intr.dir_cq_intr[id].disabled = false;
	dlb2->intr.dir_cq_intr[id].configured = true;
	dlb2->intr.dir_cq_intr[id].domain_id = domain_id;

	req.hdr.type = DLB2_MBOX_CMD_ENABLE_DIR_PORT_INTR;
	req.port_id = id;
	req.vector = i;
	req.owner_vf = owner->vf_id_state.vf_id;
	req.thresh = thresh;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"DIR CQ interrupt enable failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));
	}

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_arm_cq_interrupt(struct dlb2 *dlb2,
			 int domain_id,
			 int port_id,
			 bool is_ldb)
{
	struct dlb2_mbox_arm_cq_intr_cmd_resp resp;
	struct dlb2_mbox_arm_cq_intr_cmd_req req;
	int ret;

	req.hdr.type = DLB2_MBOX_CMD_ARM_CQ_INTR;
	req.domain_id = domain_id;
	req.port_id = port_id;
	req.is_ldb = is_ldb;

	/*
	 * Unlike other VF ioctl callbacks, this one isn't called while holding
	 * the resource mutex. However, we must serialize access to the mailbox
	 * to prevent data corruption.
	 */
	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret) {
		mutex_unlock(&dlb2->resource_mutex);
		return ret;
	}

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"LDB CQ interrupt enable failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));
	}

	mutex_unlock(&dlb2->resource_mutex);

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static struct
dlb2 *dlb2_vf_get_primary(struct dlb2 *dlb2)
{
	struct vf_id_state *vf_id_state;
	bool found = false;
	struct dlb2 *prim;

	if (!dlb2->vf_id_state.is_auxiliary_vf)
		return dlb2;

	mutex_lock(&dlb2_driver_mutex);

	vf_id_state = &dlb2->vf_id_state;

	list_for_each_entry(prim, &dlb2_dev_list, list) {
		if (DLB2_IS_VF(prim) &&
		    prim->vf_id_state.pf_id == vf_id_state->pf_id &&
		    prim->vf_id_state.vf_id == vf_id_state->primary_vf_id) {
			found = true;
			break;
		}
	}

	mutex_unlock(&dlb2_driver_mutex);

	return (found) ? prim : NULL;
}

static int
dlb2_init_siov_vdev_interrupt_state(struct dlb2 *dlb2)
{
	struct dlb2_get_num_resources_args num_rsrcs;
	int ret;

	ret = dlb2->ops->get_num_resources(&dlb2->hw, &num_rsrcs);
	if (ret)
		return ret;

	dlb2->intr.num_ldb_ports = num_rsrcs.num_ldb_ports;
	dlb2->intr.num_dir_ports = num_rsrcs.num_dir_ports;

	return 0;
}

static int
dlb2_init_auxiliary_vf_interrupts(struct dlb2 *dlb2)
{
	struct dlb2_get_num_resources_args num_rsrcs;
	struct dlb2 *aux_vf;
	int ret;

	/*
	 * If the primary hasn't been probed yet, we can't init the auxiliary's
	 * interrupts.
	 */
	if (dlb2->vf_id_state.is_auxiliary_vf && !dlb2->vf_id_state.primary_vf)
		return 0;

	if (dlb2->vf_id_state.is_auxiliary_vf)
		return dlb2->ops->init_interrupts(dlb2, dlb2->pdev);

	/*
	 * This is a primary VF, so iniitalize all of its auxiliary siblings
	 * that were already probed.
	 */
	ret = dlb2->ops->get_num_resources(&dlb2->hw, &num_rsrcs);
	if (ret)
		goto interrupt_cleanup;

	dlb2->intr.num_ldb_ports = num_rsrcs.num_ldb_ports;
	dlb2->intr.num_dir_ports = num_rsrcs.num_dir_ports;

	mutex_lock(&dlb2_driver_mutex);

	list_for_each_entry(aux_vf, &dlb2_dev_list, list) {
		if (!DLB2_IS_VF(aux_vf))
			continue;

		if (!aux_vf->vf_id_state.is_auxiliary_vf)
			continue;

		if (aux_vf->vf_id_state.pf_id != dlb2->vf_id_state.pf_id)
			continue;

		if (aux_vf->vf_id_state.primary_vf_id !=
		    dlb2->vf_id_state.vf_id)
			continue;

		aux_vf->vf_id_state.primary_vf = dlb2;

		ret = aux_vf->ops->init_interrupts(aux_vf, aux_vf->pdev);
		if (ret)
			goto interrupt_cleanup;
	}

	mutex_unlock(&dlb2_driver_mutex);

	return 0;

interrupt_cleanup:

	list_for_each_entry(aux_vf, &dlb2_dev_list, list) {
		if (aux_vf->vf_id_state.primary_vf == dlb2)
			aux_vf->ops->free_interrupts(aux_vf, aux_vf->pdev);
	}

	mutex_unlock(&dlb2_driver_mutex);

	return ret;
}

static int
dlb2_vf_register_driver(struct dlb2 *dlb2)
{
	struct dlb2_mbox_register_cmd_resp resp;
	struct dlb2_mbox_register_cmd_req req;
	int ret;

	/*
	 * Once the VF driver's BAR space is mapped in, it must inititate a
	 * handshake with the PF driver. The purpose is twofold:
	 * 1. Confirm that the drivers are using compatible mailbox interface
	 *	versions.
	 * 2. Alert the PF driver that the VF driver is in use. This causes the
	 *	PF driver to lock the VF's assigned resources, and causes the PF
	 *	driver to notify this driver whenever device-wide activities
	 *	occur (e.g. PF FLR).
	 */

	req.hdr.type = DLB2_MBOX_CMD_REGISTER;
	/* The VF driver only supports minimum interface version 3 */
	req.min_interface_version = DLB2_MBOX_MIN_INTERFACE_VERSION;
	req.max_interface_version = DLB2_MBOX_INTERFACE_VERSION;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(&dlb2->pdev->dev,
			"VF driver registration failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));

		if (resp.hdr.status == DLB2_MBOX_ST_VERSION_MISMATCH) {
			dev_err(&dlb2->pdev->dev,
				"VF driver mailbox interface version: %d\n",
				DLB2_MBOX_INTERFACE_VERSION);
			dev_err(&dlb2->pdev->dev,
				"PF driver mailbox interface version: %d\n",
				resp.interface_version);
		}

		return -1;
	}

	if (resp.interface_version != DLB2_MBOX_INTERFACE_VERSION)
		dev_warn(&dlb2->pdev->dev,
			 "PF mbox version(%d) differs from VF mbox version(%d)."
			 " Some of the features may not be supported.\n",
			 resp.interface_version, DLB2_MBOX_INTERFACE_VERSION);

	dlb2->vf_id_state.pf_id = resp.pf_id;
	dlb2->vf_id_state.vf_id = resp.vf_id;
	dlb2->vf_id_state.is_auxiliary_vf =
		resp.flags & DLB2_MBOX_FLAG_IS_AUX_VF;
	dlb2->needs_mbox_reset = resp.flags & DLB2_MBOX_FLAG_MBOX_RESET;
	dlb2->vf_id_state.primary_vf_id = resp.primary_vf_id;
	dlb2->vf_id_state.pf_interface_version = resp.interface_version;

	/*
	 * Auxiliary VF interrupts are initialized in the register_driver
	 * callback and freed in the unregister_driver callback. There are
	 * two possible cases.
	 * 1. The auxiliary VF is probed after its primary: during the aux VF's
	 *    probe, it initializes its interrupts.
	 * 2. The auxiliary VF is probed before its primary: during the primary
	 *    VF's driver registration, it initializes the interrupts of all its
	 *    aux siblings that have already been probed.
	 */

	/* If the VF is not auxiliary, dlb2_vf_get_primary() returns dlb2. */
	dlb2->vf_id_state.primary_vf = dlb2_vf_get_primary(dlb2);

	/*
	 * If this is a primary VF, initialize the interrupts of any auxiliary
	 * VFs that were already probed. If this is an auxiliary VF and its
	 * primary has been probed, initialize the auxiliary's interrupts.
	 *
	 * If this is a Scalable IOV vdev, initialize the state needed to
	 * configure and service its CQ interrupts.
	 */
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		return dlb2_init_auxiliary_vf_interrupts(dlb2);
	else
		return dlb2_init_siov_vdev_interrupt_state(dlb2);
}

static void
dlb2_vf_unregister_driver(struct dlb2 *dlb2)
{
	struct dlb2_mbox_unregister_cmd_resp resp;
	struct dlb2_mbox_unregister_cmd_req req;
	int ret;

	/*
	 * Aux VF interrupts are initialized in the register_driver callback
	 * and freed here.
	 */
	if (dlb2->vf_id_state.is_auxiliary_vf &&
	    dlb2->vf_id_state.primary_vf)
		dlb2->ops->free_interrupts(dlb2, dlb2->pdev);

	req.hdr.type = DLB2_MBOX_CMD_UNREGISTER;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"VF driver registration failed with mailbox error: %s\n",
			dlb2_mbox_st_string(&resp.hdr));
	}
}

/*******************************/
/****** Driver management ******/
/*******************************/

static int
dlb2_vf_init_driver_state(struct dlb2 *dlb2)
{
	if (movdir64b_supported()) {
		dlb2->enqueue_four = dlb2_movdir64b;
	} else {
#ifdef CONFIG_AS_SSE2
		dlb2->enqueue_four = dlb2_movntdq;
#else
		dev_err(dlb2->dev,
			"%s: Platforms without movdir64 must support SSE2\n",
			dlb2_driver_name);
		return -EINVAL;
#endif
	}

	/* Initialize software state */
	mutex_init(&dlb2->resource_mutex);

	return 0;
}

static void
dlb2_vf_free_driver_state(struct dlb2 *dlb2)
{
}

static void
dlb2_vf_init_hardware(struct dlb2 *dlb2)
{
	/* Function intentionally left blank */
}

/*****************************/
/****** Sysfs callbacks ******/
/*****************************/

static int
dlb2_vf_get_num_used_rsrcs(struct dlb2_hw *hw,
			   struct dlb2_get_num_resources_args *args)
{
	struct dlb2_mbox_get_num_resources_cmd_resp resp;
	struct dlb2_mbox_get_num_resources_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_NUM_USED_RESOURCES;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	args->num_sched_domains = resp.num_sched_domains;
	args->num_ldb_queues = resp.num_ldb_queues;
	args->num_ldb_ports = resp.num_ldb_ports;
	args->num_cos_ldb_ports[0] = resp.num_cos_ldb_ports[0];
	args->num_cos_ldb_ports[1] = resp.num_cos_ldb_ports[1];
	args->num_cos_ldb_ports[2] = resp.num_cos_ldb_ports[2];
	args->num_cos_ldb_ports[3] = resp.num_cos_ldb_ports[3];
	args->num_dir_ports = resp.num_dir_ports;
	args->num_atomic_inflights = resp.num_atomic_inflights;
	args->num_hist_list_entries = resp.num_hist_list_entries;
	args->max_contiguous_hist_list_entries =
		resp.max_contiguous_hist_list_entries;
	args->num_ldb_credits = resp.num_ldb_credits;
	args->num_dir_credits = resp.num_dir_credits;
	args->num_sn_slots[0] = resp.num_sn_slots[0];
	args->num_sn_slots[1] = resp.num_sn_slots[1];

	return dlb2_mbox_error_to_errno(resp.error_code);
}

#define DLB2_VF_TOTAL_SYSFS_SHOW(name)				\
static ssize_t total_##name##_show(				\
	struct device *dev,					\
	struct device_attribute *attr,				\
	char *buf)						\
{								\
	struct dlb2_get_num_resources_args rsrcs[2];		\
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		\
	struct dlb2_hw *hw = &dlb2->hw;				\
	unsigned int i;						\
	int val;						\
								\
	mutex_lock(&dlb2->resource_mutex);			\
								\
	if (dlb2->reset_active) {				\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2->ops->get_num_resources(hw, &rsrcs[0]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2_vf_get_num_used_rsrcs(hw, &rsrcs[1]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	mutex_unlock(&dlb2->resource_mutex);			\
								\
	val = 0;						\
	for (i = 0; i < ARRAY_SIZE(rsrcs); i++)			\
		val += rsrcs[i].name;				\
								\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		\
}

#define DLB2_VF_TOTAL_SYSFS_SHOW_COS(name, idx)			\
static ssize_t total_##name##_show(				\
	struct device *dev,					\
	struct device_attribute *attr,				\
	char *buf)						\
{								\
	struct dlb2_get_num_resources_args rsrcs[2];		\
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		\
	struct dlb2_hw *hw = &dlb2->hw;				\
	unsigned int i;						\
	int val;						\
								\
	mutex_lock(&dlb2->resource_mutex);			\
								\
	if (dlb2->reset_active) {				\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2->ops->get_num_resources(hw, &rsrcs[0]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2_vf_get_num_used_rsrcs(hw, &rsrcs[1]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	mutex_unlock(&dlb2->resource_mutex);			\
								\
	val = 0;						\
	for (i = 0; i < ARRAY_SIZE(rsrcs); i++)			\
		val += rsrcs[i].num_cos_ldb_ports[idx];		\
								\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		\
}

#define DLB2_VF_TOTAL_SYSFS_SHOW_SN_SLOTS(name, idx)		\
static ssize_t total_##name##_show(				\
	struct device *dev,					\
	struct device_attribute *attr,				\
	char *buf)						\
{								\
	struct dlb2_get_num_resources_args rsrcs[2];		\
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		\
	struct dlb2_hw *hw = &dlb2->hw;				\
	unsigned int i;						\
	int val;						\
								\
	mutex_lock(&dlb2->resource_mutex);			\
								\
	if (dlb2->reset_active) {				\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2->ops->get_num_resources(hw, &rsrcs[0]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	val = dlb2_vf_get_num_used_rsrcs(hw, &rsrcs[1]);	\
	if (val) {						\
		mutex_unlock(&dlb2->resource_mutex);		\
		return -1;					\
	}							\
								\
	mutex_unlock(&dlb2->resource_mutex);			\
								\
	val = 0;						\
	for (i = 0; i < ARRAY_SIZE(rsrcs); i++)			\
		val += rsrcs[i].num_sn_slots[idx];		\
								\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		\
}

DLB2_VF_TOTAL_SYSFS_SHOW(num_sched_domains)
DLB2_VF_TOTAL_SYSFS_SHOW(num_ldb_queues)
DLB2_VF_TOTAL_SYSFS_SHOW(num_ldb_ports)
DLB2_VF_TOTAL_SYSFS_SHOW_COS(num_cos0_ldb_ports, 0)
DLB2_VF_TOTAL_SYSFS_SHOW_COS(num_cos1_ldb_ports, 1)
DLB2_VF_TOTAL_SYSFS_SHOW_COS(num_cos2_ldb_ports, 2)
DLB2_VF_TOTAL_SYSFS_SHOW_COS(num_cos3_ldb_ports, 3)
DLB2_VF_TOTAL_SYSFS_SHOW(num_dir_ports)
DLB2_VF_TOTAL_SYSFS_SHOW(num_ldb_credits)
DLB2_VF_TOTAL_SYSFS_SHOW(num_dir_credits)
DLB2_VF_TOTAL_SYSFS_SHOW(num_atomic_inflights)
DLB2_VF_TOTAL_SYSFS_SHOW(num_hist_list_entries)
DLB2_VF_TOTAL_SYSFS_SHOW_SN_SLOTS(num_sn0_slots, 0)
DLB2_VF_TOTAL_SYSFS_SHOW_SN_SLOTS(num_sn1_slots, 1)

#define DLB2_VF_AVAIL_SYSFS_SHOW(name)				      \
static ssize_t avail_##name##_show(				      \
	struct device *dev,					      \
	struct device_attribute *attr,				      \
	char *buf)						      \
{								      \
	struct dlb2_get_num_resources_args num_avail_rsrcs;	      \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		      \
	struct dlb2_hw *hw = &dlb2->hw;				      \
	int val;						      \
								      \
	mutex_lock(&dlb2->resource_mutex);			      \
								      \
	if (dlb2->reset_active) {				      \
		mutex_unlock(&dlb2->resource_mutex);		      \
		return -1;					      \
	}							      \
								      \
	val = dlb2->ops->get_num_resources(hw, &num_avail_rsrcs);     \
								      \
	mutex_unlock(&dlb2->resource_mutex);			      \
								      \
	if (val)						      \
		return -1;					      \
								      \
	val = num_avail_rsrcs.name;				      \
								      \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		      \
}

#define DLB2_VF_AVAIL_SYSFS_SHOW_COS(name, idx)			      \
static ssize_t avail_##name##_show(				      \
	struct device *dev,					      \
	struct device_attribute *attr,				      \
	char *buf)						      \
{								      \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		      \
	struct dlb2_get_num_resources_args num_avail_rsrcs;	      \
	struct dlb2_hw *hw = &dlb2->hw;				      \
	int val;						      \
								      \
	mutex_lock(&dlb2->resource_mutex);			      \
								      \
	if (dlb2->reset_active) {				      \
		mutex_unlock(&dlb2->resource_mutex);		      \
		return -1;					      \
	}							      \
								      \
	val = dlb2->ops->get_num_resources(hw, &num_avail_rsrcs);     \
								      \
	mutex_unlock(&dlb2->resource_mutex);			      \
								      \
	if (val)						      \
		return -1;					      \
								      \
	val = num_avail_rsrcs.num_cos_ldb_ports[idx];		      \
								      \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		      \
}

#define DLB2_VF_AVAIL_SYSFS_SHOW_SN_SLOTS(name, idx)		      \
static ssize_t avail_##name##_show(				      \
	struct device *dev,					      \
	struct device_attribute *attr,				      \
	char *buf)						      \
{								      \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		      \
	struct dlb2_get_num_resources_args num_avail_rsrcs;	      \
	struct dlb2_hw *hw = &dlb2->hw;				      \
	int val;						      \
								      \
	mutex_lock(&dlb2->resource_mutex);			      \
								      \
	if (dlb2->reset_active) {				      \
		mutex_unlock(&dlb2->resource_mutex);		      \
		return -1;					      \
	}							      \
								      \
	val = dlb2->ops->get_num_resources(hw, &num_avail_rsrcs);     \
								      \
	mutex_unlock(&dlb2->resource_mutex);			      \
								      \
	if (val)						      \
		return -1;					      \
								      \
	val = num_avail_rsrcs.num_sn_slots[idx];		      \
								      \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		      \
}

DLB2_VF_AVAIL_SYSFS_SHOW(num_sched_domains)
DLB2_VF_AVAIL_SYSFS_SHOW(num_ldb_queues)
DLB2_VF_AVAIL_SYSFS_SHOW(num_ldb_ports)
DLB2_VF_AVAIL_SYSFS_SHOW_COS(num_cos0_ldb_ports, 0)
DLB2_VF_AVAIL_SYSFS_SHOW_COS(num_cos1_ldb_ports, 1)
DLB2_VF_AVAIL_SYSFS_SHOW_COS(num_cos2_ldb_ports, 2)
DLB2_VF_AVAIL_SYSFS_SHOW_COS(num_cos3_ldb_ports, 3)
DLB2_VF_AVAIL_SYSFS_SHOW(num_dir_ports)
DLB2_VF_AVAIL_SYSFS_SHOW(num_ldb_credits)
DLB2_VF_AVAIL_SYSFS_SHOW(num_dir_credits)
DLB2_VF_AVAIL_SYSFS_SHOW(num_atomic_inflights)
DLB2_VF_AVAIL_SYSFS_SHOW(num_hist_list_entries)
DLB2_VF_AVAIL_SYSFS_SHOW_SN_SLOTS(num_sn0_slots, 0)
DLB2_VF_AVAIL_SYSFS_SHOW_SN_SLOTS(num_sn1_slots, 1)

static ssize_t max_ctg_hl_entries_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct dlb2_get_num_resources_args num_avail_rsrcs;
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	struct dlb2_hw *hw = &dlb2->hw;
	int val;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2->ops->get_num_resources(hw, &num_avail_rsrcs);

	mutex_unlock(&dlb2->resource_mutex);

	if (val)
		return -1;

	val = num_avail_rsrcs.max_contiguous_hist_list_entries;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

/*
 * Device attribute name doesn't match the show function name, so we define our
 * own DEVICE_ATTR macro.
 */
#define DLB2_DEVICE_ATTR_RO(_prefix, _name) \
struct device_attribute dev_attr_##_prefix##_##_name = {\
	.attr = { .name = __stringify(_name), .mode = 0444 },\
	.show = _prefix##_##_name##_show,\
}

static DLB2_DEVICE_ATTR_RO(total, num_sched_domains);
static DLB2_DEVICE_ATTR_RO(total, num_ldb_queues);
static DLB2_DEVICE_ATTR_RO(total, num_ldb_ports);
static DLB2_DEVICE_ATTR_RO(total, num_cos0_ldb_ports);
static DLB2_DEVICE_ATTR_RO(total, num_cos1_ldb_ports);
static DLB2_DEVICE_ATTR_RO(total, num_cos2_ldb_ports);
static DLB2_DEVICE_ATTR_RO(total, num_cos3_ldb_ports);
static DLB2_DEVICE_ATTR_RO(total, num_dir_ports);
static DLB2_DEVICE_ATTR_RO(total, num_ldb_credits);
static DLB2_DEVICE_ATTR_RO(total, num_dir_credits);
static DLB2_DEVICE_ATTR_RO(total, num_atomic_inflights);
static DLB2_DEVICE_ATTR_RO(total, num_hist_list_entries);
static DLB2_DEVICE_ATTR_RO(total, num_sn0_slots);
static DLB2_DEVICE_ATTR_RO(total, num_sn1_slots);

static struct attribute *dlb2_total_attrs[] = {
	&dev_attr_total_num_sched_domains.attr,
	&dev_attr_total_num_ldb_queues.attr,
	&dev_attr_total_num_ldb_ports.attr,
	&dev_attr_total_num_cos0_ldb_ports.attr,
	&dev_attr_total_num_cos1_ldb_ports.attr,
	&dev_attr_total_num_cos2_ldb_ports.attr,
	&dev_attr_total_num_cos3_ldb_ports.attr,
	&dev_attr_total_num_dir_ports.attr,
	&dev_attr_total_num_ldb_credits.attr,
	&dev_attr_total_num_dir_credits.attr,
	&dev_attr_total_num_atomic_inflights.attr,
	&dev_attr_total_num_hist_list_entries.attr,
	&dev_attr_total_num_sn0_slots.attr,
	&dev_attr_total_num_sn1_slots.attr,
	NULL
};

static const struct attribute_group dlb2_vf_total_attr_group = {
	.attrs = dlb2_total_attrs,
	.name = "total_resources",
};

static DLB2_DEVICE_ATTR_RO(avail, num_sched_domains);
static DLB2_DEVICE_ATTR_RO(avail, num_ldb_queues);
static DLB2_DEVICE_ATTR_RO(avail, num_ldb_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_cos0_ldb_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_cos1_ldb_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_cos2_ldb_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_cos3_ldb_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_dir_ports);
static DLB2_DEVICE_ATTR_RO(avail, num_ldb_credits);
static DLB2_DEVICE_ATTR_RO(avail, num_dir_credits);
static DLB2_DEVICE_ATTR_RO(avail, num_atomic_inflights);
static DLB2_DEVICE_ATTR_RO(avail, num_hist_list_entries);
static DLB2_DEVICE_ATTR_RO(avail, num_sn0_slots);
static DLB2_DEVICE_ATTR_RO(avail, num_sn1_slots);
static DEVICE_ATTR_RO(max_ctg_hl_entries);

static struct attribute *dlb2_avail_attrs[] = {
	&dev_attr_avail_num_sched_domains.attr,
	&dev_attr_avail_num_ldb_queues.attr,
	&dev_attr_avail_num_ldb_ports.attr,
	&dev_attr_avail_num_cos0_ldb_ports.attr,
	&dev_attr_avail_num_cos1_ldb_ports.attr,
	&dev_attr_avail_num_cos2_ldb_ports.attr,
	&dev_attr_avail_num_cos3_ldb_ports.attr,
	&dev_attr_avail_num_dir_ports.attr,
	&dev_attr_avail_num_ldb_credits.attr,
	&dev_attr_avail_num_dir_credits.attr,
	&dev_attr_avail_num_atomic_inflights.attr,
	&dev_attr_avail_num_hist_list_entries.attr,
	&dev_attr_avail_num_sn0_slots.attr,
	&dev_attr_avail_num_sn1_slots.attr,
	&dev_attr_max_ctg_hl_entries.attr,
	NULL
};

static const struct attribute_group dlb2_vf_avail_attr_group = {
	.attrs = dlb2_avail_attrs,
	.name = "avail_resources",
};

static ssize_t dev_id_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", dlb2->id);
}

static ssize_t driver_ver_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", DLB2_DRIVER_VERSION);
}

static DEVICE_ATTR_RO(dev_id);
static DEVICE_ATTR_RO(driver_ver);

static struct attribute *dlb2_dev_id_attr[] = {
	&dev_attr_dev_id.attr,
	&dev_attr_driver_ver.attr,
	NULL
};

static const struct attribute_group dlb2_dev_id_attr_group = {
	.attrs = dlb2_dev_id_attr,
};

static const struct attribute_group *dlb2_vf_attr_groups[] = {
	&dlb2_dev_id_attr_group,
	&dlb2_vf_total_attr_group,
	&dlb2_vf_avail_attr_group,
	NULL,
};

static int
dlb2_vf_sysfs_create(struct dlb2 *dlb2)
{
	struct device *dev = &dlb2->pdev->dev;

	return devm_device_add_groups(dev, dlb2_vf_attr_groups);
}

static void
dlb2_vf_sysfs_reapply_configuration(struct dlb2 *dlb2)
{
}

static void
dlb2_vf_enable_pm(struct dlb2 *dlb2)
{
	/* Function intentionally left blank */
}

static int
dlb2_vf_wait_for_device_ready(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	/* Device ready check only performed on the PF */
	return 0;
}

/*****************************/
/****** IOCTL callbacks ******/
/*****************************/

static int
dlb2_vf_create_sched_domain(struct dlb2_hw *hw,
			    struct dlb2_create_sched_domain_args *args,
			    struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_create_sched_domain_cmd_resp resp;
	struct dlb2_mbox_create_sched_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CREATE_SCHED_DOMAIN;
	req.num_ldb_queues = args->num_ldb_queues;
	req.num_ldb_ports = args->num_ldb_ports;
	req.num_cos_ldb_ports[0] = args->num_cos_ldb_ports[0];
	req.num_cos_ldb_ports[1] = args->num_cos_ldb_ports[1];
	req.num_cos_ldb_ports[2] = args->num_cos_ldb_ports[2];
	req.num_cos_ldb_ports[3] = args->num_cos_ldb_ports[3];
	req.num_dir_ports = args->num_dir_ports;
	req.num_atomic_inflights = args->num_atomic_inflights;
	req.num_hist_list_entries = args->num_hist_list_entries;
	req.num_ldb_credits = args->num_ldb_credits;
	req.num_dir_credits = args->num_dir_credits;
	req.num_sn_slots[0] = args->num_sn_slots[0];
	req.num_sn_slots[1] = args->num_sn_slots[1];
	req.cos_strict = args->cos_strict;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.id;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_create_ldb_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_ldb_queue_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_create_ldb_queue_cmd_resp resp;
	struct dlb2_mbox_create_ldb_queue_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CREATE_LDB_QUEUE;
	req.domain_id = id;
	req.num_sequence_numbers = args->num_sequence_numbers;
	req.num_qid_inflights = args->num_qid_inflights;
	req.num_atomic_inflights = args->num_atomic_inflights;
	req.lock_id_comp_level = args->lock_id_comp_level;
	req.depth_threshold = args->depth_threshold;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.id;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_create_dir_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_dir_queue_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_create_dir_queue_cmd_resp resp;
	struct dlb2_mbox_create_dir_queue_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CREATE_DIR_QUEUE;
	req.domain_id = id;
	req.port_id = args->port_id;
	req.depth_threshold = args->depth_threshold;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.id;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_create_ldb_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_ldb_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_create_ldb_port_cmd_resp resp;
	struct dlb2_mbox_create_ldb_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CREATE_LDB_PORT;
	req.domain_id = id;
	req.cq_depth = args->cq_depth;
	req.cq_history_list_size = args->cq_history_list_size;
	req.cos_id = (args->cos_id == DLB2_COS_DEFAULT) ? 0 : args->cos_id;
	req.cos_strict = args->cos_strict;
	req.cq_base_address = cq_dma_base;
	req.enable_inflight_ctrl = args->enable_inflight_ctrl;
	req.inflight_threshold = args->inflight_threshold;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.id;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_create_dir_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_dir_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_create_dir_port_cmd_resp resp;
	struct dlb2_mbox_create_dir_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CREATE_DIR_PORT;
	req.domain_id = id;
	req.cq_depth = args->cq_depth;
	req.cq_base_address = cq_dma_base;
	req.queue_id = args->queue_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.id;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_start_domain(struct dlb2_hw *hw,
		     u32 id,
		     struct dlb2_start_domain_args *args,
		     struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_start_domain_cmd_resp resp;
	struct dlb2_mbox_start_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_START_DOMAIN;
	req.domain_id = id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_stop_domain(struct dlb2_hw *hw,
		     u32 id,
		     struct dlb2_stop_domain_args *args,
		     struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_stop_domain_cmd_resp resp;
	struct dlb2_mbox_stop_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_STOP_DOMAIN;
	req.domain_id = id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_map_qid(struct dlb2_hw *hw,
		u32 id,
		struct dlb2_map_qid_args *args,
		struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_map_qid_cmd_resp resp;
	struct dlb2_mbox_map_qid_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_MAP_QID;
	req.domain_id = id;
	req.port_id = args->port_id;
	req.qid = args->qid;
	req.priority = args->priority;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_unmap_qid(struct dlb2_hw *hw,
		  u32 id,
		  struct dlb2_unmap_qid_args *args,
		  struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_unmap_qid_cmd_resp resp;
	struct dlb2_mbox_unmap_qid_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_UNMAP_QID;
	req.domain_id = id;
	req.port_id = args->port_id;
	req.qid = args->qid;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_enable_ldb_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_enable_ldb_port_args *args,
			struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_enable_ldb_port_cmd_resp resp;
	struct dlb2_mbox_enable_ldb_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_ENABLE_LDB_PORT;
	req.domain_id = id;
	req.port_id = args->port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_disable_ldb_port(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_disable_ldb_port_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_disable_ldb_port_cmd_resp resp;
	struct dlb2_mbox_disable_ldb_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_DISABLE_LDB_PORT;
	req.domain_id = id;
	req.port_id = args->port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_enable_dir_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_enable_dir_port_args *args,
			struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_enable_dir_port_cmd_resp resp;
	struct dlb2_mbox_enable_dir_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_ENABLE_DIR_PORT;
	req.domain_id = id;
	req.port_id = args->port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_disable_dir_port(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_disable_dir_port_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_disable_dir_port_cmd_resp resp;
	struct dlb2_mbox_disable_dir_port_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_DISABLE_DIR_PORT;
	req.domain_id = id;
	req.port_id = args->port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_get_num_resources(struct dlb2_hw *hw,
			  struct dlb2_get_num_resources_args *args)
{
	struct dlb2_mbox_get_num_resources_cmd_resp resp;
	struct dlb2_mbox_get_num_resources_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_NUM_RESOURCES;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	args->num_sched_domains = resp.num_sched_domains;
	args->num_ldb_queues = resp.num_ldb_queues;
	args->num_ldb_ports = resp.num_ldb_ports;
	args->num_cos_ldb_ports[0] = resp.num_cos_ldb_ports[0];
	args->num_cos_ldb_ports[1] = resp.num_cos_ldb_ports[1];
	args->num_cos_ldb_ports[2] = resp.num_cos_ldb_ports[2];
	args->num_cos_ldb_ports[3] = resp.num_cos_ldb_ports[3];
	args->num_dir_ports = resp.num_dir_ports;
	args->num_atomic_inflights = resp.num_atomic_inflights;
	args->num_hist_list_entries = resp.num_hist_list_entries;
	args->max_contiguous_hist_list_entries =
		resp.max_contiguous_hist_list_entries;
	args->num_ldb_credits = resp.num_ldb_credits;
	args->num_dir_credits = resp.num_dir_credits;
	args->num_sn_slots[0] = resp.num_sn_slots[0];
	args->num_sn_slots[1] = resp.num_sn_slots[1];

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_reset_domain(struct dlb2_hw *hw, u32 id)
{
	struct dlb2_mbox_reset_sched_domain_cmd_resp resp;
	struct dlb2_mbox_reset_sched_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_RESET_SCHED_DOMAIN;
	req.id = id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_get_ldb_queue_depth(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_get_ldb_queue_depth_args *args,
			    struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_get_ldb_queue_depth_cmd_resp resp;
	struct dlb2_mbox_get_ldb_queue_depth_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_LDB_QUEUE_DEPTH;
	req.domain_id = id;
	req.queue_id = args->queue_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.depth;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_get_dir_queue_depth(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_get_dir_queue_depth_args *args,
			    struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_get_dir_queue_depth_cmd_resp resp;
	struct dlb2_mbox_get_dir_queue_depth_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_DIR_QUEUE_DEPTH;
	req.domain_id = id;
	req.queue_id = args->queue_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.depth;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_pending_port_unmaps(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_pending_port_unmaps_args *args,
			    struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_pending_port_unmaps_cmd_resp resp;
	struct dlb2_mbox_pending_port_unmaps_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_PENDING_PORT_UNMAPS;
	req.domain_id = id;
	req.port_id = args->port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.num;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_query_cq_poll_mode(struct dlb2 *dlb2,
			   struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_query_cq_poll_mode_cmd_resp resp;
	struct dlb2_mbox_query_cq_poll_mode_cmd_req req;
	int ret;

	req.hdr.type = DLB2_MBOX_CMD_QUERY_CQ_POLL_MODE;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;
	user_resp->id = resp.mode;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_enable_cq_weight(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_enable_cq_weight_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_enable_cq_weight_cmd_resp resp;
	struct dlb2_mbox_enable_cq_weight_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_ENABLE_CQ_WEIGHT;
	req.domain_id = id;
	req.port_id = args->port_id;
	req.limit = args->limit;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

static int
dlb2_vf_cq_inflight_ctrl(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_cq_inflight_ctrl_args *args,
			 struct dlb2_cmd_response *user_resp)
{
	struct dlb2_mbox_cq_inflight_ctrl_cmd_resp resp;
	struct dlb2_mbox_cq_inflight_ctrl_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_CQ_INFLIGHT_CTRL;
	req.domain_id = id;
	req.port_id = args->port_id;
	req.enable = args->enable;
	req.threshold = args->threshold;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		user_resp->status = DLB2_ST_MBOX_ERROR;

		return -1;
	}

	user_resp->status = resp.status;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

/**************************************/
/****** Resource query callbacks ******/
/**************************************/

static int
dlb2_vf_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				 u32 domain_id,
				 u32 port_id)
{
	struct dlb2_mbox_ldb_port_owned_by_domain_cmd_resp resp;
	struct dlb2_mbox_ldb_port_owned_by_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_LDB_PORT_OWNED_BY_DOMAIN;
	req.domain_id = domain_id;
	req.port_id = port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.owned;
}

static int
dlb2_vf_dir_port_owned_by_domain(struct dlb2_hw *hw,
				 u32 domain_id,
				 u32 port_id)
{
	struct dlb2_mbox_dir_port_owned_by_domain_cmd_resp resp;
	struct dlb2_mbox_dir_port_owned_by_domain_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_DIR_PORT_OWNED_BY_DOMAIN;
	req.domain_id = domain_id;
	req.port_id = port_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.owned;
}

static int
dlb2_vf_get_sn_allocation(struct dlb2_hw *hw, u32 group_id)
{
	struct dlb2_mbox_get_sn_allocation_cmd_resp resp;
	struct dlb2_mbox_get_sn_allocation_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_SN_ALLOCATION;
	req.group_id = group_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.num;
}

static int dlb2_vf_set_sn_allocation(struct dlb2_hw *hw, u32 group_id, u32 val)
{
	/* Only the PF can modify the SN allocations */
	return -EPERM;
}

static int
dlb2_vf_set_cos_bw(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth)
{
	/* Only the PF can modify class-of-service reservations */
	return -EPERM;
}

static int
dlb2_vf_get_cos_bw(struct dlb2_hw *hw, u32 cos_id)
{
	struct dlb2_mbox_get_cos_bw_cmd_resp resp;
	struct dlb2_mbox_get_cos_bw_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_COS_BW;
	req.cos_id = cos_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.num;
}

static int
dlb2_vf_get_sn_occupancy(struct dlb2_hw *hw, u32 group_id)
{
	struct dlb2_mbox_get_sn_occupancy_cmd_resp resp;
	struct dlb2_mbox_get_sn_occupancy_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_SN_OCCUPANCY;
	req.group_id = group_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.num;
}

static int dlb2_vf_get_xstats(struct dlb2_hw *hw,
			      struct dlb2_xstats_args *args) {
	struct dlb2_mbox_get_xstats_cmd_resp resp;
	struct dlb2_mbox_get_xstats_cmd_req req;
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	req.hdr.type = DLB2_MBOX_CMD_GET_XSTATS;
	req.xstats_type = args->xstats_type;
	req.xstats_id = args->xstats_id;

	ret = dlb2_send_sync_mbox_cmd(dlb2, &req, sizeof(req), DLB2_MBOX_TOUT);
	if (ret)
		return ret;

	dlb2_vf_read_pf_mbox_resp(&dlb2->hw, &resp, sizeof(resp));

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		dev_err(dlb2->dev,
			"[%s()]: failed with mailbox error: %s\n",
			__func__,
			dlb2_mbox_st_string(&resp.hdr));


		return -1;
	}

	args->xstats_val = resp.xstats_val;

	return dlb2_mbox_error_to_errno(resp.error_code);
}

/*********************************/
/****** DLB2 VF Device Ops ******/
/*********************************/

struct dlb2_device_ops dlb2_vf_ops = {
	.map_pci_bar_space = dlb2_vf_map_pci_bar_space,
	.unmap_pci_bar_space = dlb2_vf_unmap_pci_bar_space,
	.init_driver_state = dlb2_vf_init_driver_state,
	.free_driver_state = dlb2_vf_free_driver_state,
	.sysfs_create = dlb2_vf_sysfs_create,
	.sysfs_reapply = dlb2_vf_sysfs_reapply_configuration,
	.init_interrupts = dlb2_vf_init_interrupts,
	.enable_ldb_cq_interrupts = dlb2_vf_enable_ldb_cq_interrupts,
	.enable_dir_cq_interrupts = dlb2_vf_enable_dir_cq_interrupts,
	.arm_cq_interrupt = dlb2_vf_arm_cq_interrupt,
	.reinit_interrupts = dlb2_vf_reinit_interrupts,
	.free_interrupts = dlb2_vf_free_interrupts,
	.enable_pm = dlb2_vf_enable_pm,
	.wait_for_device_ready = dlb2_vf_wait_for_device_ready,
	.register_driver = dlb2_vf_register_driver,
	.unregister_driver = dlb2_vf_unregister_driver,
	.create_sched_domain = dlb2_vf_create_sched_domain,
	.create_ldb_queue = dlb2_vf_create_ldb_queue,
	.create_dir_queue = dlb2_vf_create_dir_queue,
	.create_ldb_port = dlb2_vf_create_ldb_port,
	.create_dir_port = dlb2_vf_create_dir_port,
	.start_domain = dlb2_vf_start_domain,
	.map_qid = dlb2_vf_map_qid,
	.unmap_qid = dlb2_vf_unmap_qid,
	.enable_ldb_port = dlb2_vf_enable_ldb_port,
	.enable_dir_port = dlb2_vf_enable_dir_port,
	.disable_ldb_port = dlb2_vf_disable_ldb_port,
	.disable_dir_port = dlb2_vf_disable_dir_port,
	.get_num_resources = dlb2_vf_get_num_resources,
	.reset_domain = dlb2_vf_reset_domain,
	.ldb_port_owned_by_domain = dlb2_vf_ldb_port_owned_by_domain,
	.dir_port_owned_by_domain = dlb2_vf_dir_port_owned_by_domain,
	.get_sn_allocation = dlb2_vf_get_sn_allocation,
	.set_sn_allocation = dlb2_vf_set_sn_allocation,
	.get_sn_occupancy = dlb2_vf_get_sn_occupancy,
	.get_ldb_queue_depth = dlb2_vf_get_ldb_queue_depth,
	.get_dir_queue_depth = dlb2_vf_get_dir_queue_depth,
	.pending_port_unmaps = dlb2_vf_pending_port_unmaps,
	.set_cos_bw = dlb2_vf_set_cos_bw,
	.get_cos_bw = dlb2_vf_get_cos_bw,
	.init_hardware = dlb2_vf_init_hardware,
	.query_cq_poll_mode = dlb2_vf_query_cq_poll_mode,
	.mbox_dev_reset = dlb2_vf_mbox_dev_reset,
	.enable_cq_weight = dlb2_vf_enable_cq_weight,
	.cq_inflight_ctrl = dlb2_vf_cq_inflight_ctrl,
	.get_xstats = dlb2_vf_get_xstats,
	.stop_domain = dlb2_vf_stop_domain,
};
