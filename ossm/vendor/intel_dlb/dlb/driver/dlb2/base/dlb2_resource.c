// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2016-2020 Intel Corporation

#include "uapi/linux/dlb2_user.h"

#include <linux/sort.h>
#include <linux/completion.h>
#include <linux/pm_runtime.h>
#include "dlb2_hw_types.h"
#include "dlb2_mbox.h"
#include "dlb2_osdep.h"
#include "dlb2_osdep_bitmap.h"
#include "dlb2_osdep_types.h"
#include "dlb2_regs.h"
#include "dlb2_resource.h"
#include "../dlb2_dp_priv.h"
#include "../dlb2_dp_ops.h"

#define DLB2_DOM_LIST_HEAD(head, type) \
	DLB2_LIST_HEAD((head), type, domain_list)

#define DLB2_FUNC_LIST_HEAD(head, type) \
	DLB2_LIST_HEAD((head), type, func_list)

#define DLB2_DOM_LIST_FOR(head, ptr, iter) \
	DLB2_LIST_FOR_EACH(head, ptr, domain_list, iter)

#define DLB2_FUNC_LIST_FOR(head, ptr, iter) \
	DLB2_LIST_FOR_EACH(head, ptr, func_list, iter)

#define DLB2_DOM_LIST_FOR_SAFE(head, ptr, ptr_tmp, it, it_tmp) \
	DLB2_LIST_FOR_EACH_SAFE((head), ptr, ptr_tmp, domain_list, it, it_tmp)

#define DLB2_FUNC_LIST_FOR_SAFE(head, ptr, ptr_tmp, it, it_tmp) \
	DLB2_LIST_FOR_EACH_SAFE((head), ptr, ptr_tmp, func_list, it, it_tmp)

#define DLB2_SELECT_PORT(hw, domain) (hw->probe_done && !domain->id.vdev_owned)

static int dlb2_domain_drain_ldb_cqs(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      bool toggle_port);
static int dlb2_domain_drain_dir_cqs(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      bool toggle_port);
static DECLARE_COMPLETION(dlb_pp_comp);
static int probe_level;
/*
 * The PF driver cannot assume that a register write will affect subsequent HCW
 * writes. To ensure a write completes, the driver must read back a CSR. This
 * function only need be called for configuration that can occur after the
 * domain has started; prior to starting, applications can't send HCWs.
 */
static inline void dlb2_flush_csr(struct dlb2_hw *hw)
{
	DLB2_CSR_RD(hw, SYS_TOTAL_VAS(hw->ver));
}

static void dlb2_init_fn_rsrc_lists(struct dlb2_function_resources *rsrc)
{
	int i;

	dlb2_list_init_head(&rsrc->avail_domains);
	dlb2_list_init_head(&rsrc->used_domains);
	dlb2_list_init_head(&rsrc->avail_ldb_queues);
	dlb2_list_init_head(&rsrc->avail_dir_pq_pairs);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&rsrc->avail_ldb_ports[i]);
}

static void dlb2_init_domain_rsrc_lists(struct dlb2_hw_domain *domain)
{
	int i;

	dlb2_list_init_head(&domain->used_ldb_queues);
	dlb2_list_init_head(&domain->used_dir_pq_pairs);
	dlb2_list_init_head(&domain->avail_ldb_queues);
	dlb2_list_init_head(&domain->avail_dir_pq_pairs);
	dlb2_list_init_head(&domain->rsvd_dir_pq_pairs);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&domain->used_ldb_ports[i]);
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&domain->avail_ldb_ports[i]);
}

/**
 * dlb2_resource_free() - free device state memory
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function frees software state pointed to by dlb2_hw. This function
 * should be called when resetting the device or unloading the driver.
 */
void dlb2_resource_free(struct dlb2_hw *hw)
{
	int i;

	if (hw->pf.avail_hist_list_entries)
		dlb2_bitmap_free(hw->pf.avail_hist_list_entries);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (hw->vdev[i].avail_hist_list_entries)
			dlb2_bitmap_free(hw->vdev[i].avail_hist_list_entries);
	}

	for (i = 0; i < hw->num_phys_cpus; i++) {
		if (hw->ldb_pp_allocations && hw->ldb_pp_allocations[i])
			kfree(hw->ldb_pp_allocations[i]);
		if (hw->dir_pp_allocations && hw->dir_pp_allocations[i])
			kfree(hw->dir_pp_allocations[i]);
	}
	if (hw->ldb_pp_allocations)
		kfree(hw->ldb_pp_allocations);
	if (hw->dir_pp_allocations)
		kfree(hw->dir_pp_allocations);
}

/**
 * dlb2_resource_init() - initialize the device
 * @hw: pointer to struct dlb2_hw.
 * @ver: device version.
 *
 * This function initializes the device's software state (pointed to by the hw
 * argument) and programs global scheduling QoS registers. This function should
 * be called during driver initialization, and the dlb2_hw structure should
 * be zero-initialized before calling the function.
 *
 * The dlb2_hw struct must be unique per DLB 2.0 device and persist until the
 * device is reset.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
int dlb2_resource_init(struct dlb2_hw *hw, enum dlb2_hw_ver ver)
{
	struct dlb2_list_entry *list;
	unsigned int i;
	int ret;

	/*
	 * For optimal load-balancing, ports that map to one or more QIDs in
	 * common should not be in numerical sequence. The port->QID mapping is
	 * application dependent, but the driver interleaves port IDs as much
	 * as possible to reduce the likelihood of sequential ports mapping to
	 * the same QID(s). This initial allocation of port IDs maximizes the
	 * average distance between an ID and its immediate neighbors (i.e.
	 * the distance from 1 to 0 and to 2, the distance from 2 to 1 and to
	 * 3, etc.).
	 */
	const u8 init_ldb_port_allocation[DLB2_MAX_NUM_LDB_PORTS] = {
		0,  7,  14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9,
		16, 23, 30, 21, 28, 19, 26, 17, 24, 31, 22, 29, 20, 27, 18, 25,
		32, 39, 46, 37, 44, 35, 42, 33, 40, 47, 38, 45, 36, 43, 34, 41,
		48, 55, 62, 53, 60, 51, 58, 49, 56, 63, 54, 61, 52, 59, 50, 57,
	};

	hw->ver = ver;

	dlb2_init_fn_rsrc_lists(&hw->pf);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++)
		dlb2_init_fn_rsrc_lists(&hw->vdev[i]);

	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		dlb2_init_domain_rsrc_lists(&hw->domains[i]);
		hw->domains[i].parent_func = &hw->pf;
	}
	/* Give all resources to the PF driver */
	hw->pf.num_avail_domains = DLB2_MAX_NUM_DOMAINS;
	for (i = 0; i < hw->pf.num_avail_domains; i++) {
		list = &hw->domains[i].func_list;

		dlb2_list_add(&hw->pf.avail_domains, list);
	}

	hw->pf.num_avail_ldb_queues = DLB2_MAX_NUM_LDB_QUEUES;
	for (i = 0; i < hw->pf.num_avail_ldb_queues; i++) {
		list = &hw->rsrcs.ldb_queues[i].func_list;

		dlb2_list_add(&hw->pf.avail_ldb_queues, list);
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		hw->pf.num_avail_ldb_ports[i] =
			DLB2_MAX_NUM_LDB_PORTS / DLB2_NUM_COS_DOMAINS;

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		int cos_id = i >> DLB2_NUM_COS_DOMAINS;
		struct dlb2_ldb_port *port;

		port = &hw->rsrcs.ldb_ports[init_ldb_port_allocation[i]];

		dlb2_list_add(&hw->pf.avail_ldb_ports[cos_id],
			      &port->func_list);
	}

	hw->pf.num_avail_dir_pq_pairs = DLB2_MAX_NUM_DIR_PORTS(hw->ver);
	for (i = 0; i < hw->pf.num_avail_dir_pq_pairs; i++) {
		list = &hw->rsrcs.dir_pq_pairs[i].func_list;

		dlb2_list_add(&hw->pf.avail_dir_pq_pairs, list);
	}

	hw->pf.num_avail_qed_entries = DLB2_MAX_NUM_LDB_CREDITS(hw->ver);
	hw->pf.num_avail_dqed_entries = DLB2_MAX_NUM_DIR_CREDITS(hw->ver);
	hw->pf.num_avail_aqed_entries = DLB2_MAX_NUM_AQED_ENTRIES;

	ret = dlb2_bitmap_alloc(&hw->pf.avail_hist_list_entries,
				DLB2_MAX_NUM_HIST_LIST_ENTRIES);
	if (ret)
		goto unwind;

	ret = dlb2_bitmap_fill(hw->pf.avail_hist_list_entries);
	if (ret)
		goto unwind;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		ret = dlb2_bitmap_alloc(&hw->vdev[i].avail_hist_list_entries,
					DLB2_MAX_NUM_HIST_LIST_ENTRIES);
		if (ret)
			goto unwind;

		ret = dlb2_bitmap_zero(hw->vdev[i].avail_hist_list_entries);
		if (ret)
			goto unwind;
	}

	/* Initialize the hardware resource IDs */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		hw->domains[i].id.phys_id = i;
		hw->domains[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_LDB_QUEUES; i++) {
		hw->rsrcs.ldb_queues[i].id.phys_id = i;
		hw->rsrcs.ldb_queues[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		hw->rsrcs.ldb_ports[i].id.phys_id = i;
		hw->rsrcs.ldb_ports[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(hw->ver); i++) {
		hw->rsrcs.dir_pq_pairs[i].id.phys_id = i;
		hw->rsrcs.dir_pq_pairs[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		hw->rsrcs.sn_groups[i].id = i;
		/* Default mode (0) is 64 sequence numbers per queue */
		hw->rsrcs.sn_groups[i].mode = 0;
		hw->rsrcs.sn_groups[i].sequence_numbers_per_queue = 64;
		hw->rsrcs.sn_groups[i].slot_use_bitmap = 0;

		hw->pf.num_avail_sn_slots[i] = DLB2_MAX_NUM_SEQUENCE_NUMBERS /
			hw->rsrcs.sn_groups[i].sequence_numbers_per_queue;
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		hw->cos_reservation[i] = 100 / DLB2_NUM_COS_DOMAINS;

	return 0;

unwind:
	dlb2_resource_free(hw);

	return ret;
}

static struct dlb2_hw_domain *dlb2_get_domain_from_id(struct dlb2_hw *hw,
						      u32 id,
						      bool vdev_req,
						      unsigned int vdev_id)
{
	struct dlb2_list_entry *iteration __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;

	if (id >= DLB2_MAX_NUM_DOMAINS)
		return NULL;

	if (!vdev_req)
		return &hw->domains[id];

	rsrcs = &hw->vdev[vdev_id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iteration) {
		if (domain->id.virt_id == id)
			return domain;
	}

	return NULL;
}

static struct dlb2_ldb_port *dlb2_get_ldb_port_from_id(struct dlb2_hw *hw,
						       u32 id,
						       bool vdev_req,
						       unsigned int vdev_id)
{
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int i;

	if (id >= DLB2_MAX_NUM_LDB_PORTS)
		return NULL;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	if (!vdev_req)
		return &hw->rsrcs.ldb_ports[id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iter1) {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i],
					  port,
					  iter2) {
				if (port->id.virt_id == id)
					return port;
			}
			DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i],
					  port,
					  iter2) {
				if (port->id.virt_id == id)
					return port;
			}
		}
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[i], port, iter1) {
			if (port->id.virt_id == id)
				return port;
		}
	}

	return NULL;
}

static struct dlb2_ldb_port *
dlb2_get_domain_used_ldb_port(u32 id,
			      bool vdev_req,
			      struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	if (id >= DLB2_MAX_NUM_LDB_PORTS)
		return NULL;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
		}
	}

	return NULL;
}

static struct dlb2_ldb_port *
dlb2_get_domain_ldb_port(u32 id,
			 bool vdev_req,
			 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	if (id >= DLB2_MAX_NUM_LDB_PORTS)
		return NULL;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
		}

		DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i], port, iter) {
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
		}
	}

	return NULL;
}

static struct dlb2_dir_pq_pair *dlb2_get_dir_pq_from_id(struct dlb2_hw *hw,
							u32 id,
							bool vdev_req,
							unsigned int vdev_id)
{
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;

	if (id >= DLB2_MAX_NUM_DIR_PORTS(hw->ver))
		return NULL;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	if (!vdev_req)
		return &hw->rsrcs.dir_pq_pairs[id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iter1) {
		DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter2) {
			if (port->id.virt_id == id)
				return port;
		}
		DLB2_DOM_LIST_FOR(domain->avail_dir_pq_pairs, port, iter2) {
			if (port->id.virt_id == id)
				return port;
		}
	}

	DLB2_FUNC_LIST_FOR(rsrcs->avail_dir_pq_pairs, port, iter1) {
		if (port->id.virt_id == id)
			return port;
	}

	return NULL;
}

static struct dlb2_dir_pq_pair *
dlb2_get_domain_used_dir_pq(struct dlb2_hw *hw,
			    u32 id,
			    bool vdev_req,
			    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;

	if (id >= DLB2_MAX_NUM_DIR_PORTS(hw->ver))
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;
	}

	return NULL;
}

static struct dlb2_dir_pq_pair *
dlb2_get_domain_dir_pq(struct dlb2_hw *hw,
		       u32 id,
		       bool vdev_req,
		       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;

	if (id >= DLB2_MAX_NUM_DIR_PORTS(hw->ver))
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;
	}

	DLB2_DOM_LIST_FOR(domain->avail_dir_pq_pairs, port, iter) {
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;
	}

	return NULL;
}

static struct dlb2_ldb_queue *
dlb2_get_ldb_queue_from_id(struct dlb2_hw *hw,
			   u32 id,
			   bool vdev_req,
			   unsigned int vdev_id)
{
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;

	if (id >= DLB2_MAX_NUM_LDB_QUEUES)
		return NULL;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	if (!vdev_req)
		return &hw->rsrcs.ldb_queues[id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iter1) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter2) {
			if (queue->id.virt_id == id)
				return queue;
		}
	}

	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_queues, queue, iter1) {
		if (queue->id.virt_id == id)
			return queue;
	}

	return NULL;
}

static struct dlb2_ldb_queue *
dlb2_get_domain_ldb_queue(u32 id,
			  bool vdev_req,
			  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_queue *queue;

	if (id >= DLB2_MAX_NUM_LDB_QUEUES)
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if ((!vdev_req && queue->id.phys_id == id) ||
		    (vdev_req && queue->id.virt_id == id))
			return queue;
	}

	return NULL;
}

#define DLB2_XFER_LL_RSRC(dst, src, num, type_t, name) ({		     \
	struct dlb2_list_entry *it1 __attribute__((unused));		     \
	struct dlb2_list_entry *it2 __attribute__((unused));		     \
	struct dlb2_function_resources *_src = src;			     \
	struct dlb2_function_resources *_dst = dst;			     \
	type_t *ptr, *tmp __attribute__((unused));			     \
	unsigned int i = 0;						     \
									     \
	DLB2_FUNC_LIST_FOR_SAFE(_src->avail_##name##s, ptr, tmp, it1, it2) { \
		if (i++ == (num))					     \
			break;						     \
									     \
		dlb2_list_del(&_src->avail_##name##s, &ptr->func_list);      \
		dlb2_list_add(&_dst->avail_##name##s,  &ptr->func_list);     \
		_src->num_avail_##name##s--;				     \
		_dst->num_avail_##name##s++;				     \
	}								     \
})

#define DLB2_XFER_LL_IDX_RSRC(dst, src, num, idx, type_t, name) ({	       \
	struct dlb2_list_entry *it1 __attribute__((unused));		       \
	struct dlb2_list_entry *it2 __attribute__((unused));		       \
	struct dlb2_function_resources *_src = src;			       \
	struct dlb2_function_resources *_dst = dst;			       \
	type_t *ptr, *tmp __attribute__((unused));			       \
	unsigned int i = 0;						       \
									       \
	DLB2_FUNC_LIST_FOR_SAFE(_src->avail_##name##s[idx],		       \
				 ptr, tmp, it1, it2) {			       \
		if (i++ == (num))					       \
			break;						       \
									       \
		dlb2_list_del(&_src->avail_##name##s[idx], &ptr->func_list);   \
		dlb2_list_add(&_dst->avail_##name##s[idx],  &ptr->func_list);  \
		_src->num_avail_##name##s[idx]--;			       \
		_dst->num_avail_##name##s[idx]++;			       \
	}								       \
})

#define DLB2_VF_ID_CLEAR(head, type_t) ({ \
	struct dlb2_list_entry *iter __attribute__((unused));  \
	type_t *var;					       \
							       \
	DLB2_FUNC_LIST_FOR(head, var, iter)		       \
		var->id.vdev_owned = false;		       \
})

/**
 * dlb2_update_vdev_sched_domains() - update the domains assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of scheduling domains to assign to this vdev
 *
 * This function assigns num scheduling domains to the specified vdev. If the
 * vdev already has domains assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_sched_domains(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_function_resources *src, *dst;
	struct dlb2_hw_domain *domain;
	unsigned int orig;
	int ret;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	orig = dst->num_avail_domains;

	/*
	 * Detach the destination VF's current resources before checking if
	 * enough are available, and set their IDs accordingly.
	 */
	DLB2_VF_ID_CLEAR(dst->avail_domains, struct dlb2_hw_domain);

	DLB2_XFER_LL_RSRC(src, dst, orig, struct dlb2_hw_domain, domain);

	/* Set the domains' PF backpointer */
	DLB2_FUNC_LIST_FOR(src->avail_domains, domain, iter)
		domain->parent_func = src;

	/* Are there enough available resources to satisfy the request? */
	if (num > src->num_avail_domains) {
		num = orig;
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	DLB2_XFER_LL_RSRC(dst, src, num, struct dlb2_hw_domain, domain);

	/* Set the domains' VF backpointer */
	DLB2_FUNC_LIST_FOR(dst->avail_domains, domain, iter)
		domain->parent_func = dst;

	return ret;
}

/**
 * dlb2_update_vdev_ldb_queues() - update the LDB queues assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB queues to assign to this vdev
 *
 * This function assigns num LDB queues to the specified vdev. If the vdev
 * already has LDB queues assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_queues(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	unsigned int orig;
	int ret;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	orig = dst->num_avail_ldb_queues;

	/*
	 * Detach the destination VF's current resources before checking if
	 * enough are available, and set their IDs accordingly.
	 */
	DLB2_VF_ID_CLEAR(dst->avail_ldb_queues, struct dlb2_ldb_queue);

	DLB2_XFER_LL_RSRC(src, dst, orig, struct dlb2_ldb_queue, ldb_queue);

	/* Are there enough available resources to satisfy the request? */
	if (num > src->num_avail_ldb_queues) {
		num = orig;
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	DLB2_XFER_LL_RSRC(dst, src, num, struct dlb2_ldb_queue, ldb_queue);

	return ret;
}

/**
 * dlb2_update_vdev_ldb_cos_ports() - update the LDB ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @cos: class-of-service ID
 * @num: number of LDB ports to assign to this vdev
 *
 * This function assigns num LDB ports from class-of-service cos to the
 * specified vdev. If the vdev already has LDB ports from this class-of-service
 * assigned, this existing assignment is adjusted accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_cos_ports(struct dlb2_hw *hw,
				   u32 id,
				   u32 cos,
				   u32 num)
{
	struct dlb2_function_resources *src, *dst;
	unsigned int orig;
	int ret;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	orig = dst->num_avail_ldb_ports[cos];

	/*
	 * Detach the destination VF's current resources before checking if
	 * enough are available, and set their IDs accordingly.
	 */
	DLB2_VF_ID_CLEAR(dst->avail_ldb_ports[cos], struct dlb2_ldb_port);

	DLB2_XFER_LL_IDX_RSRC(src, dst, orig, cos,
			      struct dlb2_ldb_port, ldb_port);

	/* Are there enough available resources to satisfy the request? */
	if (num > src->num_avail_ldb_ports[cos]) {
		num = orig;
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	DLB2_XFER_LL_IDX_RSRC(dst, src, num, cos,
			      struct dlb2_ldb_port, ldb_port);

	return ret;
}

static int dlb2_add_vdev_ldb_ports(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	u32 avail, orig[DLB2_NUM_COS_DOMAINS];
	int ret, i;

	if (num == 0)
		return 0;

	src = &hw->pf;
	dst = &hw->vdev[id];

	avail = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		avail += src->num_avail_ldb_ports[i];

	if (avail < num)
		return -EINVAL;

	/* Add ports to each CoS until num have been added */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS && num > 0; i++) {
		u32 curr = dst->num_avail_ldb_ports[i];
		u32 num_to_add;

		avail = src->num_avail_ldb_ports[i];

		/* Don't attempt to add more than are available */
		num_to_add = num < avail ? num : avail;

		ret = dlb2_update_vdev_ldb_cos_ports(hw, id, i,
						     curr + num_to_add);
		if (ret)
			goto cleanup;

		orig[i] = curr;
		num -= num_to_add;
	}

	return 0;

cleanup:
	DLB2_HW_ERR(hw,
		    "[%s()] Internal error: failed to add ldb ports\n",
		    __func__);

	/* Internal error, attempt to recover original configuration */
	for (i--; i >= 0; i--)
		dlb2_update_vdev_ldb_cos_ports(hw, id, i, orig[i]);

	return ret;
}

static int dlb2_del_vdev_ldb_ports(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *dst;
	u32 orig[DLB2_NUM_COS_DOMAINS];
	int ret, i;

	if (num == 0)
		return 0;

	dst = &hw->vdev[id];

	/* Remove ports from each CoS until num have been removed */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS && num > 0; i++) {
		u32 curr = dst->num_avail_ldb_ports[i];
		u32 num_to_del;

		/* Don't attempt to remove more than dst owns */
		num_to_del = num < curr ? num : curr;

		ret = dlb2_update_vdev_ldb_cos_ports(hw, id, i,
						     curr - num_to_del);
		if (ret)
			goto cleanup;

		orig[i] = curr;
		num -= curr;
	}

	return 0;

cleanup:
	DLB2_HW_ERR(hw,
		    "[%s()] Internal error: failed to remove ldb ports\n",
		    __func__);

	/* Internal error, attempt to recover original configuration */
	for (i--; i >= 0; i--)
		dlb2_update_vdev_ldb_cos_ports(hw, id, i, orig[i]);

	return ret;
}

/**
 * dlb2_update_vdev_ldb_ports() - update the LDB ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB ports to assign to this vdev
 *
 * This function assigns num LDB ports to the specified vdev. If the vdev
 * already has LDB ports assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_ports(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *dst;
	unsigned int orig;
	int i;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	orig = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		orig += dst->num_avail_ldb_ports[i];

	if (orig == num)
		return 0;
	else if (orig < num)
		return dlb2_add_vdev_ldb_ports(hw, id, num - orig);
	else
		return dlb2_del_vdev_ldb_ports(hw, id, orig - num);
}

/**
 * dlb2_update_vdev_dir_ports() - update the DIR ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of DIR ports to assign to this vdev
 *
 * This function assigns num DIR ports to the specified vdev. If the vdev
 * already has DIR ports assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_dir_ports(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	unsigned int orig;
	int ret;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	orig = dst->num_avail_dir_pq_pairs;

	/*
	 * Detach the destination VF's current resources before checking if
	 * enough are available, and set their IDs accordingly.
	 */
	DLB2_VF_ID_CLEAR(dst->avail_dir_pq_pairs, struct dlb2_dir_pq_pair);

	DLB2_XFER_LL_RSRC(src, dst, orig,
			  struct dlb2_dir_pq_pair, dir_pq_pair);

	/* Are there enough available resources to satisfy the request? */
	if (num > src->num_avail_dir_pq_pairs) {
		num = orig;
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	DLB2_XFER_LL_RSRC(dst, src, num,
			  struct dlb2_dir_pq_pair, dir_pq_pair);

	return ret;
}

static int dlb2_transfer_bitmap_resources(struct dlb2_bitmap *src,
					  struct dlb2_bitmap *dst,
					  u32 num)
{
	int orig, ret, base;

	/*
	 * Reassign the dest's bitmap entries to the source's before checking
	 * if a contiguous chunk of size 'num' is available. The reassignment
	 * may be necessary to create a sufficiently large contiguous chunk.
	 */
	orig = dlb2_bitmap_count(dst);

	dlb2_bitmap_or(src, src, dst);

	dlb2_bitmap_zero(dst);

	/* Are there enough available resources to satisfy the request? */
	base = dlb2_bitmap_find_set_bit_range(src, num);

	if (base == -ENOENT) {
		num = orig;
		base = dlb2_bitmap_find_set_bit_range(src, num);
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	dlb2_bitmap_set_range(dst, base, num);

	dlb2_bitmap_clear_range(src, base, num);

	return ret;
}

/**
 * dlb2_update_vdev_ldb_credits() - update the vdev's assigned LDB credits
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB credit credits to assign to this vdev
 *
 * This function assigns num LDB credit to the specified vdev. If the vdev
 * already has LDB credits assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of credits, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_credits(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	u32 orig;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	/*
	 * Detach the destination VF's current resources before checking
	 * if enough are available.
	 */
	orig = dst->num_avail_qed_entries;
	src->num_avail_qed_entries += orig;
	dst->num_avail_qed_entries = 0;

	if (src->num_avail_qed_entries < num) {
		src->num_avail_qed_entries -= orig;
		dst->num_avail_qed_entries = orig;
		return -EINVAL;
	}

	src->num_avail_qed_entries -= num;
	dst->num_avail_qed_entries += num;

	return 0;
}

/**
 * dlb2_update_vdev_dir_credits() - update the vdev's assigned DIR credits
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of DIR credits to assign to this vdev
 *
 * This function assigns num DIR credit to the specified vdev. If the vdev
 * already has DIR credits assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of credits, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_dir_credits(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	u32 orig;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	/*
	 * Detach the destination VF's current resources before checking
	 * if enough are available.
	 */
	orig = dst->num_avail_dqed_entries;
	src->num_avail_dqed_entries += orig;
	dst->num_avail_dqed_entries = 0;

	if (src->num_avail_dqed_entries < num) {
		src->num_avail_dqed_entries -= orig;
		dst->num_avail_dqed_entries = orig;
		return -EINVAL;
	}

	src->num_avail_dqed_entries -= num;
	dst->num_avail_dqed_entries += num;

	return 0;
}

/**
 * dlb2_update_vdev_hist_list_entries() - update the vdev's assigned HL entries
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of history list entries to assign to this vdev
 *
 * This function assigns num history list entries to the specified vdev. If the
 * vdev already has history list entries assigned, this existing assignment is
 * adjusted accordingly. vdevs are assigned a contiguous chunk of entries, so
 * this function may fail if a sufficiently large contiguous chunk is not
 * available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_hist_list_entries(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	return dlb2_transfer_bitmap_resources(src->avail_hist_list_entries,
					      dst->avail_hist_list_entries,
					      num);
}

/**
 * dlb2_update_vdev_atomic_inflights() - update the vdev's atomic inflights
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of atomic inflights to assign to this vdev
 *
 * This function assigns num atomic inflights to the specified vdev. If the vdev
 * already has atomic inflights assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of entries, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_atomic_inflights(struct dlb2_hw *hw, u32 id, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	u32 orig;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	/*
	 * Detach the destination VF's current resources before checking
	 * if enough are available.
	 */
	orig = dst->num_avail_aqed_entries;
	src->num_avail_aqed_entries += orig;
	dst->num_avail_aqed_entries = 0;

	if (src->num_avail_aqed_entries < num) {
		src->num_avail_aqed_entries -= orig;
		dst->num_avail_aqed_entries = orig;
		return -EINVAL;
	}

	src->num_avail_aqed_entries -= num;
	dst->num_avail_aqed_entries += num;

	return 0;
}

/**
 * dlb2_update_vdev_sn_slots() - update the vdev's sequence number slots
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @sn_group: Sequence number group ID
 * @num: number of sequence number slots to assign to this vdev
 *
 * This function assigns num sequence number slots to the specified vdev. If
 * the vdev already has sequence number slots assigned, this existing assignment
 * is adjusted accordingly. vdevs are assigned a contiguous chunk of entries,
 * so this function may fail if a sufficiently large contiguous chunk is not
 * available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_sn_slots(struct dlb2_hw *hw, u32 id, u32 sn_group, u32 num)
{
	struct dlb2_function_resources *src, *dst;
	u32 orig;

	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	src = &hw->pf;
	dst = &hw->vdev[id];

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	/*
	 * Detach the destination VF's current resources before checking
	 * if enough are available.
	 */
	orig = dst->num_avail_sn_slots[sn_group];
	src->num_avail_sn_slots[sn_group] += orig;
	dst->num_avail_sn_slots[sn_group] = 0;

	if (src->num_avail_sn_slots[sn_group] < num) {
		src->num_avail_sn_slots[sn_group] -= orig;
		dst->num_avail_sn_slots[sn_group] = orig;
		return -EINVAL;
	}

	src->num_avail_sn_slots[sn_group] -= num;
	dst->num_avail_sn_slots[sn_group] += num;

	return 0;
}

static int dlb2_attach_ldb_queues(struct dlb2_hw *hw,
				  struct dlb2_function_resources *rsrcs,
				  struct dlb2_hw_domain *domain,
				  u32 num_queues,
				  struct dlb2_cmd_response *resp)
{
	unsigned int i;

	if (rsrcs->num_avail_ldb_queues < num_queues) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < num_queues; i++) {
		struct dlb2_ldb_queue *queue;

		queue = DLB2_FUNC_LIST_HEAD(rsrcs->avail_ldb_queues,
					    typeof(*queue));
		if (!queue) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_ldb_queues, &queue->func_list);

		queue->domain_id = domain->id;
		queue->owned = true;

		dlb2_list_add(&domain->avail_ldb_queues, &queue->domain_list);
	}

	rsrcs->num_avail_ldb_queues -= num_queues;

	return 0;
}

static struct dlb2_ldb_port *
dlb2_get_next_ldb_port(struct dlb2_hw *hw,
		       struct dlb2_function_resources *rsrcs,
		       u32 domain_id,
		       u32 cos_id)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;

#if(0)
	/*
	 * To reduce the odds of consecutive load-balanced ports mapping to the
	 * same queue(s), the driver attempts to allocate ports whose neighbors
	 * are owned by a different domain.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[next].owned ||
		    hw->rsrcs.ldb_ports[next].domain_id.phys_id == domain_id)
			continue;

		if (!hw->rsrcs.ldb_ports[prev].owned ||
		    hw->rsrcs.ldb_ports[prev].domain_id.phys_id == domain_id)
			continue;

		return port;
	}

	/*
	 * Failing that, the driver looks for a port with one neighbor owned by
	 * a different domain and the other unallocated.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[prev].owned &&
		    hw->rsrcs.ldb_ports[next].owned &&
		    hw->rsrcs.ldb_ports[next].domain_id.phys_id != domain_id)
			return port;

		if (!hw->rsrcs.ldb_ports[next].owned &&
		    hw->rsrcs.ldb_ports[prev].owned &&
		    hw->rsrcs.ldb_ports[prev].domain_id.phys_id != domain_id)
			return port;
	}

	/*
	 * Failing that, the driver looks for a port with both neighbors
	 * unallocated.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[prev].owned &&
		    !hw->rsrcs.ldb_ports[next].owned)
			return port;
	}
#endif

	/* If all else fails, the driver returns the next available port. */
	return DLB2_FUNC_LIST_HEAD(rsrcs->avail_ldb_ports[cos_id],
				   typeof(*port));
}

static int __dlb2_attach_ldb_ports(struct dlb2_hw *hw,
				   struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_ports,
				   u32 cos_id,
				   struct dlb2_cmd_response *resp)
{
	unsigned int i;

	if (rsrcs->num_avail_ldb_ports[cos_id] < num_ports) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < num_ports; i++) {
		struct dlb2_ldb_port *port;
		int core = domain->probe_core;

		if (core >= 0) {
			int start = cos_id * DLB2_MAX_NUM_LDB_PORTS_PER_COS;

			do {
				int port_id = hw->ldb_pp_allocations[core][start++];

				port = dlb2_get_ldb_port_from_id(hw, port_id, false, 0);
			} while (port && port->owned);
		} else {
			port = dlb2_get_next_ldb_port(hw, rsrcs,
						      domain->id.phys_id, cos_id);
		}

		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_ldb_ports[cos_id],
			      &port->func_list);

		port->domain_id = domain->id;
		port->owned = true;

		dlb2_list_add(&domain->avail_ldb_ports[cos_id],
			      &port->domain_list);
	}

	rsrcs->num_avail_ldb_ports[cos_id] -= num_ports;

	return 0;
}

static int dlb2_attach_ldb_ports(struct dlb2_hw *hw,
				 struct dlb2_function_resources *rsrcs,
				 struct dlb2_hw_domain *domain,
				 struct dlb2_create_sched_domain_args *args,
				 struct dlb2_cmd_response *resp)
{
	struct dlb2_bitmap bmp = {.len = DLB2_MAX_CPU_CORES};
	unsigned int i, j;
	int ret, core = -1;

	if (DLB2_SELECT_PORT(hw, domain)) {
		bmp.map = (unsigned long *)args->core_mask;
		core = dlb2_bitmap_find_nth_set_bit(&bmp, DLB2_DEFAULT_PROBE_CORE);
		if (core >= 0)
			core %= hw->num_phys_cpus;
	}
	domain->probe_core = core;

	if (args->cos_strict) {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			u32 num = args->num_cos_ldb_ports[i];

			/* Allocate ports from specific classes-of-service */
			ret = __dlb2_attach_ldb_ports(hw,
						      rsrcs,
						      domain,
						      num,
						      i,
						      resp);
			if (ret)
				return ret;
		}
	} else {
		unsigned int k;
		u32 cos_id;

		/*
		 * Attempt to allocate from specific class-of-service, but
		 * fallback to the other classes if that fails.
		 */
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			for (j = 0; j < args->num_cos_ldb_ports[i]; j++) {
				for (k = 0; k < DLB2_NUM_COS_DOMAINS; k++) {
					cos_id = (i + k) % DLB2_NUM_COS_DOMAINS;

					ret = __dlb2_attach_ldb_ports(hw,
								      rsrcs,
								      domain,
								      1,
								      cos_id,
								      resp);
					if (ret == 0)
						break;
				}

				if (ret)
					return ret;
			}
		}
	}

	/* Allocate num_ldb_ports from any class-of-service */
	for (i = 0; i < args->num_ldb_ports; i++) {
		for (j = 0; j < DLB2_NUM_COS_DOMAINS; j++) {
			u32 cos_id;
			if (core >= 0) {
				/* Allocate from best performing cos */
				u32 cos_idx = j + DLB2_MAX_NUM_LDB_PORTS;
				cos_id = hw->ldb_pp_allocations[core][cos_idx];
			} else {
				cos_id = j;
			}

			ret = __dlb2_attach_ldb_ports(hw,
						      rsrcs,
						      domain,
						      1,
						      cos_id,
						      resp);
			if (ret == 0)
				break;
		}

		if (ret)
			return ret;
	}

	return 0;
}

static int dlb2_attach_dir_ports(struct dlb2_hw *hw,
				 struct dlb2_function_resources *rsrcs,
				 struct dlb2_hw_domain *domain,
				 struct dlb2_create_sched_domain_args *args,
				 struct dlb2_cmd_response *resp)
{
	struct dlb2_bitmap bmp = {.len = DLB2_MAX_CPU_CORES};
	u32 num_ports = args->num_dir_ports;
	int num_res = 0, i;
	int cpu = 0;

	if (rsrcs->num_avail_dir_pq_pairs < num_ports) {
		resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (DLB2_SELECT_PORT(hw, domain)) {
		bmp.map = (unsigned long *)args->pcore_mask;
		num_res = dlb2_bitmap_count(&bmp);
		if (num_res) {
			cpu = dlb2_bitmap_find_nth_set_bit(&bmp, DLB2_PROD_PROBE_CORE);
		} else {
			bmp.map = (unsigned long *)args->core_mask;
			if (dlb2_bitmap_count(&bmp))
                                cpu = dlb2_bitmap_find_nth_set_bit(&bmp, DLB2_DEFAULT_PROBE_CORE);
                }
                cpu %= hw->num_phys_cpus;
        }

	for (i = 0; i < num_ports; i++) {
		struct dlb2_dir_pq_pair *port;

		if (DLB2_SELECT_PORT(hw, domain)) {
			int cnt = 0;

			do {
				int port_id = hw->dir_pp_allocations[cpu][cnt++];

				port = dlb2_get_dir_pq_from_id(hw, port_id, false, 0);
			} while (port && port->owned);
		} else {
			port = DLB2_FUNC_LIST_HEAD(rsrcs->avail_dir_pq_pairs,
						   typeof(*port));
		}

		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_dir_pq_pairs, &port->func_list);

		port->domain_id = domain->id;
		port->owned = true;
		if (num_res) {
			dlb2_list_add(&domain->rsvd_dir_pq_pairs,
				      &port->domain_list);
			num_res--;
		} else {
			dlb2_list_add(&domain->avail_dir_pq_pairs, &port->domain_list);
		}
	}

	rsrcs->num_avail_dir_pq_pairs -= num_ports;

	return 0;
}

static int dlb2_attach_ldb_credits(struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_credits,
				   struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_qed_entries < num_credits) {
		resp->status = DLB2_ST_LDB_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_qed_entries -= num_credits;
	domain->num_ldb_credits += num_credits;
	return 0;
}

static int dlb2_attach_dir_credits(struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_credits,
				   struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_dqed_entries < num_credits) {
		resp->status = DLB2_ST_DIR_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_dqed_entries -= num_credits;
	domain->num_dir_credits += num_credits;
	return 0;
}

static int dlb2_attach_atomic_inflights(struct dlb2_function_resources *rsrcs,
					struct dlb2_hw_domain *domain,
					u32 num_atomic_inflights,
					struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_aqed_entries < num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_aqed_entries -= num_atomic_inflights;
	domain->num_avail_aqed_entries += num_atomic_inflights;
	return 0;
}

static int
dlb2_attach_domain_hist_list_entries(struct dlb2_function_resources *rsrcs,
				     struct dlb2_hw_domain *domain,
				     u32 num_hist_list_entries,
				     struct dlb2_cmd_response *resp)
{
	struct dlb2_bitmap *bitmap;
	int base;

	if (num_hist_list_entries) {
		bitmap = rsrcs->avail_hist_list_entries;

		base = dlb2_bitmap_find_set_bit_range(bitmap,
						      num_hist_list_entries);
		if (base < 0)
			goto error;

		domain->total_hist_list_entries = num_hist_list_entries;
		domain->avail_hist_list_entries = num_hist_list_entries;

		domain->hist_list_entry_base = base;
		domain->hist_list_entry_offset = 0;

		dlb2_bitmap_clear_range(bitmap, base, num_hist_list_entries);
	}
	return 0;

error:
	resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
	return -EINVAL;
}

static int dlb2_attach_sn_slots(struct dlb2_hw *hw,
				struct dlb2_function_resources *rsrcs,
				struct dlb2_hw_domain *domain,
				u32 *sn_slots,
				struct dlb2_cmd_response *resp)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		if (rsrcs->num_avail_sn_slots[i] < sn_slots[i]) {
			resp->status = DLB2_ST_SN_SLOTS_UNAVAILABLE;
			return -EINVAL;
		}

		rsrcs->num_avail_sn_slots[i] -= sn_slots[i];
		domain->num_avail_sn_slots[i] += sn_slots[i];
	}

	return 0;
}

static int
dlb2_verify_create_sched_dom_args(struct dlb2_function_resources *rsrcs,
				  struct dlb2_create_sched_domain_args *args,
				  struct dlb2_cmd_response *resp,
				  struct dlb2_hw_domain **out_domain)
{
	u32 num_avail_ldb_ports, req_ldb_ports;
	struct dlb2_bitmap *avail_hl_entries;
	unsigned int max_contig_hl_range;
	struct dlb2_hw_domain *domain;
	int i;

	avail_hl_entries = rsrcs->avail_hist_list_entries;

	max_contig_hl_range = dlb2_bitmap_longest_set_range(avail_hl_entries);

	num_avail_ldb_ports = 0;
	req_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		num_avail_ldb_ports += rsrcs->num_avail_ldb_ports[i];

		req_ldb_ports += args->num_cos_ldb_ports[i];
	}

	req_ldb_ports += args->num_ldb_ports;

	if (rsrcs->num_avail_domains < 1) {
		resp->status = DLB2_ST_DOMAIN_UNAVAILABLE;
		return -EINVAL;
	}

	domain = DLB2_FUNC_LIST_HEAD(rsrcs->avail_domains, typeof(*domain));
	if (!domain) {
		resp->status = DLB2_ST_DOMAIN_UNAVAILABLE;
		return -EFAULT;
	}

	if (rsrcs->num_avail_ldb_queues < args->num_ldb_queues) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	if (req_ldb_ports > num_avail_ldb_ports) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; args->cos_strict && i < DLB2_NUM_COS_DOMAINS; i++) {
		if (args->num_cos_ldb_ports[i] >
		    rsrcs->num_avail_ldb_ports[i]) {
			resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	if (args->num_ldb_queues > 0 && req_ldb_ports == 0) {
		resp->status = DLB2_ST_LDB_PORT_REQUIRED_FOR_LDB_QUEUES;
		return -EINVAL;
	}

	if (rsrcs->num_avail_dir_pq_pairs < args->num_dir_ports) {
		resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_qed_entries < args->num_ldb_credits) {
		resp->status = DLB2_ST_LDB_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_dqed_entries < args->num_dir_credits) {
		resp->status = DLB2_ST_DIR_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_aqed_entries < args->num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (max_contig_hl_range < args->num_hist_list_entries) {
		resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		if (rsrcs->num_avail_sn_slots[i] < args->num_sn_slots[i]) {
			resp->status = DLB2_ST_SN_SLOTS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	*out_domain = domain;

	return 0;
}

static int
dlb2_verify_create_ldb_queue_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_create_ldb_queue_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_ldb_queue **out_queue)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	int i;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (domain->started) {
		resp->status = DLB2_ST_DOMAIN_STARTED;
		return -EINVAL;
	}

	queue = DLB2_DOM_LIST_HEAD(domain->avail_ldb_queues, typeof(*queue));
	if (!queue) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	if (args->num_sequence_numbers) {
		for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
			struct dlb2_sn_group *group = &hw->rsrcs.sn_groups[i];

			if (group->sequence_numbers_per_queue ==
			    args->num_sequence_numbers &&
			    domain->num_avail_sn_slots[i] > 0 &&
			    !dlb2_sn_group_full(group))
				break;
		}

		if (i == DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS) {
			resp->status = DLB2_ST_SEQUENCE_NUMBERS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	if (args->num_qid_inflights < 1 ||
	    args->num_qid_inflights > DLB2_MAX_NUM_QID_INFLIGHTS) {
		resp->status = DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION;
		return -EINVAL;
	}

	/* Inflights must be <= number of sequence numbers if ordered */
	if (args->num_sequence_numbers != 0 &&
	    args->num_qid_inflights > args->num_sequence_numbers) {
		resp->status = DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION;
		return -EINVAL;
	}

	if (domain->num_avail_aqed_entries < args->num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (args->num_atomic_inflights &&
	    args->lock_id_comp_level != 0 &&
	    args->lock_id_comp_level != 64 &&
	    args->lock_id_comp_level != 128 &&
	    args->lock_id_comp_level != 256 &&
	    args->lock_id_comp_level != 512 &&
	    args->lock_id_comp_level != 1024 &&
	    args->lock_id_comp_level != 2048 &&
	    args->lock_id_comp_level != 4096 &&
	    args->lock_id_comp_level != 65536) {
		resp->status = DLB2_ST_INVALID_LOCK_ID_COMP_LEVEL;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_queue = queue;

	return 0;
}

static int
dlb2_create_dir_pq(struct dlb2_hw *hw,
		   u32 domain_id,
		   int pq_id,
		   bool is_port,
		   bool is_producer,
		   struct dlb2_cmd_response *resp,
		   bool vdev_req,
		   unsigned int vdev_id,
		   struct dlb2_hw_domain **out_domain,
		   struct dlb2_dir_pq_pair **out_pq)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_dir_pq_pair *pq;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (domain->started) {
		resp->status = DLB2_ST_DOMAIN_STARTED;
		return -EINVAL;
	}

	if (pq_id != -1) {
		/*
		 * If the user claims the queue is already configured, validate
		 * the queue ID, its domain, and whether the queue is
		 * configured.
		 */
		pq = dlb2_get_domain_used_dir_pq(hw,
						 pq_id,
						 vdev_req,
						 domain);

		if (!pq || pq->domain_id.phys_id != domain->id.phys_id ||
		    (is_port && !pq->queue_configured) ||
		    (!is_port && !pq->port_configured)) {
			resp->status = is_port ? DLB2_ST_INVALID_DIR_QUEUE_ID : DLB2_ST_INVALID_PORT_ID;
			return -EINVAL;
		}
	} else {
		/*
		 * If the port's queue is not configured, validate that a free
		 * port-queue pair is available.
		 *
		 * First try the 'res' list if the port is producer OR if
		 * 'avail' list is empty else fall back to 'avail' list
		 */
		if (!dlb2_list_empty(&domain->rsvd_dir_pq_pairs) &&
		    (is_producer || dlb2_list_empty(&domain->avail_dir_pq_pairs)))
			pq = DLB2_DOM_LIST_HEAD(domain->rsvd_dir_pq_pairs,
						typeof(*pq));
		else
			pq = DLB2_DOM_LIST_HEAD(domain->avail_dir_pq_pairs,
						typeof(*pq));
		if (!pq) {
			resp->status = is_port ? DLB2_ST_DIR_PORTS_UNAVAILABLE : DLB2_ST_DIR_QUEUES_UNAVAILABLE;
			return -EINVAL;
		}
	}

	*out_domain = domain;
	*out_pq = pq;

	return 0;
}

static int
dlb2_verify_create_dir_queue_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_create_dir_queue_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_dir_pq_pair **out_queue)
{
	return dlb2_create_dir_pq(hw, domain_id, args->port_id, false, false,
				  resp, vdev_req, vdev_id, out_domain, out_queue);
}

static void dlb2_configure_ldb_queue(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     struct dlb2_ldb_queue *queue,
				     struct dlb2_create_ldb_queue_args *args,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	struct dlb2_sn_group *sn_group;
	unsigned int offs;
	u32 reg = 0;
	u32 alimit;

	/* QID write permissions are turned on when the domain is started */
	offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES + queue->id.phys_id;

	DLB2_CSR_WR(hw, SYS_LDB_VASQID_V(offs), reg);

	/*
	 * Unordered QIDs get 4K inflights, ordered get as many as the number
	 * of sequence numbers.
	 */
	BITS_SET(reg, args->num_qid_inflights, LSP_QID_LDB_INFL_LIM_LIMIT);
	DLB2_CSR_WR(hw, LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id), reg);

	alimit = queue->aqed_limit;

	if (alimit > DLB2_MAX_NUM_AQED_ENTRIES)
		alimit = DLB2_MAX_NUM_AQED_ENTRIES;

	reg = 0;
	BITS_SET(reg, alimit, LSP_QID_AQED_ACTIVE_LIM_LIMIT);
	DLB2_CSR_WR(hw, LSP_QID_AQED_ACTIVE_LIM(hw->ver, queue->id.phys_id), reg);

	reg = 0;
	switch (args->lock_id_comp_level) {
	case 64:
		BITS_SET(reg, 1, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 128:
		BITS_SET(reg, 2, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 256:
		BITS_SET(reg, 3, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 512:
		BITS_SET(reg, 4, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 1024:
		BITS_SET(reg, 5, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 2048:
		BITS_SET(reg, 6, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 4096:
		BITS_SET(reg, 7, AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	default:
		/* No compression by default */
		break;
	}

	DLB2_CSR_WR(hw, AQED_QID_HID_WIDTH(queue->id.phys_id), reg);

	reg = 0;
	/* Don't timestamp QEs that pass through this queue */
	DLB2_CSR_WR(hw, SYS_LDB_QID_ITS(queue->id.phys_id), reg);

	BITS_SET(reg, args->depth_threshold, LSP_QID_ATM_DEPTH_THRSH_THRESH(hw->ver));
	DLB2_CSR_WR(hw, LSP_QID_ATM_DEPTH_THRSH(hw->ver, queue->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, args->depth_threshold, LSP_QID_NALDB_DEPTH_THRSH_THRESH(hw->ver));
	DLB2_CSR_WR(hw, LSP_QID_NALDB_DEPTH_THRSH(hw->ver, queue->id.phys_id), reg);

	/*
	 * This register limits the number of inflight flows a queue can have
	 * at one time.  It has an upper bound of 2048, but can be
	 * over-subscribed. 512 is chosen so that a single queue doesn't use
	 * the entire atomic storage, but can use a substantial portion if
	 * needed.
	 */
	reg = 0;
	BITS_SET(reg, 512, AQED_QID_FID_LIM_QID_FID_LIMIT);
	DLB2_CSR_WR(hw, AQED_QID_FID_LIM(queue->id.phys_id), reg);

	/* Configure SNs */
	reg = 0;
	sn_group = &hw->rsrcs.sn_groups[queue->sn_group];
	BITS_SET(reg, sn_group->mode, CHP_ORD_QID_SN_MAP_MODE);
	BITS_SET(reg, queue->sn_slot, CHP_ORD_QID_SN_MAP_SLOT);
	BITS_SET(reg, sn_group->id, CHP_ORD_QID_SN_MAP_GRP);

	DLB2_CSR_WR(hw, CHP_ORD_QID_SN_MAP(hw->ver, queue->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, (args->num_sequence_numbers != 0),
		 SYS_LDB_QID_CFG_V_SN_CFG_V);
	BITS_SET(reg, (args->num_atomic_inflights != 0),
		 SYS_LDB_QID_CFG_V_FID_CFG_V);

	DLB2_CSR_WR(hw, SYS_LDB_QID_CFG_V(queue->id.phys_id), reg);

	if (vdev_req) {
		offs = vdev_id * DLB2_MAX_NUM_LDB_QUEUES + queue->id.virt_id;

		reg = 0;
		BIT_SET(reg, SYS_VF_LDB_VQID_V_VQID_V);
		DLB2_CSR_WR(hw, SYS_VF_LDB_VQID_V(offs), reg);

		reg = 0;
		BITS_SET(reg, queue->id.phys_id, SYS_VF_LDB_VQID2QID_QID);
		DLB2_CSR_WR(hw, SYS_VF_LDB_VQID2QID(offs), reg);

		reg = 0;
		BITS_SET(reg, queue->id.virt_id, SYS_LDB_QID2VQID_VQID);
		DLB2_CSR_WR(hw, SYS_LDB_QID2VQID(queue->id.phys_id), reg);
	}

	reg = 0;
	BIT_SET(reg, SYS_LDB_QID_V_QID_V);
	DLB2_CSR_WR(hw, SYS_LDB_QID_V(queue->id.phys_id), reg);
}

static void dlb2_configure_dir_queue(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     struct dlb2_dir_pq_pair *queue,
				     struct dlb2_create_dir_queue_args *args,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	unsigned int offs;
	u32 reg = 0;

	/* QID write permissions are turned on when the domain is started */
	offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_QUEUES(hw->ver) +
		queue->id.phys_id;

	DLB2_CSR_WR(hw, SYS_DIR_VASQID_V(offs), reg);

	/* Don't timestamp QEs that pass through this queue */
	DLB2_CSR_WR(hw, SYS_DIR_QID_ITS(queue->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, args->depth_threshold, LSP_QID_DIR_DEPTH_THRSH_THRESH(hw->ver));
	DLB2_CSR_WR(hw, LSP_QID_DIR_DEPTH_THRSH(hw->ver, queue->id.phys_id), reg);

	if (vdev_req) {
		offs = vdev_id * DLB2_MAX_NUM_DIR_QUEUES(hw->ver) + queue->id.virt_id;

		reg = 0;
		BIT_SET(reg, SYS_VF_DIR_VQID_V_VQID_V);
		DLB2_CSR_WR(hw, SYS_VF_DIR_VQID_V(offs), reg);

		reg = 0;
		BITS_SET(reg, queue->id.phys_id, SYS_VF_DIR_VQID2QID_QID(hw->ver));
		DLB2_CSR_WR(hw, SYS_VF_DIR_VQID2QID(offs), reg);
	}

	reg = 0;
	BIT_SET(reg, SYS_DIR_QID_V_QID_V);
	DLB2_CSR_WR(hw, SYS_DIR_QID_V(queue->id.phys_id), reg);

	queue->queue_configured = true;
}

static bool
dlb2_cq_depth_is_valid(u32 depth)
{
	if (depth != 1 && depth != 2 &&
	    depth != 4 && depth != 8 &&
	    depth != 16 && depth != 32 &&
	    depth != 64 && depth != 128 &&
	    depth != 256 && depth != 512 &&
	    depth != 1024)
		return false;

	return true;
}

static int
dlb2_verify_create_ldb_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 uintptr_t cq_dma_base,
				 struct dlb2_create_ldb_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_ldb_port **out_port,
				 int *out_cos_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int i, id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (domain->started) {
		resp->status = DLB2_ST_DOMAIN_STARTED;
		return -EINVAL;
	}

	if (args->cos_id >= DLB2_NUM_COS_DOMAINS &&
	    (args->cos_id != DLB2_COS_DEFAULT || args->cos_strict)) {
		resp->status = DLB2_ST_INVALID_COS_ID;
		return -EINVAL;
	}

	if (args->cos_strict) {
		id = args->cos_id;
		port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[id],
					  typeof(*port));
	} else {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			int core = domain->probe_core;
			u32 cos_idx = i;
			if (args->cos_id == DLB2_COS_DEFAULT && core >= 0) {
				/* Allocate from best performing cos */
				cos_idx += DLB2_MAX_NUM_LDB_PORTS;
				id = hw->ldb_pp_allocations[core][cos_idx];
			} else {
				if (args->cos_id != DLB2_COS_DEFAULT)
					cos_idx += args->cos_id;
				id = cos_idx % DLB2_NUM_COS_DOMAINS;
			}

			port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[id],
						  typeof(*port));
			if (port)
				break;
		}
	}

	if (!port) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (!dlb2_cq_depth_is_valid(args->cq_depth)) {
		resp->status = DLB2_ST_INVALID_CQ_DEPTH;
		return -EINVAL;
	}

	/* The history list size must be >= 1 */
	if (!args->cq_history_list_size) {
		resp->status = DLB2_ST_INVALID_HIST_LIST_DEPTH;
		return -EINVAL;
	}

	if (args->cq_history_list_size > domain->avail_hist_list_entries) {
		resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = port;
	*out_cos_id = id;

	return 0;
}

static int
dlb2_verify_create_dir_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 uintptr_t cq_dma_base,
				 struct dlb2_create_dir_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_dir_pq_pair **out_port)
{
	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (!dlb2_cq_depth_is_valid(args->cq_depth)) {
		resp->status = DLB2_ST_INVALID_CQ_DEPTH;
		return -EINVAL;
	}

	return dlb2_create_dir_pq(hw, domain_id, args->queue_id, true, args->is_producer,
				  resp, vdev_req, vdev_id, out_domain, out_port);
}

static int dlb2_verify_start_stop_domain_args(struct dlb2_hw *hw,
					      u32 domain_id,
					      bool start_domain,
					      struct dlb2_cmd_response *resp,
					      bool vdev_req,
					      unsigned int vdev_id,
					      struct dlb2_hw_domain **out_domain)
{
	struct dlb2_hw_domain *domain;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (!(domain->started ^ start_domain)) {
		resp->status = start_domain ? DLB2_ST_DOMAIN_STARTED : DLB2_ST_DOMAIN_NOT_STARTED;
		return -EINVAL;
	}

	*out_domain = domain;

	return 0;
}

static int dlb2_verify_map_qid_args(struct dlb2_hw *hw,
				    u32 domain_id,
				    struct dlb2_map_qid_args *args,
				    struct dlb2_cmd_response *resp,
				    bool vdev_req,
				    unsigned int vdev_id,
				    struct dlb2_hw_domain **out_domain,
				    struct dlb2_ldb_port **out_port,
				    struct dlb2_ldb_queue **out_queue)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (args->priority >= DLB2_QID_PRIORITIES) {
		resp->status = DLB2_ST_INVALID_PRIORITY;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (!queue || !queue->configured) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	if (queue->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	if (port->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_queue = queue;
	*out_port = port;

	return 0;
}

static bool dlb2_port_find_slot(struct dlb2_ldb_port *port,
				enum dlb2_qid_map_state state,
				int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (port->qid_map[i].state == state)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

bool dlb2_port_find_slot_queue(struct dlb2_ldb_port *port,
			       enum dlb2_qid_map_state state,
			       struct dlb2_ldb_queue *queue,
			       int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (port->qid_map[i].state == state &&
		    port->qid_map[i].qid == queue->id.phys_id)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

static bool
dlb2_port_find_slot_with_pending_map_queue(struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		struct dlb2_ldb_port_qid_map *map = &port->qid_map[i];

		if (map->state == DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP &&
		    map->pending_qid == queue->id.phys_id)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

static int dlb2_port_slot_state_transition(struct dlb2_hw *hw,
					   struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int slot,
					   enum dlb2_qid_map_state new_state)
{
	enum dlb2_qid_map_state curr_state = port->qid_map[slot].state;
	struct dlb2_hw_domain *domain;
	int domain_id;

	domain_id = port->domain_id.phys_id;

	domain = dlb2_get_domain_from_id(hw, domain_id, false, 0);
	if (!domain) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: unable to find domain %d\n",
			    __func__, domain_id);
		return -EINVAL;
	}

	switch (curr_state) {
	case DLB2_QUEUE_UNMAPPED:
		switch (new_state) {
		case DLB2_QUEUE_MAPPED:
			queue->num_mappings++;
			port->num_mappings++;
			break;
		case DLB2_QUEUE_MAP_IN_PROG:
			queue->num_pending_additions++;
			domain->num_pending_additions++;
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_MAPPED:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			queue->num_mappings--;
			port->num_mappings--;
			break;
		case DLB2_QUEUE_UNMAP_IN_PROG:
			port->num_pending_removals++;
			domain->num_pending_removals++;
			break;
		case DLB2_QUEUE_MAPPED:
			/* Priority change, nothing to update */
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_MAP_IN_PROG:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			queue->num_pending_additions--;
			domain->num_pending_additions--;
			break;
		case DLB2_QUEUE_MAPPED:
			queue->num_mappings++;
			port->num_mappings++;
			queue->num_pending_additions--;
			domain->num_pending_additions--;
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_UNMAP_IN_PROG:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			port->num_pending_removals--;
			domain->num_pending_removals--;
			queue->num_mappings--;
			port->num_mappings--;
			break;
		case DLB2_QUEUE_MAPPED:
			port->num_pending_removals--;
			domain->num_pending_removals--;
			break;
		case DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP:
			/* Nothing to update */
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP:
		switch (new_state) {
		case DLB2_QUEUE_UNMAP_IN_PROG:
			/* Nothing to update */
			break;
		case DLB2_QUEUE_UNMAPPED:
			/*
			 * An UNMAP_IN_PROG_PENDING_MAP slot briefly
			 * becomes UNMAPPED before it transitions to
			 * MAP_IN_PROG.
			 */
			queue->num_mappings--;
			port->num_mappings--;
			port->num_pending_removals--;
			domain->num_pending_removals--;
			break;
		default:
			goto error;
		}
		break;
	default:
		goto error;
	}

	port->qid_map[slot].state = new_state;

	DLB2_HW_DBG(hw,
		    "[%s()] queue %d -> port %d state transition (%d -> %d)\n",
		    __func__, queue->id.phys_id, port->id.phys_id,
		    curr_state, new_state);
	return 0;

error:
	DLB2_HW_ERR(hw,
		    "[%s()] Internal error: invalid queue %d -> port %d state transition (%d -> %d)\n",
		    __func__, queue->id.phys_id, port->id.phys_id,
		    curr_state, new_state);
	return -EFAULT;
}

static int dlb2_verify_map_qid_slot_available(struct dlb2_ldb_port *port,
					      struct dlb2_ldb_queue *queue,
					      struct dlb2_cmd_response *resp)
{
	enum dlb2_qid_map_state state;
	int i;

	/* Unused slot available? */
	if (port->num_mappings < DLB2_MAX_NUM_QIDS_PER_LDB_CQ)
		return 0;

	/*
	 * If the queue is already mapped (from the application's perspective),
	 * this is simply a priority update.
	 */
	state = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, state, queue, &i))
		return 0;

	state = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, state, queue, &i))
		return 0;

	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i))
		return 0;

	/*
	 * If the slot contains an unmap in progress, it's considered
	 * available.
	 */
	state = DLB2_QUEUE_UNMAP_IN_PROG;
	if (dlb2_port_find_slot(port, state, &i))
		return 0;

	state = DLB2_QUEUE_UNMAPPED;
	if (dlb2_port_find_slot(port, state, &i))
		return 0;

	resp->status = DLB2_ST_NO_QID_SLOTS_AVAILABLE;
	return -EINVAL;
}

static int dlb2_verify_unmap_qid_args(struct dlb2_hw *hw,
				      u32 domain_id,
				      struct dlb2_unmap_qid_args *args,
				      struct dlb2_cmd_response *resp,
				      bool vdev_req,
				      unsigned int vdev_id,
				      struct dlb2_hw_domain **out_domain,
				      struct dlb2_ldb_port **out_port,
				      struct dlb2_ldb_queue **out_queue)
{
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	int slot;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (port->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (!queue || !queue->configured) {
		DLB2_HW_ERR(hw, "[%s()] Can't unmap unconfigured queue %d\n",
			    __func__, args->qid);
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	/*
	 * Verify that the port has the queue mapped. From the application's
	 * perspective a queue is mapped if it is actually mapped, the map is
	 * in progress, or the map is blocked pending an unmap.
	 */
	state = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, state, queue, &slot))
		goto done;

	state = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, state, queue, &slot))
		goto done;

	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &slot))
		goto done;

	resp->status = DLB2_ST_INVALID_QID;
	return -EINVAL;

done:
	*out_domain = domain;
	*out_port = port;
	*out_queue = queue;

	return 0;
}

static int
dlb2_verify_enable_ldb_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 struct dlb2_enable_ldb_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_ldb_port **out_port)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = port;

	return 0;
}

static int
dlb2_verify_enable_dir_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 struct dlb2_enable_dir_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_dir_pq_pair **out_port)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_dir_pq_pair *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_dir_pq(hw, id, vdev_req, domain);

	if (!port || !port->port_configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = port;

	return 0;
}

static int
dlb2_verify_disable_ldb_port_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_disable_ldb_port_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_ldb_port **out_port)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = port;

	return 0;
}

static int
dlb2_verify_disable_dir_port_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_disable_dir_port_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_dir_pq_pair **out_port)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_dir_pq_pair *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_dir_pq(hw, id, vdev_req, domain);

	if (!port || !port->port_configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = port;

	return 0;
}

static void dlb2_configure_domain_credits_v2(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain)
{
	u32 reg = 0;

	BITS_SET(reg, domain->num_ldb_credits, CHP_CFG_LDB_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_VAS_CRD(domain->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, domain->num_dir_credits, CHP_CFG_DIR_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, CHP_CFG_DIR_VAS_CRD(domain->id.phys_id), reg);
}

static void dlb2_configure_domain_credits_v2_5(struct dlb2_hw *hw,
					       struct dlb2_hw_domain *domain)
{
	u32 reg = 0;

	BITS_SET(reg, domain->num_ldb_credits, CHP_CFG_LDB_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, CHP_CFG_VAS_CRD(domain->id.phys_id), reg);
}

static void dlb2_configure_domain_credits(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	if (hw->ver == DLB2_HW_V2)
		dlb2_configure_domain_credits_v2(hw, domain);
	else
		dlb2_configure_domain_credits_v2_5(hw, domain);
}

static int dlb2_pp_profile(struct dlb2_hw *hw, int port, bool is_ldb)
{
	struct dlb2_hcw hcw_mem[DLB2_HCW_MEM_SIZE], *hcw;
	u64 cycle_start = 0ULL, cycle_end = 0ULL;
	void __iomem *pp_addr;
	int i;

	pp_addr = os_map_producer_port(hw, port, is_ldb);

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[DLB2_HCW_64B_OFF] &
				  ~DLB2_HCW_ALIGN_MASK);

	/*
	 * Program the first HCW for a completion and token return and
	 * the other HCWs as NOOPS
	 */

	memset(hcw, 0, (DLB2_HCW_MEM_SIZE - DLB2_HCW_64B_OFF) * sizeof(*hcw));
	hcw->qe_comp = 1;
	hcw->cq_token = 1;
	hcw->lock_id = 1;

	cycle_start = get_cycles();
	for (i = 0; i < DLB2_NUM_PROBE_ENQS; i++)
		os_enqueue_four_hcws(hw, hcw, pp_addr);

	cycle_end = get_cycles();

	os_unmap_producer_port(hw, pp_addr);
	return (int)(cycle_end - cycle_start);
}

static int dlb2_pp_cycle_comp(const void *a, const void *b)
{
	const struct dlb2_pp_thread_data *x = a;
	const struct dlb2_pp_thread_data *y = b;

	return x->cycles - y->cycles;
}

/* Probe producer ports from different CPU cores */
static void
dlb2_get_pp_allocation(struct dlb2_hw *hw, int cpu, enum dlb2_port_type port_type)
{
	struct dlb2_pp_thread_data dlb2_thread_data[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	struct dlb2_pp_thread_data cos_cycles[DLB2_NUM_COS_DOMAINS];
	int num_ports_per_sort, num_ports, num_sort, i;
	bool is_ldb = (port_type == LDB);
	int *port_allocations;

	if (is_ldb) {
		port_allocations = hw->ldb_pp_allocations[cpu];
		num_ports = DLB2_MAX_NUM_LDB_PORTS;
		num_sort = DLB2_NUM_COS_DOMAINS;
	} else {
		port_allocations = hw->dir_pp_allocations[cpu];
		num_ports = DLB2_MAX_NUM_DIR_PORTS(hw->ver);
		num_sort = 1;
	}
	num_ports_per_sort = num_ports / num_sort;

	DLB2_HW_DBG(hw, " for %s: cpu core used in pp profiling: %d\n",
		    is_ldb ? "LDB" : "DIR", cpu);

	memset(cos_cycles, 0, num_sort * sizeof(struct dlb2_pp_thread_data));
	for (i = 0; i < num_ports; i++) {
		int cos = (i >> DLB2_NUM_COS_DOMAINS) % DLB2_NUM_COS_DOMAINS;

		dlb2_thread_data[i].pp = i;
		dlb2_thread_data[i].cycles = dlb2_pp_profile(hw, i, is_ldb);
		if (is_ldb)
			cos_cycles[cos].cycles += dlb2_thread_data[i].cycles;

		if ((i + 1) % num_ports_per_sort == 0) {
			int index = 0;

			if (is_ldb) {
				cos_cycles[cos].pp = cos;
				index = cos * num_ports_per_sort;
			}
			/*
			 * For LDB ports first sort with in a cos. Later sort
			 * the best cos based on total cycles for the cos.
			 * For DIR ports, there is a single sort across all
			 * ports.
			 */
			sort(&dlb2_thread_data[index], num_ports_per_sort,
			      sizeof(struct dlb2_pp_thread_data),
			      dlb2_pp_cycle_comp, NULL);
		}
	}

	/*
	 * Sort by best cos aggregated over all ports per cos
	 * Note: After DLB2_MAX_NUM_LDB_PORTS sorted cos is stored and so'pp'
	 * is cos_id and not port id.
	 */
	if (is_ldb) {
		sort(cos_cycles, num_sort, sizeof(struct dlb2_pp_thread_data),
		     dlb2_pp_cycle_comp, NULL);
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
			port_allocations[i + DLB2_MAX_NUM_LDB_PORTS] = cos_cycles[i].pp;
	}

	for (i = 0; i < num_ports; i++) {
		port_allocations[i] = dlb2_thread_data[i].pp;
		DLB2_HW_DBG(hw, " cpu %d: pp %d cycles %d", cpu, port_allocations[i],
			    dlb2_thread_data[i].cycles);
	}
}

static int dlb2_pp_probe_func(void *data)
{
	int cpu = smp_processor_id();
	struct dlb2_hw *hw = data;

	dlb2_get_pp_allocation(hw, cpu, LDB);
	dlb2_get_pp_allocation(hw, cpu, DIR);

	if (probe_level == DLB2_PROBE_SLOW || cpu == hw->num_phys_cpus - 1)
		complete(&dlb_pp_comp);

	return 0;
}

#define DLB2_ALLOC_CHECK(ptr, size)                                                         \
	{                                                                                   \
		if ((ptr = kzalloc(size, GFP_KERNEL)) == NULL)                              \
		{                                                                           \
			printk("Failed to allocate memory of size:%ld \n", (long int)size); \
			return -ENOMEM;                                                     \
		}                                                                           \
	}

static int
dlb2_get_num_phy_cpus(void)
{
	struct cpuinfo_x86 *info = &cpu_data(num_online_cpus() - 1);
	/*
	 * We really should not put kernel version dependent code here.
	 * Todo: move it to dlb2_osdep.h
	 */
#if KERNEL_VERSION(6, 7, 0) >  LINUX_VERSION_CODE
	bool ht = info->cpu_core_id != info->cpu_index;
#else
	bool ht = info->topo.core_id != info->cpu_index;
#endif
	return num_online_cpus() >> ht;
}

int
dlb2_resource_probe(struct dlb2_hw *hw, const void *probe_args)
{
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);
	int cpu, ldb_alloc_size, dir_alloc_size;
	struct task_struct *ts;

	hw->probe_done = false;

	probe_level = dlb2_port_probe(dlb2);
	if (probe_level == DLB2_NO_PROBE)
		return 0;

	hw->num_phys_cpus = dlb2_get_num_phy_cpus();
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

	DLB2_ALLOC_CHECK(hw->ldb_pp_allocations, hw->num_phys_cpus * sizeof(int *));
	DLB2_ALLOC_CHECK(hw->dir_pp_allocations, hw->num_phys_cpus * sizeof(int *));

	hw->ver = dlb2->hw_ver;
	/* After DLB2_MAX_NUM_LDB_PORTS cos order is stored*/
	ldb_alloc_size = (DLB2_MAX_NUM_LDB_PORTS + DLB2_NUM_COS_DOMAINS) * sizeof(int);
	dir_alloc_size = DLB2_MAX_NUM_DIR_PORTS(hw->ver) * sizeof(int);

	for (cpu = 0; cpu < hw->num_phys_cpus; cpu++) {
		DLB2_ALLOC_CHECK(hw->ldb_pp_allocations[cpu], ldb_alloc_size);
		DLB2_ALLOC_CHECK(hw->dir_pp_allocations[cpu], dir_alloc_size);
		ts = kthread_create(&dlb2_pp_probe_func, hw, "%s", "kth");
		if (!ts) {
			DLB2_HW_ERR(hw, ": thread creation failed!");
			return 0;
		}
		kthread_bind(ts, cpu);
		wake_up_process(ts);
		if (probe_level == DLB2_PROBE_FAST && cpu < hw->num_phys_cpus - 1)
			mdelay(1);
		else
			wait_for_completion(&dlb_pp_comp);
	}

	hw->probe_done = true;
	dev_info(&dlb2->pdev->dev, "Probing done\n");
	
	return 0;
}

static int
dlb2_domain_attach_resources(struct dlb2_hw *hw,
			     struct dlb2_function_resources *rsrcs,
			     struct dlb2_hw_domain *domain,
			     struct dlb2_create_sched_domain_args *args,
			     struct dlb2_cmd_response *resp)
{
	int ret;

	ret = dlb2_attach_ldb_queues(hw,
				     rsrcs,
				     domain,
				     args->num_ldb_queues,
				     resp);
	if (ret)
		return ret;

	ret = dlb2_attach_ldb_ports(hw,
				    rsrcs,
				    domain,
				    args,
				    resp);
	if (ret)
		return ret;

	ret = dlb2_attach_dir_ports(hw,
				    rsrcs,
				    domain,
				    args,
				    resp);
	if (ret)
		return ret;

	ret = dlb2_attach_ldb_credits(rsrcs,
				      domain,
				      args->num_ldb_credits,
				      resp);
	if (ret)
		return ret;

	ret = dlb2_attach_dir_credits(rsrcs,
				      domain,
				      args->num_dir_credits,
				      resp);
	if (ret)
		return ret;

	ret = dlb2_attach_domain_hist_list_entries(rsrcs,
						   domain,
						   args->num_hist_list_entries,
						   resp);
	if (ret)
		return ret;

	ret = dlb2_attach_atomic_inflights(rsrcs,
					   domain,
					   args->num_atomic_inflights,
					   resp);
	if (ret)
		return ret;

	ret = dlb2_attach_sn_slots(hw,
				   rsrcs,
				   domain,
				   args->num_sn_slots,
				   resp);
	if (ret)
		return ret;

	dlb2_configure_domain_credits(hw, domain);

	domain->configured = true;

	domain->started = false;

	rsrcs->num_avail_domains--;

	return 0;
}

static int
dlb2_ldb_queue_attach_to_sn_group(struct dlb2_hw *hw,
				  struct dlb2_hw_domain *domain,
				  struct dlb2_ldb_queue *queue,
				  struct dlb2_create_ldb_queue_args *args)
{
	int slot = -1;
	int i;

	queue->sn_cfg_valid = false;

	if (args->num_sequence_numbers == 0)
		return 0;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		struct dlb2_sn_group *group = &hw->rsrcs.sn_groups[i];

		if (group->sequence_numbers_per_queue ==
		    args->num_sequence_numbers &&
		    domain->num_avail_sn_slots[i] > 0 &&
		    !dlb2_sn_group_full(group)) {
			slot = dlb2_sn_group_alloc_slot(group);
			if (slot >= 0)
				break;
		}
	}

	if (slot == -1) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no sequence number slots available\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	domain->num_avail_sn_slots[i] -= 1;
	domain->num_used_sn_slots[i] += 1;
	queue->sn_cfg_valid = true;
	queue->sn_group = i;
	queue->sn_slot = slot;
	return 0;
}

static int
dlb2_ldb_queue_attach_resources(struct dlb2_hw *hw,
				struct dlb2_hw_domain *domain,
				struct dlb2_ldb_queue *queue,
				struct dlb2_create_ldb_queue_args *args)
{
	int ret;

	ret = dlb2_ldb_queue_attach_to_sn_group(hw, domain, queue, args);
	if (ret)
		return ret;

	/* Attach QID inflights */
	queue->num_qid_inflights = args->num_qid_inflights;

	/* Attach atomic inflights */
	queue->aqed_limit = args->num_atomic_inflights;

	domain->num_avail_aqed_entries -= args->num_atomic_inflights;
	domain->num_used_aqed_entries += args->num_atomic_inflights;

	return 0;
}

void dlb2_ldb_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_ldb_port *port)
{
	u32 reg = 0;

	/*
	 * Don't re-enable the port if a removal is pending. The caller should
	 * mark this port as enabled (if it isn't already), and when the
	 * removal completes the port will be enabled.
	 */
	if (port->num_pending_removals)
		return;

	DLB2_CSR_WR(hw, LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

void dlb2_ldb_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_ldb_port *port)
{
	u32 reg = 0;

	BIT_SET(reg, LSP_CQ_LDB_DSBL_DISABLED);
	DLB2_CSR_WR(hw, LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

void dlb2_dir_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_dir_pq_pair *port)
{
	u32 reg = 0;

	DLB2_CSR_WR(hw, LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

void dlb2_dir_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_dir_pq_pair *port)
{
	u32 reg = 0;

	BIT_SET(reg, LSP_CQ_DIR_DSBL_DISABLED);
	DLB2_CSR_WR(hw, LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_ldb_port *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	u32 reg = 0;

	BITS_SET(reg, domain->id.phys_id, SYS_LDB_PP2VAS_VAS);
	DLB2_CSR_WR(hw, SYS_LDB_PP2VAS(port->id.phys_id), reg);

	if (vdev_req) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		reg = 0;
		BITS_SET(reg, port->id.phys_id, SYS_VF_LDB_VPP2PP_PP);
		offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;
		DLB2_CSR_WR(hw, SYS_VF_LDB_VPP2PP(offs), reg);

		reg = 0;
		BITS_SET(reg, vdev_id, SYS_LDB_PP2VDEV_VDEV);
		DLB2_CSR_WR(hw, SYS_LDB_PP2VDEV(port->id.phys_id), reg);

		reg = 0;
		BIT_SET(reg, SYS_VF_LDB_VPP_V_VPP_V);
		DLB2_CSR_WR(hw, SYS_VF_LDB_VPP_V(offs), reg);
	}

	reg = 0;
	BIT_SET(reg, SYS_LDB_PP_V_PP_V);
	DLB2_CSR_WR(hw, SYS_LDB_PP_V(port->id.phys_id), reg);
}

static int dlb2_ldb_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_ldb_port *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_ldb_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	u32 hl_base = 0;
	u32 reg = 0;
	u32 ds = 0;

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	BITS_SET(reg, cq_dma_base >> 6, SYS_LDB_CQ_ADDR_L_ADDR_L);
	DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(port->id.phys_id), reg);

	reg = cq_dma_base >> 32;
	DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(port->id.phys_id), reg);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	reg = 0;
	BITS_SET(reg, vdev_id, SYS_LDB_CQ2VF_PF_RO_VF);
	BITS_SET(reg, !vdev_req, SYS_LDB_CQ2VF_PF_RO_IS_PF);
	BIT_SET(reg, SYS_LDB_CQ2VF_PF_RO_RO);

	DLB2_CSR_WR(hw, SYS_LDB_CQ2VF_PF_RO(port->id.phys_id), reg);

	port->cq_depth = args->cq_depth;

	if (args->cq_depth <= 8) {
		ds = 1;
	} else if (args->cq_depth == 16) {
		ds = 2;
	} else if (args->cq_depth == 32) {
		ds = 3;
	} else if (args->cq_depth == 64) {
		ds = 4;
	} else if (args->cq_depth == 128) {
		ds = 5;
	} else if (args->cq_depth == 256) {
		ds = 6;
	} else if (args->cq_depth == 512) {
		ds = 7;
	} else if (args->cq_depth == 1024) {
		ds = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	reg = 0;
	BITS_SET(reg, ds, CHP_LDB_CQ_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT);
	DLB2_CSR_WR(hw, CHP_LDB_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id), reg);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		reg = 0;
		port->init_tkn_cnt = 8 - args->cq_depth;

		BITS_SET(reg, port->init_tkn_cnt, LSP_CQ_LDB_TKN_CNT_TOKEN_COUNT);
		DLB2_CSR_WR(hw, LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id), reg);
	} else {
		DLB2_CSR_WR(hw,
			    LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id),
			    LSP_CQ_LDB_TKN_CNT_RST);
	}

	reg = 0;
	BITS_SET(reg, ds, LSP_CQ_LDB_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT_V2);
	DLB2_CSR_WR(hw, LSP_CQ_LDB_TKN_DEPTH_SEL(hw->ver, port->id.phys_id), reg);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_WPTR(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_WPTR_RST);

	reg = 0;
	BITS_SET(reg, port->hist_list_entry_limit - 1, CHP_HIST_LIST_LIM_LIMIT);
	DLB2_CSR_WR(hw, CHP_HIST_LIST_LIM(hw->ver, port->id.phys_id), reg);

	BITS_SET(hl_base, port->hist_list_entry_base, CHP_HIST_LIST_BASE_BASE);
	DLB2_CSR_WR(hw, CHP_HIST_LIST_BASE(hw->ver, port->id.phys_id), hl_base);

	/*
	 * The inflight limit sets a cap on the number of QEs for which this CQ
	 * can owe completions at one time.
	 */
	reg = 0;
	BITS_SET(reg, args->cq_history_list_size, LSP_CQ_LDB_INFL_LIM_LIMIT);
	DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, BITS_GET(hl_base, CHP_HIST_LIST_BASE_BASE),
		 CHP_HIST_LIST_PUSH_PTR_PUSH_PTR);
	DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, BITS_GET(hl_base, CHP_HIST_LIST_BASE_BASE),
		 CHP_HIST_LIST_POP_PTR_POP_PTR);
	DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, port->id.phys_id), reg);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */

	if (hw->ver == DLB2_HW_V2) {
		reg = 0;
		DLB2_CSR_WR(hw, SYS_LDB_CQ_AT(port->id.phys_id), reg);
	}

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		reg = 0;
		BITS_SET(reg, hw->pasid[vdev_id], SYS_LDB_CQ_PASID_PASID);
		BIT_SET(reg, SYS_LDB_CQ_PASID_FMT2);
	}

	DLB2_CSR_WR(hw, SYS_LDB_CQ_PASID(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, domain->id.phys_id, CHP_LDB_CQ2VAS_CQ2VAS);
	DLB2_CSR_WR(hw, CHP_LDB_CQ2VAS(hw->ver, port->id.phys_id), reg);

	/* Disable the port's QID mappings */
	reg = 0;
	DLB2_CSR_WR(hw, LSP_CQ2PRIOV(hw->ver, port->id.phys_id), reg);

	if (hw->ver == DLB2_HW_V2_5) {
		reg = 0;
		BITS_SET(reg, args->enable_inflight_ctrl, LSP_CFG_CTRL_GENERAL_0_ENAB_IF_THRESH_V2_5);
		DLB2_CSR_WR(hw, V2_5LSP_CFG_CTRL_GENERAL_0, reg);

		if (args->enable_inflight_ctrl) {
			reg = 0;
			BITS_SET(reg, args->inflight_threshold, LSP_CQ_LDB_INFL_THRESH_THRESH);
			DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_THRESH(port->id.phys_id), reg);
		}
	}
	return 0;
}

static int dlb2_configure_ldb_port(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_ldb_port *port,
				   uintptr_t cq_dma_base,
				   struct dlb2_create_ldb_port_args *args,
				   bool vdev_req,
				   unsigned int vdev_id)
{
	int ret, i;

	port->hist_list_entry_base = domain->hist_list_entry_base +
				     domain->hist_list_entry_offset;
	port->hist_list_entry_limit = port->hist_list_entry_base +
				      args->cq_history_list_size;

	domain->hist_list_entry_offset += args->cq_history_list_size;
	domain->avail_hist_list_entries -= args->cq_history_list_size;

	ret = dlb2_ldb_port_configure_cq(hw,
					 domain,
					 port,
					 cq_dma_base,
					 args,
					 vdev_req,
					 vdev_id);
	if (ret)
		return ret;

	dlb2_ldb_port_configure_pp(hw,
				   domain,
				   port,
				   vdev_req,
				   vdev_id);

	dlb2_ldb_port_cq_enable(hw, port);

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++)
		port->qid_map[i].state = DLB2_QUEUE_UNMAPPED;
	port->num_mappings = 0;

	port->enabled = true;

	port->configured = true;

	return 0;
}

static void dlb2_dir_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_dir_pq_pair *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	u32 reg = 0;

	BITS_SET(reg, domain->id.phys_id, SYS_DIR_PP2VAS_VAS);
	DLB2_CSR_WR(hw, SYS_DIR_PP2VAS(port->id.phys_id), reg);

	if (vdev_req) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		reg = 0;
		BITS_SET(reg, port->id.phys_id, SYS_VF_DIR_VPP2PP_PP(hw->ver));
		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) + virt_id;
		DLB2_CSR_WR(hw, SYS_VF_DIR_VPP2PP(offs), reg);

		reg = 0;
		BITS_SET(reg, vdev_id, SYS_DIR_PP2VDEV_VDEV);
		DLB2_CSR_WR(hw, SYS_DIR_PP2VDEV(port->id.phys_id), reg);

		reg = 0;
		BIT_SET(reg, SYS_VF_DIR_VPP_V_VPP_V);
		DLB2_CSR_WR(hw, SYS_VF_DIR_VPP_V(offs), reg);
	}

	reg = 0;
	BIT_SET(reg, SYS_DIR_PP_V_PP_V);
	DLB2_CSR_WR(hw, SYS_DIR_PP_V(port->id.phys_id), reg);
}

static int dlb2_dir_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_dir_pq_pair *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_dir_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	u32 reg = 0;
	u32 ds = 0;

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	BITS_SET(reg, cq_dma_base >> 6, SYS_DIR_CQ_ADDR_L_ADDR_L);
	DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_L(port->id.phys_id), reg);

	reg = cq_dma_base >> 32;
	DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_U(port->id.phys_id), reg);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	reg = 0;
	BITS_SET(reg, vdev_id, SYS_DIR_CQ2VF_PF_RO_VF);
	BITS_SET(reg, !vdev_req, SYS_DIR_CQ2VF_PF_RO_IS_PF);
	BIT_SET(reg, SYS_DIR_CQ2VF_PF_RO_RO);

	DLB2_CSR_WR(hw, SYS_DIR_CQ2VF_PF_RO(port->id.phys_id), reg);

	if (args->cq_depth <= 8) {
		ds = 1;
	} else if (args->cq_depth == 16) {
		ds = 2;
	} else if (args->cq_depth == 32) {
		ds = 3;
	} else if (args->cq_depth == 64) {
		ds = 4;
	} else if (args->cq_depth == 128) {
		ds = 5;
	} else if (args->cq_depth == 256) {
		ds = 6;
	} else if (args->cq_depth == 512) {
		ds = 7;
	} else if (args->cq_depth == 1024) {
		ds = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	reg = 0;
	BITS_SET(reg, ds, CHP_DIR_CQ_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT);
	DLB2_CSR_WR(hw, CHP_DIR_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id), reg);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		reg = 0;
		port->init_tkn_cnt = 8 - args->cq_depth;

		BITS_SET(reg, port->init_tkn_cnt, LSP_CQ_DIR_TKN_CNT_COUNT(hw->ver));
		DLB2_CSR_WR(hw, LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id), reg);
	} else {
		DLB2_CSR_WR(hw,
			    LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id),
			    LSP_CQ_DIR_TKN_CNT_RST);
	}

	reg = 0;
	BITS_SET(reg, ds, LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_TOKEN_DEPTH_SELECT_V2);
	DLB2_CSR_WR(hw, LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(hw->ver, port->id.phys_id), reg);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_WPTR(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_WPTR_RST);

	/* Virtualize the PPID */
	reg = 0;
	DLB2_CSR_WR(hw, SYS_DIR_CQ_FMT(port->id.phys_id), reg);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */
	if (hw->ver == DLB2_HW_V2) {
		reg = 0;
		DLB2_CSR_WR(hw, SYS_DIR_CQ_AT(port->id.phys_id), reg);
	}

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		BITS_SET(reg, hw->pasid[vdev_id], SYS_DIR_CQ_PASID_PASID);
		BIT_SET(reg, SYS_DIR_CQ_PASID_FMT2);
	}

	DLB2_CSR_WR(hw, SYS_DIR_CQ_PASID(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BITS_SET(reg, domain->id.phys_id, CHP_DIR_CQ2VAS_CQ2VAS);
	DLB2_CSR_WR(hw, CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id), reg);

	return 0;
}

static int dlb2_configure_dir_port(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_dir_pq_pair *port,
				   uintptr_t cq_dma_base,
				   struct dlb2_create_dir_port_args *args,
				   bool vdev_req,
				   unsigned int vdev_id)
{
	int ret;

	ret = dlb2_dir_port_configure_cq(hw,
					 domain,
					 port,
					 cq_dma_base,
					 args,
					 vdev_req,
					 vdev_id);

	if (ret)
		return ret;

	dlb2_dir_port_configure_pp(hw,
				   domain,
				   port,
				   vdev_req,
				   vdev_id);

	dlb2_dir_port_cq_enable(hw, port);

	port->enabled = true;

	port->port_configured = true;

	return 0;
}

static int dlb2_ldb_port_map_qid_static(struct dlb2_hw *hw,
					struct dlb2_ldb_port *p,
					struct dlb2_ldb_queue *q,
					u8 priority)
{
	enum dlb2_qid_map_state state;
	u32 lsp_qid2cq2;
	u32 lsp_qid2cq;
	u32 atm_qid2cq;
	u32 cq2priov;
	u32 cq2qid;
	int i;

	/* Look for a pending or already mapped slot, else an unused slot */
	if (!dlb2_port_find_slot_queue(p, DLB2_QUEUE_MAP_IN_PROG, q, &i) &&
	    !dlb2_port_find_slot_queue(p, DLB2_QUEUE_MAPPED, q, &i) &&
	    !dlb2_port_find_slot(p, DLB2_QUEUE_UNMAPPED, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: CQ has no available QID mapping slots\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw, LSP_CQ2PRIOV(hw->ver, p->id.phys_id));

	cq2priov |= (1 << (i + LSP_CQ2PRIOV_V_LOC)) & LSP_CQ2PRIOV_V;
	cq2priov |= ((priority & 0x7) << (i + LSP_CQ2PRIOV_PRIO_LOC) * 3)
		    & LSP_CQ2PRIOV_PRIO;

	DLB2_CSR_WR(hw, LSP_CQ2PRIOV(hw->ver, p->id.phys_id), cq2priov);

	/* Read-modify-write the QID map register */
	if (i < 4)
		cq2qid = DLB2_CSR_RD(hw, LSP_CQ2QID0(hw->ver, p->id.phys_id));
	else
		cq2qid = DLB2_CSR_RD(hw, LSP_CQ2QID1(hw->ver, p->id.phys_id));

	if (i == 0 || i == 4)
		BITS_SET(cq2qid, q->id.phys_id, LSP_CQ2QID0_QID_P0);
	if (i == 1 || i == 5)
		BITS_SET(cq2qid, q->id.phys_id, LSP_CQ2QID0_QID_P1);
	if (i == 2 || i == 6)
		BITS_SET(cq2qid, q->id.phys_id, LSP_CQ2QID0_QID_P2);
	if (i == 3 || i == 7)
		BITS_SET(cq2qid, q->id.phys_id, LSP_CQ2QID0_QID_P3);

	if (i < 4)
		DLB2_CSR_WR(hw, LSP_CQ2QID0(hw->ver, p->id.phys_id), cq2qid);
	else
		DLB2_CSR_WR(hw, LSP_CQ2QID1(hw->ver, p->id.phys_id), cq2qid);

	atm_qid2cq = DLB2_CSR_RD(hw,
				 ATM_QID2CQIDIX(q->id.phys_id,
						p->id.phys_id / 4));

	lsp_qid2cq = DLB2_CSR_RD(hw,
				 LSP_QID2CQIDIX(hw->ver, q->id.phys_id,
						p->id.phys_id / 4));

	lsp_qid2cq2 = DLB2_CSR_RD(hw,
				  LSP_QID2CQIDIX2(hw->ver, q->id.phys_id,
						  p->id.phys_id / 4));

	switch (p->id.phys_id % 4) {
	case 0:
		BIT_SET(atm_qid2cq, 1 << (i + ATM_QID2CQIDIX_00_CQ_P0_LOC));
		BIT_SET(lsp_qid2cq, 1 << (i + LSP_QID2CQIDIX_00_CQ_P0_LOC));
		BIT_SET(lsp_qid2cq2, 1 << (i + LSP_QID2CQIDIX2_00_CQ_P0_LOC));
		break;

	case 1:
		BIT_SET(atm_qid2cq, 1 << (i + ATM_QID2CQIDIX_00_CQ_P1_LOC));
		BIT_SET(lsp_qid2cq, 1 << (i + LSP_QID2CQIDIX_00_CQ_P1_LOC));
		BIT_SET(lsp_qid2cq2, 1 << (i + LSP_QID2CQIDIX2_00_CQ_P1_LOC));
		break;

	case 2:
		BIT_SET(atm_qid2cq, 1 << (i + ATM_QID2CQIDIX_00_CQ_P2_LOC));
		BIT_SET(lsp_qid2cq, 1 << (i + LSP_QID2CQIDIX_00_CQ_P2_LOC));
		BIT_SET(lsp_qid2cq2, 1 << (i + LSP_QID2CQIDIX2_00_CQ_P2_LOC));
		break;

	case 3:
		BIT_SET(atm_qid2cq, 1 << (i + ATM_QID2CQIDIX_00_CQ_P3_LOC));
		BIT_SET(lsp_qid2cq, 1 << (i + LSP_QID2CQIDIX_00_CQ_P3_LOC));
		BIT_SET(lsp_qid2cq2, 1 << (i + LSP_QID2CQIDIX2_00_CQ_P3_LOC));
		break;
	}

	DLB2_CSR_WR(hw,
		    ATM_QID2CQIDIX(q->id.phys_id, p->id.phys_id / 4),
		    atm_qid2cq);

	DLB2_CSR_WR(hw,
		    LSP_QID2CQIDIX(hw->ver, q->id.phys_id, p->id.phys_id / 4),
		    lsp_qid2cq);

	DLB2_CSR_WR(hw,
		    LSP_QID2CQIDIX2(hw->ver, q->id.phys_id, p->id.phys_id / 4),
		    lsp_qid2cq2);

	dlb2_flush_csr(hw);

	p->qid_map[i].qid = q->id.phys_id;
	p->qid_map[i].priority = priority;

	state = DLB2_QUEUE_MAPPED;

	return dlb2_port_slot_state_transition(hw, p, q, i, state);
}

static void dlb2_ldb_port_change_qid_priority(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot,
					      struct dlb2_map_qid_args *args)
{
	u32 cq2priov;

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw, LSP_CQ2PRIOV(hw->ver, port->id.phys_id));

	cq2priov |= (1 << (slot + LSP_CQ2PRIOV_V_LOC)) & LSP_CQ2PRIOV_V;
	cq2priov |= ((args->priority & 0x7) << slot * 3) & LSP_CQ2PRIOV_PRIO;

	DLB2_CSR_WR(hw, LSP_CQ2PRIOV(hw->ver, port->id.phys_id), cq2priov);

	dlb2_flush_csr(hw);

	port->qid_map[slot].priority = args->priority;
}

static int dlb2_ldb_port_set_has_work_bits(struct dlb2_hw *hw,
					   struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int slot)
{
	u32 ctrl = 0;
	u32 active;
	u32 enq;

	/* Set the atomic scheduling haswork bit */
	active = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, queue->id.phys_id));

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_VALUE);
	BITS_SET(ctrl, BITS_GET(active, LSP_QID_AQED_ACTIVE_CNT_COUNT) > 0,
		 LSP_LDB_SCHED_CTRL_RLIST_HASWORK_V);

	/* Set the non-atomic scheduling haswork bit */
	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	enq = DLB2_CSR_RD(hw,
			  LSP_QID_LDB_ENQUEUE_CNT(hw->ver, queue->id.phys_id));

	memset(&ctrl, 0, sizeof(ctrl));

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_VALUE);
	BITS_SET(ctrl, BITS_GET(enq, LSP_QID_LDB_ENQUEUE_CNT_COUNT) > 0,
		 LSP_LDB_SCHED_CTRL_NALB_HASWORK_V);

	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);

	return 0;
}

static void dlb2_ldb_port_clear_has_work_bits(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      u8 slot)
{
	u32 ctrl = 0;

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_RLIST_HASWORK_V);

	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	memset(&ctrl, 0, sizeof(ctrl));

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_NALB_HASWORK_V);

	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_clear_queue_if_status(struct dlb2_hw *hw,
						struct dlb2_ldb_port *port,
						int slot)
{
	u32 ctrl = 0;

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_INFLIGHT_OK_V);

	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_set_queue_if_status(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot)
{
	u32 ctrl = 0;

	BITS_SET(ctrl, port->id.phys_id, LSP_LDB_SCHED_CTRL_CQ);
	BITS_SET(ctrl, slot, LSP_LDB_SCHED_CTRL_QIDIX);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_VALUE);
	BIT_SET(ctrl, LSP_LDB_SCHED_CTRL_INFLIGHT_OK_V);

	DLB2_CSR_WR(hw, LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_queue_set_inflight_limit(struct dlb2_hw *hw,
					      struct dlb2_ldb_queue *queue)
{
	u32 infl_lim = 0;

	BITS_SET(infl_lim, queue->num_qid_inflights, LSP_QID_LDB_INFL_LIM_LIMIT);

	DLB2_CSR_WR(hw, LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id), infl_lim);
}

static void dlb2_ldb_queue_clear_inflight_limit(struct dlb2_hw *hw,
						struct dlb2_ldb_queue *queue)
{
	DLB2_CSR_WR(hw,
		    LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id),
		    LSP_QID_LDB_INFL_LIM_RST);
}

/*
 * dlb2_ldb_queue_{enable, disable}_mapped_cqs() don't operate exactly as
 * their function names imply, and should only be called by the dynamic CQ
 * mapping code.
 */
static void dlb2_ldb_queue_disable_mapped_cqs(struct dlb2_hw *hw,
					      struct dlb2_hw_domain *domain,
					      struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int slot, i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			enum dlb2_qid_map_state state = DLB2_QUEUE_MAPPED;

			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			if (port->enabled)
				dlb2_ldb_port_cq_disable(hw, port);
		}
	}
}

static void dlb2_ldb_queue_enable_mapped_cqs(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain,
					     struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int slot, i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			enum dlb2_qid_map_state state = DLB2_QUEUE_MAPPED;

			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			if (port->enabled)
				dlb2_ldb_port_cq_enable(hw, port);
		}
	}
}

static int dlb2_ldb_port_finish_map_qid_dynamic(struct dlb2_hw *hw,
						struct dlb2_hw_domain *domain,
						struct dlb2_ldb_port *port,
						struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	enum dlb2_qid_map_state state;
	int slot, ret, i;
	u32 infl_cnt;
	u8 prio;

	infl_cnt = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, queue->id.phys_id));

	if (BITS_GET(infl_cnt, LSP_QID_LDB_INFL_CNT_COUNT)) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: non-zero QID inflight count\n",
			    __func__);
		return -EINVAL;
	}

	/*
	 * Static map the port and set its corresponding has_work bits.
	 */
	state = DLB2_QUEUE_MAP_IN_PROG;
	if (!dlb2_port_find_slot_queue(port, state, queue, &slot))
		return -EINVAL;

	prio = port->qid_map[slot].priority;

	/*
	 * Update the CQ2QID, CQ2PRIOV, and QID2CQIDX registers, and
	 * the port's qid_map state.
	 */
	ret = dlb2_ldb_port_map_qid_static(hw, port, queue, prio);
	if (ret)
		return ret;

	ret = dlb2_ldb_port_set_has_work_bits(hw, port, queue, slot);
	if (ret)
		return ret;

	/*
	 * Ensure IF_status(cq,qid) is 0 before enabling the port to
	 * prevent spurious schedules to cause the queue's inflight
	 * count to increase.
	 */
	dlb2_ldb_port_clear_queue_if_status(hw, port, slot);

	/* Reset the queue's inflight status */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			state = DLB2_QUEUE_MAPPED;
			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			dlb2_ldb_port_set_queue_if_status(hw, port, slot);
		}
	}

	dlb2_ldb_queue_set_inflight_limit(hw, queue);

	/* Re-enable CQs mapped to this queue */
	dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

	/* If this queue has other mappings pending, clear its inflight limit */
	if (queue->num_pending_additions > 0)
		dlb2_ldb_queue_clear_inflight_limit(hw, queue);

	return 0;
}

/**
 * dlb2_ldb_port_map_qid_dynamic() - perform a "dynamic" QID->CQ mapping
 * @hw: dlb2_hw handle for a particular device.
 * @port: load-balanced port
 * @queue: load-balanced queue
 * @priority: queue servicing priority
 *
 * Returns 0 if the queue was mapped, 1 if the mapping is scheduled to occur
 * at a later point, and <0 if an error occurred.
 */
static int dlb2_ldb_port_map_qid_dynamic(struct dlb2_hw *hw,
					 struct dlb2_ldb_port *port,
					 struct dlb2_ldb_queue *queue,
					 u8 priority)
{
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	int domain_id, slot, ret;
	u32 infl_cnt;

	domain_id = port->domain_id.phys_id;

	domain = dlb2_get_domain_from_id(hw, domain_id, false, 0);
	if (!domain) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: unable to find domain %d\n",
			    __func__, port->domain_id.phys_id);
		return -EINVAL;
	}

	/*
	 * Set the QID inflight limit to 0 to prevent further scheduling of the
	 * queue.
	 */
	DLB2_CSR_WR(hw, LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id), 0);

	if (!dlb2_port_find_slot(port, DLB2_QUEUE_UNMAPPED, &slot)) {
		DLB2_HW_ERR(hw,
			    "Internal error: No available unmapped slots\n");
		return -EFAULT;
	}

	port->qid_map[slot].qid = queue->id.phys_id;
	port->qid_map[slot].priority = priority;

	state = DLB2_QUEUE_MAP_IN_PROG;
	ret = dlb2_port_slot_state_transition(hw, port, queue, slot, state);
	if (ret)
		return ret;

	infl_cnt = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, queue->id.phys_id));

	if (BITS_GET(infl_cnt, LSP_QID_LDB_INFL_CNT_COUNT)) {
		/*
		 * The queue is owed completions so it's not safe to map it
		 * yet. Schedule a kernel thread to complete the mapping later,
		 * once software has completed all the queue's inflight events.
		 */
		if (!os_worker_active(hw))
			os_schedule_work(hw);

		return 1;
	}

	/*
	 * Disable the affected CQ, and the CQs already mapped to the QID,
	 * before reading the QID's inflight count a second time. There is an
	 * unlikely race in which the QID may schedule one more QE after we
	 * read an inflight count of 0, and disabling the CQs guarantees that
	 * the race will not occur after a re-read of the inflight count
	 * register.
	 */
	if (port->enabled)
		dlb2_ldb_port_cq_disable(hw, port);

	dlb2_ldb_queue_disable_mapped_cqs(hw, domain, queue);

	infl_cnt = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, queue->id.phys_id));

	if (BITS_GET(infl_cnt, LSP_QID_LDB_INFL_CNT_COUNT)) {
		if (port->enabled)
			dlb2_ldb_port_cq_enable(hw, port);

		dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

		/*
		 * The queue is owed completions so it's not safe to map it
		 * yet. Schedule a kernel thread to complete the mapping later,
		 * once software has completed all the queue's inflight events.
		 */
		if (!os_worker_active(hw))
			os_schedule_work(hw);

		return 1;
	}

	return dlb2_ldb_port_finish_map_qid_dynamic(hw, domain, port, queue);
}

static int dlb2_ldb_port_map_qid(struct dlb2_hw *hw,
				 struct dlb2_hw_domain *domain,
				 struct dlb2_ldb_port *port,
				 struct dlb2_ldb_queue *queue,
				 u8 prio)
{
	if (domain->started)
		return dlb2_ldb_port_map_qid_dynamic(hw, port, queue, prio);
	else
		return dlb2_ldb_port_map_qid_static(hw, port, queue, prio);
}

static int dlb2_ldb_port_unmap_qid(struct dlb2_hw *hw,
				   struct dlb2_ldb_port *port,
				   struct dlb2_ldb_queue *queue)
{
	enum dlb2_qid_map_state mapped, in_progress, pending_map, unmapped;
	u32 lsp_qid2cq2;
	u32 lsp_qid2cq;
	u32 atm_qid2cq;
	u32 cq2priov;
	u32 queue_id;
	u32 port_id;
	int i;

	/* Find the queue's slot */
	mapped = DLB2_QUEUE_MAPPED;
	in_progress = DLB2_QUEUE_UNMAP_IN_PROG;
	pending_map = DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP;

	if (!dlb2_port_find_slot_queue(port, mapped, queue, &i) &&
	    !dlb2_port_find_slot_queue(port, in_progress, queue, &i) &&
	    !dlb2_port_find_slot_queue(port, pending_map, queue, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: QID %d isn't mapped\n",
			    __func__, __LINE__, queue->id.phys_id);
		return -EFAULT;
	}

	port_id = port->id.phys_id;
	queue_id = queue->id.phys_id;

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw, LSP_CQ2PRIOV(hw->ver, port_id));

	cq2priov &= ~(1 << (i + LSP_CQ2PRIOV_V_LOC));

	DLB2_CSR_WR(hw, LSP_CQ2PRIOV(hw->ver, port_id), cq2priov);

	atm_qid2cq = DLB2_CSR_RD(hw, ATM_QID2CQIDIX(queue_id, port_id / 4));

	lsp_qid2cq = DLB2_CSR_RD(hw, LSP_QID2CQIDIX(hw->ver, queue_id, port_id / 4));

	lsp_qid2cq2 = DLB2_CSR_RD(hw, LSP_QID2CQIDIX2(hw->ver, queue_id, port_id / 4));

	switch (port_id % 4) {
	case 0:
		atm_qid2cq &= ~(1 << (i + ATM_QID2CQIDIX_00_CQ_P0_LOC));
		lsp_qid2cq &= ~(1 << (i + LSP_QID2CQIDIX_00_CQ_P0_LOC));
		lsp_qid2cq2 &= ~(1 << (i + LSP_QID2CQIDIX2_00_CQ_P0_LOC));
		break;

	case 1:
		atm_qid2cq &= ~(1 << (i + ATM_QID2CQIDIX_00_CQ_P1_LOC));
		lsp_qid2cq &= ~(1 << (i + LSP_QID2CQIDIX_00_CQ_P1_LOC));
		lsp_qid2cq2 &= ~(1 << (i + LSP_QID2CQIDIX2_00_CQ_P1_LOC));
		break;

	case 2:
		atm_qid2cq &= ~(1 << (i + ATM_QID2CQIDIX_00_CQ_P2_LOC));
		lsp_qid2cq &= ~(1 << (i + LSP_QID2CQIDIX_00_CQ_P2_LOC));
		lsp_qid2cq2 &= ~(1 << (i + LSP_QID2CQIDIX2_00_CQ_P2_LOC));
		break;

	case 3:
		atm_qid2cq &= ~(1 << (i + ATM_QID2CQIDIX_00_CQ_P3_LOC));
		lsp_qid2cq &= ~(1 << (i + LSP_QID2CQIDIX_00_CQ_P3_LOC));
		lsp_qid2cq2 &= ~(1 << (i + LSP_QID2CQIDIX2_00_CQ_P3_LOC));
		break;
	}

	DLB2_CSR_WR(hw, ATM_QID2CQIDIX(queue_id, port_id / 4), atm_qid2cq);

	DLB2_CSR_WR(hw, LSP_QID2CQIDIX(hw->ver, queue_id, port_id / 4), lsp_qid2cq);

	DLB2_CSR_WR(hw, LSP_QID2CQIDIX2(hw->ver, queue_id, port_id / 4), lsp_qid2cq2);

	dlb2_flush_csr(hw);

	unmapped = DLB2_QUEUE_UNMAPPED;

	return dlb2_port_slot_state_transition(hw, port, queue, i, unmapped);
}

static void
dlb2_log_create_sched_domain_args(struct dlb2_hw *hw,
				  struct dlb2_create_sched_domain_args *args,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create sched domain arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tNumber of LDB queues:          %d\n",
		    args->num_ldb_queues);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (any CoS): %d\n",
		    args->num_ldb_ports);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 0):   %d\n",
		    args->num_cos_ldb_ports[0]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 1):   %d\n",
		    args->num_cos_ldb_ports[1]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 2):   %d\n",
		    args->num_cos_ldb_ports[2]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 3):   %d\n",
		    args->num_cos_ldb_ports[3]);
	DLB2_HW_DBG(hw, "\tStrict CoS allocation:         %d\n",
		    args->cos_strict);
	DLB2_HW_DBG(hw, "\tNumber of DIR ports:           %d\n",
		    args->num_dir_ports);
	DLB2_HW_DBG(hw, "\tNumber of ATM inflights:       %d\n",
		    args->num_atomic_inflights);
	DLB2_HW_DBG(hw, "\tNumber of hist list entries:   %d\n",
		    args->num_hist_list_entries);
	DLB2_HW_DBG(hw, "\tNumber of LDB credits:         %d\n",
		    args->num_ldb_credits);
	DLB2_HW_DBG(hw, "\tNumber of DIR credits:         %d\n",
		    args->num_dir_credits);
}

/**
 * dlb2_hw_create_sched_domain() - create a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @args: scheduling domain creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a scheduling domain containing the resources specified
 * in args. The individual resources (queues, ports, credits) can be configured
 * after creating a scheduling domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the domain ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, or the requested domain name
 *	    is already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_sched_domain(struct dlb2_hw *hw,
				struct dlb2_create_sched_domain_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	int ret;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	if (hw->ver == DLB2_HW_V2_5){
		args->num_ldb_credits += args->num_dir_credits;
		args->num_dir_credits = 0;
	}

	dlb2_log_create_sched_domain_args(hw, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_sched_dom_args(rsrcs, args, resp, &domain);
	if (ret)
		return ret;

	dlb2_init_domain_rsrc_lists(domain);

	ret = dlb2_domain_attach_resources(hw, rsrcs, domain, args, resp);

	if (ret) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to verify args.\n",
			    __func__);

		return ret;
	}

	dlb2_list_del(&rsrcs->avail_domains, &domain->func_list);

	dlb2_list_add(&rsrcs->used_domains, &domain->func_list);

	resp->id = (vdev_req) ? domain->id.virt_id : domain->id.phys_id;
	resp->status = 0;

	return 0;
}

static void
dlb2_log_create_ldb_queue_args(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_ldb_queue_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create load-balanced queue arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                  %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tNumber of sequence numbers: %d\n",
		    args->num_sequence_numbers);
	DLB2_HW_DBG(hw, "\tNumber of QID inflights:    %d\n",
		    args->num_qid_inflights);
	DLB2_HW_DBG(hw, "\tNumber of ATM inflights:    %d\n",
		    args->num_atomic_inflights);
}

/**
 * dlb2_hw_create_ldb_queue() - create a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    the domain has already been started, or the requested queue name is
 *	    already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_ldb_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_ldb_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	int ret;

	dlb2_log_create_ldb_queue_args(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_ldb_queue_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id,
						&domain,
						&queue);
	if (ret)
		return ret;

	ret = dlb2_ldb_queue_attach_resources(hw, domain, queue, args);
	if (ret) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: failed to attach the ldb queue resources\n",
			    __func__, __LINE__);
		return ret;
	}

	dlb2_configure_ldb_queue(hw, domain, queue, args, vdev_req, vdev_id);

	queue->num_mappings = 0;

	queue->configured = true;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list.
	 */
	dlb2_list_del(&domain->avail_ldb_queues, &queue->domain_list);

	dlb2_list_add(&domain->used_ldb_queues, &queue->domain_list);

	resp->status = 0;
	resp->id = (vdev_req) ? queue->id.virt_id : queue->id.phys_id;

	return 0;
}

static void
dlb2_log_create_dir_queue_args(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_dir_queue_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create directed queue arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n", args->port_id);
}

/**
 * dlb2_hw_create_dir_queue() - create a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_dir_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_dir_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *queue;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_create_dir_queue_args(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_dir_queue_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id,
						&domain,
						&queue);
	if (ret)
		return ret;

	dlb2_configure_dir_queue(hw, domain, queue, args, vdev_req, vdev_id);

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list (if it's not already there).
	 */
	if (args->port_id == -1) {
		dlb2_list_del(&domain->avail_dir_pq_pairs,
			      &queue->domain_list);

		dlb2_list_add(&domain->used_dir_pq_pairs,
			      &queue->domain_list);
	}

	resp->status = 0;

	resp->id = (vdev_req) ? queue->id.virt_id : queue->id.phys_id;

	return 0;
}

static void
dlb2_log_create_ldb_port_args(struct dlb2_hw *hw,
			      u32 domain_id,
			      uintptr_t cq_dma_base,
			      struct dlb2_create_ldb_port_args *args,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create load-balanced port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                 %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tCQ depth:                  %d\n",
		    args->cq_depth);
	DLB2_HW_DBG(hw, "\tCQ hist list size:         %d\n",
		    args->cq_history_list_size);
	DLB2_HW_DBG(hw, "\tCQ base address:           0x%lx\n",
		    cq_dma_base);
	DLB2_HW_DBG(hw, "\tCoS ID:                    %u\n", args->cos_id);
	DLB2_HW_DBG(hw, "\tStrict CoS allocation:     %u\n",
		    args->cos_strict);
}

/**
 * dlb2_hw_create_ldb_port() - create a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_ldb_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret, cos_id;

	dlb2_log_create_ldb_port_args(hw,
				      domain_id,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_ldb_port_args(hw,
					       domain_id,
					       cq_dma_base,
					       args,
					       resp,
					       vdev_req,
					       vdev_id,
					       &domain,
					       &port,
					       &cos_id);
	if (ret)
		return ret;

	ret = dlb2_configure_ldb_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret)
		return ret;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list.
	 */
	dlb2_list_del(&domain->avail_ldb_ports[cos_id], &port->domain_list);

	dlb2_list_add(&domain->used_ldb_ports[cos_id], &port->domain_list);

	resp->status = 0;
	resp->id = (vdev_req) ? port->id.virt_id : port->id.phys_id;

	return 0;
}

static void
dlb2_log_create_dir_port_args(struct dlb2_hw *hw,
			      u32 domain_id,
			      uintptr_t cq_dma_base,
			      struct dlb2_create_dir_port_args *args,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create directed port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                 %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tCQ depth:                  %d\n",
		    args->cq_depth);
	DLB2_HW_DBG(hw, "\tCQ base address:           0x%lx\n",
		    cq_dma_base);
}

/**
 * dlb2_hw_create_dir_port() - create a directed port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a directed port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_dir_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_create_dir_port_args(hw,
				      domain_id,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_dir_port_args(hw,
					       domain_id,
					       cq_dma_base,
					       args,
					       resp,
					       vdev_req,
					       vdev_id,
					       &domain,
					       &port);
	if (ret)
		return ret;

	ret = dlb2_configure_dir_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret)
		return ret;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list (if it's not already there).
	 */
	if (args->queue_id == -1) {
		struct dlb2_list_head *res = &domain->rsvd_dir_pq_pairs;
		struct dlb2_list_head *avail = &domain->avail_dir_pq_pairs;

		if ((args->is_producer && !dlb2_list_empty(res)) ||
		    dlb2_list_empty(avail))
			dlb2_list_del(res, &port->domain_list);
		else
			dlb2_list_del(avail, &port->domain_list);

		dlb2_list_add(&domain->used_dir_pq_pairs, &port->domain_list);
	}

	resp->status = 0;
	resp->id = (vdev_req) ? port->id.virt_id : port->id.phys_id;

	return 0;
}

static void dlb2_log_start_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 start domain arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
}

static int dlb2_hw_start_stop_domain(struct dlb2_hw *hw,
				     u32 domain_id,
				     bool start_domain,
				     struct dlb2_cmd_response *resp,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *dir_queue;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_start_domain(hw, domain_id, vdev_req, vdev_id);

	ret = dlb2_verify_start_stop_domain_args(hw,
						 domain_id,
						 start_domain,
						 resp,
						 vdev_req,
						 vdev_id,
						 &domain);
	if (ret)
		return ret;

	/*
	 * Enable load-balanced and directed queue write permissions for the
	 * queues this domain owns. Without this, the DLB2 will drop all
	 * incoming traffic to those queues.
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, ldb_queue, iter) {
		u32 vasqid_v = 0;
		unsigned int offs;

		if (start_domain)
			BIT_SET(vasqid_v, SYS_LDB_VASQID_V_VASQID_V);

		offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES +
			ldb_queue->id.phys_id;

		DLB2_CSR_WR(hw, SYS_LDB_VASQID_V(offs), vasqid_v);
	}

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_queue, iter) {
		u32 vasqid_v = 0;
		unsigned int offs;

		if (start_domain)
			BIT_SET(vasqid_v, SYS_DIR_VASQID_V_VASQID_V);

		offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) +
			dir_queue->id.phys_id;

		DLB2_CSR_WR(hw, SYS_DIR_VASQID_V(offs), vasqid_v);
	}

	dlb2_flush_csr(hw);

	/* Return any pending tokens before stopping the domain. */
	if (!start_domain) {
		dlb2_domain_drain_ldb_cqs(hw, domain, false);
		dlb2_domain_drain_dir_cqs(hw, domain, false);
	}
	domain->started = start_domain;

	resp->status = 0;

	return 0;
}
/**
 * dlb2_hw_start_domain() - start a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @arg: start domain arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function starts a scheduling domain, which allows applications to send
 * traffic through it. Once a domain is started, its resources can no longer be
 * configured (besides QID remapping and port enable/disable).
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - the domain is not configured, or the domain is already started.
 */
int dlb2_hw_start_domain(struct dlb2_hw *hw,
			 u32 domain_id,
			 struct dlb2_start_domain_args *args,
			 struct dlb2_cmd_response *resp,
			 bool vdev_req,
			 unsigned int vdev_id)
{
	return dlb2_hw_start_stop_domain(hw, domain_id, true, resp, vdev_req, vdev_id);
}

/**
 * dlb2_hw_stop_domain() - stop a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @arg: stop domain arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function stops a scheduling domain, which allows applications to send
 * traffic through it. Once a domain is stoped, its resources can no longer be
 * configured (besides QID remapping and port enable/disable).
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - the domain is not configured, or the domain is already stoped.
 */
int dlb2_hw_stop_domain(struct dlb2_hw *hw,
			u32 domain_id,
			__attribute((unused)) struct dlb2_stop_domain_args *args,
			struct dlb2_cmd_response *resp,
			bool vdev_req,
			unsigned int vdev_id)
{
	return dlb2_hw_start_stop_domain(hw, domain_id, false, resp, vdev_req, vdev_id);
}

static void
dlb2_domain_finish_unmap_port_slot(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_ldb_port *port,
				   int slot)
{
	enum dlb2_qid_map_state state;
	struct dlb2_ldb_queue *queue;

	queue = &hw->rsrcs.ldb_queues[port->qid_map[slot].qid];

	state = port->qid_map[slot].state;

	/* Update the QID2CQIDX and CQ2QID vectors */
	dlb2_ldb_port_unmap_qid(hw, port, queue);

	/*
	 * Ensure the QID will not be serviced by this {CQ, slot} by clearing
	 * the has_work bits
	 */
	dlb2_ldb_port_clear_has_work_bits(hw, port, slot);

	/* Reset the {CQ, slot} to its default state */
	dlb2_ldb_port_set_queue_if_status(hw, port, slot);

	/* Re-enable the CQ if it wasn't manually disabled by the user */
	if (port->enabled)
		dlb2_ldb_port_cq_enable(hw, port);

	/*
	 * If there is a mapping that is pending this slot's removal, perform
	 * the mapping now.
	 */
	if (state == DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP) {
		struct dlb2_ldb_port_qid_map *map;
		struct dlb2_ldb_queue *map_queue;
		u8 prio;

		map = &port->qid_map[slot];

		map->qid = map->pending_qid;
		map->priority = map->pending_priority;

		map_queue = &hw->rsrcs.ldb_queues[map->qid];
		prio = map->priority;

		dlb2_ldb_port_map_qid(hw, domain, port, map_queue, prio);
	}
}

static bool dlb2_domain_finish_unmap_port(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain,
					  struct dlb2_ldb_port *port)
{
	u32 infl_cnt;
	int i;

	if (port->num_pending_removals == 0)
		return false;

	/*
	 * The unmap requires all the CQ's outstanding inflights to be
	 * completed.
	 */
	infl_cnt = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_CNT(hw->ver, port->id.phys_id));
	if (BITS_GET(infl_cnt, LSP_CQ_LDB_INFL_CNT_COUNT) > 0)
		return false;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		struct dlb2_ldb_port_qid_map *map;

		map = &port->qid_map[i];

		if (map->state != DLB2_QUEUE_UNMAP_IN_PROG &&
		    map->state != DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP)
			continue;

		dlb2_domain_finish_unmap_port_slot(hw, domain, port, i);
	}

	return true;
}

static unsigned int
dlb2_domain_finish_unmap_qid_procedures(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	if (!domain->configured || domain->num_pending_removals == 0)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			dlb2_domain_finish_unmap_port(hw, domain, port);
	}

	return domain->num_pending_removals;
}

/**
 * dlb2_finish_unmap_qid_procedures() - finish any pending unmap procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding unmap procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb2_finish_unmap_qid_procedures(struct dlb2_hw *hw)
{
	int i, num = 0;

	/* Finish queue unmap jobs for any domain that needs it */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		struct dlb2_hw_domain *domain = &hw->domains[i];

		num += dlb2_domain_finish_unmap_qid_procedures(hw, domain);
	}

	return num;
}

static void dlb2_domain_finish_map_port(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain,
					struct dlb2_ldb_port *port)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		u32 infl_cnt;
		struct dlb2_ldb_queue *queue;
		int qid;

		if (port->qid_map[i].state != DLB2_QUEUE_MAP_IN_PROG)
			continue;

		qid = port->qid_map[i].qid;

		queue = dlb2_get_ldb_queue_from_id(hw, qid, false, 0);

		if (!queue) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: unable to find queue %d\n",
				    __func__, qid);
			continue;
		}

		infl_cnt = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, qid));

		if (BITS_GET(infl_cnt, LSP_QID_LDB_INFL_CNT_COUNT))
			continue;

		/*
		 * Disable the affected CQ, and the CQs already mapped to the
		 * QID, before reading the QID's inflight count a second time.
		 * There is an unlikely race in which the QID may schedule one
		 * more QE after we read an inflight count of 0, and disabling
		 * the CQs guarantees that the race will not occur after a
		 * re-read of the inflight count register.
		 */
		if (port->enabled)
			dlb2_ldb_port_cq_disable(hw, port);

		dlb2_ldb_queue_disable_mapped_cqs(hw, domain, queue);

		infl_cnt = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, qid));

		if (BITS_GET(infl_cnt, LSP_QID_LDB_INFL_CNT_COUNT)) {
			if (port->enabled)
				dlb2_ldb_port_cq_enable(hw, port);

			dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

			continue;
		}

		dlb2_ldb_port_finish_map_qid_dynamic(hw, domain, port, queue);
	}
}

static unsigned int
dlb2_domain_finish_map_qid_procedures(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	if (!domain->configured || domain->num_pending_additions == 0)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			dlb2_domain_finish_map_port(hw, domain, port);
	}

	return domain->num_pending_additions;
}

/**
 * dlb2_finish_map_qid_procedures() - finish any pending map procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding map procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb2_finish_map_qid_procedures(struct dlb2_hw *hw)
{
	int i, num = 0;

	/* Finish queue map jobs for any domain that needs it */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		struct dlb2_hw_domain *domain = &hw->domains[i];

		num += dlb2_domain_finish_map_qid_procedures(hw, domain);
	}

	return num;
}

static void dlb2_log_map_qid(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_map_qid_args *args,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 map QID arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    args->port_id);
	DLB2_HW_DBG(hw, "\tQueue ID:  %d\n",
		    args->qid);
	DLB2_HW_DBG(hw, "\tPriority:  %d\n",
		    args->priority);
}

/**
 * dlb2_hw_map_qid() - map a load-balanced queue to a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: map QID arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs from the specified queue
 * to the specified port. Each load-balanced port can be mapped to up to 8
 * queues; each load-balanced queue can potentially map to all the
 * load-balanced ports.
 *
 * A successful return does not necessarily mean the mapping was configured. If
 * this function is unable to immediately map the queue to the port, it will
 * add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. In a sense, this is
 * an asynchronous function.
 *
 * This asynchronicity creates two views of the state of hardware: the actual
 * hardware state and the requested state (as if every request completed
 * immediately). If there are any pending map/unmap operations, the requested
 * state will differ from the actual state. All validation is performed with
 * respect to the pending state; for instance, if there are 8 pending map
 * operations for port X, a request for a 9th will fail because a load-balanced
 * port can only map up to 8 queues.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_map_qid(struct dlb2_hw *hw,
		    u32 domain_id,
		    struct dlb2_map_qid_args *args,
		    struct dlb2_cmd_response *resp,
		    bool vdev_req,
		    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	enum dlb2_qid_map_state st;
	struct dlb2_ldb_port *port;
	int ret, i;
	u8 prio;

	dlb2_log_map_qid(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_map_qid_args(hw,
				       domain_id,
				       args,
				       resp,
				       vdev_req,
				       vdev_id,
				       &domain,
				       &port,
				       &queue);
	if (ret)
		return ret;

	prio = args->priority;

	/*
	 * If there are any outstanding detach operations for this port,
	 * attempt to complete them. This may be necessary to free up a QID
	 * slot for this requested mapping.
	 */
	if (port->num_pending_removals)
		dlb2_domain_finish_unmap_port(hw, domain, port);

	ret = dlb2_verify_map_qid_slot_available(port, queue, resp);
	if (ret)
		return ret;

	/* Hardware requires disabling the CQ before mapping QIDs. */
	if (port->enabled)
		dlb2_ldb_port_cq_disable(hw, port);

	/*
	 * If this is only a priority change, don't perform the full QID->CQ
	 * mapping procedure
	 */
	st = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (prio != port->qid_map[i].priority) {
			dlb2_ldb_port_change_qid_priority(hw, port, i, args);
			DLB2_HW_DBG(hw, "DLB2 map: priority change\n");
		}

		st = DLB2_QUEUE_MAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto map_qid_done;
	}

	st = DLB2_QUEUE_UNMAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (prio != port->qid_map[i].priority) {
			dlb2_ldb_port_change_qid_priority(hw, port, i, args);
			DLB2_HW_DBG(hw, "DLB2 map: priority change\n");
		}

		st = DLB2_QUEUE_MAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto map_qid_done;
	}

	/*
	 * If this is a priority change on an in-progress mapping, don't
	 * perform the full QID->CQ mapping procedure.
	 */
	st = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		port->qid_map[i].priority = prio;

		DLB2_HW_DBG(hw, "DLB2 map: priority change only\n");

		goto map_qid_done;
	}

	/*
	 * If this is a priority change on a pending mapping, update the
	 * pending priority
	 */
	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i)) {
		port->qid_map[i].pending_priority = prio;

		DLB2_HW_DBG(hw, "DLB2 map: priority change only\n");

		goto map_qid_done;
	}

	/*
	 * If all the CQ's slots are in use, then there's an unmap in progress
	 * (guaranteed by dlb2_verify_map_qid_slot_available()), so add this
	 * mapping to pending_map and return. When the removal is completed for
	 * the slot's current occupant, this mapping will be performed.
	 */
	if (!dlb2_port_find_slot(port, DLB2_QUEUE_UNMAPPED, &i)) {
		if (dlb2_port_find_slot(port, DLB2_QUEUE_UNMAP_IN_PROG, &i)) {
			enum dlb2_qid_map_state new_st;

			port->qid_map[i].pending_qid = queue->id.phys_id;
			port->qid_map[i].pending_priority = prio;

			new_st = DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP;

			ret = dlb2_port_slot_state_transition(hw, port, queue,
							      i, new_st);
			if (ret)
				return ret;

			DLB2_HW_DBG(hw, "DLB2 map: map pending removal\n");

			goto map_qid_done;
		}
	}

	/*
	 * If the domain has started, a special "dynamic" CQ->queue mapping
	 * procedure is required in order to safely update the CQ<->QID tables.
	 * The "static" procedure cannot be used when traffic is flowing,
	 * because the CQ<->QID tables cannot be updated atomically and the
	 * scheduler won't see the new mapping unless the queue's if_status
	 * changes, which isn't guaranteed.
	 */
	ret = dlb2_ldb_port_map_qid(hw, domain, port, queue, prio);

	/* If ret is less than zero, it's due to an internal error */
	if (ret < 0)
		return ret;

map_qid_done:
	if (port->enabled)
		dlb2_ldb_port_cq_enable(hw, port);

	resp->status = 0;

	return 0;
}

static void dlb2_log_unmap_qid(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_unmap_qid_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 unmap QID arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    args->port_id);
	DLB2_HW_DBG(hw, "\tQueue ID:  %d\n",
		    args->qid);
	if (args->qid < DLB2_MAX_NUM_LDB_QUEUES)
		DLB2_HW_DBG(hw, "\tQueue's num mappings:  %d\n",
			    hw->rsrcs.ldb_queues[args->qid].num_mappings);
}

/**
 * dlb2_hw_unmap_qid() - Unmap a load-balanced queue from a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: unmap QID arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs from the specified
 * queue to the specified port.
 *
 * A successful return does not necessarily mean the mapping was removed. If
 * this function is unable to immediately unmap the queue from the port, it
 * will add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. See
 * dlb2_hw_map_qid() for more details.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_unmap_qid(struct dlb2_hw *hw,
		      u32 domain_id,
		      struct dlb2_unmap_qid_args *args,
		      struct dlb2_cmd_response *resp,
		      bool vdev_req,
		      unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	enum dlb2_qid_map_state st;
	struct dlb2_ldb_port *port;
	bool unmap_complete;
	int i, ret;

	dlb2_log_unmap_qid(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_unmap_qid_args(hw,
					 domain_id,
					 args,
					 resp,
					 vdev_req,
					 vdev_id,
					 &domain,
					 &port,
					 &queue);
	if (ret)
		return ret;

	/*
	 * If the queue hasn't been mapped yet, we need to update the slot's
	 * state and re-enable the queue's inflights.
	 */
	st = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		/*
		 * Since the in-progress map was aborted, re-enable the QID's
		 * inflights.
		 */
		if (queue->num_pending_additions == 0)
			dlb2_ldb_queue_set_inflight_limit(hw, queue);

		st = DLB2_QUEUE_UNMAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto unmap_qid_done;
	}

	/*
	 * If the queue mapping is on hold pending an unmap, we simply need to
	 * update the slot's state.
	 */
	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i)) {
		st = DLB2_QUEUE_UNMAP_IN_PROG;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto unmap_qid_done;
	}

	st = DLB2_QUEUE_MAPPED;
	if (!dlb2_port_find_slot_queue(port, st, queue, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: no available CQ slots\n",
			    __func__);
		return -EFAULT;
	}

	/*
	 * QID->CQ mapping removal is an asychronous procedure. It requires
	 * stopping the DLB2 from scheduling this CQ, draining all inflights
	 * from the CQ, then unmapping the queue from the CQ. This function
	 * simply marks the port as needing the queue unmapped, and (if
	 * necessary) starts the unmapping worker thread.
	 */
	dlb2_ldb_port_cq_disable(hw, port);

	st = DLB2_QUEUE_UNMAP_IN_PROG;
	ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
	if (ret)
		return ret;

	/*
	 * Attempt to finish the unmapping now, in case the port has no
	 * outstanding inflights. If that's not the case, this will fail and
	 * the unmapping will be completed at a later time.
	 */
	unmap_complete = dlb2_domain_finish_unmap_port(hw, domain, port);

	/*
	 * If the unmapping couldn't complete immediately, launch the worker
	 * thread (if it isn't already launched) to finish it later.
	 */
	if (!unmap_complete && !os_worker_active(hw))
		os_schedule_work(hw);

unmap_qid_done:
	resp->status = 0;

	return 0;
}

static void dlb2_log_enable_port(struct dlb2_hw *hw,
				 u32 domain_id,
				 u32 port_id,
				 bool vdev_req,
				 unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 enable port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    port_id);
}

/**
 * dlb2_hw_enable_ldb_port() - enable a load-balanced port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs to a load-balanced port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_enable_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_ldb_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret;

	dlb2_log_enable_port(hw, domain_id, args->port_id, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_enable_ldb_port_args(hw,
					       domain_id,
					       args,
					       resp,
					       vdev_req,
					       vdev_id,
					       &domain,
					       &port);
	if (ret)
		return ret;

	if (!port->enabled) {
		dlb2_ldb_port_cq_enable(hw, port);
		port->enabled = true;
	}

	resp->status = 0;

	return 0;
}

static void dlb2_log_disable_port(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 disable port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    port_id);
}

/**
 * dlb2_hw_disable_ldb_port() - disable a load-balanced port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs to a load-balanced
 * port. Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_disable_ldb_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_ldb_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret;

	dlb2_log_disable_port(hw, domain_id, args->port_id, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_disable_ldb_port_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id,
						&domain,
						&port);
	if (ret)
		return ret;

	if (port->enabled) {
		dlb2_ldb_port_cq_disable(hw, port);
		port->enabled = false;
	}

	resp->status = 0;

	return 0;
}

/**
 * dlb2_hw_enable_dir_port() - enable a directed port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs to a directed port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_enable_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_dir_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_enable_port(hw, domain_id, args->port_id, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_enable_dir_port_args(hw,
					       domain_id,
					       args,
					       resp,
					       vdev_req,
					       vdev_id,
					       &domain,
					       &port);
	if (ret)
		return ret;

	if (!port->enabled) {
		dlb2_dir_port_cq_enable(hw, port);
		port->enabled = true;
	}

	resp->status = 0;

	return 0;
}

/**
 * dlb2_hw_disable_dir_port() - disable a directed port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs to a directed port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_disable_dir_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_dir_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_disable_port(hw, domain_id, args->port_id, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_disable_dir_port_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id,
						&domain,
						&port);
	if (ret)
		return ret;

	if (port->enabled) {
		dlb2_dir_port_cq_disable(hw, port);
		port->enabled = false;
	}

	resp->status = 0;

	return 0;
}

/**
 * dlb2_notify_vf() - send an alarm to a VF
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID
 * @notification: notification
 *
 * This function sends a notification (as defined in dlb2_mbox.h) to a VF.
 *
 * Return:
 * Returns 0 upon success, <0 if the VF doesn't ACK the PF->VF interrupt.
 */
int dlb2_notify_vf(struct dlb2_hw *hw,
		   unsigned int vf_id,
		   u32 notification)
{
	struct dlb2_mbox_vf_notification_cmd_req req;
	int ret, retry_cnt;

	req.hdr.type = DLB2_MBOX_VF_CMD_NOTIFICATION;
	req.notification = notification;

	ret = dlb2_pf_write_vf_mbox_req(hw, vf_id, &req, sizeof(req));
	if (ret)
		return ret;

	dlb2_send_async_pf_to_vdev_msg(hw, vf_id);

	/* Timeout after 1 second of inactivity */
	retry_cnt = 1000;
	do {
		if (dlb2_pf_to_vdev_complete(hw, vf_id))
			break;
		os_msleep(1);
	} while (--retry_cnt);

	if (!retry_cnt) {
		DLB2_HW_ERR(hw,
			    "PF driver timed out waiting for mbox response\n");
		return -ETIMEDOUT;
	}

	/* No response data expected for notifications. */

	return 0;
}

/**
 * dlb2_vdev_in_use() - query whether a virtual device is in use
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function sends a mailbox request to the vdev to query whether the vdev
 * is in use.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 for false, 1 for true, and <0 if the mailbox request times out or
 * an internal error occurs.
 */
int dlb2_vdev_in_use(struct dlb2_hw *hw, unsigned int id)
{
	struct dlb2_mbox_vf_in_use_cmd_resp resp;
	struct dlb2_mbox_vf_in_use_cmd_req req;
	int ret, retry_cnt;

	req.hdr.type = DLB2_MBOX_VF_CMD_IN_USE;

	ret = dlb2_pf_write_vf_mbox_req(hw, id, &req, sizeof(req));
	if (ret)
		return ret;

	dlb2_send_async_pf_to_vdev_msg(hw, id);

	/* Timeout after 1 second of inactivity */
	retry_cnt = 1000;
	do {
		if (dlb2_pf_to_vdev_complete(hw, id))
			break;
		os_msleep(1);
	} while (--retry_cnt);

	if (!retry_cnt) {
		DLB2_HW_ERR(hw,
			    "PF driver timed out waiting for mbox response\n");
		return -ETIMEDOUT;
	}

	ret = dlb2_pf_read_vf_mbox_resp(hw, id, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (resp.hdr.status != DLB2_MBOX_ST_SUCCESS) {
		DLB2_HW_ERR(hw,
			    "[%s()]: failed with mailbox error: %s\n",
			    __func__,
			    dlb2_mbox_st_string(&resp.hdr));

		return -1;
	}

	return resp.in_use;
}

static int dlb2_notify_vf_alarm(struct dlb2_hw *hw,
				unsigned int vf_id,
				u32 domain_id,
				u32 alert_id,
				u32 aux_alert_data)
{
	struct dlb2_mbox_vf_alert_cmd_req req;
	int ret, retry_cnt;

	req.hdr.type = DLB2_MBOX_VF_CMD_DOMAIN_ALERT;
	req.domain_id = domain_id;
	req.alert_id = alert_id;
	req.aux_alert_data = aux_alert_data;

	ret = dlb2_pf_write_vf_mbox_req(hw, vf_id, &req, sizeof(req));
	if (ret)
		return ret;

	dlb2_send_async_pf_to_vdev_msg(hw, vf_id);

	/* Timeout after 1 second of inactivity */
	retry_cnt = 1000;
	do {
		if (dlb2_pf_to_vdev_complete(hw, vf_id))
			break;
		os_msleep(1);
	} while (--retry_cnt);

	if (!retry_cnt) {
		DLB2_HW_ERR(hw,
			    "PF driver timed out waiting for mbox response\n");
		return -ETIMEDOUT;
	}

	/* No response data expected for alarm notifications. */

	return 0;
}

/**
 * dlb2_set_msix_mode() - enable certain hardware alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @mode: MSI-X mode (DLB2_MSIX_MODE_PACKED or DLB2_MSIX_MODE_COMPRESSED)
 *
 * This function configures the hardware to use either packed or compressed
 * mode. This function should not be called if using MSI interrupts.
 */
void dlb2_set_msix_mode(struct dlb2_hw *hw, int mode)
{
	u32 msix_mode = 0;

	BITS_SET(msix_mode, mode, SYS_MSIX_MODE_MODE_V2);

	DLB2_CSR_WR(hw, SYS_MSIX_MODE, msix_mode);
}

/**
 * dlb2_configure_ldb_cq_interrupt() - configure load-balanced CQ for
 *					interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: load-balanced port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_MSI or DLB2_CQ_ISR_MODE_MSIX)
 * @vf: If the port is VF-owned, the VF's ID. This is used for translating the
 *	virtual port ID to a physical port ID. Ignored if mode is not MSI.
 * @owner_vf: the VF to route the interrupt to. Ignore if mode is not MSI.
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for load-balanced CQ's
 * interrupts. This doesn't enable the CQ's interrupt; that can be done with
 * dlb2_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb2_configure_ldb_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold)
{
	struct dlb2_ldb_port *port;
	bool vdev_req;
	u32 reg = 0;

	vdev_req = (mode == DLB2_CQ_ISR_MODE_MSI ||
		    mode == DLB2_CQ_ISR_MODE_ADI ||
		    mode ==  DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV);

	port = dlb2_get_ldb_port_from_id(hw, port_id, vdev_req, vf);
	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s()]: Internal error: failed to enable LDB CQ int\n\tport_id: %u, vdev_req: %u, vdev: %u\n",
			    __func__, port_id, vdev_req, vf);
		return -EINVAL;
	}

	/* Workaround for DLB 2.0 SIOV */
	if (mode == DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV)
		mode = DLB2_CQ_ISR_MODE_MSIX;

	/* Trigger the interrupt when threshold or more QEs arrive in the CQ */
	BITS_SET(reg, threshold - 1, CHP_LDB_CQ_INT_DEPTH_THRSH_DEPTH_THRESHOLD);
	DLB2_CSR_WR(hw, CHP_LDB_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BIT_SET(reg, CHP_LDB_CQ_INT_ENB_EN_DEPTH);
	DLB2_CSR_WR(hw, CHP_LDB_CQ_INT_ENB(hw->ver, port->id.phys_id), reg);

	reg = 0;
	if (mode == DLB2_CQ_ISR_MODE_ADI) {
		/* For DLB 2.5, there are (64 + 96) IMS entries. HW uses both
		 * SYS_LDB_CQ_ISR_VECTOR and a part of SYS_LDB_CQ_ISR_VF field
		 * to store vector [0:7].
		 */
		reg = port->id.ims_idx & (SYS_LDB_CQ_ISR_VECTOR
		      | SYS_LDB_CQ_ISR_VF);
	} else {
		BITS_SET(reg, vector, SYS_LDB_CQ_ISR_VECTOR);
		BITS_SET(reg, owner_vf, SYS_LDB_CQ_ISR_VF);
	}

	BITS_SET(reg, mode, SYS_LDB_CQ_ISR_EN_CODE);

	DLB2_CSR_WR(hw, SYS_LDB_CQ_ISR(port->id.phys_id), reg);

	return 0;
}

/**
 * dlb2_hw_ldb_cq_interrupt_enabled() - Check if the interrupt is enabled
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 *
 * This function returns whether the load-balanced CQ interrupt is enabled.
 */
int dlb2_hw_ldb_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id)
{
	u32 isr = 0;

	isr = DLB2_CSR_RD(hw, SYS_LDB_CQ_ISR(port_id));

	return BITS_GET(isr, SYS_LDB_CQ_ISR_EN_CODE) != DLB2_CQ_ISR_MODE_DIS;
}

/**
 * dlb2_hw_ldb_cq_interrupt_set_mode() - Program the CQ interrupt mode
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_{DIS, MSI, MSIX, ADI})
 *
 * This function can be used to disable (MODE_DIS) and re-enable the
 * load-balanced CQ's interrupt. It should only be called after the interrupt
 * has been configured with dlb2_configure_ldb_cq_interrupt().
 */
void dlb2_hw_ldb_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode)
{
	u32 isr = 0;

	isr = DLB2_CSR_RD(hw, SYS_LDB_CQ_ISR(port_id));

	BITS_SET(isr, mode, SYS_LDB_CQ_ISR_EN_CODE);

	DLB2_CSR_WR(hw, SYS_LDB_CQ_ISR(port_id), isr);
}

/**
 * dlb2_configure_dir_cq_interrupt() - configure directed CQ for interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: load-balanced port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_MSI or DLB2_CQ_ISR_MODE_MSIX)
 * @vf: If the port is VF-owned, the VF's ID. This is used for translating the
 *	virtual port ID to a physical port ID. Ignored if mode is not MSI.
 * @owner_vf: the VF to route the interrupt to. Ignore if mode is not MSI.
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for directed CQ's interrupts.
 * This doesn't enable the CQ's interrupt; that can be done with
 * dlb2_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb2_configure_dir_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold)
{
	struct dlb2_dir_pq_pair *port;
	bool vdev_req;
	u32 reg = 0;

	vdev_req = (mode == DLB2_CQ_ISR_MODE_MSI ||
		    mode == DLB2_CQ_ISR_MODE_ADI ||
		    mode ==  DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV);

	port = dlb2_get_dir_pq_from_id(hw, port_id, vdev_req, vf);
	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s()]: Internal error: failed to enable DIR CQ int\n\tport_id: %u, vdev_req: %u, vdev: %u\n",
			    __func__, port_id, vdev_req, vf);
		return -EINVAL;
	}

	/* Workaround for DLB 2.0 SIOV */
	if (mode == DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV)
		mode = DLB2_CQ_ISR_MODE_MSIX;

	/* Trigger the interrupt when threshold or more QEs arrive in the CQ */
	BITS_SET(reg, threshold - 1, CHP_DIR_CQ_INT_DEPTH_THRSH_DEPTH_THRESHOLD);
	DLB2_CSR_WR(hw, CHP_DIR_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id), reg);

	reg = 0;
	BIT_SET(reg, CHP_DIR_CQ_INT_ENB_EN_DEPTH);
	DLB2_CSR_WR(hw, CHP_DIR_CQ_INT_ENB(hw->ver, port->id.phys_id), reg);

	reg = 0;
	if (mode == DLB2_CQ_ISR_MODE_ADI) {
		/* For DLB 2.5, there are (64 + 96) IMS entries. HW uses both
		 * SYS_DIR_CQ_ISR_VECTOR and a part of SYS_DIR_CQ_ISR_VF field
		 * to store vector [0:7].
		 */
		reg = port->id.ims_idx & (SYS_DIR_CQ_ISR_VECTOR
		      | SYS_DIR_CQ_ISR_VF);
	} else {
		BITS_SET(reg, vector, SYS_DIR_CQ_ISR_VECTOR);
		BITS_SET(reg, owner_vf, SYS_DIR_CQ_ISR_VF);
	}

	BITS_SET(reg, mode, SYS_DIR_CQ_ISR_EN_CODE);

	DLB2_CSR_WR(hw, SYS_DIR_CQ_ISR(port->id.phys_id), reg);

	return 0;
}

/**
 * dlb2_hw_dir_cq_interrupt_enabled() - Check if the interrupt is enabled
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 *
 * This function returns whether the load-balanced CQ interrupt is enabled.
 */
int dlb2_hw_dir_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id)
{
	u32 isr = 0;

	isr = DLB2_CSR_RD(hw, SYS_DIR_CQ_ISR(port_id));

	return BITS_GET(isr, SYS_DIR_CQ_ISR_EN_CODE) != DLB2_CQ_ISR_MODE_DIS;
}

/**
 * dlb2_hw_dir_cq_interrupt_set_mode() - Program the CQ interrupt mode
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical directed port ID.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_{DIS, MSI, MSIX, ADI})
 *
 * This function can be used to disable (MODE_DIS) and re-enable the
 * directed CQ's interrupt. It should only be called after the interrupt
 * has been configured with dlb2_configure_dir_cq_interrupt().
 */
void dlb2_hw_dir_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode)
{
	u32 isr = 0;

	isr = DLB2_CSR_RD(hw, SYS_DIR_CQ_ISR(port_id));

	BITS_SET(isr, mode, SYS_DIR_CQ_ISR_EN_CODE);

	DLB2_CSR_WR(hw, SYS_DIR_CQ_ISR(port_id), isr);
}

/**
 * dlb2_arm_cq_interrupt() - arm a CQ's interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function arms the CQ's interrupt. The CQ must be configured prior to
 * calling this function.
 *
 * The function does no parameter validation; that is the caller's
 * responsibility.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return: returns 0 upon success, <0 otherwise.
 *
 * EINVAL - Invalid port ID.
 */
int dlb2_arm_cq_interrupt(struct dlb2_hw *hw,
			  int port_id,
			  bool is_ldb,
			  bool vdev_req,
			  unsigned int vdev_id)
{
	u32 val;
	u32 reg;

	if (vdev_req && is_ldb) {
		struct dlb2_ldb_port *ldb_port;

		ldb_port = dlb2_get_ldb_port_from_id(hw, port_id,
						     true, vdev_id);

		if (!ldb_port || !ldb_port->configured)
			return -EINVAL;

		port_id = ldb_port->id.phys_id;
	} else if (vdev_req && !is_ldb) {
		struct dlb2_dir_pq_pair *dir_port;

		dir_port = dlb2_get_dir_pq_from_id(hw, port_id, true, vdev_id);

		if (!dir_port || !dir_port->port_configured)
			return -EINVAL;

		port_id = dir_port->id.phys_id;
	}

	val = 1 << (port_id % 32);

	if (is_ldb && port_id < 32)
		reg = CHP_LDB_CQ_INTR_ARMED0(hw->ver);
	else if (is_ldb && port_id < 64)
		reg = CHP_LDB_CQ_INTR_ARMED1(hw->ver);
	else if (!is_ldb && port_id < 32)
		reg = CHP_DIR_CQ_INTR_ARMED0(hw->ver);
	else if (!is_ldb && port_id < 64)
		reg = CHP_DIR_CQ_INTR_ARMED1(hw->ver);
	else
		reg = CHP_DIR_CQ_INTR_ARMED2;

	DLB2_CSR_WR(hw, reg, val);

	dlb2_flush_csr(hw);

	return 0;
}

/**
 * dlb2_read_compressed_cq_intr_status() - read compressed CQ interrupt status
 * @hw: dlb2_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function can be called from a compressed CQ interrupt handler to
 * determine which CQ interrupts have fired. The caller should take appropriate
 * (such as waking threads blocked on a CQ's interrupt) then ack the interrupts
 * with dlb2_ack_compressed_cq_intr().
 */
void dlb2_read_compressed_cq_intr_status(struct dlb2_hw *hw,
					 u32 *ldb_interrupts,
					 u32 *dir_interrupts)
{
	/* Read every CQ's interrupt status */

	ldb_interrupts[0] = DLB2_CSR_RD(hw, SYS_LDB_CQ_31_0_OCC_INT_STS);
	ldb_interrupts[1] = DLB2_CSR_RD(hw, SYS_LDB_CQ_63_32_OCC_INT_STS);

	dir_interrupts[0] = DLB2_CSR_RD(hw, SYS_DIR_CQ_31_0_OCC_INT_STS);
	dir_interrupts[1] = DLB2_CSR_RD(hw, SYS_DIR_CQ_63_32_OCC_INT_STS);
	if (hw->ver == DLB2_HW_V2_5)
		dir_interrupts[2] = DLB2_CSR_RD(hw, SYS_DIR_CQ_95_64_OCC_INT_STS);
}

/**
 * dlb2_ack_msix_interrupt() - Ack an MSI-X interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @vector: interrupt vector.
 *
 * Note: Only needed for PF service interrupts (vector 0). CQ interrupts are
 * acked in dlb2_ack_compressed_cq_intr().
 */
void dlb2_ack_msix_interrupt(struct dlb2_hw *hw, int vector)
{
	u32 ack = 0;

	switch (vector) {
	case 0:
		BIT_SET(ack, SYS_MSIX_ACK_MSIX_0_ACK);
		break;
	case 1:
		BIT_SET(ack, SYS_MSIX_ACK_MSIX_1_ACK);
		/*
		 * CSSY-1650
		 * workaround h/w bug for lost MSI-X interrupts
		 *
		 * The recommended workaround for acknowledging
		 * vector 1 interrupts is :
		 *   1: set   MSI-X mask
		 *   2: set   MSIX_PASSTHROUGH
		 *   3: clear MSIX_ACK
		 *   4: clear MSIX_PASSTHROUGH
		 *   5: clear MSI-X mask
		 *
		 * The MSIX-ACK (step 3) is cleared for all vectors
		 * below. We handle steps 1 & 2 for vector 1 here.
		 *
		 * The bitfields for MSIX_ACK and MSIX_PASSTHRU are
		 * defined the same, so we just use the MSIX_ACK
		 * value when writing to PASSTHRU.
		 */

		/* set MSI-X mask and passthrough for vector 1 */
		DLB2_FUNC_WR(hw, MSIX_VECTOR_CTRL(1), 1);
		DLB2_CSR_WR(hw, SYS_MSIX_PASSTHRU, ack);
		break;
	}

	/* clear MSIX_ACK (write one to clear) */
	DLB2_CSR_WR(hw, SYS_MSIX_ACK, ack);

	if (vector == 1) {
		/*
		 * finish up steps 4 & 5 of the workaround -
		 * clear pasthrough and mask
		 */
		DLB2_CSR_WR(hw, SYS_MSIX_PASSTHRU, 0);
		DLB2_FUNC_WR(hw, MSIX_VECTOR_CTRL(1), 0);
	}

	dlb2_flush_csr(hw);
}

/**
 * dlb2_ack_compressed_cq_intr() - ack compressed CQ interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function ACKs compressed CQ interrupts. Its arguments should be the
 * same ones passed to dlb2_read_compressed_cq_intr_status().
 */
void dlb2_ack_compressed_cq_intr(struct dlb2_hw *hw,
				 u32 *ldb_interrupts,
				 u32 *dir_interrupts)
{
	/* Write back the status regs to ack the interrupts */
	if (ldb_interrupts[0])
		DLB2_CSR_WR(hw,
			    SYS_LDB_CQ_31_0_OCC_INT_STS,
			    ldb_interrupts[0]);
	if (ldb_interrupts[1])
		DLB2_CSR_WR(hw,
			    SYS_LDB_CQ_63_32_OCC_INT_STS,
			    ldb_interrupts[1]);

	if (dir_interrupts[0])
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_31_0_OCC_INT_STS,
			    dir_interrupts[0]);
	if (dir_interrupts[1])
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_63_32_OCC_INT_STS,
			    dir_interrupts[1]);
	if (hw->ver == DLB2_HW_V2_5 && dir_interrupts[2])
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_95_64_OCC_INT_STS,
			    dir_interrupts[2]);
}

/**
 * dlb2_read_vf_intr_status() - read the VF interrupt status register
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function can be called from a VF's interrupt handler to determine
 * which interrupts have fired. The first 31 bits correspond to CQ interrupt
 * vectors, and the final bit is for the PF->VF mailbox interrupt vector.
 *
 * Return:
 * Returns a bit vector indicating which interrupt vectors are active.
 */
u32 dlb2_read_vf_intr_status(struct dlb2_hw *hw)
{
	return DLB2_FUNC_RD(hw, VF_VF_MSI_ISR);
}

/**
 * dlb2_ack_vf_intr_status() - ack VF interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @interrupts: 32-bit bitmap
 *
 * This function ACKs a VF's interrupts. Its interrupts argument should be the
 * value returned by dlb2_read_vf_intr_status().
 */
void dlb2_ack_vf_intr_status(struct dlb2_hw *hw, u32 interrupts)
{
	DLB2_FUNC_WR(hw, VF_VF_MSI_ISR, interrupts);
}

/**
 * dlb2_ack_vf_msi_intr() - ack VF MSI interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @interrupts: 32-bit bitmap
 *
 * This function clears the VF's MSI interrupt pending register. Its interrupts
 * argument should be contain the MSI vectors to ACK. For example, if MSI MME
 * is in mode 0, then one bit 0 should ever be set.
 */
void dlb2_ack_vf_msi_intr(struct dlb2_hw *hw, u32 interrupts)
{
	DLB2_FUNC_WR(hw, VF_VF_MSI_ISR_PEND, interrupts);
}

/**
 * dlb2_ack_pf_mbox_int() - ack PF->VF mailbox interrupt
 * @hw: dlb2_hw handle for a particular device.
 *
 * When done processing the PF mailbox request, this function unsets
 * the PF's mailbox ISR register.
 */
void dlb2_ack_pf_mbox_int(struct dlb2_hw *hw)
{
	u32 isr = 0;

	if (hw->virt_mode == DLB2_VIRT_SIOV)
		BITS_CLR(isr, VF_PF2VF_MAILBOX_ISR_PF_ISR);
	else
		BIT_SET(isr, VF_PF2VF_MAILBOX_ISR_PF_ISR);

	DLB2_FUNC_WR(hw, VF_PF2VF_MAILBOX_ISR, isr);
}

/**
 * dlb2_enable_ingress_error_alarms() - enable ingress error alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 */
void dlb2_enable_ingress_error_alarms(struct dlb2_hw *hw)
{
	u32 en;

	en = DLB2_CSR_RD(hw, SYS_INGRESS_ALARM_ENBL);

	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_HCW);
	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_PP);
	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_PASID);
	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_QID);
	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_DISABLED_QID);
	BIT_SET(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_LDB_QID_CFG);

	DLB2_CSR_WR(hw, SYS_INGRESS_ALARM_ENBL, en);
}

/**
 * dlb2_disable_ingress_error_alarms() - disable ingress error alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 */
void dlb2_disable_ingress_error_alarms(struct dlb2_hw *hw)
{
	u32 en;

	en = DLB2_CSR_RD(hw, SYS_INGRESS_ALARM_ENBL);

	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_HCW);
	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_PP);
	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_PASID);
	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_QID);
	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_DISABLED_QID);
	BITS_CLR(en, SYS_INGRESS_ALARM_ENBL_ILLEGAL_LDB_QID_CFG);

	DLB2_CSR_WR(hw, SYS_INGRESS_ALARM_ENBL, en);
}

static void dlb2_log_alarm_syndrome(struct dlb2_hw *hw,
				    const char *str,
				    u32 synd)
{
	DLB2_HW_ERR(hw, "%s:\n", str);
	DLB2_HW_ERR(hw, "\tsyndrome: 0x%x\n", SYND(SYNDROME));
	DLB2_HW_ERR(hw, "\trtype:    0x%x\n", SYND(RTYPE));
	DLB2_HW_ERR(hw, "\talarm:    0x%x\n", SYND(ALARM));
	DLB2_HW_ERR(hw, "\tcwd:      0x%x\n", SYND(CWD));
	DLB2_HW_ERR(hw, "\tvf_pf_mb: 0x%x\n", SYND(VF_PF_MB));
	DLB2_HW_ERR(hw, "\tcls:      0x%x\n", SYND(CLS));
	DLB2_HW_ERR(hw, "\taid:      0x%x\n", SYND(AID));
	DLB2_HW_ERR(hw, "\tunit:     0x%x\n", SYND(UNIT));
	DLB2_HW_ERR(hw, "\tsource:   0x%x\n", SYND(SOURCE));
	DLB2_HW_ERR(hw, "\tmore:     0x%x\n", SYND(MORE));
	DLB2_HW_ERR(hw, "\tvalid:    0x%x\n", SYND(VALID));
}

/* Note: this array's contents must match dlb2_alert_id() */
static const char dlb2_alert_strings[NUM_DLB2_DOMAIN_ALERTS][128] = {
	[DLB2_DOMAIN_ALERT_PP_ILLEGAL_ENQ] = "Illegal enqueue",
	[DLB2_DOMAIN_ALERT_PP_EXCESS_TOKEN_POPS] = "Excess token pops",
	[DLB2_DOMAIN_ALERT_ILLEGAL_HCW] = "Illegal HCW",
	[DLB2_DOMAIN_ALERT_ILLEGAL_QID] = "Illegal QID",
	[DLB2_DOMAIN_ALERT_DISABLED_QID] = "Disabled QID",
};

static void dlb2_log_pf_vf_syndrome(struct dlb2_hw *hw,
				    const char *str,
				    u32 synd0,
				    u32 synd1,
				    u32 synd2,
				    u32 alert_id)
{
	DLB2_HW_ERR(hw, "%s:\n", str);
	if (alert_id < NUM_DLB2_DOMAIN_ALERTS)
		DLB2_HW_ERR(hw, "Alert: %s\n", dlb2_alert_strings[alert_id]);
	DLB2_HW_ERR(hw, "\tsyndrome:     0x%x\n", SYND0(SYNDROME));
	DLB2_HW_ERR(hw, "\trtype:        0x%x\n", SYND0(RTYPE));
	DLB2_HW_ERR(hw, "\tis_ldb:       0x%x\n", SYND0(IS_LDB));
	DLB2_HW_ERR(hw, "\tcls:          0x%x\n", SYND0(CLS));
	DLB2_HW_ERR(hw, "\taid:          0x%x\n", SYND0(AID));
	DLB2_HW_ERR(hw, "\tunit:         0x%x\n", SYND0(UNIT));
	DLB2_HW_ERR(hw, "\tsource:       0x%x\n", SYND0(SOURCE));
	DLB2_HW_ERR(hw, "\tmore:         0x%x\n", SYND0(MORE));
	DLB2_HW_ERR(hw, "\tvalid:        0x%x\n", SYND0(VALID));
	DLB2_HW_ERR(hw, "\tdsi:          0x%x\n", SYND1(DSI));
	DLB2_HW_ERR(hw, "\tqid:          0x%x\n", SYND1(QID));
	DLB2_HW_ERR(hw, "\tqtype:        0x%x\n", SYND1(QTYPE));
	DLB2_HW_ERR(hw, "\tqpri:         0x%x\n", SYND1(QPRI));
	DLB2_HW_ERR(hw, "\tmsg_type:     0x%x\n", SYND1(MSG_TYPE));
	DLB2_HW_ERR(hw, "\tlock_id:      0x%x\n", SYND2(LOCK_ID));
	DLB2_HW_ERR(hw, "\tmeas:         0x%x\n", SYND2(MEAS));
	DLB2_HW_ERR(hw, "\tdebug:        0x%x\n", SYND2(DEBUG));
	DLB2_HW_ERR(hw, "\tcq_pop:       0x%x\n", SYND2(CQ_POP));
	DLB2_HW_ERR(hw, "\tqe_uhl:       0x%x\n", SYND2(QE_UHL));
	DLB2_HW_ERR(hw, "\tqe_orsp:      0x%x\n", SYND2(QE_ORSP));
	DLB2_HW_ERR(hw, "\tqe_valid:     0x%x\n", SYND2(QE_VALID));
	DLB2_HW_ERR(hw, "\tcq_int_rearm: 0x%x\n", SYND2(CQ_INT_REARM));
	DLB2_HW_ERR(hw, "\tdsi_error:    0x%x\n", SYND2(DSI_ERROR));
}

static void dlb2_clear_syndrome_register(struct dlb2_hw *hw, u32 offset)
{
	u32 synd = 0;

	BIT_SET(synd, SYS_ALARM_HW_SYND_VALID);
	BIT_SET(synd, SYS_ALARM_HW_SYND_MORE);

	DLB2_CSR_WR(hw, offset, synd);
}

/**
 * dlb2_process_alarm_interrupt() - process an alarm interrupt
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads and logs the alarm syndrome, then acks the interrupt.
 * This function should be called from the alarm interrupt handler when
 * interrupt vector DLB2_INT_ALARM fires.
 */
void dlb2_process_alarm_interrupt(struct dlb2_hw *hw)
{
	u32 synd;

	DLB2_HW_DBG(hw, "Processing alarm interrupt\n");

	synd = DLB2_CSR_RD(hw, SYS_ALARM_HW_SYND);

	dlb2_log_alarm_syndrome(hw, "HW alarm syndrome", synd);

	dlb2_clear_syndrome_register(hw, SYS_ALARM_HW_SYND);
}

static u32 dlb2_hw_read_vf_to_pf_int_bitvec(struct dlb2_hw *hw)
{
	/*
	 * The PF has one VF->PF MBOX ISR register per VF space, but they all
	 * alias to the same physical register.
	 */
	return DLB2_FUNC_RD(hw, PF_VF2PF_MAILBOX_ISR(0));
}

static u32 dlb2_sw_read_vdev_to_pf_int_bitvec(struct dlb2_hw *hw)
{
	u32 bitvec = 0;
	int i;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (hw->mbox[i].vdev_to_pf.isr_in_progress &&
		    *hw->mbox[i].vdev_to_pf.isr_in_progress)
			bitvec |= (1 << i);
	}

	return bitvec;
}

/**
 * dlb2_read_vdev_to_pf_int_bitvec() - return a bit vector of all requesting
 *					vdevs
 * @hw: dlb2_hw handle for a particular device.
 *
 * When the vdev->PF ISR fires, this function can be called to determine which
 * vdev(s) are requesting service. This bitvector must be passed to
 * dlb2_ack_vdev_to_pf_int() when processing is complete for all requesting
 * vdevs.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns a bit vector indicating which VFs (0-15) have requested service.
 */
u32 dlb2_read_vdev_to_pf_int_bitvec(struct dlb2_hw *hw)
{
	if (hw->virt_mode == DLB2_VIRT_SIOV)
		return dlb2_sw_read_vdev_to_pf_int_bitvec(hw);
	else
		return dlb2_hw_read_vf_to_pf_int_bitvec(hw);
}

static void dlb2_hw_ack_vf_mbox_int(struct dlb2_hw *hw, u32 bitvec)
{
	/*
	 * The PF has one VF->PF MBOX ISR register per VF space, but
	 * they all alias to the same physical register.
	 */
	DLB2_FUNC_WR(hw, PF_VF2PF_MAILBOX_ISR(0), bitvec);
}

static void dlb2_sw_ack_vdev_mbox_int(struct dlb2_hw *hw, u32 bitvec)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if ((bitvec & (1 << i)) == 0 ||
		    !hw->mbox[i].vdev_to_pf.isr_in_progress)
			continue;

		*hw->mbox[i].vdev_to_pf.isr_in_progress = 0;
	}
}

/**
 * dlb2_ack_vdev_mbox_int() - ack processed vdev->PF mailbox interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @bitvec: bit vector returned by dlb2_read_vdev_to_pf_int_bitvec()
 *
 * When done processing all VF mailbox requests, this function unsets the VF's
 * mailbox ISR register.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_ack_vdev_mbox_int(struct dlb2_hw *hw, u32 bitvec)
{
	if (hw->virt_mode == DLB2_VIRT_SIOV)
		dlb2_sw_ack_vdev_mbox_int(hw, bitvec);
	else
		dlb2_hw_ack_vf_mbox_int(hw, bitvec);
}

/**
 * dlb2_read_vf_flr_int_bitvec() - return a bit vector of all VFs requesting
 *				    FLR
 * @hw: dlb2_hw handle for a particular device.
 *
 * When the VF FLR ISR fires, this function can be called to determine which
 * VF(s) are requesting FLRs. This bitvector must passed to
 * dlb2_ack_vf_flr_int() when processing is complete for all requesting VFs.
 *
 * Return:
 * Returns a bit vector indicating which VFs (0-15) have requested FLRs.
 */
u32 dlb2_read_vf_flr_int_bitvec(struct dlb2_hw *hw)
{
	/*
	 * The PF has one VF->PF FLR ISR register per VF space, but they all
	 * alias to the same physical register.
	 */
	return DLB2_FUNC_RD(hw, PF_VF2PF_FLR_ISR(0));
}

/**
 * dlb2_ack_vf_flr_int() - ack processed VF<->PF interrupt(s)
 * @hw: dlb2_hw handle for a particular device.
 * @bitvec: bit vector returned by dlb2_read_vf_flr_int_bitvec()
 *
 * When done processing all VF FLR requests, this function unsets the VF's FLR
 * ISR register.
 */
void dlb2_ack_vf_flr_int(struct dlb2_hw *hw, u32 bitvec)
{
	u32 dis = 0;
	int i;

	if (!bitvec)
		return;

	/* Re-enable access to the VF BAR */
	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (!(bitvec & (1 << i)))
			continue;

		DLB2_CSR_WR(hw, IOSF_FUNC_VF_BAR_DSBL(i), dis);
	}

	/* Notify the VF driver that the reset has completed */
	DLB2_FUNC_WR(hw, PF_VF_RESET_IN_PROGRESS(0), bitvec);

	/* Mark the FLR ISR as complete */
	DLB2_FUNC_WR(hw, PF_VF2PF_FLR_ISR(0), bitvec);
}

/**
 * dlb2_ack_vdev_to_pf_int() - ack processed VF mbox and FLR interrupt(s)
 * @hw: dlb2_hw handle for a particular device.
 * @mbox_bitvec: bit vector returned by dlb2_read_vdev_to_pf_int_bitvec()
 * @flr_bitvec: bit vector returned by dlb2_read_vf_flr_int_bitvec()
 *
 * When done processing all VF requests, this function communicates to the
 * hardware that processing is complete.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_ack_vdev_to_pf_int(struct dlb2_hw *hw,
			     u32 mbox_bitvec,
			     u32 flr_bitvec)
{
	int i;

	/* If using Scalable IOV, this is a noop */
	if (hw->virt_mode == DLB2_VIRT_SIOV)
		return;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		u32 isr = 0;

		if (!((mbox_bitvec & (1 << i)) || (flr_bitvec & (1 << i))))
			continue;

		/* Unset the VF's ISR pending bit */
		BIT_SET(isr, PF_VF2PF_ISR_PEND_ISR_PEND);
		DLB2_FUNC_WR(hw, PF_VF2PF_ISR_PEND(i), isr);
	}
}

/**
 * dlb2_process_wdt_interrupt() - process watchdog timer interrupts
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads the watchdog timer interrupt cause registers to
 * determine which port(s) had a watchdog timeout, and notifies the
 * application(s) that own the port(s).
 */
void dlb2_process_wdt_interrupt(struct dlb2_hw *hw)
{
	u32 alert_id = DLB2_DOMAIN_ALERT_CQ_WATCHDOG_TIMEOUT;
	u32 dwdto_0, dwdto_1, dwdto_2;
	u32 lwdto_0, lwdto_1;
	int i, ret;

	dwdto_0 = DLB2_CSR_RD(hw, CHP_CFG_DIR_WDTO_0(hw->ver));
	dwdto_1 = DLB2_CSR_RD(hw, CHP_CFG_DIR_WDTO_1(hw->ver));
	if (hw->ver == DLB2_HW_V2_5)
		dwdto_2 = DLB2_CSR_RD(hw, CHP_CFG_DIR_WDTO_2);
	else
		dwdto_2 = 0x0;
	lwdto_0 = DLB2_CSR_RD(hw, CHP_CFG_LDB_WDTO_0(hw->ver));
	lwdto_1 = DLB2_CSR_RD(hw, CHP_CFG_LDB_WDTO_1(hw->ver));

	/* Alert applications for affected directed ports */
	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(hw->ver); i++) {
		struct dlb2_dir_pq_pair *port;
		int idx = i % 32;

		if (i < 32 && !(dwdto_0 & (1 << idx)))
			continue;
		if (i >= 32 && i < 64 && !(dwdto_1 & (1 << idx)))
			continue;
		if (i >= 64 && !(dwdto_2 & (1 << idx)))
			continue;

		port = dlb2_get_dir_pq_from_id(hw, i, false, 0);
		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()]: Internal error: unable to find DIR port %u\n",
				    __func__, i);
			return;
		}

		if (port->id.vdev_owned)
			ret = dlb2_notify_vf_alarm(hw,
						   port->id.vdev_id,
						   port->domain_id.virt_id,
						   alert_id,
						   port->id.virt_id);
		else
			ret = os_notify_user_space(hw,
						   port->domain_id.phys_id,
						   alert_id,
						   i);
		if (ret)
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to notify\n",
				    __func__);
	}

	/* Alert applications for affected load-balanced ports */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		struct dlb2_ldb_port *port;
		int idx = i % 32;

		if (i < 32 && !(lwdto_0 & (1 << idx)))
			continue;
		if (i >= 32 && !(lwdto_1 & (1 << idx)))
			continue;

		port = dlb2_get_ldb_port_from_id(hw, i, false, 0);
		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()]: Internal error: unable to find LDB port %u\n",
				    __func__, i);
			return;
		}

		/* aux_alert_data[8] is 1 to indicate a load-balanced port */
		if (port->id.vdev_owned)
			ret = dlb2_notify_vf_alarm(hw,
						   port->id.vdev_id,
						   port->domain_id.virt_id,
						   alert_id,
						   (1 << 8) | port->id.virt_id);
		else
			ret = os_notify_user_space(hw,
						   port->domain_id.phys_id,
						   alert_id,
						   (1 << 8) | i);
		if (ret)
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to notify\n",
				    __func__);
	}

	/* Clear watchdog timeout flag(s) (W1CLR) */
	DLB2_CSR_WR(hw, CHP_CFG_DIR_WDTO_0(hw->ver), dwdto_0);
	DLB2_CSR_WR(hw, CHP_CFG_DIR_WDTO_1(hw->ver), dwdto_1);
	if (hw->ver == DLB2_HW_V2_5)
		DLB2_CSR_WR(hw, CHP_CFG_DIR_WDTO_2, dwdto_2);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WDTO_0(hw->ver), lwdto_0);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WDTO_1(hw->ver), lwdto_1);

	dlb2_flush_csr(hw);

	/* Re-enable watchdog timeout(s) (W1CLR) */
	DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE0(hw->ver), dwdto_0);
	DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE1(hw->ver), dwdto_1);
	if (hw->ver == DLB2_HW_V2_5)
		DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE2, dwdto_2);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_DISABLE0(hw->ver), lwdto_0);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_DISABLE1(hw->ver), lwdto_1);
}

static void dlb2_process_ingress_error(struct dlb2_hw *hw,
				       u32 synd0,
				       u32 alert_id,
				       bool vf_error,
				       unsigned int vf_id)
{
	struct dlb2_hw_domain *domain;
	bool is_ldb, not_siov;
	u8 port_id;
	int ret;

	port_id = SYND0(SYNDROME) & 0x7F;
	if (SYND0(SOURCE) == DLB2_ALARM_HW_SOURCE_SYS)
		is_ldb = SYND0(IS_LDB);
	else
		is_ldb = (SYND0(SYNDROME) & 0x80) != 0;

	not_siov = (hw->virt_mode != DLB2_VIRT_SIOV);

	/* Get the domain ID and, if it's a VF domain, the virtual port ID */
	if (is_ldb) {
		struct dlb2_ldb_port *port;

		/* for SIOV, port_id is the physical port id. It is the virtual
		 * port id for SRIOV.
		 */
		port = dlb2_get_ldb_port_from_id(hw, port_id,
						 vf_error && not_siov, vf_id);
		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()]: Internal error: unable to find LDB port\n\tport: %u, vf_error: %u, vf_id: %u\n",
				    __func__, port_id, vf_error, vf_id);
			return;
		}

		if (vf_error)
			port_id = port->id.virt_id;

		domain = &hw->domains[port->domain_id.phys_id];
	} else {
		struct dlb2_dir_pq_pair *port;

		/* for SIOV, port_id is the physical port id. It is the virtual
		 * port id for SRIOV.
		 */
		port = dlb2_get_dir_pq_from_id(hw, port_id,
					       vf_error && not_siov, vf_id);
		if (!port) {
			DLB2_HW_ERR(hw,
				    "[%s()]: Internal error: unable to find DIR port\n\tport: %u, vf_error: %u, vf_id: %u\n",
				    __func__, port_id, vf_error, vf_id);
			return;
		}

		if (vf_error)
			port_id = port->id.virt_id;

		domain = &hw->domains[port->domain_id.phys_id];
	}

	if (vf_error)
		ret = dlb2_notify_vf_alarm(hw,
					   vf_id,
					   domain->id.virt_id,
					   alert_id,
					   (is_ldb << 8) | port_id);
	else
		ret = os_notify_user_space(hw,
					   domain->id.phys_id,
					   alert_id,
					   (is_ldb << 8) | port_id);
	if (ret)
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to notify\n",
			    __func__);
}

static u32 dlb2_alert_id(u32 synd0)
{
	if (SYND0(UNIT) == DLB2_ALARM_HW_UNIT_CHP &&
	    SYND0(AID) == DLB2_ALARM_HW_CHP_AID_ILLEGAL_ENQ)
		return DLB2_DOMAIN_ALERT_PP_ILLEGAL_ENQ;
	else if (SYND0(UNIT) == DLB2_ALARM_HW_UNIT_CHP &&
		 SYND0(AID) == DLB2_ALARM_HW_CHP_AID_EXCESS_TOKEN_POPS)
		return DLB2_DOMAIN_ALERT_PP_EXCESS_TOKEN_POPS;
	else if (SYND0(SOURCE) == DLB2_ALARM_HW_SOURCE_SYS &&
		 SYND0(AID) == DLB2_ALARM_SYS_AID_ILLEGAL_HCW)
		return DLB2_DOMAIN_ALERT_ILLEGAL_HCW;
	else if (SYND0(SOURCE) == DLB2_ALARM_HW_SOURCE_SYS &&
		 SYND0(AID) == DLB2_ALARM_SYS_AID_ILLEGAL_QID)
		return DLB2_DOMAIN_ALERT_ILLEGAL_QID;
	else if (SYND0(SOURCE) == DLB2_ALARM_HW_SOURCE_SYS &&
		 SYND0(AID) == DLB2_ALARM_SYS_AID_DISABLED_QID)
		return DLB2_DOMAIN_ALERT_DISABLED_QID;
	else
		return NUM_DLB2_DOMAIN_ALERTS;
}

/**
 * dlb2_process_ingress_error_interrupt() - process ingress error interrupts
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads the alarm syndrome, logs it, notifies user-space, and
 * acks the interrupt. This function should be called from the alarm interrupt
 * handler when interrupt vector DLB2_INT_INGRESS_ERROR fires.
 *
 * Return:
 * Returns true if an ingress error interrupt occurred, false otherwise
 */
bool dlb2_process_ingress_error_interrupt(struct dlb2_hw *hw)
{
	u32 synd0, synd1, synd2;
	u32 alert_id;
	bool valid;
	int i;

	synd0 = DLB2_CSR_RD(hw, SYS_ALARM_PF_SYND0);

	valid = SYND0(VALID);

	if (valid) {
		synd1 = DLB2_CSR_RD(hw, SYS_ALARM_PF_SYND1);
		synd2 = DLB2_CSR_RD(hw, SYS_ALARM_PF_SYND2);

		alert_id = dlb2_alert_id(synd0);

		dlb2_log_pf_vf_syndrome(hw,
					"PF Ingress error alarm",
					synd0, synd1, synd2, alert_id);

		dlb2_clear_syndrome_register(hw, SYS_ALARM_PF_SYND0);

		dlb2_process_ingress_error(hw, synd0, alert_id, false, 0);
	}

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		synd0 = DLB2_CSR_RD(hw, SYS_ALARM_VF_SYND0(i));

		valid |= SYND0(VALID);

		if (!SYND0(VALID))
			continue;

		synd1 = DLB2_CSR_RD(hw, SYS_ALARM_VF_SYND1(i));
		synd2 = DLB2_CSR_RD(hw, SYS_ALARM_VF_SYND2(i));

		alert_id = dlb2_alert_id(synd0);

		dlb2_log_pf_vf_syndrome(hw,
					"VF Ingress error alarm",
					synd0, synd1, synd2, alert_id);

		dlb2_clear_syndrome_register(hw, SYS_ALARM_VF_SYND0(i));

		dlb2_process_ingress_error(hw, synd0, alert_id, true, i);
	}

	return valid;
}

/**
 * dlb2_get_group_sequence_numbers() - return a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the configured number of sequence numbers per queue
 * for the specified group.
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_numbers(struct dlb2_hw *hw, u32 group_id)
{
	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	return hw->rsrcs.sn_groups[group_id].sequence_numbers_per_queue;
}

/**
 * dlb2_get_group_sequence_number_occupancy() - return a group's in-use slots
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the group's number of in-use slots (i.e. load-balanced
 * queues using the specified group).
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_number_occupancy(struct dlb2_hw *hw, u32 group_id)
{
	struct dlb2_get_num_resources_args arg;

	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	if (dlb2_hw_get_num_used_resources(hw, &arg, false, 0))
		return -EINVAL;

	return arg.num_sn_slots[group_id];
}

static void dlb2_log_set_group_sequence_numbers(struct dlb2_hw *hw,
						u32 group_id,
						u32 val)
{
	DLB2_HW_DBG(hw, "DLB2 set group sequence numbers:\n");
	DLB2_HW_DBG(hw, "\tGroup ID: %u\n", group_id);
	DLB2_HW_DBG(hw, "\tValue:    %u\n", val);
}

/**
 * dlb2_set_group_sequence_numbers() - assign a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 * @val: requested amount of sequence numbers per queue.
 *
 * This function configures the group's number of sequence numbers per queue.
 * val can be a power-of-two between 32 and 1024, inclusive. This setting can
 * be configured until the first ordered load-balanced queue is configured, at
 * which point the configuration is locked.
 *
 * Return:
 * Returns 0 upon success; -EINVAL if group_id or val is invalid, -EPERM if an
 * ordered queue is configured.
 */
int dlb2_set_group_sequence_numbers(struct dlb2_hw *hw,
				    u32 group_id,
				    u32 val)
{
	const u32 valid_allocations[] = {64, 128, 256, 512, 1024};
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);
	struct dlb2_sn_group *group;
	u32 num_sn;
	int mode;

	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	group = &hw->rsrcs.sn_groups[group_id];

	/*
	 * Once the first load-balanced queue using an SN group is configured,
	 * the group cannot be changed.
	 */
	if (group->slot_use_bitmap != 0)
		return -EPERM;

	/* if any slots are assigned to vf/vdev, the group cannot be changed
	 * either.
	 */
	num_sn = hw->pf.num_avail_sn_slots[group_id] *
		    group->sequence_numbers_per_queue;

	if (num_sn != DLB2_MAX_NUM_SEQUENCE_NUMBERS &&
	    group->sequence_numbers_per_queue != val)
		return -EPERM;

	for (mode = 0; mode < DLB2_MAX_NUM_SEQUENCE_NUMBER_MODES; mode++)
		if (val == valid_allocations[mode])
			break;

	if (mode == DLB2_MAX_NUM_SEQUENCE_NUMBER_MODES)
		return -EINVAL;

	if (group->sequence_numbers_per_queue != val)
		hw->pf.num_avail_sn_slots[group_id] =
			DLB2_MAX_NUM_SEQUENCE_NUMBERS / val;

	group->mode = mode;
	group->sequence_numbers_per_queue = val;

	/* MMIO registers are accessible only when the device is active (
	 * (in D0 PCI state). User may use sysfs to set parameter when the
	 * device is in D3 state. val is saved in driver, is used to reconfigure
	 * the system when the device is waked up.
	 */
	if(!pm_runtime_suspended(&dlb2->pdev->dev)) {
		u32 sn_mode = 0;

		BITS_SET(sn_mode, hw->rsrcs.sn_groups[0].mode, RO_GRP_SN_MODE_SN_MODE_0);
		BITS_SET(sn_mode, hw->rsrcs.sn_groups[1].mode, RO_GRP_SN_MODE_SN_MODE_1);

		DLB2_CSR_WR(hw, RO_GRP_SN_MODE(hw->ver), sn_mode);
	}

	dlb2_log_set_group_sequence_numbers(hw, group_id, val);

	return 0;
}

static u32 dlb2_ldb_cq_inflight_count(struct dlb2_hw *hw,
				      struct dlb2_ldb_port *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_CNT(hw->ver, port->id.phys_id));

	return BITS_GET(cnt, LSP_CQ_LDB_INFL_CNT_COUNT);
}

u32 dlb2_ldb_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_ldb_port *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw, LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return BITS_GET(cnt, LSP_CQ_LDB_TKN_CNT_TOKEN_COUNT) - port->init_tkn_cnt;
}

static int dlb2_drain_ldb_cq(struct dlb2_hw *hw, struct dlb2_ldb_port *port)
{
	u32 infl_cnt, tkn_cnt;
	unsigned int i;

	infl_cnt = dlb2_ldb_cq_inflight_count(hw, port);
	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

	if (infl_cnt || tkn_cnt) {
		struct dlb2_hcw hcw_mem[8], *hcw;
		void __iomem *pp_addr;

		pp_addr = os_map_producer_port(hw, port->id.phys_id, true);

		/* Point hcw to a 64B-aligned location */
		hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

		/*
		 * Program the first HCW for a completion and token return and
		 * the other HCWs as NOOPS
		 */

		memset(hcw, 0, 4 * sizeof(*hcw));
		hcw->qe_comp = (infl_cnt > 0);
		hcw->cq_token = (tkn_cnt > 0);
		hcw->lock_id = tkn_cnt - 1;

		/* Return tokens in the first HCW */
		os_enqueue_four_hcws(hw, hcw, pp_addr);

		hcw->cq_token = 0;

		/* Issue remaining completions (if any) */
		for (i = 1; i < infl_cnt; i++)
			os_enqueue_four_hcws(hw, hcw, pp_addr);

		os_fence_hcw(hw, pp_addr);

		os_unmap_producer_port(hw, pp_addr);
	}

	return tkn_cnt;
}

static int dlb2_domain_wait_for_ldb_cqs_to_empty(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			int j;

			for (j = 0; j < DLB2_MAX_CQ_COMP_CHECK_LOOPS; j++) {
				if (dlb2_ldb_cq_inflight_count(hw, port) == 0)
					break;
			}

			if (j == DLB2_MAX_CQ_COMP_CHECK_LOOPS) {
				DLB2_HW_ERR(hw,
					    "[%s()] Internal error: failed to flush load-balanced port %d's completions.\n",
					    __func__, port->id.phys_id);
				return -EFAULT;
			}
		}
	}

	return 0;
}

static int dlb2_domain_reset_software_state(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_dir_pq_pair *tmp_dir_port __attribute__((unused));
	struct dlb2_ldb_queue *tmp_ldb_queue __attribute__((unused));
	struct dlb2_ldb_port *tmp_ldb_port __attribute__((unused));
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_list_head *list;
	int ret, i;

	rsrcs = domain->parent_func;

	/* Move the domain's ldb queues to the function's avail list */
	list = &domain->used_ldb_queues;
	DLB2_DOM_LIST_FOR_SAFE(*list, ldb_queue, tmp_ldb_queue, iter1, iter2) {
		if (ldb_queue->sn_cfg_valid) {
			struct dlb2_sn_group *grp;

			grp = &hw->rsrcs.sn_groups[ldb_queue->sn_group];

			dlb2_sn_group_free_slot(grp, ldb_queue->sn_slot);
			ldb_queue->sn_cfg_valid = false;
		}

		ldb_queue->owned = false;
		ldb_queue->num_mappings = 0;
		ldb_queue->num_pending_additions = 0;

		dlb2_list_del(&domain->used_ldb_queues,
			      &ldb_queue->domain_list);
		dlb2_list_add(&domain->avail_ldb_queues,
			      &ldb_queue->domain_list);
	}

	list = &domain->avail_ldb_queues;
	DLB2_DOM_LIST_FOR_SAFE(*list, ldb_queue, tmp_ldb_queue, iter1, iter2) {
		ldb_queue->owned = false;

		dlb2_list_del(&domain->avail_ldb_queues,
			      &ldb_queue->domain_list);
		dlb2_list_add(&rsrcs->avail_ldb_queues,
			      &ldb_queue->func_list);
		rsrcs->num_avail_ldb_queues++;
	}

	/* Move the domain's ldb ports to the function's avail list */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		list = &domain->used_ldb_ports[i];
		DLB2_DOM_LIST_FOR_SAFE(*list, ldb_port, tmp_ldb_port,
				       iter1, iter2) {
			int j;

			ldb_port->owned = false;
			ldb_port->configured = false;
			ldb_port->num_pending_removals = 0;
			ldb_port->num_mappings = 0;
			ldb_port->init_tkn_cnt = 0;
			ldb_port->cq_depth = 0;
			for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++)
				ldb_port->qid_map[j].state =
					DLB2_QUEUE_UNMAPPED;

			dlb2_list_del(&domain->used_ldb_ports[i],
				      &ldb_port->domain_list);
			dlb2_list_add(&domain->avail_ldb_ports[i],
				      &ldb_port->domain_list);
		}

		list = &domain->avail_ldb_ports[i];
		DLB2_DOM_LIST_FOR_SAFE(*list, ldb_port, tmp_ldb_port,
				       iter1, iter2) {
			ldb_port->owned = false;

			dlb2_list_del(&domain->avail_ldb_ports[i],
				      &ldb_port->domain_list);
			dlb2_list_add(&rsrcs->avail_ldb_ports[i],
				      &ldb_port->func_list);
			rsrcs->num_avail_ldb_ports[i]++;
		}
	}

	/* Move the domain's dir ports to the function's avail list */
	list = &domain->used_dir_pq_pairs;
	DLB2_DOM_LIST_FOR_SAFE(*list, dir_port, tmp_dir_port, iter1, iter2) {
		dir_port->owned = false;
		dir_port->port_configured = false;
		dir_port->init_tkn_cnt = 0;

		dlb2_list_del(&domain->used_dir_pq_pairs,
			      &dir_port->domain_list);
		dlb2_list_add(&domain->avail_dir_pq_pairs,
			      &dir_port->domain_list);
	}

	list = &domain->rsvd_dir_pq_pairs;
	DLB2_DOM_LIST_FOR_SAFE(*list, dir_port, tmp_dir_port, iter1, iter2) {
		dir_port->owned = false;

		dlb2_list_del(&domain->rsvd_dir_pq_pairs,
			      &dir_port->domain_list);
		dlb2_list_add(&domain->avail_dir_pq_pairs,
			      &dir_port->domain_list);
	}

	list = &domain->avail_dir_pq_pairs;
	DLB2_DOM_LIST_FOR_SAFE(*list, dir_port, tmp_dir_port, iter1, iter2) {
		dir_port->owned = false;

		dlb2_list_del(&domain->avail_dir_pq_pairs,
			      &dir_port->domain_list);

		dlb2_list_add(&rsrcs->avail_dir_pq_pairs,
			      &dir_port->func_list);
		rsrcs->num_avail_dir_pq_pairs++;
	}

	/* Return hist list entries to the function */
	ret = dlb2_bitmap_set_range(rsrcs->avail_hist_list_entries,
				    domain->hist_list_entry_base,
				    domain->total_hist_list_entries);
	if (ret) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: domain hist list base doesn't match the function's bitmap.\n",
			    __func__);
		return ret;
	}

	domain->total_hist_list_entries = 0;
	domain->avail_hist_list_entries = 0;
	domain->hist_list_entry_base = 0;
	domain->hist_list_entry_offset = 0;

	rsrcs->num_avail_qed_entries += domain->num_ldb_credits;
	domain->num_ldb_credits = 0;

	rsrcs->num_avail_dqed_entries += domain->num_dir_credits;
	domain->num_dir_credits = 0;

	rsrcs->num_avail_aqed_entries += domain->num_avail_aqed_entries;
	rsrcs->num_avail_aqed_entries += domain->num_used_aqed_entries;
	domain->num_avail_aqed_entries = 0;
	domain->num_used_aqed_entries = 0;

	domain->num_pending_removals = 0;
	domain->num_pending_additions = 0;
	domain->configured = false;
	domain->started = false;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		rsrcs->num_avail_sn_slots[i] += domain->num_avail_sn_slots[i];
		rsrcs->num_avail_sn_slots[i] += domain->num_used_sn_slots[i];
		domain->num_avail_sn_slots[i] = 0;
		domain->num_used_sn_slots[i] = 0;
	}

	/*
	 * Move the domain out of the used_domains list and back to the
	 * function's avail_domains list.
	 */
	dlb2_list_del(&rsrcs->used_domains, &domain->func_list);
	dlb2_list_add(&rsrcs->avail_domains, &domain->func_list);
	rsrcs->num_avail_domains++;

	return 0;
}

/**
 * dlb2_resource_reset() - reset in-use resources to their initial state
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function resets in-use resources, and makes them available for use.
 * All resources go back to their owning function, whether a PF or a VF.
 */
void dlb2_resource_reset(struct dlb2_hw *hw)
{
	struct dlb2_hw_domain *domain, *next __attribute__((unused));
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	int i;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		DLB2_FUNC_LIST_FOR_SAFE(hw->vdev[i].used_domains, domain,
					next, iter1, iter2)
			dlb2_domain_reset_software_state(hw, domain);
	}

	DLB2_FUNC_LIST_FOR_SAFE(hw->pf.used_domains, domain,
				next, iter1, iter2)
		dlb2_domain_reset_software_state(hw, domain);
}

static u32 dlb2_dir_queue_depth(struct dlb2_hw *hw,
				struct dlb2_dir_pq_pair *queue)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw, LSP_QID_DIR_ENQUEUE_CNT(hw->ver, queue->id.phys_id));

	return BITS_GET(cnt, LSP_QID_DIR_ENQUEUE_CNT_COUNT);
}

static bool dlb2_dir_queue_is_empty(struct dlb2_hw *hw,
				    struct dlb2_dir_pq_pair *queue)
{
	return dlb2_dir_queue_depth(hw, queue) == 0;
}

static void dlb2_log_get_dir_queue_depth(struct dlb2_hw *hw,
					 u32 domain_id,
					 u32 queue_id,
					 bool vdev_req,
					 unsigned int vf_id)
{
	DLB2_HW_DBG(hw, "DLB get directed queue depth:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vf_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tQueue ID: %d\n", queue_id);
}

/**
 * dlb2_hw_get_dir_queue_depth() - returns the depth of a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the depth of a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb2_hw_get_dir_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_dir_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *queue;
	struct dlb2_hw_domain *domain;
	int id;

	id = domain_id;

	dlb2_log_get_dir_queue_depth(hw, domain_id, args->queue_id,
				     vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, id, vdev_req, vdev_id);
	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	id = args->queue_id;

	queue = dlb2_get_domain_used_dir_pq(hw, id, vdev_req, domain);
	if (!queue) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	resp->id = dlb2_dir_queue_depth(hw, queue);

	return 0;
}

static void
dlb2_log_pending_port_unmaps_args(struct dlb2_hw *hw,
				  struct dlb2_pending_port_unmaps_args *args,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB unmaps in progress arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tPort ID: %d\n", args->port_id);
}

/**
 * dlb2_hw_pending_port_unmaps() - returns the number of unmap operations in
 *	progress.
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: number of unmaps in progress args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the number of unmaps in progress.
 *
 * Errors:
 * EINVAL - Invalid port ID.
 */
int dlb2_hw_pending_port_unmaps(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_pending_port_unmaps_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;

	dlb2_log_pending_port_unmaps_args(hw, args, vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	port = dlb2_get_domain_used_ldb_port(args->port_id, vdev_req, domain);
	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	resp->id = port->num_pending_removals;

	return 0;
}

static u32 dlb2_ldb_queue_depth(struct dlb2_hw *hw,
				struct dlb2_ldb_queue *queue)
{
	u32 aqed, ldb, atm;

	aqed = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, queue->id.phys_id));
	ldb = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, queue->id.phys_id));
	atm = DLB2_CSR_RD(hw, LSP_QID_ATM_ACTIVE(hw->ver, queue->id.phys_id));

	return BITS_GET(aqed, LSP_QID_AQED_ACTIVE_CNT_COUNT)
	       + BITS_GET(ldb, LSP_QID_LDB_ENQUEUE_CNT_COUNT)
	       + BITS_GET(atm, LSP_QID_ATM_ACTIVE_COUNT(hw->ver));
}

static bool dlb2_ldb_queue_is_empty(struct dlb2_hw *hw,
				    struct dlb2_ldb_queue *queue)
{
	return dlb2_ldb_queue_depth(hw, queue) == 0;
}

static void dlb2_log_get_ldb_queue_depth(struct dlb2_hw *hw,
					 u32 domain_id,
					 u32 queue_id,
					 bool vdev_req,
					 unsigned int vf_id)
{
	DLB2_HW_DBG(hw, "DLB get load-balanced queue depth:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vf_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tQueue ID: %d\n", queue_id);
}

/**
 * dlb2_hw_get_ldb_queue_depth() - returns the depth of a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the depth of a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb2_hw_get_ldb_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_ldb_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;

	dlb2_log_get_ldb_queue_depth(hw, domain_id, args->queue_id,
				     vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->queue_id, vdev_req, domain);
	if (!queue) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	resp->id = dlb2_ldb_queue_depth(hw, queue);

	return 0;
}

static void __dlb2_domain_reset_ldb_port_registers(struct dlb2_hw *hw,
						   struct dlb2_ldb_port *port)
{
	DLB2_CSR_WR(hw,
		    SYS_LDB_PP2VAS(port->id.phys_id),
		    SYS_LDB_PP2VAS_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ2VAS(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_PP2VDEV(port->id.phys_id),
		    SYS_LDB_PP2VDEV_RST);

	if (port->id.vdev_owned) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = port->id.vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

		DLB2_CSR_WR(hw,
			    SYS_VF_LDB_VPP2PP(offs),
			    SYS_VF_LDB_VPP2PP_RST);

		DLB2_CSR_WR(hw,
			    SYS_VF_LDB_VPP_V(offs),
			    SYS_VF_LDB_VPP_V_RST);
	}

	DLB2_CSR_WR(hw,
		    SYS_LDB_PP_V(port->id.phys_id),
		    SYS_LDB_PP_V_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_DSBL_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_DEPTH(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_DEPTH_RST);

	if (hw->ver != DLB2_HW_V2) {
		DLB2_CSR_WR(hw,
			    LSP_CFG_CQ_LDB_WU_LIMIT(port->id.phys_id),
			    LSP_CFG_CQ_LDB_WU_LIMIT_RST);
		DLB2_CSR_WR(hw,
			    LSP_CQ_LDB_INFL_THRESH(port->id.phys_id),
			    LSP_CQ_LDB_INFL_THRESH_RST);
	}

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_INFL_LIM(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_INFL_LIM_RST);

	DLB2_CSR_WR(hw,
		    CHP_HIST_LIST_LIM(hw->ver, port->id.phys_id),
		    CHP_HIST_LIST_LIM_RST);

	DLB2_CSR_WR(hw,
		    CHP_HIST_LIST_BASE(hw->ver, port->id.phys_id),
		    CHP_HIST_LIST_BASE_RST);

	DLB2_CSR_WR(hw,
		    CHP_HIST_LIST_POP_PTR(hw->ver, port->id.phys_id),
		    CHP_HIST_LIST_POP_PTR_RST);

	DLB2_CSR_WR(hw,
		    CHP_HIST_LIST_PUSH_PTR(hw->ver, port->id.phys_id),
		    CHP_HIST_LIST_PUSH_PTR_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_TMR_THRSH(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_INT_ENB(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ_ISR(port->id.phys_id),
		    SYS_LDB_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    CHP_LDB_CQ_WPTR(hw->ver, port->id.phys_id),
		    CHP_LDB_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_TKN_CNT_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ_ADDR_L(port->id.phys_id),
		    SYS_LDB_CQ_ADDR_L_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ_ADDR_U(port->id.phys_id),
		    SYS_LDB_CQ_ADDR_U_RST);

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw,
			    SYS_LDB_CQ_AT(port->id.phys_id),
			    SYS_LDB_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ_PASID(hw->ver, port->id.phys_id),
		    SYS_LDB_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ2VF_PF_RO(port->id.phys_id),
		    SYS_LDB_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_TOT_SCH_CNTL(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_LDB_TOT_SCH_CNTH(hw->ver, port->id.phys_id),
		    LSP_CQ_LDB_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ2QID0(hw->ver, port->id.phys_id),
		    LSP_CQ2QID0_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ2QID1(hw->ver, port->id.phys_id),
		    LSP_CQ2QID1_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ2PRIOV(hw->ver, port->id.phys_id),
		    LSP_CQ2PRIOV_RST);
}

static void dlb2_domain_reset_ldb_port_registers(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			__dlb2_domain_reset_ldb_port_registers(hw, port);
	}
}

static void
__dlb2_domain_reset_dir_port_registers(struct dlb2_hw *hw,
				       struct dlb2_dir_pq_pair *port)
{
	u32 reg = 0;

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id),
		    LSP_CQ_DIR_DSBL_RST);

	BIT_SET(reg, SYS_WB_DIR_CQ_STATE_CQ_OPT_CLR);

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw, SYS_DIR_CQ_OPT_CLR, port->id.phys_id);
	else
		DLB2_CSR_WR(hw, SYS_WB_DIR_CQ_STATE(port->id.phys_id), reg);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_DEPTH(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_DEPTH_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_TMR_THRSH(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_INT_ENB(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_ISR(port->id.phys_id),
		    SYS_DIR_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(hw->ver, port->id.phys_id),
		    LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ_WPTR(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id),
		    LSP_CQ_DIR_TKN_CNT_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_ADDR_L(port->id.phys_id),
		    SYS_DIR_CQ_ADDR_L_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_ADDR_U(port->id.phys_id),
		    SYS_DIR_CQ_ADDR_U_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_AT(port->id.phys_id),
		    SYS_DIR_CQ_AT_RST);

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_AT(port->id.phys_id),
			    SYS_DIR_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_PASID(hw->ver, port->id.phys_id),
		    SYS_DIR_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_FMT(port->id.phys_id),
		    SYS_DIR_CQ_FMT_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ2VF_PF_RO(port->id.phys_id),
		    SYS_DIR_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_DIR_TOT_SCH_CNTL(hw->ver, port->id.phys_id),
		    LSP_CQ_DIR_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    LSP_CQ_DIR_TOT_SCH_CNTH(hw->ver, port->id.phys_id),
		    LSP_CQ_DIR_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_PP2VAS(port->id.phys_id),
		    SYS_DIR_PP2VAS_RST);

	DLB2_CSR_WR(hw,
		    CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id),
		    CHP_DIR_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    SYS_DIR_PP2VDEV(port->id.phys_id),
		    SYS_DIR_PP2VDEV_RST);

	if (port->id.vdev_owned) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = port->id.vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) +
			virt_id;

		DLB2_CSR_WR(hw,
			    SYS_VF_DIR_VPP2PP(offs),
			    SYS_VF_DIR_VPP2PP_RST);

		DLB2_CSR_WR(hw,
			    SYS_VF_DIR_VPP_V(offs),
			    SYS_VF_DIR_VPP_V_RST);
	}

	DLB2_CSR_WR(hw,
		    SYS_DIR_PP_V(port->id.phys_id),
		    SYS_DIR_PP_V_RST);
}

static void dlb2_domain_reset_dir_port_registers(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter)
		__dlb2_domain_reset_dir_port_registers(hw, port);
}

static void dlb2_domain_reset_ldb_queue_registers(struct dlb2_hw *hw,
						  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_queue *queue;

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		unsigned int queue_id = queue->id.phys_id;
		int i;

		DLB2_CSR_WR(hw,
			    LSP_QID_NALDB_TOT_ENQ_CNTL(hw->ver, queue_id),
			    LSP_QID_NALDB_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_NALDB_TOT_ENQ_CNTH(hw->ver, queue_id),
			    LSP_QID_NALDB_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_ATM_TOT_ENQ_CNTL(hw->ver, queue_id),
			    LSP_QID_ATM_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_ATM_TOT_ENQ_CNTH(hw->ver, queue_id),
			    LSP_QID_ATM_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_NALDB_MAX_DEPTH(hw->ver, queue_id),
			    LSP_QID_NALDB_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_LDB_INFL_LIM(hw->ver, queue_id),
			    LSP_QID_LDB_INFL_LIM_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_AQED_ACTIVE_LIM(hw->ver, queue_id),
			    LSP_QID_AQED_ACTIVE_LIM_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_ATM_DEPTH_THRSH(hw->ver, queue_id),
			    LSP_QID_ATM_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_NALDB_DEPTH_THRSH(hw->ver, queue_id),
			    LSP_QID_NALDB_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    SYS_LDB_QID_ITS(queue_id),
			    SYS_LDB_QID_ITS_RST);

		DLB2_CSR_WR(hw,
			    CHP_ORD_QID_SN(hw->ver, queue_id),
			    CHP_ORD_QID_SN_RST);

		DLB2_CSR_WR(hw,
			    CHP_ORD_QID_SN_MAP(hw->ver, queue_id),
			    CHP_ORD_QID_SN_MAP_RST);

		DLB2_CSR_WR(hw,
			    SYS_LDB_QID_V(queue_id),
			    SYS_LDB_QID_V_RST);

		DLB2_CSR_WR(hw,
			    SYS_LDB_QID_CFG_V(queue_id),
			    SYS_LDB_QID_CFG_V_RST);

		if (queue->sn_cfg_valid) {
			u32 offs[2];

			offs[0] = RO_GRP_0_SLT_SHFT(hw->ver, queue->sn_slot);
			offs[1] = RO_GRP_1_SLT_SHFT(hw->ver, queue->sn_slot);

			DLB2_CSR_WR(hw,
				    offs[queue->sn_group],
				    RO_GRP_0_SLT_SHFT_RST);
		}

		for (i = 0; i < LSP_QID2CQIDIX_NUM; i++) {
			DLB2_CSR_WR(hw,
				    LSP_QID2CQIDIX(hw->ver, queue_id, i),
				    LSP_QID2CQIDIX_00_RST);

			DLB2_CSR_WR(hw,
				    LSP_QID2CQIDIX2(hw->ver, queue_id, i),
				    LSP_QID2CQIDIX2_00_RST);

			DLB2_CSR_WR(hw,
				    ATM_QID2CQIDIX(queue_id, i),
				    ATM_QID2CQIDIX_00_RST);
		}
	}
}

static void dlb2_domain_reset_dir_queue_registers(struct dlb2_hw *hw,
						  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *queue;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		DLB2_CSR_WR(hw,
			    LSP_QID_DIR_MAX_DEPTH(hw->ver, queue->id.phys_id),
			    LSP_QID_DIR_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_DIR_TOT_ENQ_CNTL(hw->ver, queue->id.phys_id),
			    LSP_QID_DIR_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_DIR_TOT_ENQ_CNTH(hw->ver, queue->id.phys_id),
			    LSP_QID_DIR_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    LSP_QID_DIR_DEPTH_THRSH(hw->ver, queue->id.phys_id),
			    LSP_QID_DIR_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    SYS_DIR_QID_ITS(queue->id.phys_id),
			    SYS_DIR_QID_ITS_RST);

		DLB2_CSR_WR(hw,
			    SYS_DIR_QID_V(queue->id.phys_id),
			    SYS_DIR_QID_V_RST);
	}
}

u32 dlb2_dir_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_dir_pq_pair *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw, LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return BITS_GET(cnt, LSP_CQ_DIR_TKN_CNT_COUNT(hw->ver)) - port->init_tkn_cnt;
}

static int dlb2_domain_verify_reset_success(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_ldb_queue *queue;
	int i;

	/*
	 * Confirm that all the domain's queue's inflight counts and AQED
	 * active counts are 0.
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (!dlb2_ldb_queue_is_empty(hw, queue)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty ldb queue %d\n",
				    __func__, queue->id.phys_id);
			return -EFAULT;
		}
	}

	/* Confirm that all the domain's CQs inflight and token counts are 0. */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], ldb_port, iter) {
			if (dlb2_ldb_cq_inflight_count(hw, ldb_port) ||
			    dlb2_ldb_cq_token_count(hw, ldb_port)) {
				DLB2_HW_ERR(hw,
					    "[%s()] Internal error: failed to empty ldb port %d\n",
					    __func__, ldb_port->id.phys_id);
				return -EFAULT;
			}
		}
	}

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_port, iter) {
		if (!dlb2_dir_queue_is_empty(hw, dir_port)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty dir queue %d\n",
				    __func__, dir_port->id.phys_id);
			return -EFAULT;
		}

		if (dlb2_dir_cq_token_count(hw, dir_port)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty dir port %d\n",
				    __func__, dir_port->id.phys_id);
			return -EFAULT;
		}
	}

	return 0;
}

static void dlb2_domain_reset_registers(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	dlb2_domain_reset_ldb_port_registers(hw, domain);

	dlb2_domain_reset_dir_port_registers(hw, domain);

	dlb2_domain_reset_ldb_queue_registers(hw, domain);

	dlb2_domain_reset_dir_queue_registers(hw, domain);

	if (hw->ver == DLB2_HW_V2) {
		DLB2_CSR_WR(hw,
			    CHP_CFG_LDB_VAS_CRD(domain->id.phys_id),
			    CHP_CFG_LDB_VAS_CRD_RST);

		DLB2_CSR_WR(hw,
			    CHP_CFG_DIR_VAS_CRD(domain->id.phys_id),
			    CHP_CFG_DIR_VAS_CRD_RST);
	} else
		DLB2_CSR_WR(hw,
			    CHP_CFG_VAS_CRD(domain->id.phys_id),
			    CHP_CFG_VAS_CRD_RST);
}

static int dlb2_domain_drain_ldb_cqs(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      bool toggle_port)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int drain_cnt = 0;
	int i;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if (toggle_port)
				dlb2_ldb_port_cq_disable(hw, port);

			drain_cnt += dlb2_drain_ldb_cq(hw, port);

			if (toggle_port)
				dlb2_ldb_port_cq_enable(hw, port);
		}
	}

	return drain_cnt;
}

static bool dlb2_domain_mapped_queues_empty(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_queue *queue;

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (queue->num_mappings == 0)
			continue;

		if (!dlb2_ldb_queue_is_empty(hw, queue))
			return false;
	}

	return true;
}

static int dlb2_domain_drain_mapped_queues(struct dlb2_hw *hw,
					   struct dlb2_hw_domain *domain)
{
	int i;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	if (domain->num_pending_removals > 0) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to unmap domain queues\n",
			    __func__);
		return -EFAULT;
	}

	for (i = 0; i < DLB2_MAX_QID_EMPTY_CHECK_LOOPS(hw->ver); i++) {
		int drain_cnt;

		drain_cnt = dlb2_domain_drain_ldb_cqs(hw, domain, false);

		if (dlb2_domain_mapped_queues_empty(hw, domain))
			break;

		/* Wait for 50 ns to let DLB scheduling QEs before draining
		 * the CQs again.
		 */
		if (!drain_cnt)
			ndelay(50);
	}

	if (i == DLB2_MAX_QID_EMPTY_CHECK_LOOPS(hw->ver)) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to empty queues\n",
			    __func__);
		return -EFAULT;
	}

	/*
	 * Drain the CQs one more time. For the queues to go empty, they would
	 * have scheduled one or more QEs.
	 */
	dlb2_domain_drain_ldb_cqs(hw, domain, true);

	return 0;
}

static int dlb2_domain_drain_unmapped_queue(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain,
					    struct dlb2_ldb_queue *queue)
{
	struct dlb2_ldb_port *port = NULL;
	int ret, i;

	/* If a domain has LDB queues, it must have LDB ports */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		port = DLB2_DOM_LIST_HEAD(domain->used_ldb_ports[i],
					  typeof(*port));
		if (port)
			break;
	}

	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: No configured LDB ports\n",
			    __func__);
		return -EFAULT;
	}

	/* If necessary, free up a QID slot in this CQ */
	if (port->num_mappings == DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		struct dlb2_ldb_queue *mapped_queue;

		mapped_queue = &hw->rsrcs.ldb_queues[port->qid_map[0].qid];

		ret = dlb2_ldb_port_unmap_qid(hw, port, mapped_queue);
		if (ret)
			return ret;
	}

	ret = dlb2_ldb_port_map_qid_dynamic(hw, port, queue, 0);
	if (ret)
		return ret;

	return dlb2_domain_drain_mapped_queues(hw, domain);
}

static int dlb2_domain_drain_unmapped_queues(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_queue *queue;
	int ret;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	/*
	 * Pre-condition: the unattached queue must not have any outstanding
	 * completions. This is ensured by calling dlb2_domain_drain_ldb_cqs()
	 * prior to this in dlb2_domain_drain_mapped_queues().
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (queue->num_mappings != 0 ||
		    dlb2_ldb_queue_is_empty(hw, queue))
			continue;

		ret = dlb2_domain_drain_unmapped_queue(hw, domain, queue);
		if (ret)
			return ret;
	}

	return 0;
}

static int dlb2_drain_dir_cq(struct dlb2_hw *hw,
			      struct dlb2_dir_pq_pair *port)
{
	unsigned int port_id = port->id.phys_id;
	u32 cnt;

	/* Return any outstanding tokens */
	cnt = dlb2_dir_cq_token_count(hw, port);

	if (cnt != 0) {
		struct dlb2_hcw hcw_mem[8], *hcw;
		void __iomem *pp_addr;

		pp_addr = os_map_producer_port(hw, port_id, false);

		/* Point hcw to a 64B-aligned location */
		hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

		/*
		 * Program the first HCW for a batch token return and
		 * the rest as NOOPS
		 */
		memset(hcw, 0, 4 * sizeof(*hcw));
		hcw->cq_token = 1;
		hcw->lock_id = cnt - 1;

		os_enqueue_four_hcws(hw, hcw, pp_addr);

		os_fence_hcw(hw, pp_addr);

		os_unmap_producer_port(hw, pp_addr);
	}

	return cnt;
}

static int dlb2_domain_drain_dir_cqs(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     bool toggle_port)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;
	int drain_cnt = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		/*
		 * Can't drain a port if it's not configured, and there's
		 * nothing to drain if its queue is unconfigured.
		 */
		if (!port->port_configured || !port->queue_configured)
			continue;

		if (toggle_port)
			dlb2_dir_port_cq_disable(hw, port);

		drain_cnt += dlb2_drain_dir_cq(hw, port);

		if (toggle_port)
			dlb2_dir_port_cq_enable(hw, port);
	}

	return drain_cnt;
}

static bool dlb2_domain_dir_queues_empty(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *queue;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		if (!dlb2_dir_queue_is_empty(hw, queue))
			return false;
	}

	return true;
}

static int dlb2_domain_drain_dir_queues(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	int i;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_MAX_QID_EMPTY_CHECK_LOOPS(hw->ver); i++) {
		int drain_cnt;

		drain_cnt = dlb2_domain_drain_dir_cqs(hw, domain, false);

		if (dlb2_domain_dir_queues_empty(hw, domain))
			break;

		/* Wait for 50 ns to let DLB scheduling QEs before draining
		 * the CQs again.
		 */
		if (!drain_cnt)
			ndelay(50);
	}

	if (i == DLB2_MAX_QID_EMPTY_CHECK_LOOPS(hw->ver)) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to empty queues\n",
			    __func__);
		return -EFAULT;
	}

	/*
	 * Drain the CQs one more time. For the queues to go empty, they would
	 * have scheduled one or more QEs.
	 */
	dlb2_domain_drain_dir_cqs(hw, domain, true);

	return 0;
}

static void
dlb2_domain_disable_dir_producer_ports(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;
	u32 pp_v = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		DLB2_CSR_WR(hw,
			    SYS_DIR_PP_V(port->id.phys_id),
			    pp_v);
	}
}

static void
dlb2_domain_disable_ldb_producer_ports(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	u32 pp_v = 0;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    SYS_LDB_PP_V(port->id.phys_id),
				    pp_v);
		}
	}
}

static void dlb2_domain_disable_dir_vpps(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain,
					 unsigned int vdev_id)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;
	u32 vpp_v = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		unsigned int offs;
		u32 virt_id;

		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) + virt_id;

		DLB2_CSR_WR(hw, SYS_VF_DIR_VPP_V(offs), vpp_v);
	}
}

static void dlb2_domain_disable_ldb_vpps(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain,
					 unsigned int vdev_id)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	u32 vpp_v = 0;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			unsigned int offs;
			u32 virt_id;

			if (hw->virt_mode == DLB2_VIRT_SRIOV)
				virt_id = port->id.virt_id;
			else
				virt_id = port->id.phys_id;

			offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

			DLB2_CSR_WR(hw, SYS_VF_LDB_VPP_V(offs), vpp_v);
		}
	}
}

static void dlb2_domain_disable_ldb_seq_checks(struct dlb2_hw *hw,
					       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	u32 chk_en = 0;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    CHP_SN_CHK_ENBL(hw->ver, port->id.phys_id),
				    chk_en);
		}
	}
}

static void
dlb2_domain_disable_ldb_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	u32 int_en = 0;
	u32 wd_en = 0;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    CHP_LDB_CQ_INT_ENB(hw->ver, port->id.phys_id),
				    int_en);

			DLB2_CSR_WR(hw,
				    CHP_LDB_CQ_WD_ENB(hw->ver, port->id.phys_id),
				    wd_en);
		}
	}
}

static void
dlb2_domain_disable_dir_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;
	u32 int_en = 0;
	u32 wd_en = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		DLB2_CSR_WR(hw,
			    CHP_DIR_CQ_INT_ENB(hw->ver, port->id.phys_id),
			    int_en);

		DLB2_CSR_WR(hw,
			    CHP_DIR_CQ_WD_ENB(hw->ver, port->id.phys_id),
			    wd_en);
	}
}

static void
dlb2_domain_disable_ldb_queue_write_perms(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	int domain_offset = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES;
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_queue *queue;

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		int idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, SYS_LDB_VASQID_V(idx), 0);

		if (queue->id.vdev_owned) {
			DLB2_CSR_WR(hw, SYS_LDB_QID2VQID(queue->id.phys_id), 0);

			idx = queue->id.vdev_id * DLB2_MAX_NUM_LDB_QUEUES +
				queue->id.virt_id;

			DLB2_CSR_WR(hw, SYS_VF_LDB_VQID_V(idx), 0);

			DLB2_CSR_WR(hw, SYS_VF_LDB_VQID2QID(idx), 0);
		}
	}
}

static void
dlb2_domain_disable_dir_queue_write_perms(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *queue;
	unsigned long max_ports;
	int domain_offset;

	max_ports = DLB2_MAX_NUM_DIR_PORTS(hw->ver);

	domain_offset = domain->id.phys_id * max_ports;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		int idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, SYS_DIR_VASQID_V(idx), 0);

		if (queue->id.vdev_owned) {
			idx = queue->id.vdev_id * max_ports + queue->id.virt_id;

			DLB2_CSR_WR(hw, SYS_VF_DIR_VQID_V(idx), 0);

			DLB2_CSR_WR(hw, SYS_VF_DIR_VQID2QID(idx), 0);
		}
	}
}

static void dlb2_domain_disable_dir_cqs(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *port;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		port->enabled = false;

		dlb2_dir_port_cq_disable(hw, port);
	}
}

static void dlb2_domain_disable_ldb_cqs(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			port->enabled = false;

			dlb2_ldb_port_cq_disable(hw, port);
		}
	}
}

static void dlb2_domain_enable_ldb_cqs(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_ldb_port *port;
	int i;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			port->enabled = true;

			dlb2_ldb_port_cq_enable(hw, port);
		}
	}
}

static void dlb2_log_reset_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 reset domain:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
}

/**
 * dlb2_reset_domain() - reset a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function resets and frees a DLB 2.0 scheduling domain and its associated
 * resources.
 *
 * Pre-condition: the driver must ensure software has stopped sending QEs
 * through this domain's producer ports before invoking this function, or
 * undefined behavior will result.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -1 otherwise.
 *
 * EINVAL - Invalid domain ID, or the domain is not configured.
 * EFAULT - Internal error. (Possibly caused if software is the pre-condition
 *	    is not met.)
 * ETIMEDOUT - Hardware component didn't reset in the expected time.
 */
int dlb2_reset_domain(struct dlb2_hw *hw,
		      u32 domain_id,
		      bool vdev_req,
		      unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_reset_domain(hw, domain_id, vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain || !domain->configured)
		return -EINVAL;

	/* Disable VPPs */
	if (vdev_req) {
		dlb2_domain_disable_dir_vpps(hw, domain, vdev_id);

		dlb2_domain_disable_ldb_vpps(hw, domain, vdev_id);
	}

	/* Disable CQ interrupts */
	dlb2_domain_disable_dir_port_interrupts(hw, domain);

	dlb2_domain_disable_ldb_port_interrupts(hw, domain);

	/*
	 * For each queue owned by this domain, disable its write permissions to
	 * cause any traffic sent to it to be dropped. Well-behaved software
	 * should not be sending QEs at this point.
	 */
	dlb2_domain_disable_dir_queue_write_perms(hw, domain);

	dlb2_domain_disable_ldb_queue_write_perms(hw, domain);

	/* Turn off completion tracking on all the domain's PPs. */
	dlb2_domain_disable_ldb_seq_checks(hw, domain);

	/*
	 * Disable the LDB CQs and drain them in order to complete the map and
	 * unmap procedures, which require zero CQ inflights and zero QID
	 * inflights respectively.
	 */
	dlb2_domain_disable_ldb_cqs(hw, domain);

	dlb2_domain_drain_ldb_cqs(hw, domain, false);

	ret = dlb2_domain_wait_for_ldb_cqs_to_empty(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_finish_unmap_qid_procedures(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_finish_map_qid_procedures(hw, domain);
	if (ret)
		return ret;

	/* Re-enable the CQs in order to drain the mapped queues. */
	dlb2_domain_enable_ldb_cqs(hw, domain);

	ret = dlb2_domain_drain_mapped_queues(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_drain_unmapped_queues(hw, domain);
	if (ret)
		return ret;

	/* Done draining LDB QEs, so disable the CQs. */
	dlb2_domain_disable_ldb_cqs(hw, domain);

	dlb2_domain_drain_dir_queues(hw, domain);

	/* Done draining DIR QEs, so disable the CQs. */
	dlb2_domain_disable_dir_cqs(hw, domain);

	/* Disable PPs */
	dlb2_domain_disable_dir_producer_ports(hw, domain);

	dlb2_domain_disable_ldb_producer_ports(hw, domain);

	ret = dlb2_domain_verify_reset_success(hw, domain);
	if (ret)
		return ret;

	/* Reset the QID and port state. */
	dlb2_domain_reset_registers(hw, domain);

	/* Hardware reset complete. Reset the domain's software state */
	return dlb2_domain_reset_software_state(hw, domain);
}

/**
 * dlb2_reset_vdev() - reset the hardware owned by a virtual device
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function resets the hardware owned by a vdev, by resetting the vdev's
 * domains one by one.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
int dlb2_reset_vdev(struct dlb2_hw *hw, unsigned int id)
{
	struct dlb2_hw_domain *domain, *next __attribute__((unused));
	struct dlb2_list_entry *it1 __attribute__((unused));
	struct dlb2_list_entry *it2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;

	if (id >= DLB2_MAX_NUM_VDEVS) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: invalid vdev ID %d\n",
			    __func__, id);
		return -1;
	}

	rsrcs = &hw->vdev[id];

	DLB2_FUNC_LIST_FOR_SAFE(rsrcs->used_domains, domain, next, it1, it2) {
		int ret = dlb2_reset_domain(hw,
					    domain->id.virt_id,
					    true,
					    id);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * dlb2_ldb_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns whether a load-balanced port is owned by a specified
 * domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb2_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;

	if (vdev_req && vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain || !domain->configured)
		return -EINVAL;

	port = dlb2_get_domain_ldb_port(port_id, vdev_req, domain);

	if (!port)
		return -EINVAL;

	return port->domain_id.phys_id == domain->id.phys_id;
}

/**
 * dlb2_dir_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns whether a directed port is owned by a specified
 * domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb2_dir_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;

	if (vdev_req && vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain || !domain->configured)
		return -EINVAL;

	port = dlb2_get_domain_dir_pq(hw, port_id, vdev_req, domain);

	if (!port)
		return -EINVAL;

	return port->domain_id.phys_id == domain->id.phys_id;
}

static inline bool dlb2_ldb_port_owned_by_vf(struct dlb2_hw *hw,
					     u32 vdev_id,
					     u32 port_id)
{
	return (hw->rsrcs.ldb_ports[port_id].id.vdev_owned &&
		hw->rsrcs.ldb_ports[port_id].id.vdev_id == vdev_id);
}

static inline bool dlb2_dir_port_owned_by_vf(struct dlb2_hw *hw,
					     u32 vdev_id,
					     u32 port_id)
{
	return (hw->rsrcs.dir_pq_pairs[port_id].id.vdev_owned &&
		hw->rsrcs.dir_pq_pairs[port_id].id.vdev_id == vdev_id);
}

/**
 * dlb2_hw_get_num_resources() - query the PCI function's available resources
 * @hw: dlb2_hw handle for a particular device.
 * @arg: pointer to resource counts.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the number of available resources for the PF or for a
 * VF.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -EINVAL if vdev_req is true and vdev_id is
 * invalid.
 */
int dlb2_hw_get_num_resources(struct dlb2_hw *hw,
			      struct dlb2_get_num_resources_args *arg,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	struct dlb2_function_resources *rsrcs;
	struct dlb2_bitmap *map;
	int i;

	if (vdev_req && vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	if (vdev_req)
		rsrcs = &hw->vdev[vdev_id];
	else
		rsrcs = &hw->pf;

	arg->num_sched_domains = rsrcs->num_avail_domains;

	arg->num_ldb_queues = rsrcs->num_avail_ldb_queues;

	arg->num_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		arg->num_ldb_ports += rsrcs->num_avail_ldb_ports[i];
		arg->num_cos_ldb_ports[i] = rsrcs->num_avail_ldb_ports[i];
	}

	arg->num_dir_ports = rsrcs->num_avail_dir_pq_pairs;

	arg->num_atomic_inflights = rsrcs->num_avail_aqed_entries;

	map = rsrcs->avail_hist_list_entries;

	arg->num_hist_list_entries = dlb2_bitmap_count(map);

	arg->max_contiguous_hist_list_entries =
		dlb2_bitmap_longest_set_range(map);

	arg->num_ldb_credits = rsrcs->num_avail_qed_entries;

	arg->num_dir_credits = rsrcs->num_avail_dqed_entries;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++)
		arg->num_sn_slots[i] = rsrcs->num_avail_sn_slots[i];

	return 0;
}

/**
 * dlb2_hw_get_num_used_resources() - query the PCI function's used resources
 * @hw: dlb2_hw handle for a particular device.
 * @arg: pointer to resource counts.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the number of resources in use by the PF or a VF. It
 * fills in the fields that args points to, except the following:
 * - max_contiguous_atomic_inflights
 * - max_contiguous_hist_list_entries
 * - max_contiguous_ldb_credits
 * - max_contiguous_dir_credits
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -EINVAL if vdev_req is true and vdev_id is
 * invalid.
 */
int dlb2_hw_get_num_used_resources(struct dlb2_hw *hw,
				   struct dlb2_get_num_resources_args *arg,
				   bool vdev_req,
				   unsigned int vdev_id)
{
	struct dlb2_list_entry *iter1 __attribute__((unused));
	struct dlb2_list_entry *iter2 __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;

	if (vdev_req && vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	memset(arg, 0, sizeof(*arg));

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iter1) {
		struct dlb2_ldb_queue *queue;
		struct dlb2_ldb_port *ldb_port;
		struct dlb2_dir_pq_pair *dir_port;
		int i;

		arg->num_sched_domains++;

		arg->num_atomic_inflights += domain->num_used_aqed_entries;

		DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter2)
			arg->num_ldb_queues++;
		DLB2_DOM_LIST_FOR(domain->avail_ldb_queues, queue, iter2)
			arg->num_ldb_queues++;

		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i],
					  ldb_port, iter2)
				arg->num_ldb_ports++;
			DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i],
					  ldb_port, iter2)
				arg->num_ldb_ports++;

			DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i],
					  ldb_port, iter2)
				arg->num_cos_ldb_ports[i]++;
			DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i],
					  ldb_port, iter2)
				arg->num_cos_ldb_ports[i]++;
		}

		DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_port, iter2)
			arg->num_dir_ports++;
		DLB2_DOM_LIST_FOR(domain->avail_dir_pq_pairs, dir_port, iter2)
			arg->num_dir_ports++;

		arg->num_ldb_credits += domain->num_ldb_credits;

		arg->num_dir_credits += domain->num_dir_credits;

		arg->num_hist_list_entries += domain->total_hist_list_entries;

		for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
			arg->num_sn_slots[i] += domain->num_avail_sn_slots[i];
			arg->num_sn_slots[i] += domain->num_used_sn_slots[i];
		}
	}

	return 0;
}

void dlb2_disable_ldb_sched_perf_ctrl(struct dlb2_hw *hw)
{
        u32 r0;

        r0 = DLB2_CSR_RD(hw, LSP_LDB_SCHED_PERF_CTRL);
	BIT_SET(r0, LSP_LDB_SCHED_PERF_CTRL_CLR);
	DLB2_CSR_WR(hw, LSP_LDB_SCHED_PERF_CTRL, r0);
	dlb2_flush_csr(hw);
}

void dlb2_enable_ldb_sched_perf_ctrl(struct dlb2_hw *hw)
{
	u32 r0;

	r0 = DLB2_CSR_RD(hw, LSP_LDB_SCHED_PERF_CTRL);
	BIT_SET(r0, LSP_LDB_SCHED_PERF_CTRL_ENAB);
	DLB2_CSR_WR(hw, LSP_LDB_SCHED_PERF_CTRL, r0);
	dlb2_flush_csr(hw);
}

static u64 dlb2_read_perf_counter(struct dlb2_hw *hw,
				  u32 low_offset,
				  u32 high_offset)
{
	u32 low, high, cmp;

	high = DLB2_CSR_RD(hw, high_offset);
	low  = DLB2_CSR_RD(hw, low_offset);
	cmp  = DLB2_CSR_RD(hw, high_offset);

	/* Handle the wrap case */
	if (high != cmp) {
		high = cmp;
		low = DLB2_CSR_RD(hw, low_offset);
	}
	return ((((u64)high) << 32) | low);
}

void dlb2_read_sched_idle_counts(struct dlb2_hw *hw,
				 struct dlb2_sched_idle_counts *data,
				 int counter_idx)
{
	u32 lo, hi;

	memset(data, 0, sizeof(*data));

	switch(counter_idx) {
	case DLB2_LDB_PERF_NOWORK_IDLE_CNT:
		lo = LSP_LDB_SCHED_PERF_0_L;
		hi = LSP_LDB_SCHED_PERF_0_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_NOWORK_IDLE_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_LDB_PERF_NOSPACE_IDLE_CNT:
		lo = LSP_LDB_SCHED_PERF_1_L;
		hi = LSP_LDB_SCHED_PERF_1_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_NOSPACE_IDLE_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_LDB_PERF_SCHED_CNT:
		lo = LSP_LDB_SCHED_PERF_2_L;
		hi = LSP_LDB_SCHED_PERF_2_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_SCHED_CNT] = 
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_LDB_PERF_PFRICTION_IDLE_CNT:
		lo = LSP_LDB_SCHED_PERF_3_L;
		hi = LSP_LDB_SCHED_PERF_3_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_PFRICTION_IDLE_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);

		lo = LSP_LDB_SCHED_PERF_5_L;
		hi = LSP_LDB_SCHED_PERF_5_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_PFRICTION_IDLE_CNT] +=
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_LDB_PERF_IFLIMIT_IDLE_CNT:
		lo = LSP_LDB_SCHED_PERF_4_L;
		hi = LSP_LDB_SCHED_PERF_4_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_IFLIMIT_IDLE_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_LDB_PERF_FIDLIMIT_IDLE_CNT:
		lo = LSP_LDB_SCHED_PERF_6_L;
		hi = LSP_LDB_SCHED_PERF_6_H;
		data->ldb_perf_counters[DLB2_LDB_PERF_FIDLIMIT_IDLE_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_PERF_PROC_ON_CNT:
		lo = CM_PROC_ON_CNT_L;
		hi = CM_PROC_ON_CNT_H;
		data->ldb_perf_counters[DLB2_PERF_PROC_ON_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_PERF_CLK_ON_CNT:
		lo = CM_CLK_ON_CNT_L;
		hi = CM_CLK_ON_CNT_H;
		data->ldb_perf_counters[DLB2_PERF_CLK_ON_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	case DLB2_HW_ERR_CNT:
		lo = SYS_DLB_SYS_CNT_4;
		hi = SYS_DLB_SYS_CNT_5;
		data->ldb_perf_counters[DLB2_HW_ERR_CNT] =
					dlb2_read_perf_counter(hw, lo, hi);

		lo = CHP_CFG_CNTR_CHP_ERR_DROP_L;
		hi = CHP_CFG_CNTR_CHP_ERR_DROP_H;
		data->ldb_perf_counters[DLB2_HW_ERR_CNT] +=
					dlb2_read_perf_counter(hw, lo, hi);
		break;
	}
}

static void dlb2_hw_send_async_pf_to_vf_msg(struct dlb2_hw *hw,
					    unsigned int vf_id)
{
	u32 isr = 0;

	switch (vf_id) {
	case 0:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF0_ISR);
		break;
	case 1:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF1_ISR);
		break;
	case 2:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF2_ISR);
		break;
	case 3:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF3_ISR);
		break;
	case 4:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF4_ISR);
		break;
	case 5:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF5_ISR);
		break;
	case 6:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF6_ISR);
		break;
	case 7:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF7_ISR);
		break;
	case 8:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF8_ISR);
		break;
	case 9:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF9_ISR);
		break;
	case 10:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF10_ISR);
		break;
	case 11:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF11_ISR);
		break;
	case 12:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF12_ISR);
		break;
	case 13:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF13_ISR);
		break;
	case 14:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF14_ISR);
		break;
	case 15:
		BIT_SET(isr, PF_PF2VF_MAILBOX_ISR_VF15_ISR);
		break;
	default:
		break;
	}

	DLB2_FUNC_WR(hw, PF_PF2VF_MAILBOX_ISR(0), isr);
}

static void dlb2_sw_send_async_pf_to_vdev_msg(struct dlb2_hw *hw,
					      unsigned int vdev_id)
{
	void *arg = hw->mbox[vdev_id].pf_to_vdev_inject_arg;

	/* Set the ISR in progress bit. The vdev driver will clear it. */
	*hw->mbox[vdev_id].pf_to_vdev.isr_in_progress = 1;

	hw->mbox[vdev_id].pf_to_vdev_inject(arg);
}

/**
 * dlb2_send_async_pf_to_vdev_msg() - (PF only) send a mailbox message to a
 *					vdev
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function sends a PF->vdev mailbox message. It is asynchronous, so it
 * returns once the message is sent but potentially before the vdev has
 * processed the message. The caller must call dlb2_pf_to_vdev_complete() to
 * determine when the vdev has finished processing the request.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_send_async_pf_to_vdev_msg(struct dlb2_hw *hw, unsigned int vdev_id)
{
	if (hw->virt_mode == DLB2_VIRT_SIOV)
		dlb2_sw_send_async_pf_to_vdev_msg(hw, vdev_id);
	else
		dlb2_hw_send_async_pf_to_vf_msg(hw, vdev_id);
}

static bool dlb2_hw_pf_to_vf_complete(struct dlb2_hw *hw, unsigned int vf_id)
{
	u32 isr;

	isr = DLB2_FUNC_RD(hw, PF_PF2VF_MAILBOX_ISR(vf_id));

	return (isr & (1 << vf_id)) == 0;
}

static bool dlb2_sw_pf_to_vdev_complete(struct dlb2_hw *hw,
					unsigned int vdev_id)
{
	return !(*hw->mbox[vdev_id].pf_to_vdev.isr_in_progress);
}

/**
 * dlb2_pf_to_vdev_complete() - check the status of an asynchronous mailbox
 *			       request
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function returns a boolean indicating whether the vdev has finished
 * processing a PF->vdev mailbox request. It should only be called after
 * sending an asynchronous request with dlb2_send_async_pf_to_vdev_msg().
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_pf_to_vdev_complete(struct dlb2_hw *hw, unsigned int vdev_id)
{
	if (hw->virt_mode == DLB2_VIRT_SIOV)
		return dlb2_sw_pf_to_vdev_complete(hw, vdev_id);
	else
		return dlb2_hw_pf_to_vf_complete(hw, vdev_id);
}

/**
 * dlb2_send_async_vdev_to_pf_msg() - (vdev only) send a mailbox message to
 *				       the PF
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function sends a VF->PF mailbox message. It is asynchronous, so it
 * returns once the message is sent but potentially before the PF has processed
 * the message. The caller must call dlb2_vdev_to_pf_complete() to determine
 * when the PF has finished processing the request.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_send_async_vdev_to_pf_msg(struct dlb2_hw *hw)
{
	u32 isr = 0;
	u32 offs;

	if (hw->virt_mode == DLB2_VIRT_SIOV)
		offs = VF_SIOV_MBOX_ISR_TRIGGER;
	else
		offs = VF_VF2PF_MAILBOX_ISR;

	BIT_SET(isr, VF_VF2PF_MAILBOX_ISR_ISR);

	DLB2_FUNC_WR(hw, offs, isr);
}

/**
 * dlb2_vdev_to_pf_complete() - check the status of an asynchronous mailbox
 *				 request
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether the PF has finished
 * processing a VF->PF mailbox request. It should only be called after sending
 * an asynchronous request with dlb2_send_async_vdev_to_pf_msg().
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_vdev_to_pf_complete(struct dlb2_hw *hw)
{
	u32 isr;

	isr = DLB2_FUNC_RD(hw, VF_VF2PF_MAILBOX_ISR);

	return (BITS_GET(isr, VF_VF2PF_MAILBOX_ISR_ISR) == 0);
}

/**
 * dlb2_vf_flr_complete() - check the status of a VF FLR
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether the PF has finished
 * executing the VF FLR. It should only be called after setting the VF's FLR
 * bit.
 */
bool dlb2_vf_flr_complete(struct dlb2_hw *hw)
{
	u32 rip;

	rip = DLB2_FUNC_RD(hw, VF_VF_RESET_IN_PROGRESS);

	return (BITS_GET(rip, VF_VF_RESET_IN_PROGRESS_RESET_IN_PROGRESS) == 0);
}

static u32 dlb2_read_vf2pf_mbox(struct dlb2_hw *hw,
				unsigned int id,
				int offs,
				bool req)
{
	u32 idx = offs;

	if (req)
		idx += DLB2_VF2PF_REQ_BASE_WORD;
	else
		idx += DLB2_VF2PF_RESP_BASE_WORD;

	if (hw->virt_mode == DLB2_VIRT_SIOV)
		return hw->mbox[id].vdev_to_pf.mbox[idx];
	else
		return DLB2_FUNC_RD(hw, PF_VF2PF_MAILBOX(id, idx));
}

/**
 * dlb2_pf_read_vf_mbox_req() - (PF only) read a VF->PF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies one of the PF's VF->PF mailboxes into the array pointed
 * to by data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_REQ_BYTES.
 */
int dlb2_pf_read_vf_mbox_req(struct dlb2_hw *hw,
			     unsigned int vdev_id,
			     void *data,
			     int len)
{
	u32 buf[DLB2_VF2PF_REQ_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_VF2PF_REQ_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > VF->PF mailbox req size\n",
			    __func__, len);
		return -EINVAL;
	}

	if (len == 0) {
		DLB2_HW_ERR(hw, "[%s()] invalid len (0)\n", __func__);
		return -EINVAL;
	}

	if (hw->virt_mode == DLB2_VIRT_SIOV &&
	    !hw->mbox[vdev_id].vdev_to_pf.mbox) {
		DLB2_HW_ERR(hw, "[%s()] No mailbox registered for vdev %d\n",
			    __func__, vdev_id);
		return -EINVAL;
	}

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++)
		buf[i] = dlb2_read_vf2pf_mbox(hw, vdev_id, i, true);

	memcpy(data, buf, len);

	return 0;
}

/**
 * dlb2_pf_read_vf_mbox_resp() - (PF only) read a VF->PF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies one of the PF's VF->PF mailboxes into the array pointed
 * to by data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_RESP_BYTES.
 */
int dlb2_pf_read_vf_mbox_resp(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      void *data,
			      int len)
{
	u32 buf[DLB2_VF2PF_RESP_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_VF2PF_RESP_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > VF->PF mailbox resp size\n",
			    __func__, len);
		return -EINVAL;
	}

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++)
		buf[i] = dlb2_read_vf2pf_mbox(hw, vdev_id, i, false);

	memcpy(data, buf, len);

	return 0;
}

static void dlb2_write_pf2vf_mbox_resp(struct dlb2_hw *hw,
				       unsigned int vdev_id,
				       int offs,
				       u32 data)
{
	u32 idx = offs + DLB2_PF2VF_RESP_BASE_WORD;

	if (hw->virt_mode == DLB2_VIRT_SIOV)
		hw->mbox[vdev_id].pf_to_vdev.mbox[idx] = data;
	else
		DLB2_FUNC_WR(hw,
			     PF_PF2VF_MAILBOX(vdev_id, idx),
			     data);
}

/**
 * dlb2_pf_write_vf_mbox_resp() - (PF only) write a PF->VF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the PF's VF->PF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_RESP_BYTES.
 */
int dlb2_pf_write_vf_mbox_resp(struct dlb2_hw *hw,
			       unsigned int vdev_id,
			       void *data,
			       int len)
{
	u32 buf[DLB2_PF2VF_RESP_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_PF2VF_RESP_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > PF->VF mailbox resp size\n",
			    __func__, len);
		return -EINVAL;
	}

	if (hw->virt_mode == DLB2_VIRT_SIOV &&
	    !hw->mbox[vdev_id].pf_to_vdev.mbox) {
		DLB2_HW_ERR(hw, "[%s()] No mailbox registered for vdev %d\n",
			    __func__, vdev_id);
		return -EINVAL;
	}

	memcpy(buf, data, len);

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++)
		dlb2_write_pf2vf_mbox_resp(hw, vdev_id, i, buf[i]);

	return 0;
}

static void dlb2_write_pf2vf_mbox_req(struct dlb2_hw *hw,
				      unsigned int vdev_id,
				      int offs,
				      u32 data)
{
	u32 idx = offs + DLB2_PF2VF_REQ_BASE_WORD;

	if (hw->virt_mode == DLB2_VIRT_SIOV)
		hw->mbox[vdev_id].pf_to_vdev.mbox[idx] = data;
	else
		DLB2_FUNC_WR(hw,
			     PF_PF2VF_MAILBOX(vdev_id, idx),
			     data);
}

/**
 * dlb2_pf_write_vf_mbox_req() - (PF only) write a PF->VF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the PF's VF->PF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_REQ_BYTES.
 */
int dlb2_pf_write_vf_mbox_req(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      void *data,
			      int len)
{
	u32 buf[DLB2_PF2VF_REQ_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_PF2VF_REQ_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > PF->VF mailbox req size\n",
			    __func__, len);
		return -EINVAL;
	}

	memcpy(buf, data, len);

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++)
		dlb2_write_pf2vf_mbox_req(hw, vdev_id, i, buf[i]);

	return 0;
}

/**
 * dlb2_vf_read_pf_mbox_resp() - (VF only) read a PF->VF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the VF's PF->VF mailbox into the array pointed to by
 * data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_RESP_BYTES.
 */
int dlb2_vf_read_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len)
{
	u32 buf[DLB2_PF2VF_RESP_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_PF2VF_RESP_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > PF->VF mailbox resp size\n",
			    __func__, len);
		return -EINVAL;
	}

	if (len == 0) {
		DLB2_HW_ERR(hw, "[%s()] invalid len (0)\n", __func__);
		return -EINVAL;
	}

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++) {
		u32 idx = i + DLB2_PF2VF_RESP_BASE_WORD;

		buf[i] = DLB2_FUNC_RD(hw, VF_PF2VF_MAILBOX(idx));
	}

	memcpy(data, buf, len);

	return 0;
}

/**
 * dlb2_vf_read_pf_mbox_req() - (VF only) read a PF->VF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the VF's PF->VF mailbox into the array pointed to by
 * data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_REQ_BYTES.
 */
int dlb2_vf_read_pf_mbox_req(struct dlb2_hw *hw, void *data, int len)
{
	u32 buf[DLB2_PF2VF_REQ_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_PF2VF_REQ_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > PF->VF mailbox req size\n",
			    __func__, len);
		return -EINVAL;
	}

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if ((len % 4) != 0)
		num_words++;

	for (i = 0; i < num_words; i++) {
		u32 idx = i + DLB2_PF2VF_REQ_BASE_WORD;

		buf[i] = DLB2_FUNC_RD(hw, VF_PF2VF_MAILBOX(idx));
	}

	memcpy(data, buf, len);

	return 0;
}

/**
 * dlb2_vf_write_pf_mbox_req() - (VF only) write a VF->PF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the VF's PF->VF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_REQ_BYTES.
 */
int dlb2_vf_write_pf_mbox_req(struct dlb2_hw *hw, void *data, int len)
{
	u32 buf[DLB2_VF2PF_REQ_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_VF2PF_REQ_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > VF->PF mailbox req size\n",
			    __func__, len);
		return -EINVAL;
	}

	memcpy(buf, data, len);

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++) {
		u32 idx = i + DLB2_VF2PF_REQ_BASE_WORD;

		DLB2_FUNC_WR(hw, VF_VF2PF_MAILBOX(idx), buf[i]);
	}

	return 0;
}

/**
 * dlb2_vf_write_pf_mbox_resp() - (VF only) write a VF->PF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the VF's PF->VF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_RESP_BYTES.
 */
int dlb2_vf_write_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len)
{
	u32 buf[DLB2_VF2PF_RESP_BYTES / 4];
	int num_words;
	int i;

	if (len > DLB2_VF2PF_RESP_BYTES) {
		DLB2_HW_ERR(hw,
			    "[%s()] len (%d) > VF->PF mailbox resp size\n",
			    __func__, len);
		return -EINVAL;
	}

	memcpy(buf, data, len);

	/*
	 * Round up len to the nearest 4B boundary, since the mailbox registers
	 * are 32b wide.
	 */
	num_words = len / 4;
	if (len % 4 != 0)
		num_words++;

	for (i = 0; i < num_words; i++) {
		u32 idx = i + DLB2_VF2PF_RESP_BASE_WORD;

		DLB2_FUNC_WR(hw, VF_VF2PF_MAILBOX(idx), buf[i]);
	}

	return 0;
}

/**
 * dlb2_vdev_is_locked() - check whether the vdev's resources are locked
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function returns whether or not the vdev's resource assignments are
 * locked. If locked, no resources can be added to or subtracted from the
 * group.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_vdev_is_locked(struct dlb2_hw *hw, unsigned int id)
{
	return hw->vdev[id].locked;
}

/**
 * dlb2_vdev_set_ims_idx() - Set ims index for vdev ports.
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @ims_idx: IMS index array
 *
 * This function sets IMS index for each of ports in a vdev.
 *
 * The function is for a Scalable IOV virtual device only, and should be
 * called only after vdev is locked.
 */
void dlb2_vdev_set_ims_idx(struct dlb2_hw *hw, unsigned int id, u32 *ims_idx)
{
	struct dlb2_function_resources *rsrcs = &hw->vdev[id];
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_port *ldb_port;
	int num_ldb_ports;
	int i, j;

	i = 0;
	for (j = 0; j < DLB2_NUM_COS_DOMAINS; j++) {
		DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[j], ldb_port, iter) {
			ldb_port->id.ims_idx = ims_idx[i];
			i++;
		}
	}

	num_ldb_ports = i;
	i = 0;
	DLB2_FUNC_LIST_FOR(rsrcs->avail_dir_pq_pairs, dir_port, iter) {
		dir_port->id.ims_idx = ims_idx[i + num_ldb_ports];
		i++;
	}
}

static void dlb2_vf_set_rsrc_virt_ids(struct dlb2_function_resources *rsrcs,
				      unsigned int id)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_hw_domain *domain;
	int i, j;

	i = 0;
	DLB2_FUNC_LIST_FOR(rsrcs->avail_domains, domain, iter) {
		domain->id.virt_id = i;
		domain->id.vdev_owned = true;
		domain->id.vdev_id = id;
		i++;
	}

	i = 0;
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_queues, ldb_queue, iter) {
		ldb_queue->id.virt_id = i;
		ldb_queue->id.vdev_owned = true;
		ldb_queue->id.vdev_id = id;
		i++;
	}

	i = 0;
	for (j = 0; j < DLB2_NUM_COS_DOMAINS; j++) {
		DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[j], ldb_port, iter) {
			ldb_port->id.virt_id = i;
			ldb_port->id.vdev_owned = true;
			ldb_port->id.vdev_id = id;
			i++;
		}
	}

	i = 0;
	DLB2_FUNC_LIST_FOR(rsrcs->avail_dir_pq_pairs, dir_port, iter) {
		dir_port->id.virt_id = i;
		dir_port->id.vdev_owned = true;
		dir_port->id.vdev_id = id;
		i++;
	}
}

/**
 * dlb2_lock_vdev() - lock the vdev's resources
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function sets a flag indicating that the vdev is using its resources.
 * When the vdev is locked, its resource assignment cannot be changed.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_lock_vdev(struct dlb2_hw *hw, unsigned int id)
{
	struct dlb2_function_resources *rsrcs = &hw->vdev[id];

	rsrcs->locked = true;

	dlb2_vf_set_rsrc_virt_ids(rsrcs, id);
}

/**
 * dlb2_unlock_vdev() - unlock the vdev's resources
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function unlocks the vdev's resource assignment, allowing it to be
 * modified.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_unlock_vdev(struct dlb2_hw *hw, unsigned int id)
{
	hw->vdev[id].locked = false;
}

/**
 * dlb2_reset_vdev_resources() - reassign the vdev's resources to the PF
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function takes any resources currently assigned to the vdev and
 * reassigns them to the PF.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_reset_vdev_resources(struct dlb2_hw *hw, unsigned int id)
{
	if (id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	/* If the VF is locked, its resource assignment can't be changed */
	if (dlb2_vdev_is_locked(hw, id))
		return -EPERM;

	dlb2_update_vdev_sched_domains(hw, id, 0);
	dlb2_update_vdev_ldb_queues(hw, id, 0);
	dlb2_update_vdev_ldb_ports(hw, id, 0);
	dlb2_update_vdev_dir_ports(hw, id, 0);
	dlb2_update_vdev_ldb_credits(hw, id, 0);
	dlb2_update_vdev_dir_credits(hw, id, 0);
	dlb2_update_vdev_hist_list_entries(hw, id, 0);
	dlb2_update_vdev_atomic_inflights(hw, id, 0);

	dlb2_update_vdev_sn_slots(hw, id, 0, 0);
	dlb2_update_vdev_sn_slots(hw, id, 1, 0);
	return 0;
}

/**
 * dlb2_clr_pmcsr_disable() - power on bulk of DLB 2.0 logic
 * @hw: dlb2_hw handle for a particular device.
 * @ver: device version.
 *
 * Clearing the PMCSR must be done at initialization to make the device fully
 * operational.
 */
void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw, enum dlb2_hw_ver ver)
{
	u32 pmcsr_dis;

	pmcsr_dis = DLB2_CSR_RD(hw, CM_CFG_PM_PMCSR_DISABLE(ver));

	BITS_CLR(pmcsr_dis, CM_CFG_PM_PMCSR_DISABLE_DISABLE);

	DLB2_CSR_WR(hw, CM_CFG_PM_PMCSR_DISABLE(ver), pmcsr_dis);
}

/**
 * dlb2_hw_set_virt_mode() - set the device's virtualization mode
 * @hw: dlb2_hw handle for a particular device.
 * @mode: either none, SR-IOV, or Scalable IOV.
 *
 * This function sets the virtualization mode of the device. This controls
 * whether the device uses a software or hardware mailbox.
 *
 * This should be called by the PF driver when either SR-IOV or Scalable IOV is
 * selected as the virtualization mechanism, and by the VF/VDEV driver during
 * initialization after recognizing itself as an SR-IOV or Scalable IOV device.
 *
 * Errors:
 * EINVAL - Invalid mode.
 */
int dlb2_hw_set_virt_mode(struct dlb2_hw *hw, enum dlb2_virt_mode mode)
{
	if (mode >= NUM_DLB2_VIRT_MODES)
		return -EINVAL;

	hw->virt_mode = mode;

	return 0;
}

/**
 * dlb2_hw_get_virt_mode() - get the device's virtualization mode
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function gets the virtualization mode of the device.
 */
enum dlb2_virt_mode dlb2_hw_get_virt_mode(struct dlb2_hw *hw)
{
	return hw->virt_mode;
}

/**
 * dlb2_hw_get_ldb_port_phys_id() - get a physical port ID from its virt ID
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual port ID.
 * @vdev_id: vdev ID.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
s32 dlb2_hw_get_ldb_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id)
{
	struct dlb2_ldb_port *port;

	port = dlb2_get_ldb_port_from_id(hw, id, true, vdev_id);
	if (!port)
		return -1;

	return port->id.phys_id;
}

/**
 * dlb2_hw_get_dir_port_phys_id() - get a physical port ID from its virt ID
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual port ID.
 * @vdev_id: vdev ID.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
s32 dlb2_hw_get_dir_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;

	port = dlb2_get_dir_pq_from_id(hw, id, true, vdev_id);
	if (!port)
		return -1;

	return port->id.phys_id;
}

/**
 * dlb2_hw_register_sw_mbox() - register a software mailbox
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @vdev_to_pf_mbox: pointer to a 4KB memory page for vdev->PF communication.
 * @pf_to_vdev_mbox: pointer to a 4KB memory page for PF->vdev communication.
 * @inject: callback function for injecting a PF->vdev interrupt.
 * @inject_arg: user argument for inject callback.
 *
 * When Scalable IOV is enabled, the VDCM must register a software mailbox for
 * every virtual device during vdev creation.
 *
 * This function notifies the driver to use a software mailbox using the
 * provided pointers, instead of the device's hardware mailbox. When the driver
 * calls mailbox functions like dlb2_pf_write_vf_mbox_req(), the request will
 * go to the software mailbox instead of the hardware one. This is used in
 * Scalable IOV virtualization.
 */
void dlb2_hw_register_sw_mbox(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      u32 *vdev_to_pf_mbox,
			      u32 *pf_to_vdev_mbox,
			      void (*inject)(void *),
			      void *inject_arg)
{
	u32 offs;

	offs = VF_VF2PF_MAILBOX_ISR % 0x1000;

	hw->mbox[vdev_id].vdev_to_pf.mbox = vdev_to_pf_mbox;
	hw->mbox[vdev_id].vdev_to_pf.isr_in_progress =
		(u32 *)((u8 *)vdev_to_pf_mbox + offs);

	offs = (VF_PF2VF_MAILBOX_ISR % 0x1000);

	hw->mbox[vdev_id].pf_to_vdev.mbox = pf_to_vdev_mbox;
	hw->mbox[vdev_id].pf_to_vdev.isr_in_progress =
		(u32 *)((u8 *)pf_to_vdev_mbox + offs);

	hw->mbox[vdev_id].pf_to_vdev_inject = inject;
	hw->mbox[vdev_id].pf_to_vdev_inject_arg = inject_arg;
}

/**
 * dlb2_hw_unregister_sw_mbox() - unregister a software mailbox
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function notifies the driver to stop using a previously registered
 * software mailbox.
 */
void dlb2_hw_unregister_sw_mbox(struct dlb2_hw *hw, unsigned int vdev_id)
{
	hw->mbox[vdev_id].vdev_to_pf.mbox = NULL;
	hw->mbox[vdev_id].pf_to_vdev.mbox = NULL;
	hw->mbox[vdev_id].vdev_to_pf.isr_in_progress = NULL;
	hw->mbox[vdev_id].pf_to_vdev.isr_in_progress = NULL;
	hw->mbox[vdev_id].pf_to_vdev_inject = NULL;
	hw->mbox[vdev_id].pf_to_vdev_inject_arg = NULL;
}

/**
 * dlb2_hw_register_pasid() - register a vdev's PASID
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @pasid: the vdev's PASID.
 *
 * This function stores the user-supplied PASID, and uses it when configuring
 * the vdev's CQs.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
int dlb2_hw_register_pasid(struct dlb2_hw *hw,
			   unsigned int vdev_id,
			   unsigned int pasid)
{
	if (vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -1;

	hw->pasid[vdev_id] = pasid;

	return 0;
}

/**
 * dlb2_hw_get_cos_bandwidth() - returns the percent of bandwidth allocated
 *	to a port class-of-service.
 * @hw: dlb2_hw handle for a particular device.
 * @cos_id: class-of-service ID.
 *
 * Return:
 * Returns -EINVAL if cos_id is invalid, else the class' bandwidth allocation.
 */
int dlb2_hw_get_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id)
{
	if (cos_id >= DLB2_NUM_COS_DOMAINS)
		return -EINVAL;

	return hw->cos_reservation[cos_id];
}

static void dlb2_log_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bw)
{
	DLB2_HW_DBG(hw, "DLB2 set port CoS bandwidth:\n");
	DLB2_HW_DBG(hw, "\tCoS ID:    %u\n", cos_id);
	DLB2_HW_DBG(hw, "\tBandwidth: %u\n", bw);
}

#define DLB2_MAX_BW_PCT 100

/**
 * dlb2_hw_set_cos_bandwidth() - set a bandwidth allocation percentage for a
 *	port class-of-service.
 * @hw: dlb2_hw handle for a particular device.
 * @cos_id: class-of-service ID.
 * @bandwidth: class-of-service bandwidth.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid cos ID, bandwidth is greater than 100, or bandwidth would
 *	    cause the total bandwidth across all classes of service to exceed
 *	    100%.
 */
int dlb2_hw_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth)
{
	struct dlb2 *dlb2 = container_of(hw, struct dlb2, hw);
	unsigned int i;
	u8 total;

	if (cos_id >= DLB2_NUM_COS_DOMAINS)
		return -EINVAL;

	if (bandwidth > DLB2_MAX_BW_PCT)
		return -EINVAL;

	total = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		total += (i == cos_id) ? bandwidth : hw->cos_reservation[i];

	if (total > DLB2_MAX_BW_PCT)
		return -EINVAL;

	/* MMIO registers are accessible only when the device is active (
	 * (in D0 PCI state). User may use sysfs to set parameter when the
	 * device is in D3 state. val is saved in driver, is used to reconfigure
	 * the system when the device is waked up.
	 */
	if(!pm_runtime_suspended(&dlb2->pdev->dev)) {
		u32 reg;

		reg = DLB2_CSR_RD(hw, LSP_CFG_SHDW_RANGE_COS(hw->ver, cos_id));

		/*
		* Normalize the bandwidth to a value in the range 0-255. Integer
		* division may leave unreserved scheduling slots; these will be
		* divided among the 4 classes of service.
		*/
		BITS_SET(reg, (bandwidth * 256) / 100, LSP_CFG_SHDW_RANGE_COS_BW_RANGE);
		DLB2_CSR_WR(hw, LSP_CFG_SHDW_RANGE_COS(hw->ver, cos_id), reg);

		reg = 0;
		BIT_SET(reg, LSP_CFG_SHDW_CTRL_TRANSFER);
		/* Atomically transfer the newly configured service weight */
		DLB2_CSR_WR(hw, LSP_CFG_SHDW_CTRL(hw->ver), reg);
	}

	dlb2_log_set_cos_bandwidth(hw, cos_id, bandwidth);

	hw->cos_reservation[cos_id] = bandwidth;

	return 0;
}

struct dlb2_wd_config {
	u32 threshold;
	u32 interval;
};

/**
 * dlb2_hw_enable_wd_timer() - enable the CQ watchdog timers with a
 *	caller-specified timeout.
 * @hw: dlb2_hw handle for a particular device.
 * @tmo: watchdog timeout.
 *
 * This function should be called during device initialization and after reset.
 * The watchdog timer interrupt must also be enabled per-CQ, using either
 * dlb2_hw_enable_dir_cq_wd_int() or dlb2_hw_enable_ldb_cq_wd_int().
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid timeout.
 */
int dlb2_hw_enable_wd_timer(struct dlb2_hw *hw, enum dlb2_wd_tmo tmo)
{
	/* Timeout = num_ports * threshold * (sample interval + 1) / 100 MHz */
	const struct dlb2_wd_config wd_config[NUM_DLB2_WD_TMOS] = {
		[DLB2_WD_TMO_40S] = {30, 0x1FFFFF},
		[DLB2_WD_TMO_10S] = {30, 0x7FFFF},
		[DLB2_WD_TMO_1S]  = {24, 0xFFFF},
	};
	u32 dir_thresh = 0;
	u32 ldb_thresh = 0;
	u32 dir_en = 0;
	u32 ldb_en = 0;

	if (tmo >= NUM_DLB2_WD_TMOS)
		return -EINVAL;

	BITS_SET(dir_thresh, wd_config[tmo].threshold, CHP_CFG_DIR_WD_THRESHOLD_WD_THRESHOLD);
	BITS_SET(ldb_thresh, wd_config[tmo].threshold, CHP_CFG_LDB_WD_THRESHOLD_WD_THRESHOLD);

	DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_THRESHOLD(hw->ver), dir_thresh);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_THRESHOLD(hw->ver), ldb_thresh);

	BITS_SET(dir_en, wd_config[tmo].interval, CHP_CFG_DIR_WD_ENB_INTERVAL_SAMPLE_INTERVAL);
	BITS_SET(ldb_en, wd_config[tmo].interval, CHP_CFG_LDB_WD_ENB_INTERVAL_SAMPLE_INTERVAL);
	BIT_SET(dir_en, CHP_CFG_DIR_WD_ENB_INTERVAL_ENB);
	BIT_SET(ldb_en, CHP_CFG_LDB_WD_ENB_INTERVAL_ENB);

	/* If running on the emulation platform, adjust accordingly */
	if (DLB2_HZ == 2000000) {
		BITS_SET(dir_en, (dir_en
			 & CHP_CFG_DIR_WD_ENB_INTERVAL_SAMPLE_INTERVAL) / 400,
			 CHP_CFG_DIR_WD_ENB_INTERVAL_SAMPLE_INTERVAL);
		BITS_SET(ldb_en, (ldb_en
			 & CHP_CFG_LDB_WD_ENB_INTERVAL_SAMPLE_INTERVAL) / 400,
			 CHP_CFG_LDB_WD_ENB_INTERVAL_SAMPLE_INTERVAL);
	}

	DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_ENB_INTERVAL(hw->ver), dir_en);
	DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_ENB_INTERVAL(hw->ver), ldb_en);

	return 0;
}

/**
 * dlb2_hw_enable_dir_cq_wd_int() - enable the CQ watchdog interrupt on an
 *	individual CQ.
 * @hw: dlb2_hw handle for a particular device.
 * @id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid directed port ID.
 */
int dlb2_hw_enable_dir_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	u32 wd_dis = 0;
	u32 wd_en = 0;

	port = dlb2_get_dir_pq_from_id(hw, id, vdev_req, vdev_id);
	if (!port)
		return -EINVAL;

	BIT_SET(wd_en, CHP_DIR_CQ_WD_ENB_WD_ENABLE);

	DLB2_CSR_WR(hw, CHP_DIR_CQ_WD_ENB(hw->ver, port->id.phys_id), wd_en);

	wd_dis |= 1 << (port->id.phys_id % 32);

	/* WD_DISABLE registers are W1CLR */
	if (port->id.phys_id < 32)
		DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE0(hw->ver), wd_dis);
	else if (port->id.phys_id >= 32 && port->id.phys_id < 64)
		DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE1(hw->ver), wd_dis);
	else
		DLB2_CSR_WR(hw, CHP_CFG_DIR_WD_DISABLE2, wd_dis);

	return 0;
}

/**
 * dlb2_hw_enable_ldb_cq_wd_int() - enable the CQ watchdog interrupt on an
 *	individual CQ.
 * @hw: dlb2_hw handle for a particular device.
 * @id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid load-balanced port ID.
 */
int dlb2_hw_enable_ldb_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id)
{
	struct dlb2_ldb_port *port;
	u32 wd_dis = 0;
	u32 wd_en = 0;

	port = dlb2_get_ldb_port_from_id(hw, id, vdev_req, vdev_id);
	if (!port)
		return -EINVAL;

	BIT_SET(wd_en, CHP_LDB_CQ_WD_ENB_WD_ENABLE);

	DLB2_CSR_WR(hw, CHP_LDB_CQ_WD_ENB(hw->ver, port->id.phys_id), wd_en);

	wd_dis |= 1 << (port->id.phys_id % 32);

	/* WD_DISABLE registers are W1CLR */
	if (port->id.phys_id < 32)
		DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_DISABLE0(hw->ver), wd_dis);
	else
		DLB2_CSR_WR(hw, CHP_CFG_LDB_WD_DISABLE1(hw->ver), wd_dis);

	return 0;
}

/**
 * dlb2_hw_enable_sparse_ldb_cq_mode() - enable sparse mode for load-balanced
 *	ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw)
{
	u32 ctrl;

	ctrl = DLB2_CSR_RD(hw, CHP_CFG_CHP_CSR_CTRL);

	BIT_SET(ctrl, CHP_CFG_CHP_CSR_CTRL_CFG_64BYTES_QE_LDB_CQ_MODE);

	DLB2_CSR_WR(hw, CHP_CFG_CHP_CSR_CTRL, ctrl);
}

/**
 * dlb2_hw_enable_sparse_dir_cq_mode() - enable sparse mode for directed ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw)
{
	u32 ctrl;

	ctrl = DLB2_CSR_RD(hw, CHP_CFG_CHP_CSR_CTRL);

	BIT_SET(ctrl, CHP_CFG_CHP_CSR_CTRL_CFG_64BYTES_QE_DIR_CQ_MODE);

	DLB2_CSR_WR(hw, CHP_CFG_CHP_CSR_CTRL, ctrl);
}

/**
 * dlb2_hw_set_qe_arbiter_weights() - program QE arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qe_arbiter_weights(struct dlb2_hw *hw, u8 weight[8])
{
	u32 reg = 0;

	BITS_SET(reg, weight[1], ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN0);
	BITS_SET(reg, weight[3], ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN1);
	BITS_SET(reg, weight[5], ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN2);
	BITS_SET(reg, weight[7], ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN3);
	DLB2_CSR_WR(hw, ATM_CFG_ARB_WEIGHTS_RDY_BIN, reg);

	reg = 0;
	BITS_SET(reg, weight[1], NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI0);
	BITS_SET(reg, weight[3], NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI1);
	BITS_SET(reg, weight[5], NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI2);
	BITS_SET(reg, weight[7], NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI3);
	DLB2_CSR_WR(hw, NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0(hw->ver), reg);

	reg = 0;
	BITS_SET(reg, weight[1], NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI0);
	BITS_SET(reg, weight[3], NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI1);
	BITS_SET(reg, weight[5], NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI2);
	BITS_SET(reg, weight[7], NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI3);
	DLB2_CSR_WR(hw, NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0(hw->ver), reg);

	reg = 0;
	BITS_SET(reg, weight[1], DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI0);
	BITS_SET(reg, weight[3], DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI1);
	BITS_SET(reg, weight[5], DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI2);
	BITS_SET(reg, weight[7], DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI3);
	DLB2_CSR_WR(hw, DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0, reg);

	reg = 0;
	BITS_SET(reg, weight[1], DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI0);
	BITS_SET(reg, weight[3], DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI1);
	BITS_SET(reg, weight[5], DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI2);
	BITS_SET(reg, weight[7], DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI3);
	DLB2_CSR_WR(hw, DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0, reg);

	reg = 0;
	BITS_SET(reg, weight[1], NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI0);
	BITS_SET(reg, weight[3], NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI1);
	BITS_SET(reg, weight[5], NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI2);
	BITS_SET(reg, weight[7], NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI3);
	DLB2_CSR_WR(hw, NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0(hw->ver), reg);

	reg = 0;
	BITS_SET(reg, weight[1], ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN0);
	BITS_SET(reg, weight[3], ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN1);
	BITS_SET(reg, weight[5], ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN2);
	BITS_SET(reg, weight[7], ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN3);
	DLB2_CSR_WR(hw, ATM_CFG_ARB_WEIGHTS_SCHED_BIN, reg);

	reg = 0;
	BITS_SET(reg, weight[1], AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI0);
	BITS_SET(reg, weight[3], AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI1);
	BITS_SET(reg, weight[5], AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI2);
	BITS_SET(reg, weight[7], AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI3);
	DLB2_CSR_WR(hw, AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0, reg);
}

/**
 * dlb2_hw_set_qid_arbiter_weights() - program QID arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qid_arbiter_weights(struct dlb2_hw *hw, u8 weight[8])
{
	u32 reg = 0;

	BITS_SET(reg, weight[1], LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI0_WEIGHT);
	BITS_SET(reg, weight[3], LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI1_WEIGHT);
	BITS_SET(reg, weight[5], LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI2_WEIGHT);
	BITS_SET(reg, weight[7], LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI3_WEIGHT);
	DLB2_CSR_WR(hw, LSP_CFG_ARB_WEIGHT_LDB_QID_0(hw->ver), reg);

	reg = 0;
	BITS_SET(reg, weight[1], LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI0_WEIGHT);
	BITS_SET(reg, weight[3], LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI1_WEIGHT);
	BITS_SET(reg, weight[5], LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI2_WEIGHT);
	BITS_SET(reg, weight[7], LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI3_WEIGHT);
	DLB2_CSR_WR(hw, LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0(hw->ver), reg);
}

static void dlb2_log_enable_cq_weight(struct dlb2_hw *hw,
				      u32 domain_id,
				      struct dlb2_enable_cq_weight_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 enable CQ weight arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    args->port_id);
	DLB2_HW_DBG(hw, "\tLimit:   %d\n",
		    args->limit);
}

static int
dlb2_verify_enable_cq_weight_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_enable_cq_weight_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;

	if (hw->ver == DLB2_HW_V2) {
		resp->status = DLB2_ST_FEATURE_UNAVAILABLE;
		return -EINVAL;
	}

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (domain->started) {
		resp->status = DLB2_ST_DOMAIN_STARTED;
		return -EINVAL;
	}

	port = dlb2_get_domain_used_ldb_port(args->port_id, vdev_req, domain);
	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (args->limit == 0 || args->limit > port->cq_depth) {
		resp->status = DLB2_ST_INVALID_CQ_WEIGHT_LIMIT;
		return -EINVAL;
	}

	return 0;
}

int dlb2_enable_cq_weight(struct dlb2_hw *hw,
			  u32 domain_id,
			  struct dlb2_enable_cq_weight_args *args,
			  struct dlb2_cmd_response *resp,
			  bool vdev_req,
			  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret, id;
	u32 reg = 0;

	dlb2_log_enable_cq_weight(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_enable_cq_weight_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id);
	if (ret)
		return ret;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (!domain) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: domain not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);
	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	BIT_SET(reg, LSP_CFG_CQ_LDB_WU_LIMIT_V);
	BITS_SET(reg, args->limit, LSP_CFG_CQ_LDB_WU_LIMIT_LIMIT);

	DLB2_CSR_WR(hw, LSP_CFG_CQ_LDB_WU_LIMIT(port->id.phys_id), reg);

	resp->status = 0;

	return 0;
}

int dlb2_cq_inflight_ctrl(struct dlb2_hw *hw,
			  u32 domain_id,
			  struct dlb2_cq_inflight_ctrl_args *args,
			  struct dlb2_cmd_response *resp,
			  bool vdev_req,
			  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	u32 reg = 0;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (!domain) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: domain not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	id = args->port_id;

	port = dlb2_get_domain_ldb_port(id, vdev_req, domain);
	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}
	BITS_SET(reg, args->enable, LSP_CFG_CTRL_GENERAL_0_ENAB_IF_THRESH_V2_5);
	DLB2_CSR_WR(hw, V2_5LSP_CFG_CTRL_GENERAL_0, reg);

	if (args->enable) {
		reg = 0;
		BITS_SET(reg, args->threshold, LSP_CQ_LDB_INFL_THRESH_THRESH);
		DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_THRESH(port->id.phys_id), reg);
	}

	resp->status = 0;

	return 0;
}

void dlb2_hw_set_rate_limit(struct dlb2_hw *hw, int rate_limit)
{
	u32 reg;

	reg = DLB2_CSR_RD(hw, SYS_WRITE_BUFFER_CTL);

	BITS_SET(reg, rate_limit, SYS_WRITE_BUFFER_CTL_SCH_RATE_LIMIT(hw->ver));
	DLB2_CSR_WR(hw, SYS_WRITE_BUFFER_CTL, reg);
}

void dlb2_hw_set_qidx_wrr_scheduler_weight(struct dlb2_hw *hw, int weight)
{
	u32 reg;

	reg = DLB2_CSR_RD(hw, LSP_CFG_LSP_CSR_CONTROL(hw->ver));

	BITS_SET(reg, weight, LSP_CFG_LSP_CSR_CONTROL_LDB_WRR_COUNT_BASE_V2_5);
	DLB2_CSR_WR(hw, LSP_CFG_LSP_CSR_CONTROL(hw->ver), reg);
}

int dlb2_get_xstats(struct dlb2_hw *hw, struct dlb2_xstats_args *args,
		    bool vdev_req, unsigned int vdev_id)
{
	uint16_t xstats_base = DLB2_GET_XSTATS_BASE(args->xstats_type);
	uint64_t val = 0;
	int id = -1;

	if(xstats_base >= MAX_XSTATS) {
		return -EINVAL;
	}

	if (xstats_base == LDB_QUEUE_XSTATS) {
		struct dlb2_ldb_queue *queue;

		queue = dlb2_get_ldb_queue_from_id(hw, args->xstats_id,
						   vdev_req, vdev_id);
		if (queue)
			id = queue->id.phys_id;
	} else if (xstats_base == LDB_PORT_XSTATS) {
		struct dlb2_ldb_port *port;

		port = dlb2_get_ldb_port_from_id(hw, args->xstats_id,
						 vdev_req, vdev_id);
		if (port)
			id = port->id.phys_id;
	} else if (xstats_base == DIR_PQ_XSTATS) {
		struct dlb2_dir_pq_pair *pq;

		pq = dlb2_get_dir_pq_from_id(hw, args->xstats_id,
					     vdev_req, vdev_id);
		if (pq)
			id = pq->id.phys_id;
	}

	if (id == -1)
		return 0;

	if (args->xstats_type == DLB_CFG_QID_LDB_INFLIGHT_COUNT)
		val = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_CNT(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_LDB_INFLIGHT_LIMIT)
		val = DLB2_CSR_RD(hw, LSP_QID_LDB_INFL_LIM(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_ATM_ACTIVE)
		val = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_ATM_DEPTH_THRSH)
		val = DLB2_CSR_RD(hw, LSP_QID_ATM_DEPTH_THRSH(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_NALB_DEPTH_THRSH)
		val = DLB2_CSR_RD(hw, LSP_QID_NALDB_DEPTH_THRSH(hw->ver, id));
	/*else if (args->xstats_type == DLB_CFG_QID_ATQ_ENQ_CNT)
		val = DLB2_CSR_RD(hw, LSP_QID_ATQ_(hw->ver, id));*/
	else if (args->xstats_type == DLB_CFG_QID_LDB_ENQ_CNT)
		val = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_LDB_DEPTH)
		val = DLB2_CSR_RD(hw, CHP_LDB_CQ_DEPTH(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_LDB_TOKEN_COUNT)
		val = DLB2_CSR_RD(hw, LSP_CQ_LDB_TKN_CNT(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_LDB_TOKEN_DEPTH_SELECT)
		val = DLB2_CSR_RD(hw, LSP_CQ_LDB_TKN_DEPTH_SEL(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_LDB_INFLIGHT_COUNT)
		val = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_CNT(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_DIR_DEPTH)
		val = DLB2_CSR_RD(hw, CHP_DIR_CQ_DEPTH(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_CQ_DIR_TOKEN_DEPTH_SELECT)
		val = DLB2_CSR_RD(hw, CHP_DIR_CQ_TKN_DEPTH_SEL(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_DIR_DEPTH_THRSH)
		val = DLB2_CSR_RD(hw, LSP_QID_DIR_DEPTH_THRSH(hw->ver, id));
	else if (args->xstats_type == DLB_CFG_QID_DIR_ENQ_CNT)
		val = DLB2_CSR_RD(hw, LSP_QID_DIR_ENQUEUE_CNT(hw->ver, id));
	else
		DLB2_HW_DBG(hw,
			    "Unsupported stats %x: %d\n",
			    args->xstats_type, args->xstats_id);

	args->xstats_val = val;
	return 0;
}
