// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/eventfd.h>
#include <linux/vfio.h>

#include "base/dlb2_mbox.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_resource.h"
#include "dlb2_dp_ops.h"
#include "dlb2_intr.h"
#include "dlb2_ioctl.h"
#include "dlb2_sriov.h"
#include "dlb2_vdcm.h"

/********************************/
/****** PCI BAR management ******/
/********************************/

static void
dlb2_pf_unmap_pci_bar_space(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	pci_iounmap(pdev, dlb2->hw.csr_kva);
	pci_iounmap(pdev, dlb2->hw.func_kva);
}

static int
dlb2_pf_map_pci_bar_space(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	dlb2->hw.func_kva = pci_iomap(pdev, DLB2_FUNC_BAR, 0);
	dlb2->hw.func_phys_addr = pci_resource_start(pdev, DLB2_FUNC_BAR);

	if (!dlb2->hw.func_kva) {
		dev_err(&pdev->dev, "Cannot iomap BAR 0 (size %llu)\n",
			pci_resource_len(pdev, 0));

		return -EIO;
	}

	dlb2->hw.csr_kva = pci_iomap(pdev, DLB2_CSR_BAR, 0);
	dlb2->hw.csr_phys_addr = pci_resource_start(pdev, DLB2_CSR_BAR);

	if (!dlb2->hw.csr_kva) {
		dev_err(&pdev->dev, "Cannot iomap BAR 2 (size %llu)\n",
			pci_resource_len(pdev, 2));

		pci_iounmap(pdev, dlb2->hw.func_kva);
		return -EIO;
	}

	return 0;
}

/*******************************/
/****** Mailbox callbacks ******/
/*******************************/

static enum dlb2_mbox_error_code
dlb2_errno_to_mbox_error(int ret)
{
	switch (ret) {
	case 0: return DLB2_MBOX_SUCCESS;
	case -EFAULT: return DLB2_MBOX_EFAULT;
	case -EPERM: return DLB2_MBOX_EPERM;
	case -ETIMEDOUT: return DLB2_MBOX_ETIMEDOUT;
	case -EINVAL: /* fallthrough */
	default: return DLB2_MBOX_EINVAL;
	}
}

/*
 * Return -1 if no interfaces in the range are supported, else return the
 * newest version.
 */
static int
dlb2_mbox_version_supported(u16 min)
{
	/* Only version 1 exists at this time */
	if (min > DLB2_MBOX_INTERFACE_VERSION)
		return -1;

	return DLB2_MBOX_INTERFACE_VERSION;
}

static void
dlb2_mbox_cmd_register_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_register_cmd_req *req = data;
	struct dlb2_mbox_register_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	/*
	 * Given an interface version range ('min' to 'max', inclusive)
	 * requested by the VF driver:
	 * - If PF supports any versions in that range, it returns the newest
	 *   supported version.
	 * - Else PF responds with MBOX_ST_VERSION_MISMATCH
	 */
	ret = dlb2_mbox_version_supported(req->min_interface_version);
	if (ret == -1) {
		resp.hdr.status = DLB2_MBOX_ST_VERSION_MISMATCH;
		resp.interface_version = DLB2_MBOX_INTERFACE_VERSION;

		goto done;
	}

	resp.interface_version = ret;

	/* Scalable IOV vdev locking is handled in the VDCM */
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		dlb2_lock_vdev(&dlb2->hw, vf_id);

	/*
	 * The VF can re-register without sending an unregister mailbox
	 * command (for example if the guest OS crashes). To protect against
	 * this, reset any in-use resources assigned to the driver now.
	 */
	if (dlb2_reset_vdev(&dlb2->hw, vf_id))
		dev_err(dlb2->dev, "[%s()] Internal error\n", __func__);

	dlb2->vf_registered[vf_id] = true;

	if (!send_resp)
		return;

	resp.pf_id = dlb2->id;
	resp.vf_id = vf_id;
	resp.flags = 0;
	if (dlb2->child_id_state[vf_id].is_auxiliary_vf)
		resp.flags |= DLB2_MBOX_FLAG_IS_AUX_VF;
	resp.primary_vf_id = dlb2->child_id_state[vf_id].primary_vf_id;
	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

done:
	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_unregister_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_unregister_cmd_resp resp;

	memset(&resp, 0, sizeof(resp));

	dlb2->vf_registered[vf_id] = false;

	if (dlb2_reset_vdev(&dlb2->hw, vf_id))
		dev_err(dlb2->dev, "[%s()] Internal error\n", __func__);

	/* Scalable IOV vdev locking is handled in the VDCM */
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		dlb2_unlock_vdev(&dlb2->hw, vf_id);

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_num_resources_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_get_num_resources_cmd_resp resp;
	struct dlb2_get_num_resources_args arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_hw_get_num_resources(&dlb2->hw, &arg, true, vf_id);

	if (!send_resp)
		return;

	resp.num_sched_domains = arg.num_sched_domains;
	resp.num_ldb_queues = arg.num_ldb_queues;
	resp.num_ldb_ports = arg.num_ldb_ports;
	resp.num_cos_ldb_ports[0] = arg.num_cos_ldb_ports[0];
	resp.num_cos_ldb_ports[1] = arg.num_cos_ldb_ports[1];
	resp.num_cos_ldb_ports[2] = arg.num_cos_ldb_ports[2];
	resp.num_cos_ldb_ports[3] = arg.num_cos_ldb_ports[3];
	resp.num_dir_ports = arg.num_dir_ports;
	resp.num_atomic_inflights = arg.num_atomic_inflights;
	resp.num_hist_list_entries = arg.num_hist_list_entries;
	resp.max_contiguous_hist_list_entries =
		arg.max_contiguous_hist_list_entries;
	resp.num_ldb_credits = arg.num_ldb_credits;
	resp.num_dir_credits = arg.num_dir_credits;
	resp.num_sn_slots[0] = arg.num_sn_slots[0];
	resp.num_sn_slots[1] = arg.num_sn_slots[1];

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_create_sched_domain_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_create_sched_domain_cmd_req *req = data;
	struct dlb2_mbox_create_sched_domain_cmd_resp resp;
	struct dlb2_create_sched_domain_args hw_arg;
	struct dlb2_cmd_response hw_response = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.num_ldb_queues = req->num_ldb_queues;
	hw_arg.num_ldb_ports = req->num_ldb_ports;
	hw_arg.num_cos_ldb_ports[0] = req->num_cos_ldb_ports[0];
	hw_arg.num_cos_ldb_ports[1] = req->num_cos_ldb_ports[1];
	hw_arg.num_cos_ldb_ports[2] = req->num_cos_ldb_ports[2];
	hw_arg.num_cos_ldb_ports[3] = req->num_cos_ldb_ports[3];
	hw_arg.num_dir_ports = req->num_dir_ports;
	hw_arg.num_hist_list_entries = req->num_hist_list_entries;
	hw_arg.num_atomic_inflights = req->num_atomic_inflights;
	hw_arg.num_ldb_credits = req->num_ldb_credits;
	hw_arg.num_dir_credits = req->num_dir_credits;
	hw_arg.num_sn_slots[0] = req->num_sn_slots[0];
	hw_arg.num_sn_slots[1] = req->num_sn_slots[1];
	hw_arg.cos_strict = req->cos_strict;

	ret = dlb2_hw_create_sched_domain(&dlb2->hw,
					  &hw_arg,
					  &hw_response,
					  true,
					  vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.id = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_reset_sched_domain_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_reset_sched_domain_cmd_req *req = data;
	struct dlb2_mbox_reset_sched_domain_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_reset_domain(&dlb2->hw, req->id, true, vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_create_ldb_queue_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_create_ldb_queue_cmd_req *req = data;
	struct dlb2_mbox_create_ldb_queue_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_create_ldb_queue_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.num_sequence_numbers = req->num_sequence_numbers;
	hw_arg.num_qid_inflights = req->num_qid_inflights;
	hw_arg.num_atomic_inflights = req->num_atomic_inflights;
	hw_arg.lock_id_comp_level = req->lock_id_comp_level;
	hw_arg.depth_threshold = req->depth_threshold;

	ret = dlb2_hw_create_ldb_queue(&dlb2->hw,
				       req->domain_id,
				       &hw_arg,
				       &hw_response,
				       true,
				       vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.id = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_create_dir_queue_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_create_dir_queue_cmd_resp resp;
	struct dlb2_mbox_create_dir_queue_cmd_req *req;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_create_dir_queue_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	req = (struct dlb2_mbox_create_dir_queue_cmd_req *)data;

	hw_arg.port_id = req->port_id;
	hw_arg.depth_threshold = req->depth_threshold;

	ret = dlb2_hw_create_dir_queue(&dlb2->hw,
				       req->domain_id,
				       &hw_arg,
				       &hw_response,
				       true,
				       vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.id = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_create_ldb_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_create_ldb_port_cmd_req *req = data;
	struct dlb2_mbox_create_ldb_port_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_create_ldb_port_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.cq_depth = req->cq_depth;
	hw_arg.cq_history_list_size = req->cq_history_list_size;
	hw_arg.cos_id = req->cos_id;
	hw_arg.cos_strict = req->cos_strict;
	hw_arg.enable_inflight_ctrl = req->enable_inflight_ctrl;
	hw_arg.inflight_threshold = req->inflight_threshold;

	ret = dlb2_hw_create_ldb_port(&dlb2->hw,
				      req->domain_id,
				      &hw_arg,
				      req->cq_base_address,
				      &hw_response,
				      true,
				      vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.id = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_create_dir_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_create_ldb_port_cmd_resp resp;
	struct dlb2_mbox_create_dir_port_cmd_req *req;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_create_dir_port_args hw_arg = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	req = (struct dlb2_mbox_create_dir_port_cmd_req *)data;

	hw_arg.cq_depth = req->cq_depth;
	hw_arg.queue_id = req->queue_id;

	ret = dlb2_hw_create_dir_port(&dlb2->hw,
				      req->domain_id,
				      &hw_arg,
				      req->cq_base_address,
				      &hw_response,
				      true,
				      vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.id = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_enable_ldb_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_enable_ldb_port_cmd_req *req = data;
	struct dlb2_mbox_enable_ldb_port_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_enable_ldb_port_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;

	ret = dlb2_hw_enable_ldb_port(&dlb2->hw,
				      req->domain_id,
				      &hw_arg,
				      &hw_response,
				      true,
				      vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_disable_ldb_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_disable_ldb_port_cmd_req *req = data;
	struct dlb2_mbox_disable_ldb_port_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_disable_ldb_port_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;

	ret = dlb2_hw_disable_ldb_port(&dlb2->hw,
				       req->domain_id,
				       &hw_arg,
				       &hw_response,
				       true,
				       vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_enable_dir_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_enable_dir_port_cmd_req *req = data;
	struct dlb2_mbox_enable_dir_port_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_enable_dir_port_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;

	ret = dlb2_hw_enable_dir_port(&dlb2->hw,
				      req->domain_id,
				      &hw_arg,
				      &hw_response,
				      true,
				      vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_disable_dir_port_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_disable_dir_port_cmd_req *req = data;
	struct dlb2_mbox_disable_dir_port_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_disable_dir_port_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;

	ret = dlb2_hw_disable_dir_port(&dlb2->hw,
				       req->domain_id,
				       &hw_arg,
				       &hw_response,
				       true,
				       vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_ldb_port_owned_by_domain_fn(struct dlb2 *dlb2,
					  int vf_id,
					  void *data, bool send_resp)
{
	struct dlb2_mbox_ldb_port_owned_by_domain_cmd_req *req = data;
	struct dlb2_mbox_ldb_port_owned_by_domain_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_ldb_port_owned_by_domain(&dlb2->hw,
					    req->domain_id,
					    req->port_id,
					    true,
					    vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.owned = ret;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_dir_port_owned_by_domain_fn(struct dlb2 *dlb2,
					  int vf_id,
					  void *data, bool send_resp)
{
	struct dlb2_mbox_dir_port_owned_by_domain_cmd_req *req = data;
	struct dlb2_mbox_dir_port_owned_by_domain_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_dir_port_owned_by_domain(&dlb2->hw,
					    req->domain_id,
					    req->port_id,
					    true,
					    vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.owned = ret;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_map_qid_fn(struct dlb2 *dlb2,
			 int vf_id,
			 void *data, bool send_resp)
{
	struct dlb2_mbox_map_qid_cmd_req *req = data;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_mbox_map_qid_cmd_resp resp;
	struct dlb2_map_qid_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;
	hw_arg.qid = req->qid;
	hw_arg.priority = req->priority;

	ret = dlb2_hw_map_qid(&dlb2->hw,
			      req->domain_id,
			      &hw_arg,
			      &hw_response,
			      true,
			      vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_unmap_qid_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_unmap_qid_cmd_req *req = data;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_mbox_unmap_qid_cmd_resp resp;
	struct dlb2_unmap_qid_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;
	hw_arg.qid = req->qid;

	ret = dlb2_hw_unmap_qid(&dlb2->hw,
				req->domain_id,
				&hw_arg,
				&hw_response,
				true,
				vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_start_domain_fn(struct dlb2 *dlb2,
			      int vf_id,
			      void *data, bool send_resp)
{
	struct dlb2_mbox_start_domain_cmd_req *req = data;
	struct dlb2_mbox_start_domain_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_start_domain_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_hw_start_domain(&dlb2->hw,
				   req->domain_id,
				   &hw_arg,
				   &hw_response,
				   true,
				   vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_stop_domain_fn(struct dlb2 *dlb2,
			      int vf_id,
			      void *data, bool send_resp)
{
	struct dlb2_mbox_stop_domain_cmd_req *req = data;
	struct dlb2_mbox_stop_domain_cmd_resp resp;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_stop_domain_args hw_arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_hw_stop_domain(&dlb2->hw,
				  req->domain_id,
				  &hw_arg,
				  &hw_response,
				  true,
				  vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_enable_ldb_port_intr_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_enable_ldb_port_intr_cmd_req *req = data;
	struct dlb2_mbox_enable_ldb_port_intr_cmd_resp resp;
	int ret, mode, intr_vector;

	memset(&resp, 0, sizeof(resp));

	if (req->owner_vf >= DLB2_MAX_NUM_VDEVS ||
	    (dlb2->child_id_state[req->owner_vf].is_auxiliary_vf &&
	     dlb2->child_id_state[req->owner_vf].primary_vf_id != vf_id) ||
	    (!dlb2->child_id_state[req->owner_vf].is_auxiliary_vf &&
	     req->owner_vf != vf_id)) {
		resp.hdr.status = DLB2_MBOX_ST_INVALID_OWNER_VF;
		goto finish;
	}

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		mode = DLB2_CQ_ISR_MODE_MSI;
	else
#ifndef DLB2_SIOV_IMS_WORKAROUND
		mode = DLB2_CQ_ISR_MODE_ADI;
#else
		mode = DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV;
#endif

	/* In DLB 2.0, the IMS entry and arrangement are not compatible with new
	 * CONFIG_IMS_MSI_ARRAY framework proposed for kernel 5.9 and later.
	 * As a workaround we use MSI-X for both pf interrupts and the SIOV
	 * vf interrupts. We have one CQ interrupt vector per VF (instead of one
	 * interrupt vector per CQ in IMS).
	 *
	 * INT = 0 ---> alert, watchdog, pf-vf mbox
	 * INT = 1 ---> PF CQ interrupts
	 * INT = 2 + vf_id ---> VF CQ interrupts (shown 1 + vf_id in the
	 *	 following call because a fixed value of 1 is added in HW
	 *	 to produce MSI-X vector for CQ interrupts.
	 */
#ifndef DLB2_SIOV_IMS_WORKAROUND
	intr_vector = req->vector;
#else
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		intr_vector = req->vector;
	else
		intr_vector = 1 + vf_id;
#endif
	ret = dlb2_configure_ldb_cq_interrupt(&dlb2->hw,
					      req->port_id,
					      intr_vector,
					      mode,
					      vf_id,
					      req->owner_vf,
					      req->thresh);

	if (ret == 0 && !dlb2_wdto_disable)
		ret = dlb2_hw_enable_ldb_cq_wd_int(&dlb2->hw,
						   req->port_id,
						   true,
						   vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

finish:
	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_enable_dir_port_intr_fn(struct dlb2 *dlb2,
				      int vf_id,
				      void *data, bool send_resp)
{
	struct dlb2_mbox_enable_dir_port_intr_cmd_req *req = data;
	struct dlb2_mbox_enable_dir_port_intr_cmd_resp resp;
	int ret, mode, intr_vector;

	memset(&resp, 0, sizeof(resp));

	if (req->owner_vf >= DLB2_MAX_NUM_VDEVS ||
	    (dlb2->child_id_state[req->owner_vf].is_auxiliary_vf &&
	     dlb2->child_id_state[req->owner_vf].primary_vf_id != vf_id) ||
	    (!dlb2->child_id_state[req->owner_vf].is_auxiliary_vf &&
	     req->owner_vf != vf_id)) {
		resp.hdr.status = DLB2_MBOX_ST_INVALID_OWNER_VF;
		goto finish;
	}

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		mode = DLB2_CQ_ISR_MODE_MSI;
	else
#ifndef DLB2_SIOV_IMS_WORKAROUND
		mode = DLB2_CQ_ISR_MODE_ADI;
#else
		mode = DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV;
#endif

	/* In DLB 2.0, the IMS entry and arrangement are not compatible with new
	 * CONFIG_IMS_MSI_ARRAY framework proposed for kernel 5.9 and later.
	 * As a workaround we use MSI-X for both pf interrupts and the SIOV
	 * vf interrupts. We have one CQ interrupt vector per VF (instead of one
	 * interrupt vector per CQ in IMS).
	 *
	 * INT = 0 ---> alert, watchdog, pf-vf mbox
	 * INT = 1 ---> PF CQ interrupts
	 * INT = 2 + vf_id ---> VF CQ interrupts (shown 1 + vf_id in the
	 *	 following call because a fixed value of 1 is added in HW
	 *	 to produce MSI-X vector for CQ interrupts.
	 */
#ifndef DLB2_SIOV_IMS_WORKAROUND
	intr_vector = req->vector;
#else
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV)
		intr_vector = req->vector;
	else
		intr_vector = 1 + vf_id;
#endif
	ret = dlb2_configure_dir_cq_interrupt(&dlb2->hw,
					      req->port_id,
					      intr_vector,
					      mode,
					      vf_id,
					      req->owner_vf,
					      req->thresh);

	if (ret == 0 && !dlb2_wdto_disable)
		ret = dlb2_hw_enable_dir_cq_wd_int(&dlb2->hw,
						   req->port_id,
						   true,
						   vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

finish:
	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_arm_cq_intr_fn(struct dlb2 *dlb2,
			     int vf_id,
			     void *data, bool send_resp)
{
	struct dlb2_mbox_arm_cq_intr_cmd_req *req = data;
	struct dlb2_mbox_arm_cq_intr_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	if (req->is_ldb)
		ret = dlb2_ldb_port_owned_by_domain(&dlb2->hw,
						    req->domain_id,
						    req->port_id,
						    true,
						    vf_id);
	else
		ret = dlb2_dir_port_owned_by_domain(&dlb2->hw,
						    req->domain_id,
						    req->port_id,
						    true,
						    vf_id);

	if (ret != 1) {
		resp.error_code = -EINVAL;
		goto finish;
	}

	resp.error_code = dlb2_arm_cq_interrupt(&dlb2->hw,
						req->port_id,
						req->is_ldb,
						true,
						vf_id);

finish:
	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	if (!send_resp)
		return;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_num_used_resources_fn(struct dlb2 *dlb2,
					int vf_id,
					void *data, bool send_resp)
{
	struct dlb2_mbox_get_num_resources_cmd_resp resp;
	struct dlb2_get_num_resources_args arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_hw_get_num_used_resources(&dlb2->hw, &arg, true, vf_id);

	if (!send_resp)
		return;

	resp.num_sched_domains = arg.num_sched_domains;
	resp.num_ldb_queues = arg.num_ldb_queues;
	resp.num_ldb_ports = arg.num_ldb_ports;
	resp.num_cos_ldb_ports[0] = arg.num_cos_ldb_ports[0];
	resp.num_cos_ldb_ports[1] = arg.num_cos_ldb_ports[1];
	resp.num_cos_ldb_ports[2] = arg.num_cos_ldb_ports[2];
	resp.num_cos_ldb_ports[3] = arg.num_cos_ldb_ports[3];
	resp.num_dir_ports = arg.num_dir_ports;
	resp.num_atomic_inflights = arg.num_atomic_inflights;
	resp.num_hist_list_entries = arg.num_hist_list_entries;
	resp.max_contiguous_hist_list_entries =
		arg.max_contiguous_hist_list_entries;
	resp.num_ldb_credits = arg.num_ldb_credits;
	resp.num_dir_credits = arg.num_dir_credits;
	resp.num_sn_slots[0] = arg.num_sn_slots[0];
	resp.num_sn_slots[1] = arg.num_sn_slots[1];

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_sn_allocation_fn(struct dlb2 *dlb2,
				   int vf_id,
				   void *data, bool send_resp)
{
	struct dlb2_mbox_get_sn_allocation_cmd_req *req = data;
	struct dlb2_mbox_get_sn_allocation_cmd_resp resp;

	memset(&resp, 0, sizeof(resp));

	resp.num = dlb2_get_group_sequence_numbers(&dlb2->hw, req->group_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void dlb2_mbox_cmd_get_xstats_fn(struct dlb2 *dlb2, int vf_id,
					void *data, bool send_resp)
{
	struct dlb2_mbox_get_xstats_cmd_req *req = data;
	struct dlb2_mbox_get_xstats_cmd_resp resp;
	struct dlb2_xstats_args arg = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	arg.xstats_type = req->xstats_type;
	arg.xstats_id = req->xstats_id;
	ret = dlb2_get_xstats(&dlb2->hw, &arg, true, vf_id);

	if (ret)
		return;

	resp.xstats_val = arg.xstats_val;
	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_sn_occupancy_fn(struct dlb2 *dlb2,
				  int vf_id,
				  void *data, bool send_resp)
{
	struct dlb2_mbox_get_sn_occupancy_cmd_req *req = data;
	struct dlb2_mbox_get_sn_occupancy_cmd_resp resp;
	struct dlb2_get_num_resources_args arg;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_hw_get_num_used_resources(&dlb2->hw, &arg, true, vf_id);

	if (!send_resp)
		return;

	if (req->group_id < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS) {
		resp.num = arg.num_sn_slots[req->group_id];
		resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	} else {
		resp.hdr.status = DLB2_MBOX_ST_INVALID_DATA;
	}

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_ldb_queue_depth_fn(struct dlb2 *dlb2,
				     int vf_id,
				     void *data, bool send_resp)
{
	struct dlb2_mbox_get_ldb_queue_depth_cmd_req *req = data;
	struct dlb2_mbox_get_ldb_queue_depth_cmd_resp resp;
	struct dlb2_get_ldb_queue_depth_args hw_arg;
	struct dlb2_cmd_response hw_response = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.queue_id = req->queue_id;

	ret = dlb2_hw_get_ldb_queue_depth(&dlb2->hw,
					  req->domain_id,
					  &hw_arg,
					  &hw_response,
					  true,
					  vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.depth = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_dir_queue_depth_fn(struct dlb2 *dlb2,
				     int vf_id,
				     void *data, bool send_resp)
{
	struct dlb2_mbox_get_dir_queue_depth_cmd_req *req = data;
	struct dlb2_mbox_get_dir_queue_depth_cmd_resp resp;
	struct dlb2_get_dir_queue_depth_args hw_arg;
	struct dlb2_cmd_response hw_response = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.queue_id = req->queue_id;

	ret = dlb2_hw_get_dir_queue_depth(&dlb2->hw,
					  req->domain_id,
					  &hw_arg,
					  &hw_response,
					  true,
					  vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.depth = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_pending_port_unmaps_fn(struct dlb2 *dlb2,
				     int vf_id,
				     void *data, bool send_resp)
{
	struct dlb2_mbox_pending_port_unmaps_cmd_req *req = data;
	struct dlb2_mbox_pending_port_unmaps_cmd_resp resp;
	struct dlb2_pending_port_unmaps_args hw_arg;
	struct dlb2_cmd_response hw_response = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	hw_arg.port_id = req->port_id;

	ret = dlb2_hw_pending_port_unmaps(&dlb2->hw,
					  req->domain_id,
					  &hw_arg,
					  &hw_response,
					  true,
					  vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;
	resp.num = hw_response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_get_cos_bw_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_get_cos_bw_cmd_req *req = data;
	struct dlb2_mbox_get_cos_bw_cmd_resp resp;

	memset(&resp, 0, sizeof(resp));

	resp.num = dlb2_hw_get_cos_bandwidth(&dlb2->hw, req->cos_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static bool
dlb2_sparse_cq_enabled = true;

static int
dlb2_pf_query_cq_poll_mode(struct dlb2 *dlb2,
			   struct dlb2_cmd_response *user_resp)
{
	if (dlb2_sparse_cq_enabled) {
		user_resp->status = 0;
		user_resp->id = DLB2_CQ_POLL_MODE_SPARSE;
	}

	return 0;
}

static void
dlb2_mbox_cmd_query_cq_poll_mode_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_query_cq_poll_mode_cmd_resp resp;
	struct dlb2_cmd_response response = {0};
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_pf_query_cq_poll_mode(dlb2, &response);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = response.status;
	resp.mode = response.id;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_dev_reset_fn(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp)
{
	struct dlb2_mbox_dev_reset_cmd_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = dlb2_reset_vdev(&dlb2->hw, vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void
dlb2_mbox_cmd_enable_cq_weight_fn(struct dlb2 *dlb2,
				  int vf_id,
				  void *data, bool send_resp)
{
	struct dlb2_mbox_enable_cq_weight_cmd_resp resp = { {0} };
	struct dlb2_mbox_enable_cq_weight_cmd_req *req = data;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_enable_cq_weight_args hw_arg;
	int ret;

	hw_arg.port_id = req->port_id;
	hw_arg.limit = req->limit;

	ret = dlb2_enable_cq_weight(&dlb2->hw,
				    req->domain_id,
				    &hw_arg,
				    &hw_response,
				    true,
				    vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void dlb2_mbox_cmd_cq_inflight_ctrl(struct dlb2 *dlb2, int vf_id,
					   void *data, bool send_resp) {
	struct dlb2_mbox_cq_inflight_ctrl_cmd_resp resp = { {0} };
	struct dlb2_mbox_cq_inflight_ctrl_cmd_req *req = data;
	struct dlb2_cmd_response hw_response = {0};
	struct dlb2_cq_inflight_ctrl_args hw_arg;
	int ret;

	hw_arg.port_id = req->port_id;
	hw_arg.enable = req->enable;
	hw_arg.threshold = req->threshold;

	ret = dlb2_cq_inflight_ctrl(&dlb2->hw,
				    req->domain_id,
				    &hw_arg,
				    &hw_response,
				    true,
				    vf_id);

	if (!send_resp)
		return;

	resp.hdr.status = DLB2_MBOX_ST_SUCCESS;
	resp.error_code = dlb2_errno_to_mbox_error(ret);
	resp.status = hw_response.status;

	dlb2_pf_write_vf_mbox_resp(&dlb2->hw, vf_id, &resp, sizeof(resp));
}

static void (*mbox_fn_table[])(struct dlb2 *dlb2, int vf_id, void *data, bool send_resp) = {
	dlb2_mbox_cmd_register_fn,
	dlb2_mbox_cmd_unregister_fn,
	dlb2_mbox_cmd_get_num_resources_fn,
	dlb2_mbox_cmd_create_sched_domain_fn,
	dlb2_mbox_cmd_reset_sched_domain_fn,
	dlb2_mbox_cmd_create_ldb_queue_fn,
	dlb2_mbox_cmd_create_dir_queue_fn,
	dlb2_mbox_cmd_create_ldb_port_fn,
	dlb2_mbox_cmd_create_dir_port_fn,
	dlb2_mbox_cmd_enable_ldb_port_fn,
	dlb2_mbox_cmd_disable_ldb_port_fn,
	dlb2_mbox_cmd_enable_dir_port_fn,
	dlb2_mbox_cmd_disable_dir_port_fn,
	dlb2_mbox_cmd_ldb_port_owned_by_domain_fn,
	dlb2_mbox_cmd_dir_port_owned_by_domain_fn,
	dlb2_mbox_cmd_map_qid_fn,
	dlb2_mbox_cmd_unmap_qid_fn,
	dlb2_mbox_cmd_start_domain_fn,
	dlb2_mbox_cmd_enable_ldb_port_intr_fn,
	dlb2_mbox_cmd_enable_dir_port_intr_fn,
	dlb2_mbox_cmd_arm_cq_intr_fn,
	dlb2_mbox_cmd_get_num_used_resources_fn,
	dlb2_mbox_cmd_get_sn_allocation_fn,
	dlb2_mbox_cmd_get_ldb_queue_depth_fn,
	dlb2_mbox_cmd_get_dir_queue_depth_fn,
	dlb2_mbox_cmd_pending_port_unmaps_fn,
	dlb2_mbox_cmd_get_cos_bw_fn,
	dlb2_mbox_cmd_get_sn_occupancy_fn,
	dlb2_mbox_cmd_query_cq_poll_mode_fn,
	dlb2_mbox_cmd_dev_reset_fn,
	dlb2_mbox_cmd_enable_cq_weight_fn,
	dlb2_mbox_cmd_cq_inflight_ctrl,
	dlb2_mbox_cmd_get_xstats_fn,
	dlb2_mbox_cmd_stop_domain_fn,
};

static u32
dlb2_handle_vf_flr_interrupt(struct dlb2 *dlb2)
{
	u32 bitvec;
	int i;

	bitvec = dlb2_read_vf_flr_int_bitvec(&dlb2->hw);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (!(bitvec & (1 << i)))
			continue;

		dev_dbg(dlb2->dev,
			"Received VF FLR ISR from VF %d\n",
			i);

		if (dlb2_reset_vdev(&dlb2->hw, i))
			dev_err(dlb2->dev,
				"[%s()] Internal error\n", __func__);
	}

	dlb2_ack_vf_flr_int(&dlb2->hw, bitvec);

	return bitvec;
}

static int
dlb2_pf_mbox_dev_reset(struct dlb2 *dlb2)
{
	/* Function intentionally left blank */
	return 0;
}

#if defined(DLB2_VDCM_MIGRATION_V1) || defined(DLB2_VDCM_MIGRATION_V2)
void dlb2_handle_migration_cmds(struct dlb2 *dlb2, int vdev_id, u8 *data)
{
	mbox_fn_table[DLB2_MBOX_CMD_TYPE(data)](dlb2, vdev_id, data, false);
}
#endif

/**********************************/
/****** Interrupt management ******/
/**********************************/

void dlb2_handle_mbox_interrupt(struct dlb2 *dlb2, int id)
{
	u8 data[DLB2_VF2PF_REQ_BYTES];

	dev_dbg(dlb2->dev, "Received VF->PF ISR from VF %d\n", id);

	dlb2_pf_read_vf_mbox_req(&dlb2->hw, id, data, sizeof(data));

	/* Unrecognized request command, send an error response */
	if (DLB2_MBOX_CMD_TYPE(data) >= NUM_DLB2_MBOX_CMD_TYPES) {
		struct dlb2_mbox_resp_hdr resp = {0};

		resp.status = DLB2_MBOX_ST_INVALID_CMD_TYPE;

		dlb2_pf_write_vf_mbox_resp(&dlb2->hw,
					   id,
					   &resp,
					   sizeof(resp));
	} else {
		struct dlb2_mbox_req_hdr *hdr = (void *)data;

		dev_dbg(dlb2->dev,
			"Received mbox command %s\n",
			dlb2_mbox_cmd_string(hdr));

		mbox_fn_table[DLB2_MBOX_CMD_TYPE(data)](dlb2, id, data, true);

#if defined(CONFIG_INTEL_DLB2_SIOV) && (defined(DLB2_VDCM_MIGRATION_V1) || \
					defined(DLB2_VDCM_MIGRATION_V2))
		dlb2_save_cmd_for_migration(dlb2, id, data, DLB2_VF2PF_REQ_BYTES);
#endif

	}

	dlb2_ack_vdev_mbox_int(&dlb2->hw, 1 << id);
}

static u32
dlb2_handle_vf_to_pf_interrupt(struct dlb2 *dlb2)
{
	u32 bitvec;
	int i;

	bitvec = dlb2_read_vdev_to_pf_int_bitvec(&dlb2->hw);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (!(bitvec & (1 << i)))
			continue;

		dlb2_handle_mbox_interrupt(dlb2, i);
	}

	return bitvec;
}

static u32
dlb2_handle_vf_requests(struct dlb2_hw *hw)
{
	struct dlb2 *dlb2;
	u32 mbox_bitvec;
	u32 flr_bitvec;

	dlb2 = container_of(hw, struct dlb2, hw);

	flr_bitvec = dlb2_handle_vf_flr_interrupt(dlb2);

	mbox_bitvec = dlb2_handle_vf_to_pf_interrupt(dlb2);

	dlb2_ack_vdev_to_pf_int(hw, mbox_bitvec, flr_bitvec);

	return mbox_bitvec | flr_bitvec;
}

static void dlb2_detect_ingress_err_overload(struct dlb2 *dlb2)
{
	s64 delta_us;

	if (dlb2->ingress_err.count == 0)
		dlb2->ingress_err.ts = ktime_get();

	dlb2->ingress_err.count++;

	/* Don't check for overload until OVERLOAD_THRESH ISRs have run */
	if (dlb2->ingress_err.count < DLB2_ISR_OVERLOAD_THRESH)
		return;

	delta_us = ktime_us_delta(ktime_get(), dlb2->ingress_err.ts);

	/* Reset stats for next measurement period */
	dlb2->ingress_err.count = 0;
	dlb2->ingress_err.ts = ktime_get();

	/* Check for overload during this measurement period */
	if (delta_us > DLB2_ISR_OVERLOAD_PERIOD_S * USEC_PER_SEC)
		return;

	/*
	 * Alarm interrupt overload: disable software-generated alarms,
	 * so only hardware problems (e.g. ECC errors) interrupt the PF.
	 */
	dlb2_disable_ingress_error_alarms(&dlb2->hw);

	dlb2->ingress_err.enabled = false;

	dev_err(dlb2->dev,
		"[%s()] Overloaded detected: disabling ingress error interrupts",
		__func__);
}

static void dlb2_detect_mbox_overload(struct dlb2 *dlb2, int id)
{
	s64 delta_us;
	u32 dis = 0;

	if (dlb2->mbox[id].count == 0)
		dlb2->mbox[id].ts = ktime_get();

	dlb2->mbox[id].count++;

	/* Don't check for overload until OVERLOAD_THRESH ISRs have run */
	if (dlb2->mbox[id].count < DLB2_ISR_OVERLOAD_THRESH)
		return;

	delta_us = ktime_us_delta(ktime_get(), dlb2->mbox[id].ts);

	/* Reset stats for next measurement period */
	dlb2->mbox[id].count = 0;
	dlb2->mbox[id].ts = ktime_get();

	/* Check for overload during this measurement period */
	if (delta_us > DLB2_ISR_OVERLOAD_PERIOD_S * USEC_PER_SEC)
		return;

	/*
	 * Mailbox interrupt overload: disable the VF FUNC BAR to prevent
	 * further abuse. The FUNC BAR is re-enabled when the device is reset
	 * or the driver is reloaded.
	 */
	BIT_SET(dis, IOSF_FUNC_VF_BAR_DSBL_FUNC_VF_BAR_DIS);

	DLB2_CSR_WR(&dlb2->hw, IOSF_FUNC_VF_BAR_DSBL(id), dis);

	dlb2->mbox[id].enabled = false;

	dev_err(dlb2->dev,
		"[%s()] Overloaded detected: disabling VF %d's FUNC BAR",
		__func__, id);
}

/*
 * The alarm handler logs the alarm syndrome and, for user-caused errors,
 * reports the alarm to user-space through the per-domain device file interface.
 *
 * This function runs as a bottom-half handler because it can call printk
 * and/or acquire a mutex. These alarms don't need to be handled immediately --
 * they represent a serious, unexpected error (either in hardware or software)
 * that can't be recovered without restarting the application or resetting the
 * device. The VF->PF operations are also non-trivial and require running in a
 * bottom-half handler.
 */
static irqreturn_t
dlb2_service_intr_handler(int irq, void *hdlr_ptr)
{
	struct dlb2 *dlb2 = (struct dlb2 *)hdlr_ptr;
	u32 synd, bitvec;
	int i;

	mutex_lock(&dlb2->resource_mutex);

	synd = DLB2_CSR_RD(&dlb2->hw, SYS_ALARM_HW_SYND);

	/*
	 * Clear the MSI-X ack bit before processing the VF->PF or watchdog
	 * timer interrupts. This order is necessary so that if an interrupt
	 * event arrives after reading the corresponding bit vector, the event
	 * won't be lost.
	 */
	dlb2_ack_msix_interrupt(&dlb2->hw, DLB2_INT_NON_CQ);

	if (SYND(ALARM) & SYND(VALID))
		dlb2_process_alarm_interrupt(&dlb2->hw);

	if (dlb2_process_ingress_error_interrupt(&dlb2->hw))
		dlb2_detect_ingress_err_overload(dlb2);

	if (SYND(CWD) & SYND(VALID))
		dlb2_process_wdt_interrupt(&dlb2->hw);

	if (SYND(VF_PF_MB) & SYND(VALID)) {
		bitvec = dlb2_handle_vf_requests(&dlb2->hw);
		for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
			if (bitvec & (1 << i))
				dlb2_detect_mbox_overload(dlb2, i);
		}
	}

	mutex_unlock(&dlb2->resource_mutex);

	return IRQ_HANDLED;
}

static int
dlb2_init_alarm_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int i, ret;

	for (i = 0; i < DLB2_PF_NUM_NON_CQ_INTERRUPT_VECTORS; i++) {
		ret = request_threaded_irq(pci_irq_vector(pdev, i),
					   NULL,
					   dlb2_service_intr_handler,
					   IRQF_ONESHOT,
					   "dlb2_alarm",
					   dlb2);
		if (ret)
			return ret;

		dlb2->intr.isr_registered[i] = true;
	}

	dlb2_enable_ingress_error_alarms(&dlb2->hw);

	return 0;
}

static irqreturn_t
dlb2_compressed_cq_intr_handler(int irq, void *hdlr_ptr)
{
	u32 ldb_cq_interrupts[DLB2_MAX_NUM_LDB_PORTS / 32];
	u32 dir_cq_interrupts[DLB2_MAX_NUM_DIR_PORTS_V2_5 / 32];
	struct dlb2 *dlb2 = (struct dlb2 *)hdlr_ptr;
	unsigned long idx, mask;
	int i;

	dlb2_read_compressed_cq_intr_status(&dlb2->hw,
					    ldb_cq_interrupts,
					    dir_cq_interrupts);

	dlb2_ack_compressed_cq_intr(&dlb2->hw,
				    ldb_cq_interrupts,
				    dir_cq_interrupts);

	dlb2_ack_msix_interrupt(&dlb2->hw,
				DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID);

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		mask = 1 << (i % 32);
		idx = i / 32;

		if (!(ldb_cq_interrupts[idx] & mask))
			continue;

		dev_dbg(dlb2->dev, "[%s()] Waking LDB port %d\n",
			__func__, i);

		if (dlb2->ldb_port[i].efd_ctx)
			DLB2_EVENTFD_SIGNAL(dlb2->ldb_port[i].efd_ctx);
		else
			dlb2_wake_thread(&dlb2->intr.ldb_cq_intr[i], WAKE_CQ_INTR);
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++) {
		mask = 1 << (i % 32);
		idx = i / 32;

		if (!(dir_cq_interrupts[idx] & mask))
			continue;

		dev_dbg(dlb2->dev, "[%s()] Waking DIR port %d\n",
			__func__, i);

		if (dlb2->dir_port[i].efd_ctx)
			DLB2_EVENTFD_SIGNAL(dlb2->dir_port[i].efd_ctx);
		else
			dlb2_wake_thread(&dlb2->intr.dir_cq_intr[i], WAKE_CQ_INTR);
	}

	return IRQ_HANDLED;
}

static int
dlb2_init_compressed_mode_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int ret, irq;

	irq = pci_irq_vector(pdev, DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID);

	ret = request_irq(irq,
			  dlb2_compressed_cq_intr_handler,
			  0,
			  "dlb2_compressed_cq",
			  dlb2);
	if (ret)
		return ret;

	dlb2->intr.isr_registered[DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID] = true;

#ifndef DLB2_SIOV_IMS_WORKAROUND
	dlb2->intr.mode = DLB2_MSIX_MODE_COMPRESSED;
	dlb2_set_msix_mode(&dlb2->hw, DLB2_MSIX_MODE_COMPRESSED);
#else
	/* Use the packed mode since we need one interrupt vector per VDEV
	 * for SIOV.
	 */
	dlb2->intr.mode = DLB2_MSIX_MODE_PACKED;
	dlb2_set_msix_mode(&dlb2->hw, DLB2_MSIX_MODE_PACKED);
#endif

	return 0;
}

static void
dlb2_pf_free_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int i;

	for (i = 0; i < dlb2->intr.num_vectors; i++) {
		if (dlb2->intr.isr_registered[i])
			free_irq(pci_irq_vector(pdev, i), dlb2);
	}

	pci_free_irq_vectors(pdev);
}

static int
dlb2_pf_init_interrupts(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	int num_int, ret, i;

	/*
	 * DLB supports two modes for CQ interrupts:
	 * - "compressed mode": all CQ interrupts are packed into a single
	 *	vector. The ISR reads six interrupt status registers to
	 *	determine the source(s).
	 * - "packed mode" (unused): the hardware supports up to 64 vectors.
	 *
	 * Update for DLB 2.0:
	 * - "packed mode" is used in the siov ims workaround for DLB 2.0
	 * when DLB2_SIOV_IMS_WORKAROUND is defined. pf and each vdev is
	 * assigned a MSI-X vector for the CQ interrupt. Watchdog/alert uses
	 * interrupt 0. Total 2 + DLB2_MAX_NUM_VDEVS vectors are used.
	 */

#ifndef DLB2_SIOV_IMS_WORKAROUND
	num_int = DLB2_PF_NUM_COMPRESSED_MODE_VECTORS;
#else
	num_int = DLB2_PF_NUM_COMPRESSED_MODE_VECTORS + DLB2_MAX_NUM_VDEVS;
#endif

	ret = pci_alloc_irq_vectors(pdev, num_int, num_int, PCI_IRQ_MSIX);
	if (ret < 0)
		return ret;

	dlb2->intr.num_vectors = ret;
	dlb2->intr.base_vector = pci_irq_vector(pdev, 0);

	ret = dlb2_init_alarm_interrupts(dlb2, pdev);
	if (ret) {
		dlb2_pf_free_interrupts(dlb2, pdev);
		return ret;
	}

	ret = dlb2_init_compressed_mode_interrupts(dlb2, pdev);
	if (ret) {
		dlb2_pf_free_interrupts(dlb2, pdev);
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

/*
 * If the device is reset during use, its interrupt registers need to be
 * reinitialized.
 */
static void
dlb2_pf_reinit_interrupts(struct dlb2 *dlb2)
{
	int i;

	/* Re-enable alarms after device reset */
	dlb2_enable_ingress_error_alarms(&dlb2->hw);

	if (!dlb2->ingress_err.enabled)
		dev_err(dlb2->dev,
			"[%s()] Re-enabling ingress error interrupts",
			__func__);

	dlb2->ingress_err.enabled = true;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (!dlb2->mbox[i].enabled)
			dev_err(dlb2->dev,
				"[%s()] Re-enabling VF %d's FUNC BAR",
				__func__, i);

		dlb2->mbox[i].enabled = true;
	}

#ifndef DLB2_SIOV_IMS_WORKAROUND
	dlb2_set_msix_mode(&dlb2->hw, DLB2_MSIX_MODE_COMPRESSED);
#else
	dlb2_set_msix_mode(&dlb2->hw, DLB2_MSIX_MODE_PACKED);
#endif
}

static int
dlb2_pf_enable_ldb_cq_interrupts(struct dlb2 *dlb2,
				 int domain_id,
				 int id,
				 u16 thresh)
{
	int mode, vec, ret;

	if (dlb2->intr.mode == DLB2_MSIX_MODE_COMPRESSED) {
		mode = DLB2_CQ_ISR_MODE_MSIX;
		vec = 0;
	} else {
		mode = DLB2_CQ_ISR_MODE_MSIX;
#ifndef DLB2_SIOV_IMS_WORKAROUND
		vec = id % 64;
#else
		/* use only one vector for all pf cq interrupts */
		vec = 0;
#endif
	}

	dlb2->intr.ldb_cq_intr[id].disabled = false;
	dlb2->intr.ldb_cq_intr[id].configured = true;
	dlb2->intr.ldb_cq_intr[id].domain_id = domain_id;

	ret = dlb2_configure_ldb_cq_interrupt(&dlb2->hw, id, vec,
					      mode, 0, 0, thresh);

	if (ret || dlb2_wdto_disable)
		return ret;

	return dlb2_hw_enable_ldb_cq_wd_int(&dlb2->hw, id, false, 0);
}

static int
dlb2_pf_enable_dir_cq_interrupts(struct dlb2 *dlb2,
				 int domain_id,
				 int id,
				 u16 thresh)
{
	int mode, vec, ret;

	if (dlb2->intr.mode == DLB2_MSIX_MODE_COMPRESSED) {
		mode = DLB2_CQ_ISR_MODE_MSIX;
		vec = 0;
	} else {
		mode = DLB2_CQ_ISR_MODE_MSIX;
#ifndef DLB2_SIOV_IMS_WORKAROUND
		vec = id % 64;
#else
		/* use only one vector for all pf cq interrupts */
		vec = 0;
#endif
	}

	dlb2->intr.dir_cq_intr[id].disabled = false;
	dlb2->intr.dir_cq_intr[id].configured = true;
	dlb2->intr.dir_cq_intr[id].domain_id = domain_id;

	ret = dlb2_configure_dir_cq_interrupt(&dlb2->hw, id, vec,
					      mode, 0, 0, thresh);

	if (ret || dlb2_wdto_disable)
		return ret;

	return dlb2_hw_enable_dir_cq_wd_int(&dlb2->hw, id, false, 0);
}

static int
dlb2_pf_arm_cq_interrupt(struct dlb2 *dlb2,
			 int domain_id,
			 int port_id,
			 bool is_ldb)
{
	int ret;

	if (is_ldb)
		ret = dlb2->ops->ldb_port_owned_by_domain(&dlb2->hw,
							  domain_id,
							  port_id);
	else
		ret = dlb2->ops->dir_port_owned_by_domain(&dlb2->hw,
							  domain_id,
							  port_id);

	if (ret != 1)
		return -EINVAL;

	return dlb2_arm_cq_interrupt(&dlb2->hw, port_id, is_ldb, false, 0);
}

/*******************************/
/****** Driver management ******/
/*******************************/

static int
dlb2_pf_init_driver_state(struct dlb2 *dlb2)
{
	int ret, i;

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

#ifdef CONFIG_INTEL_DLB2_SIOV
	ret = dlb2_vdcm_init(dlb2);
	if (ret)
		dev_info(dlb2->dev,
			 "VDCM initialization failed, no SIOV support\n");
#endif

	/* Initialize software state */
	INIT_WORK(&dlb2->work, dlb2_complete_queue_map_unmap);

	dlb2->ingress_err.count = 0;
	dlb2->ingress_err.enabled = true;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		dlb2->mbox[i].count = 0;
		dlb2->mbox[i].enabled = true;
	}

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++)
		dlb2->child_id_state[i].is_auxiliary_vf = false;

	ret = dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);
	if (ret) {
		dev_err(dlb2->dev,
			"[%s()] dlb2_hw_set_virt_mode failed: %d\n",
			__func__, ret);
		goto set_virt_mode_fail;
	}

	mutex_init(&dlb2->resource_mutex);
	mutex_init(&dlb2->svc_isr_mutex);

	/*
	 * Allow PF runtime power-management (forbidden by default by the PCI
	 * layer during scan). The driver puts the device into D3hot while
	 * there are no scheduling domains to service.
	 */
	pm_runtime_allow(&dlb2->pdev->dev);

	return 0;

set_virt_mode_fail:
#ifdef CONFIG_INTEL_DLB2_SIOV
	dlb2_vdcm_exit(dlb2->pdev);
#endif
	return ret;
}

static void
dlb2_pf_free_driver_state(struct dlb2 *dlb2)
{
#ifdef CONFIG_INTEL_DLB2_SIOV
	dlb2_vdcm_exit(dlb2->pdev);
#endif
}

static int
dlb2_pf_register_driver(struct dlb2 *dlb2)
{
	/* Function intentionally left blank */
	return 0;
}

static void
dlb2_pf_unregister_driver(struct dlb2 *dlb2)
{
	/* Function intentionally left blank */
}

static void
dlb2_pf_enable_pm(struct dlb2 *dlb2)
{
	/*
	 * Clear the power-management-disable register to power on the bulk of
	 * the device's hardware.
	 */
	dlb2_clr_pmcsr_disable(&dlb2->hw, dlb2->hw_ver);
}

#define DLB2_READY_RETRY_LIMIT 1000
static int
dlb2_pf_wait_for_device_ready(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	u32 retries = DLB2_READY_RETRY_LIMIT;

	/* Allow at least 1s for the device to become active after power-on */
	do {
		u32 idle, pm_st, addr;

		addr = CM_CFG_PM_STATUS(dlb2->hw_ver);

		pm_st = DLB2_CSR_RD(&dlb2->hw, addr);

		addr = CM_CFG_DIAGNOSTIC_IDLE_STATUS(dlb2->hw_ver);

		idle = DLB2_CSR_RD(&dlb2->hw, addr);

		if (BITS_GET(pm_st, CM_CFG_PM_STATUS_PMSM) == 1 &&
		    BITS_GET(idle, CM_CFG_DIAGNOSTIC_IDLE_STATUS_DLB_FUNC_IDLE)
		    == 1)
			break;

		usleep_range(1000, 2000);
	} while (--retries);

	if (!retries) {
		dev_err(&pdev->dev, "Device idle test failed\n");
		return -EIO;
	}

	return 0;
}

static void dlb2_pf_calc_arbiter_weights(struct dlb2_hw *hw,
					 u8 *weight,
					 unsigned int pct)
{
	int val, i;

	/* Largest possible weight (100% SA case): 32 */
	val = (DLB2_MAX_WEIGHT + 1) / DLB2_NUM_ARB_WEIGHTS;

	/* Scale val according to the starvation avoidance percentage */
	val = (val * pct) / 100;
	if (val == 0 && pct != 0)
		val = 1;

	/* Prio 7 always has weight 0xff */
	weight[DLB2_NUM_ARB_WEIGHTS - 1] = DLB2_MAX_WEIGHT;

	for (i = DLB2_NUM_ARB_WEIGHTS - 2; i >= 0; i--)
		weight[i] = weight[i + 1] - val;
}

static void
dlb2_pf_init_hardware(struct dlb2 *dlb2)
{
	int dlb2_rate_limit;

	if (!dlb2_wdto_disable)
		dlb2_hw_enable_wd_timer(&dlb2->hw, DLB2_WD_TMO_10S);

	if (dlb2_sparse_cq_enabled) {
		dlb2_hw_enable_sparse_ldb_cq_mode(&dlb2->hw);

		dlb2_hw_enable_sparse_dir_cq_mode(&dlb2->hw);
	}

	/* Configure arbitration weights for QE selection */
	if (dlb2_qe_sa_pct <= 100) {
		u8 weight[DLB2_NUM_ARB_WEIGHTS];

		dlb2_pf_calc_arbiter_weights(&dlb2->hw,
					     weight,
					     dlb2_qe_sa_pct);

		dlb2_hw_set_qe_arbiter_weights(&dlb2->hw, weight);
	}

	/* Configure arbitration weights for QID selection */
	if (dlb2_qid_sa_pct <= 100) {
		u8 weight[DLB2_NUM_ARB_WEIGHTS];

		dlb2_pf_calc_arbiter_weights(&dlb2->hw,
					     weight,
					     dlb2_qid_sa_pct);

		dlb2_hw_set_qid_arbiter_weights(&dlb2->hw, weight);
	}

	/* Configure rate limit for DLB
	 * sch_rate_limit field of write_buffer_ctl register can be used to
	 * limit the total throughput. The HW default value is zero which
	 * corresponds to 266 MDPS (LDB + DIR) for DLB2. sch_rate_limit = 3
	 * brings the total rate possible to 200 MDPS.
	 *
         */
	dlb2_rate_limit = DLB2_WB_CNTL_RATE_LIMIT;
	if (dlb2_rate_limit)
		dlb2_hw_set_rate_limit(&dlb2->hw, dlb2_rate_limit);

	/*Replace the current across priority group, strict random round robin
	 *QIDIX selection arbiters with a standard weighted round robin arbiter.
	 *This changes permits back-to-back enqueues of the QEs with the same QID
	 *to the same CQ. Since QE ~ code pointer, this may increase the CQ's core's
	 *code cache hit rate. All QIDIX share a common 3 bit weight register.
	 *The register supports values from 0-7 and schedules back to
	 *back from same QIDIX to CQ  value+1 times. A weight of 0 implements
	 *a standard RR, a weight of 1 means the same QEs for
	 *the CQ may be scheduled 2 times before rotating. Default is set to 0.
	 */
	if (dlb2_qidx_wrr_weight > DLB2_MAX_QIDX_WRR_SCHEDULER_WEIGHT)
		 dlb2_qidx_wrr_weight = DLB2_DEFAULT_QIDX_WRR_SCHEDULER_WEIGHT;
	dlb2_hw_set_qidx_wrr_scheduler_weight(&dlb2->hw, dlb2_qidx_wrr_weight);

}

/*****************************/
/****** Sysfs callbacks ******/
/*****************************/

static ssize_t
dlb2_sysfs_aux_vf_ids_read(struct device *dev,
			   struct device_attribute *attr,
			   char *buf,
			   int vf_id)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	int i, size;

	mutex_lock(&dlb2->resource_mutex);

	size = 0;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (!dlb2->child_id_state[i].is_auxiliary_vf)
			continue;

		if (dlb2->child_id_state[i].primary_vf_id != vf_id)
			continue;

		size += scnprintf(&buf[size], PAGE_SIZE - size, "%d,", i);
	}

	if (size == 0)
		size = 1;

	/* Replace the last comma with a newline */
	size += scnprintf(&buf[size - 1], PAGE_SIZE - size, "\n");

	mutex_unlock(&dlb2->resource_mutex);

	return size;
}

static ssize_t
dlb2_sysfs_aux_vf_ids_write(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count,
			    int primary_vf_id)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	char *user_buf = (char *)buf;
	char *vf_id_str;
	int vf_id;

	mutex_lock(&dlb2->resource_mutex);

	/*
	 * If the primary VF is locked, no auxiliary VFs can be added to or
	 * removed from it.
	 */
	if (dlb2_vdev_is_locked(&dlb2->hw, primary_vf_id)) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	vf_id_str = strsep(&user_buf, ",");
	while (vf_id_str) {
		struct vf_id_state *child_id_state;
		int ret;

		ret = kstrtoint(vf_id_str, 0, &vf_id);
		if (ret) {
			mutex_unlock(&dlb2->resource_mutex);
			return -EINVAL;
		}

		if (vf_id >= dlb2->num_vfs) {
			mutex_unlock(&dlb2->resource_mutex);
			return -EINVAL;
		}

		child_id_state = &dlb2->child_id_state[vf_id];

		if (vf_id == primary_vf_id) {
			mutex_unlock(&dlb2->resource_mutex);
			return -EINVAL;
		}

		/* Check if the aux-primary VF relationship already exists */
		if (child_id_state->is_auxiliary_vf &&
		    child_id_state->primary_vf_id == primary_vf_id) {
			vf_id_str = strsep(&user_buf, ",");
			continue;
		}

		/* If the desired VF is locked, it can't be made auxiliary */
		if (dlb2_vdev_is_locked(&dlb2->hw, vf_id)) {
			mutex_unlock(&dlb2->resource_mutex);
			return -EINVAL;
		}

		/* Attempt to reassign the VF */
		child_id_state->is_auxiliary_vf = true;
		child_id_state->primary_vf_id = primary_vf_id;

		/* Reassign any of the desired VF's resources back to the PF */
		if (dlb2_reset_vdev_resources(&dlb2->hw, vf_id)) {
			mutex_unlock(&dlb2->resource_mutex);
			return -EINVAL;
		}

		vf_id_str = strsep(&user_buf, ",");
	}

	mutex_unlock(&dlb2->resource_mutex);

	return count;
}

static ssize_t
dlb2_sysfs_vf_read(struct device *dev,
		   struct device_attribute *attr,
		   char *buf,
		   int vf_id)
{
	struct dlb2_get_num_resources_args num_avail_rsrcs;
	struct dlb2_get_num_resources_args num_used_rsrcs;
	struct dlb2_get_num_resources_args num_rsrcs;
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	struct dlb2_hw *hw = &dlb2->hw;
	int val, i;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &num_avail_rsrcs, true, vf_id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &num_used_rsrcs, true, vf_id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	num_rsrcs.num_sched_domains = num_avail_rsrcs.num_sched_domains +
		num_used_rsrcs.num_sched_domains;
	num_rsrcs.num_ldb_queues = num_avail_rsrcs.num_ldb_queues +
		num_used_rsrcs.num_ldb_queues;
	num_rsrcs.num_ldb_ports = num_avail_rsrcs.num_ldb_ports +
		num_used_rsrcs.num_ldb_ports;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		num_rsrcs.num_cos_ldb_ports[i] =
			num_avail_rsrcs.num_cos_ldb_ports[i] +
			num_used_rsrcs.num_cos_ldb_ports[i];
	num_rsrcs.num_dir_ports = num_avail_rsrcs.num_dir_ports +
		num_used_rsrcs.num_dir_ports;
	num_rsrcs.num_ldb_credits = num_avail_rsrcs.num_ldb_credits +
		num_used_rsrcs.num_ldb_credits;
	num_rsrcs.num_dir_credits = num_avail_rsrcs.num_dir_credits +
		num_used_rsrcs.num_dir_credits;
	num_rsrcs.num_hist_list_entries =
		num_avail_rsrcs.num_hist_list_entries +
		num_used_rsrcs.num_hist_list_entries;
	num_rsrcs.num_atomic_inflights = num_avail_rsrcs.num_atomic_inflights +
		num_used_rsrcs.num_atomic_inflights;
	num_rsrcs.num_sn_slots[0] = num_avail_rsrcs.num_sn_slots[0] +
		num_used_rsrcs.num_sn_slots[0];
	num_rsrcs.num_sn_slots[1] = num_avail_rsrcs.num_sn_slots[1] +
		num_used_rsrcs.num_sn_slots[1];

	if (strncmp(attr->attr.name, "num_sched_domains",
		    sizeof("num_sched_domains")) == 0)
		val = num_rsrcs.num_sched_domains;
	else if (strncmp(attr->attr.name, "num_ldb_queues",
			 sizeof("num_ldb_queues")) == 0)
		val = num_rsrcs.num_ldb_queues;
	else if (strncmp(attr->attr.name, "num_ldb_ports",
			 sizeof("num_ldb_ports")) == 0)
		val = num_rsrcs.num_ldb_ports;
	else if (strncmp(attr->attr.name, "num_cos0_ldb_ports",
			 sizeof("num_cos0_ldb_ports")) == 0)
		val = num_rsrcs.num_cos_ldb_ports[0];
	else if (strncmp(attr->attr.name, "num_cos1_ldb_ports",
			 sizeof("num_cos1_ldb_ports")) == 0)
		val = num_rsrcs.num_cos_ldb_ports[1];
	else if (strncmp(attr->attr.name, "num_cos2_ldb_ports",
			 sizeof("num_cos2_ldb_ports")) == 0)
		val = num_rsrcs.num_cos_ldb_ports[2];
	else if (strncmp(attr->attr.name, "num_cos3_ldb_ports",
			 sizeof("num_cos3_ldb_ports")) == 0)
		val = num_rsrcs.num_cos_ldb_ports[3];
	else if (strncmp(attr->attr.name, "num_dir_ports",
			 sizeof("num_dir_ports")) == 0)
		val = num_rsrcs.num_dir_ports;
	else if (strncmp(attr->attr.name, "num_ldb_credits",
			 sizeof("num_ldb_credits")) == 0)
		val = num_rsrcs.num_ldb_credits;
	else if (strncmp(attr->attr.name, "num_dir_credits",
			 sizeof("num_dir_credits")) == 0)
		val = num_rsrcs.num_dir_credits;
	else if (strncmp(attr->attr.name, "num_hist_list_entries",
			 sizeof("num_hist_list_entries")) == 0)
		val = num_rsrcs.num_hist_list_entries;
	else if (strncmp(attr->attr.name, "num_atomic_inflights",
			 sizeof("num_atomic_inflights")) == 0)
		val = num_rsrcs.num_atomic_inflights;
	else if (strncmp(attr->attr.name, "num_sn0_slots",
			 sizeof("num_sn0_slots")) == 0)
		val = num_rsrcs.num_sn_slots[0];
	else if (strncmp(attr->attr.name, "num_sn1_slots",
			 sizeof("num_sn1_slots")) == 0)
		val = num_rsrcs.num_sn_slots[1];
	else if (strncmp(attr->attr.name, "locked",
			 sizeof("locked")) == 0)
		val = (int)dlb2_vdev_is_locked(hw, vf_id);
	else if (strncmp(attr->attr.name, "func_bar_en",
			 sizeof("func_bar_en")) == 0)
		val = (int)dlb2->mbox[vf_id].enabled;
	else
		return -1;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t
dlb2_sysfs_vf_write(struct device *dev,
		    struct device_attribute *attr,
		    const char *buf,
		    size_t count,
		    int vf_id)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	struct dlb2_hw *hw = &dlb2->hw;
	unsigned long num;
	const char *name;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	name = attr->attr.name;

	mutex_lock(&dlb2->resource_mutex);

	if (strncmp(name, "num_sched_domains",
		    sizeof("num_sched_domains")) == 0) {
		ret = dlb2_update_vdev_sched_domains(hw, vf_id, num);
	} else if (strncmp(name, "num_ldb_queues",
			   sizeof("num_ldb_queues")) == 0) {
		ret = dlb2_update_vdev_ldb_queues(hw, vf_id, num);
	} else if (strncmp(name, "num_ldb_ports",
			   sizeof("num_ldb_ports")) == 0) {
		ret = dlb2_update_vdev_ldb_ports(hw, vf_id, num);
	} else if (strncmp(name, "num_cos0_ldb_ports",
			   sizeof("num_cos0_ldb_ports")) == 0) {
		ret = dlb2_update_vdev_ldb_cos_ports(hw, vf_id, 0, num);
	} else if (strncmp(name, "num_cos1_ldb_ports",
			   sizeof("num_cos1_ldb_ports")) == 0) {
		ret = dlb2_update_vdev_ldb_cos_ports(hw, vf_id, 1, num);
	} else if (strncmp(name, "num_cos2_ldb_ports",
			   sizeof("num_cos2_ldb_ports")) == 0) {
		ret = dlb2_update_vdev_ldb_cos_ports(hw, vf_id, 2, num);
	} else if (strncmp(name, "num_cos3_ldb_ports",
			   sizeof("num_cos3_ldb_ports")) == 0) {
		ret = dlb2_update_vdev_ldb_cos_ports(hw, vf_id, 3, num);
	} else if (strncmp(name, "num_dir_ports",
			   sizeof("num_dir_ports")) == 0) {
		ret = dlb2_update_vdev_dir_ports(hw, vf_id, num);
	} else if (strncmp(name, "num_ldb_credits",
			   sizeof("num_ldb_credits")) == 0) {
		ret = dlb2_update_vdev_ldb_credits(hw, vf_id, num);
	} else if (strncmp(name, "num_dir_credits",
			   sizeof("num_dir_credits")) == 0) {
		ret = dlb2_update_vdev_dir_credits(hw, vf_id, num);
	} else if (strncmp(attr->attr.name, "num_hist_list_entries",
			   sizeof("num_hist_list_entries")) == 0) {
		ret = dlb2_update_vdev_hist_list_entries(hw, vf_id, num);
	} else if (strncmp(attr->attr.name, "num_atomic_inflights",
			   sizeof("num_atomic_inflights")) == 0) {
		ret = dlb2_update_vdev_atomic_inflights(hw, vf_id, num);
	} else if (strncmp(attr->attr.name, "num_sn0_slots",
			   sizeof("num_sn0_slots")) == 0) {
		ret = dlb2_update_vdev_sn_slots(hw, vf_id, 0, num);
	} else if (strncmp(attr->attr.name, "num_sn1_slots",
			   sizeof("num_sn1_slots")) == 0) {
		ret = dlb2_update_vdev_sn_slots(hw, vf_id, 1, num);
	} else if (strncmp(attr->attr.name, "func_bar_en",
			   sizeof("func_bar_en")) == 0) {
		if (!dlb2->mbox[vf_id].enabled && num) {
			DLB2_CSR_WR(&dlb2->hw, IOSF_FUNC_VF_BAR_DSBL(vf_id), 0);

			dev_err(dlb2->dev,
				"[%s()] Re-enabling VDEV %d's FUNC BAR",
				__func__, vf_id);

			dlb2->mbox[vf_id].enabled = true;
		}
	} else {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

#define DLB2_VF_SYSFS_RD_FUNC(id) \
static ssize_t dlb2_sysfs_vf ## id ## _read(		      \
	struct device *dev,				      \
	struct device_attribute *attr,			      \
	char *buf)					      \
{							      \
	return dlb2_sysfs_vf_read(dev, attr, buf, id);	      \
}							      \

#define DLB2_VF_SYSFS_WR_FUNC(id) \
static ssize_t dlb2_sysfs_vf ## id ## _write(		       \
	struct device *dev,				       \
	struct device_attribute *attr,			       \
	const char *buf,				       \
	size_t count)					       \
{							       \
	return dlb2_sysfs_vf_write(dev, attr, buf, count, id); \
}

#define DLB2_AUX_VF_ID_RD_FUNC(id) \
static ssize_t dlb2_sysfs_vf ## id ## _vf_ids_read(		      \
	struct device *dev,					      \
	struct device_attribute *attr,				      \
	char *buf)						      \
{								      \
	return dlb2_sysfs_aux_vf_ids_read(dev, attr, buf, id);	      \
}								      \

#define DLB2_AUX_VF_ID_WR_FUNC(id) \
static ssize_t dlb2_sysfs_vf ## id ## _vf_ids_write(		       \
	struct device *dev,					       \
	struct device_attribute *attr,				       \
	const char *buf,					       \
	size_t count)						       \
{								       \
	return dlb2_sysfs_aux_vf_ids_write(dev, attr, buf, count, id); \
}

/* Read-write per-resource-group sysfs files */
#define DLB2_VF_DEVICE_ATTRS(id) \
static struct device_attribute			    \
dev_attr_vf ## id ## _sched_domains =		    \
	__ATTR(num_sched_domains,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _ldb_queues =		    \
	__ATTR(num_ldb_queues,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _ldb_ports =		    \
	__ATTR(num_ldb_ports,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _cos0_ldb_ports =		    \
	__ATTR(num_cos0_ldb_ports,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _cos1_ldb_ports =		    \
	__ATTR(num_cos1_ldb_ports,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _cos2_ldb_ports =		    \
	__ATTR(num_cos2_ldb_ports,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _cos3_ldb_ports =		    \
	__ATTR(num_cos3_ldb_ports,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _dir_ports =		    \
	__ATTR(num_dir_ports,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _ldb_credits =		    \
	__ATTR(num_ldb_credits,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _dir_credits =		    \
	__ATTR(num_dir_credits,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _hist_list_entries =	    \
	__ATTR(num_hist_list_entries,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _atomic_inflights =	    \
	__ATTR(num_atomic_inflights,		    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _sn0_slots =		    \
	__ATTR(num_sn0_slots,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _sn1_slots =		    \
	__ATTR(num_sn1_slots,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _locked =			    \
	__ATTR(locked,				    \
	       0444,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       NULL);				    \
static struct device_attribute			    \
dev_attr_vf ## id ## _func_bar_en =		    \
	__ATTR(func_bar_en,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _read,	    \
	       dlb2_sysfs_vf ## id ## _write);	    \
static struct device_attribute			    \
dev_attr_vf ## id ## _aux_vf_ids =		    \
	__ATTR(aux_vf_ids,			    \
	       0644,				    \
	       dlb2_sysfs_vf ## id ## _vf_ids_read,  \
	       dlb2_sysfs_vf ## id ## _vf_ids_write) \

#define DLB2_VF_SYSFS_ATTRS(id) \
DLB2_VF_DEVICE_ATTRS(id);				\
static struct attribute *dlb2_vf ## id ## _attrs[] = {	\
	&dev_attr_vf ## id ## _sched_domains.attr,	\
	&dev_attr_vf ## id ## _ldb_queues.attr,		\
	&dev_attr_vf ## id ## _ldb_ports.attr,		\
	&dev_attr_vf ## id ## _cos0_ldb_ports.attr,	\
	&dev_attr_vf ## id ## _cos1_ldb_ports.attr,	\
	&dev_attr_vf ## id ## _cos2_ldb_ports.attr,	\
	&dev_attr_vf ## id ## _cos3_ldb_ports.attr,	\
	&dev_attr_vf ## id ## _dir_ports.attr,		\
	&dev_attr_vf ## id ## _ldb_credits.attr,	\
	&dev_attr_vf ## id ## _dir_credits.attr,	\
	&dev_attr_vf ## id ## _hist_list_entries.attr,	\
	&dev_attr_vf ## id ## _atomic_inflights.attr,	\
	&dev_attr_vf ## id ## _sn0_slots.attr,		\
	&dev_attr_vf ## id ## _sn1_slots.attr,		\
	&dev_attr_vf ## id ## _locked.attr,		\
	&dev_attr_vf ## id ## _func_bar_en.attr,	\
	&dev_attr_vf ## id ## _aux_vf_ids.attr,		\
	NULL						\
}

#define DLB2_VF_SYSFS_ATTR_GROUP(id) \
DLB2_VF_SYSFS_ATTRS(id);					\
static struct attribute_group dlb2_vf ## id ## _attr_group = {	\
	.attrs = dlb2_vf ## id ## _attrs,			\
	.name = "vf" #id "_resources"				\
}

DLB2_VF_SYSFS_RD_FUNC(0);
DLB2_VF_SYSFS_RD_FUNC(1);
DLB2_VF_SYSFS_RD_FUNC(2);
DLB2_VF_SYSFS_RD_FUNC(3);
DLB2_VF_SYSFS_RD_FUNC(4);
DLB2_VF_SYSFS_RD_FUNC(5);
DLB2_VF_SYSFS_RD_FUNC(6);
DLB2_VF_SYSFS_RD_FUNC(7);
DLB2_VF_SYSFS_RD_FUNC(8);
DLB2_VF_SYSFS_RD_FUNC(9);
DLB2_VF_SYSFS_RD_FUNC(10);
DLB2_VF_SYSFS_RD_FUNC(11);
DLB2_VF_SYSFS_RD_FUNC(12);
DLB2_VF_SYSFS_RD_FUNC(13);
DLB2_VF_SYSFS_RD_FUNC(14);
DLB2_VF_SYSFS_RD_FUNC(15);

DLB2_VF_SYSFS_WR_FUNC(0);
DLB2_VF_SYSFS_WR_FUNC(1);
DLB2_VF_SYSFS_WR_FUNC(2);
DLB2_VF_SYSFS_WR_FUNC(3);
DLB2_VF_SYSFS_WR_FUNC(4);
DLB2_VF_SYSFS_WR_FUNC(5);
DLB2_VF_SYSFS_WR_FUNC(6);
DLB2_VF_SYSFS_WR_FUNC(7);
DLB2_VF_SYSFS_WR_FUNC(8);
DLB2_VF_SYSFS_WR_FUNC(9);
DLB2_VF_SYSFS_WR_FUNC(10);
DLB2_VF_SYSFS_WR_FUNC(11);
DLB2_VF_SYSFS_WR_FUNC(12);
DLB2_VF_SYSFS_WR_FUNC(13);
DLB2_VF_SYSFS_WR_FUNC(14);
DLB2_VF_SYSFS_WR_FUNC(15);

DLB2_AUX_VF_ID_RD_FUNC(0);
DLB2_AUX_VF_ID_RD_FUNC(1);
DLB2_AUX_VF_ID_RD_FUNC(2);
DLB2_AUX_VF_ID_RD_FUNC(3);
DLB2_AUX_VF_ID_RD_FUNC(4);
DLB2_AUX_VF_ID_RD_FUNC(5);
DLB2_AUX_VF_ID_RD_FUNC(6);
DLB2_AUX_VF_ID_RD_FUNC(7);
DLB2_AUX_VF_ID_RD_FUNC(8);
DLB2_AUX_VF_ID_RD_FUNC(9);
DLB2_AUX_VF_ID_RD_FUNC(10);
DLB2_AUX_VF_ID_RD_FUNC(11);
DLB2_AUX_VF_ID_RD_FUNC(12);
DLB2_AUX_VF_ID_RD_FUNC(13);
DLB2_AUX_VF_ID_RD_FUNC(14);
DLB2_AUX_VF_ID_RD_FUNC(15);

DLB2_AUX_VF_ID_WR_FUNC(0);
DLB2_AUX_VF_ID_WR_FUNC(1);
DLB2_AUX_VF_ID_WR_FUNC(2);
DLB2_AUX_VF_ID_WR_FUNC(3);
DLB2_AUX_VF_ID_WR_FUNC(4);
DLB2_AUX_VF_ID_WR_FUNC(5);
DLB2_AUX_VF_ID_WR_FUNC(6);
DLB2_AUX_VF_ID_WR_FUNC(7);
DLB2_AUX_VF_ID_WR_FUNC(8);
DLB2_AUX_VF_ID_WR_FUNC(9);
DLB2_AUX_VF_ID_WR_FUNC(10);
DLB2_AUX_VF_ID_WR_FUNC(11);
DLB2_AUX_VF_ID_WR_FUNC(12);
DLB2_AUX_VF_ID_WR_FUNC(13);
DLB2_AUX_VF_ID_WR_FUNC(14);
DLB2_AUX_VF_ID_WR_FUNC(15);

DLB2_VF_SYSFS_ATTR_GROUP(0);
DLB2_VF_SYSFS_ATTR_GROUP(1);
DLB2_VF_SYSFS_ATTR_GROUP(2);
DLB2_VF_SYSFS_ATTR_GROUP(3);
DLB2_VF_SYSFS_ATTR_GROUP(4);
DLB2_VF_SYSFS_ATTR_GROUP(5);
DLB2_VF_SYSFS_ATTR_GROUP(6);
DLB2_VF_SYSFS_ATTR_GROUP(7);
DLB2_VF_SYSFS_ATTR_GROUP(8);
DLB2_VF_SYSFS_ATTR_GROUP(9);
DLB2_VF_SYSFS_ATTR_GROUP(10);
DLB2_VF_SYSFS_ATTR_GROUP(11);
DLB2_VF_SYSFS_ATTR_GROUP(12);
DLB2_VF_SYSFS_ATTR_GROUP(13);
DLB2_VF_SYSFS_ATTR_GROUP(14);
DLB2_VF_SYSFS_ATTR_GROUP(15);

const struct attribute_group *dlb2_vf_attrs[] = {
	&dlb2_vf0_attr_group,
	&dlb2_vf1_attr_group,
	&dlb2_vf2_attr_group,
	&dlb2_vf3_attr_group,
	&dlb2_vf4_attr_group,
	&dlb2_vf5_attr_group,
	&dlb2_vf6_attr_group,
	&dlb2_vf7_attr_group,
	&dlb2_vf8_attr_group,
	&dlb2_vf9_attr_group,
	&dlb2_vf10_attr_group,
	&dlb2_vf11_attr_group,
	&dlb2_vf12_attr_group,
	&dlb2_vf13_attr_group,
	&dlb2_vf14_attr_group,
	&dlb2_vf15_attr_group,
};

#define DLB2_TOTAL_SYSFS_SHOW_VER(name, macro)			\
static ssize_t total_##name##_show(				\
	struct device *dev,					\
	struct device_attribute *attr,				\
	char *buf)						\
{								\
	struct dlb2 *dlb2 = dev_get_drvdata(dev);		\
	int val = DLB2_MAX_NUM_##macro(dlb2->hw_ver);		\
								\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);		\
}

#define DLB2_TOTAL_SYSFS_SHOW(name, macro)		\
static ssize_t total_##name##_show(			\
	struct device *dev,				\
	struct device_attribute *attr,			\
	char *buf)					\
{							\
	int val = DLB2_MAX_NUM_##macro;			\
							\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);	\
}

#define DLB2_TOTAL_SYSFS_SHOW_SN(name, id)		\
static ssize_t total_##name##_show(			\
	struct device *dev,				\
	struct device_attribute *attr,			\
	char *buf)					\
{							\
	struct dlb2 *dlb2 = dev_get_drvdata(dev);	\
	struct dlb2_hw *hw = &dlb2->hw;			\
	int val;					\
							\
	mutex_lock(&dlb2->resource_mutex);		\
							\
	val = dlb2_get_group_sequence_numbers(hw, id);	\
							\
	mutex_unlock(&dlb2->resource_mutex);		\
							\
	if (!val)					\
		return -1;				\
							\
	val = DLB2_MAX_NUM_SEQUENCE_NUMBERS / val;	\
							\
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);	\
}

DLB2_TOTAL_SYSFS_SHOW(num_sched_domains, DOMAINS)
DLB2_TOTAL_SYSFS_SHOW(num_ldb_queues, LDB_QUEUES)
DLB2_TOTAL_SYSFS_SHOW(num_ldb_ports, LDB_PORTS)
DLB2_TOTAL_SYSFS_SHOW(num_cos0_ldb_ports, LDB_PORTS / DLB2_NUM_COS_DOMAINS)
DLB2_TOTAL_SYSFS_SHOW(num_cos1_ldb_ports, LDB_PORTS / DLB2_NUM_COS_DOMAINS)
DLB2_TOTAL_SYSFS_SHOW(num_cos2_ldb_ports, LDB_PORTS / DLB2_NUM_COS_DOMAINS)
DLB2_TOTAL_SYSFS_SHOW(num_cos3_ldb_ports, LDB_PORTS / DLB2_NUM_COS_DOMAINS)
DLB2_TOTAL_SYSFS_SHOW_VER(num_dir_ports, DIR_PORTS)
DLB2_TOTAL_SYSFS_SHOW_VER(num_ldb_credits, LDB_CREDITS)
DLB2_TOTAL_SYSFS_SHOW_VER(num_dir_credits, DIR_CREDITS)
DLB2_TOTAL_SYSFS_SHOW(num_atomic_inflights, AQED_ENTRIES)
DLB2_TOTAL_SYSFS_SHOW(num_hist_list_entries, HIST_LIST_ENTRIES)

DLB2_TOTAL_SYSFS_SHOW_SN(num_sn0_slots, 0)
DLB2_TOTAL_SYSFS_SHOW_SN(num_sn1_slots, 1)

#define DLB2_AVAIL_SYSFS_SHOW(name)			     \
static ssize_t avail_##name##_show(			     \
	struct device *dev,				     \
	struct device_attribute *attr,			     \
	char *buf)					     \
{							     \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);	     \
	struct dlb2_get_num_resources_args arg;		     \
	struct dlb2_hw *hw = &dlb2->hw;			     \
	int val;					     \
							     \
	mutex_lock(&dlb2->resource_mutex);		     \
							     \
	val = dlb2_hw_get_num_resources(hw, &arg, false, 0); \
							     \
	mutex_unlock(&dlb2->resource_mutex);		     \
							     \
	if (val)					     \
		return -1;				     \
							     \
	val = arg.name;					     \
							     \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);	     \
}

#define DLB2_AVAIL_SYSFS_SHOW_COS(name, idx)		     \
static ssize_t avail_##name##_show(			     \
	struct device *dev,				     \
	struct device_attribute *attr,			     \
	char *buf)					     \
{							     \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);	     \
	struct dlb2_get_num_resources_args arg;		     \
	struct dlb2_hw *hw = &dlb2->hw;			     \
	int val;					     \
							     \
	mutex_lock(&dlb2->resource_mutex);		     \
							     \
	val = dlb2_hw_get_num_resources(hw, &arg, false, 0); \
							     \
	mutex_unlock(&dlb2->resource_mutex);		     \
							     \
	if (val)					     \
		return -1;				     \
							     \
	val = arg.num_cos_ldb_ports[idx];		     \
							     \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);	     \
}

#define DLB2_AVAIL_SYSFS_SHOW_SN(name, idx)		     \
static ssize_t avail_##name##_show(			     \
	struct device *dev,				     \
	struct device_attribute *attr,			     \
	char *buf)					     \
{							     \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);	     \
	struct dlb2_get_num_resources_args arg;		     \
	struct dlb2_hw *hw = &dlb2->hw;			     \
	int val;					     \
							     \
	mutex_lock(&dlb2->resource_mutex);		     \
							     \
	val = dlb2_hw_get_num_resources(hw, &arg, false, 0); \
							     \
	mutex_unlock(&dlb2->resource_mutex);		     \
							     \
	if (val)					     \
		return -1;				     \
							     \
	val = arg.num_sn_slots[idx];			     \
							     \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);	     \
}

DLB2_AVAIL_SYSFS_SHOW(num_sched_domains)
DLB2_AVAIL_SYSFS_SHOW(num_ldb_queues)
DLB2_AVAIL_SYSFS_SHOW(num_ldb_ports)
DLB2_AVAIL_SYSFS_SHOW_COS(num_cos0_ldb_ports, 0)
DLB2_AVAIL_SYSFS_SHOW_COS(num_cos1_ldb_ports, 1)
DLB2_AVAIL_SYSFS_SHOW_COS(num_cos2_ldb_ports, 2)
DLB2_AVAIL_SYSFS_SHOW_COS(num_cos3_ldb_ports, 3)
DLB2_AVAIL_SYSFS_SHOW(num_dir_ports)
DLB2_AVAIL_SYSFS_SHOW(num_ldb_credits)
DLB2_AVAIL_SYSFS_SHOW(num_dir_credits)
DLB2_AVAIL_SYSFS_SHOW(num_atomic_inflights)
DLB2_AVAIL_SYSFS_SHOW(num_hist_list_entries)
DLB2_AVAIL_SYSFS_SHOW_SN(num_sn0_slots, 0)
DLB2_AVAIL_SYSFS_SHOW_SN(num_sn1_slots, 1)

static ssize_t max_ctg_hl_entries_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	struct dlb2_get_num_resources_args arg;
	struct dlb2_hw *hw = &dlb2->hw;
	int val;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &arg, false, 0);

	mutex_unlock(&dlb2->resource_mutex);

	if (val)
		return -1;

	val = arg.max_contiguous_hist_list_entries;

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

static const struct attribute_group dlb2_total_attr_group = {
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

static const struct attribute_group dlb2_avail_attr_group = {
	.attrs = dlb2_avail_attrs,
	.name = "avail_resources",
};

#define DLB2_GROUP_SNS_PER_QUEUE_SHOW(id)	       \
static ssize_t group##id##_sns_per_queue_show(	       \
	struct device *dev,			       \
	struct device_attribute *attr,		       \
	char *buf)				       \
{						       \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);      \
	struct dlb2_hw *hw = &dlb2->hw;		       \
	int val;				       \
						       \
	mutex_lock(&dlb2->resource_mutex);	       \
						       \
	val = dlb2_get_group_sequence_numbers(hw, id); \
						       \
	mutex_unlock(&dlb2->resource_mutex);	       \
						       \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val); \
}

DLB2_GROUP_SNS_PER_QUEUE_SHOW(0)
DLB2_GROUP_SNS_PER_QUEUE_SHOW(1)

#define DLB2_GROUP_SNS_PER_QUEUE_STORE(id)		    \
static ssize_t group##id##_sns_per_queue_store(		    \
	struct device *dev,				    \
	struct device_attribute *attr,			    \
	const char *buf,				    \
	size_t count)					    \
{							    \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);	    \
	struct dlb2_hw *hw = &dlb2->hw;			    \
	unsigned long val;				    \
	int err;					    \
							    \
	err = kstrtoul(buf, 0, &val);			    \
	if (err)					    \
		return -1;				    \
							    \
	mutex_lock(&dlb2->resource_mutex);		    \
							    \
	err = dlb2_set_group_sequence_numbers(hw, id, val); \
							    \
	mutex_unlock(&dlb2->resource_mutex);		    \
							    \
	if (err)					    \
		return err;				    \
							    \
	return count;					    \
}

DLB2_GROUP_SNS_PER_QUEUE_STORE(0)
DLB2_GROUP_SNS_PER_QUEUE_STORE(1)

/* RW sysfs files in the sequence_numbers/ subdirectory */
static DEVICE_ATTR_RW(group0_sns_per_queue);
static DEVICE_ATTR_RW(group1_sns_per_queue);

static struct attribute *dlb2_sequence_number_attrs[] = {
	&dev_attr_group0_sns_per_queue.attr,
	&dev_attr_group1_sns_per_queue.attr,
	NULL
};

static const struct attribute_group dlb2_sequence_number_attr_group = {
	.attrs = dlb2_sequence_number_attrs,
	.name = "sequence_numbers"
};

#define DLB2_COS_BW_PERCENT_SHOW(id)		       \
static ssize_t cos##id##_bw_percent_show(	       \
	struct device *dev,			       \
	struct device_attribute *attr,		       \
	char *buf)				       \
{						       \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);      \
	struct dlb2_hw *hw = &dlb2->hw;		       \
	int val;				       \
						       \
	mutex_lock(&dlb2->resource_mutex);	       \
						       \
	val = dlb2_hw_get_cos_bandwidth(hw, id);       \
						       \
	mutex_unlock(&dlb2->resource_mutex);	       \
						       \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val); \
}

DLB2_COS_BW_PERCENT_SHOW(0)
DLB2_COS_BW_PERCENT_SHOW(1)
DLB2_COS_BW_PERCENT_SHOW(2)
DLB2_COS_BW_PERCENT_SHOW(3)

#define DLB2_COS_BW_PERCENT_STORE(id)		      \
static ssize_t cos##id##_bw_percent_store(	      \
	struct device *dev,			      \
	struct device_attribute *attr,		      \
	const char *buf,			      \
	size_t count)				      \
{						      \
	struct dlb2 *dlb2 = dev_get_drvdata(dev);     \
	struct dlb2_hw *hw = &dlb2->hw;		      \
	unsigned long val;			      \
	int err;				      \
						      \
	err = kstrtoul(buf, 0, &val);		      \
	if (err)				      \
		return -1;			      \
						      \
	mutex_lock(&dlb2->resource_mutex);	      \
						      \
	err = dlb2_hw_set_cos_bandwidth(hw, id, val); \
						      \
	mutex_unlock(&dlb2->resource_mutex);	      \
						      \
	if (err)				      \
		return err;			      \
						      \
	return count;				      \
}

DLB2_COS_BW_PERCENT_STORE(0)
DLB2_COS_BW_PERCENT_STORE(1)
DLB2_COS_BW_PERCENT_STORE(2)
DLB2_COS_BW_PERCENT_STORE(3)

/* RW sysfs files in the sequence_numbers/ subdirectory */
static DEVICE_ATTR_RW(cos0_bw_percent);
static DEVICE_ATTR_RW(cos1_bw_percent);
static DEVICE_ATTR_RW(cos2_bw_percent);
static DEVICE_ATTR_RW(cos3_bw_percent);

static struct attribute *dlb2_cos_bw_percent_attrs[] = {
	&dev_attr_cos0_bw_percent.attr,
	&dev_attr_cos1_bw_percent.attr,
	&dev_attr_cos2_bw_percent.attr,
	&dev_attr_cos3_bw_percent.attr,
	NULL
};

static const struct attribute_group dlb2_cos_bw_percent_attr_group = {
	.attrs = dlb2_cos_bw_percent_attrs,
	.name = "cos_bw"
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

static ssize_t ingress_err_en_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&dlb2->resource_mutex);

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dlb2->ingress_err.enabled);

	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

static ssize_t ingress_err_en_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct dlb2 *dlb2 = dev_get_drvdata(dev);
	unsigned long num;
	ssize_t ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	mutex_lock(&dlb2->resource_mutex);

	if (!dlb2->ingress_err.enabled && num) {
		dlb2_enable_ingress_error_alarms(&dlb2->hw);

		dev_err(dlb2->dev,
			"[%s()] Re-enabling ingress error interrupts",
			__func__);

		dlb2->ingress_err.enabled = true;
	}

	mutex_unlock(&dlb2->resource_mutex);

	return (ret == 0) ? count : ret;
}

static DEVICE_ATTR_RW(ingress_err_en);

static struct attribute *dlb2_ingress_err_en_attr[] = {
	&dev_attr_ingress_err_en.attr,
	NULL
};

static const struct attribute_group dlb2_ingress_err_en_attr_group = {
	.attrs = dlb2_ingress_err_en_attr,
};

static const struct attribute_group *dlb2_pf_attr_groups[] = {
	&dlb2_ingress_err_en_attr_group,
	&dlb2_dev_id_attr_group,
	&dlb2_total_attr_group,
	&dlb2_avail_attr_group,
	&dlb2_sequence_number_attr_group,
	&dlb2_cos_bw_percent_attr_group,
	NULL,
};

static int
dlb2_pf_sysfs_create(struct dlb2 *dlb2)
{
	struct device *dev = &dlb2->pdev->dev;
	int ret;
	int i;

	ret = devm_device_add_groups(dev, dlb2_pf_attr_groups);
	if (ret) {
		dev_err(dev,
			"Failed to create dlb pf attribute group: %d\n", ret);
		return ret;
	}

	for (i = 0; i < pci_num_vf(dlb2->pdev); i++) {
		ret = devm_device_add_group(dev, dlb2_vf_attrs[i]);
		if (ret) {
			dev_err(dev,
				"Failed to create dlb vf attribute group: %d, %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

static void
dlb2_pf_sysfs_reapply_configuration(struct dlb2 *dlb2)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		int num_sns = dlb2_get_group_sequence_numbers(&dlb2->hw, i);

		dlb2_set_group_sequence_numbers(&dlb2->hw, i, num_sns);
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		int bw = dlb2_hw_get_cos_bandwidth(&dlb2->hw, i);

		dlb2_hw_set_cos_bandwidth(&dlb2->hw, i, bw);
	}
}

/*****************************/
/****** IOCTL callbacks ******/
/*****************************/

static int
dlb2_pf_create_sched_domain(struct dlb2_hw *hw,
			    struct dlb2_create_sched_domain_args *args,
			    struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_sched_domain(hw, args, resp, false, 0);
}

static int
dlb2_pf_create_ldb_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_ldb_queue_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_ldb_queue(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_create_dir_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_dir_queue_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_dir_queue(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_create_ldb_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_ldb_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_ldb_port(hw, id, args, cq_dma_base,
				       resp, false, 0);
}

static int
dlb2_pf_create_dir_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_dir_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_dir_port(hw, id, args, cq_dma_base,
				       resp, false, 0);
}

static int
dlb2_pf_start_domain(struct dlb2_hw *hw,
		     u32 id,
		     struct dlb2_start_domain_args *args,
		     struct dlb2_cmd_response *resp)
{
	return dlb2_hw_start_domain(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_stop_domain(struct dlb2_hw *hw,
		     u32 id,
		     struct dlb2_stop_domain_args *args,
		     struct dlb2_cmd_response *resp)
{
	return dlb2_hw_stop_domain(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_map_qid(struct dlb2_hw *hw,
		u32 id,
		struct dlb2_map_qid_args *args,
		struct dlb2_cmd_response *resp)
{
	return dlb2_hw_map_qid(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_unmap_qid(struct dlb2_hw *hw,
		  u32 id,
		  struct dlb2_unmap_qid_args *args,
		  struct dlb2_cmd_response *resp)
{
	return dlb2_hw_unmap_qid(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_pending_port_unmaps(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_pending_port_unmaps_args *args,
			    struct dlb2_cmd_response *resp)
{
	return dlb2_hw_pending_port_unmaps(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_enable_ldb_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_enable_ldb_port_args *args,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_enable_ldb_port(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_disable_ldb_port(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_disable_ldb_port_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_disable_ldb_port(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_enable_dir_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_enable_dir_port_args *args,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_enable_dir_port(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_disable_dir_port(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_disable_dir_port_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_disable_dir_port(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_get_num_resources(struct dlb2_hw *hw,
			  struct dlb2_get_num_resources_args *args)
{
	return dlb2_hw_get_num_resources(hw, args, false, 0);
}
static int
dlb2_pf_get_xstats(struct dlb2_hw *hw,
		   struct dlb2_xstats_args *args) {
	return dlb2_get_xstats(hw, args, false, 0);
}

static int
dlb2_pf_reset_domain(struct dlb2_hw *hw, u32 id)
{
	return dlb2_reset_domain(hw, id, false, 0);
}

static int
dlb2_pf_get_ldb_queue_depth(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_get_ldb_queue_depth_args *args,
			    struct dlb2_cmd_response *resp)
{
	return dlb2_hw_get_ldb_queue_depth(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_get_dir_queue_depth(struct dlb2_hw *hw,
			    u32 id,
			    struct dlb2_get_dir_queue_depth_args *args,
			    struct dlb2_cmd_response *resp)
{
	return dlb2_hw_get_dir_queue_depth(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_enable_cq_weight(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_enable_cq_weight_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_enable_cq_weight(hw, id, args, resp, false, 0);
}

static int
dlb2_pf_cq_inflight_ctrl(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_cq_inflight_ctrl_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_cq_inflight_ctrl(hw, id, args, resp, false, 0);
}

/**************************************/
/****** Resource query callbacks ******/
/**************************************/

static int
dlb2_pf_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				 u32 domain_id,
				 u32 port_id)
{
	return dlb2_ldb_port_owned_by_domain(hw, domain_id, port_id, false, 0);
}

static int
dlb2_pf_dir_port_owned_by_domain(struct dlb2_hw *hw,
				 u32 domain_id,
				 u32 port_id)
{
	return dlb2_dir_port_owned_by_domain(hw, domain_id, port_id, false, 0);
}

/********************************/
/****** DLB2 PF Device Ops ******/
/********************************/

struct dlb2_device_ops dlb2_pf_ops = {
	.map_pci_bar_space = dlb2_pf_map_pci_bar_space,
	.unmap_pci_bar_space = dlb2_pf_unmap_pci_bar_space,
	.init_driver_state = dlb2_pf_init_driver_state,
	.free_driver_state = dlb2_pf_free_driver_state,
	.sysfs_create = dlb2_pf_sysfs_create,
	.sysfs_reapply = dlb2_pf_sysfs_reapply_configuration,
	.init_interrupts = dlb2_pf_init_interrupts,
	.enable_ldb_cq_interrupts = dlb2_pf_enable_ldb_cq_interrupts,
	.enable_dir_cq_interrupts = dlb2_pf_enable_dir_cq_interrupts,
	.arm_cq_interrupt = dlb2_pf_arm_cq_interrupt,
	.reinit_interrupts = dlb2_pf_reinit_interrupts,
	.free_interrupts = dlb2_pf_free_interrupts,
	.enable_pm = dlb2_pf_enable_pm,
	.wait_for_device_ready = dlb2_pf_wait_for_device_ready,
	.register_driver = dlb2_pf_register_driver,
	.unregister_driver = dlb2_pf_unregister_driver,
	.create_sched_domain = dlb2_pf_create_sched_domain,
	.create_ldb_queue = dlb2_pf_create_ldb_queue,
	.create_dir_queue = dlb2_pf_create_dir_queue,
	.create_ldb_port = dlb2_pf_create_ldb_port,
	.create_dir_port = dlb2_pf_create_dir_port,
	.start_domain = dlb2_pf_start_domain,
	.stop_domain = dlb2_pf_stop_domain,
	.map_qid = dlb2_pf_map_qid,
	.unmap_qid = dlb2_pf_unmap_qid,
	.pending_port_unmaps = dlb2_pf_pending_port_unmaps,
	.enable_ldb_port = dlb2_pf_enable_ldb_port,
	.enable_dir_port = dlb2_pf_enable_dir_port,
	.disable_ldb_port = dlb2_pf_disable_ldb_port,
	.disable_dir_port = dlb2_pf_disable_dir_port,
	.get_num_resources = dlb2_pf_get_num_resources,
	.reset_domain = dlb2_pf_reset_domain,
	.ldb_port_owned_by_domain = dlb2_pf_ldb_port_owned_by_domain,
	.dir_port_owned_by_domain = dlb2_pf_dir_port_owned_by_domain,
	.get_sn_allocation = dlb2_get_group_sequence_numbers,
	.set_sn_allocation = dlb2_set_group_sequence_numbers,
	.get_sn_occupancy = dlb2_get_group_sequence_number_occupancy,
	.get_ldb_queue_depth = dlb2_pf_get_ldb_queue_depth,
	.get_dir_queue_depth = dlb2_pf_get_dir_queue_depth,
	.set_cos_bw = dlb2_hw_set_cos_bandwidth,
	.get_cos_bw = dlb2_hw_get_cos_bandwidth,
	.init_hardware = dlb2_pf_init_hardware,
	.query_cq_poll_mode = dlb2_pf_query_cq_poll_mode,
	.mbox_dev_reset = dlb2_pf_mbox_dev_reset,
	.enable_cq_weight = dlb2_pf_enable_cq_weight,
	.cq_inflight_ctrl = dlb2_pf_cq_inflight_ctrl,
	.get_xstats = dlb2_pf_get_xstats,
};
