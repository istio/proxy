/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_RESOURCE_H
#define __DLB2_RESOURCE_H

#include "uapi/linux/dlb2_user.h"

#include "dlb2_hw_types.h"
#include "dlb2_osdep_types.h"

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


int dlb2_resource_init(struct dlb2_hw *hw, enum dlb2_hw_ver ver);

int dlb2_resource_probe(struct dlb2_hw *hw, const void *probe_args);

void dlb2_resource_free(struct dlb2_hw *hw);

void dlb2_resource_reset(struct dlb2_hw *hw);

int dlb2_hw_create_sched_domain(struct dlb2_hw *hw,
				struct dlb2_create_sched_domain_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id);

int dlb2_hw_create_ldb_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_ldb_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id);

int dlb2_hw_create_dir_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_dir_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id);

int dlb2_hw_create_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_dir_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id);

int dlb2_hw_create_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_ldb_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id);

int dlb2_hw_start_domain(struct dlb2_hw *hw,
			 u32 domain_id,
			 struct dlb2_start_domain_args *args,
			 struct dlb2_cmd_response *resp,
			 bool vdev_req,
			 unsigned int vdev_id);

int dlb2_hw_stop_domain(struct dlb2_hw *hw,
			u32 domain_id,
			struct dlb2_stop_domain_args *args,
			struct dlb2_cmd_response *resp,
			bool vdev_req,
			unsigned int vdev_id);

int dlb2_hw_map_qid(struct dlb2_hw *hw,
		    u32 domain_id,
		    struct dlb2_map_qid_args *args,
		    struct dlb2_cmd_response *resp,
		    bool vdev_req,
		    unsigned int vdev_id);

int dlb2_hw_unmap_qid(struct dlb2_hw *hw,
		      u32 domain_id,
		      struct dlb2_unmap_qid_args *args,
		      struct dlb2_cmd_response *resp,
		      bool vdev_req,
		      unsigned int vdev_id);

unsigned int dlb2_finish_unmap_qid_procedures(struct dlb2_hw *hw);

unsigned int dlb2_finish_map_qid_procedures(struct dlb2_hw *hw);

int dlb2_hw_enable_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_ldb_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id);

int dlb2_hw_disable_ldb_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_ldb_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id);

int dlb2_hw_enable_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_dir_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id);

int dlb2_hw_disable_dir_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_dir_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id);

int dlb2_configure_ldb_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold);

int dlb2_configure_dir_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold);

void dlb2_enable_ingress_error_alarms(struct dlb2_hw *hw);

void dlb2_disable_ingress_error_alarms(struct dlb2_hw *hw);

void dlb2_set_msix_mode(struct dlb2_hw *hw, int mode);

void dlb2_ack_msix_interrupt(struct dlb2_hw *hw, int vector);

int dlb2_arm_cq_interrupt(struct dlb2_hw *hw,
			  int port_id,
			  bool is_ldb,
			  bool vdev_req,
			  unsigned int vdev_id);

void dlb2_read_compressed_cq_intr_status(struct dlb2_hw *hw,
					 u32 *ldb_interrupts,
					 u32 *dir_interrupts);

void dlb2_ack_compressed_cq_intr(struct dlb2_hw *hw,
				 u32 *ldb_interrupts,
				 u32 *dir_interrupts);

u32 dlb2_read_vf_intr_status(struct dlb2_hw *hw);

void dlb2_ack_vf_intr_status(struct dlb2_hw *hw, u32 interrupts);

void dlb2_ack_vf_msi_intr(struct dlb2_hw *hw, u32 interrupts);

void dlb2_ack_pf_mbox_int(struct dlb2_hw *hw);

u32 dlb2_read_vdev_to_pf_int_bitvec(struct dlb2_hw *hw);

void dlb2_ack_vdev_mbox_int(struct dlb2_hw *hw, u32 bitvec);

u32 dlb2_read_vf_flr_int_bitvec(struct dlb2_hw *hw);

void dlb2_ack_vf_flr_int(struct dlb2_hw *hw, u32 bitvec);

void dlb2_ack_vdev_to_pf_int(struct dlb2_hw *hw,
			     u32 mbox_bitvec,
			     u32 flr_bitvec);

void dlb2_process_wdt_interrupt(struct dlb2_hw *hw);

void dlb2_process_alarm_interrupt(struct dlb2_hw *hw);

bool dlb2_process_ingress_error_interrupt(struct dlb2_hw *hw);

int dlb2_get_group_sequence_numbers(struct dlb2_hw *hw, u32 group_id);

int dlb2_get_group_sequence_number_occupancy(struct dlb2_hw *hw, u32 group_id);

int dlb2_set_group_sequence_numbers(struct dlb2_hw *hw,
				    u32 group_id,
				    u32 val);

int dlb2_reset_domain(struct dlb2_hw *hw,
		      u32 domain_id,
		      bool vdev_req,
		      unsigned int vdev_id);

int dlb2_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_req,
				  unsigned int vdev_id);

int dlb2_dir_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_req,
				  unsigned int vdev_id);

int dlb2_hw_get_num_resources(struct dlb2_hw *hw,
			      struct dlb2_get_num_resources_args *arg,
			      bool vdev_req,
			      unsigned int vdev_id);

int dlb2_hw_get_num_used_resources(struct dlb2_hw *hw,
				   struct dlb2_get_num_resources_args *arg,
				   bool vdev_req,
				   unsigned int vdev_id);

void dlb2_send_async_vdev_to_pf_msg(struct dlb2_hw *hw);

bool dlb2_vdev_to_pf_complete(struct dlb2_hw *hw);

bool dlb2_vf_flr_complete(struct dlb2_hw *hw);

void dlb2_send_async_pf_to_vdev_msg(struct dlb2_hw *hw, unsigned int vdev_id);

bool dlb2_pf_to_vdev_complete(struct dlb2_hw *hw, unsigned int vdev_id);

int dlb2_pf_read_vf_mbox_req(struct dlb2_hw *hw,
			     unsigned int vdev_id,
			     void *data,
			     int len);

int dlb2_pf_read_vf_mbox_resp(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      void *data,
			      int len);

int dlb2_pf_write_vf_mbox_resp(struct dlb2_hw *hw,
			       unsigned int vdev_id,
			       void *data,
			       int len);

int dlb2_pf_write_vf_mbox_req(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      void *data,
			      int len);

int dlb2_vf_read_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len);

int dlb2_vf_read_pf_mbox_req(struct dlb2_hw *hw, void *data, int len);

int dlb2_vf_write_pf_mbox_req(struct dlb2_hw *hw, void *data, int len);

int dlb2_vf_write_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len);

int dlb2_reset_vdev(struct dlb2_hw *hw, unsigned int id);

bool dlb2_vdev_is_locked(struct dlb2_hw *hw, unsigned int id);

void dlb2_lock_vdev(struct dlb2_hw *hw, unsigned int id);

void dlb2_unlock_vdev(struct dlb2_hw *hw, unsigned int id);

void dlb2_vdev_set_ims_idx(struct dlb2_hw *hw, unsigned int id, u32 *ims_idx);

int dlb2_update_vdev_sched_domains(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_ldb_queues(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_ldb_ports(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_ldb_cos_ports(struct dlb2_hw *hw,
				   u32 id,
				   u32 cos,
				   u32 num);

int dlb2_update_vdev_dir_ports(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_ldb_credits(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_dir_credits(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_hist_list_entries(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_atomic_inflights(struct dlb2_hw *hw, u32 id, u32 num);

int dlb2_update_vdev_sn_slots(struct dlb2_hw *hw, u32 id,
			      u32 sn_group, u32 num);

int dlb2_reset_vdev_resources(struct dlb2_hw *hw, unsigned int id);

int dlb2_notify_vf(struct dlb2_hw *hw,
		   unsigned int vf_id,
		   u32 notification);

int dlb2_vdev_in_use(struct dlb2_hw *hw, unsigned int id);

void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw, enum dlb2_hw_ver ver);

int dlb2_hw_get_ldb_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_ldb_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id);

int dlb2_hw_get_dir_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_dir_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id);

enum dlb2_virt_mode {
	DLB2_VIRT_NONE,
	DLB2_VIRT_SRIOV,
	DLB2_VIRT_SIOV,

	/* NUM_DLB2_VIRT_MODES must be last */
	NUM_DLB2_VIRT_MODES,
};

int dlb2_hw_set_virt_mode(struct dlb2_hw *hw, enum dlb2_virt_mode mode);

enum dlb2_virt_mode dlb2_hw_get_virt_mode(struct dlb2_hw *hw);

s32 dlb2_hw_get_ldb_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id);

s32 dlb2_hw_get_dir_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id);

void dlb2_hw_register_sw_mbox(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      u32 *vdev_to_pf_mbox,
			      u32 *pf_to_vdev_mbox,
			      void (*inject)(void *),
			      void *inject_arg);

void dlb2_hw_unregister_sw_mbox(struct dlb2_hw *hw, unsigned int vdev_id);

void dlb2_hw_setup_cq_ims_entry(struct dlb2_hw *hw,
				unsigned int vdev_id,
				u32 virt_cq_id,
				bool is_ldb,
				u32 addr_hi,
				u32 addr_lo,
				u32 data);

void dlb2_hw_clear_cq_ims_entry(struct dlb2_hw *hw,
				unsigned int vdev_id,
				u32 virt_cq_id,
				bool is_ldb);

int dlb2_hw_register_pasid(struct dlb2_hw *hw,
			   unsigned int vdev_id,
			   unsigned int pasid);

int dlb2_hw_pending_port_unmaps(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_pending_port_unmaps_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id);

int dlb2_hw_get_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id);

int dlb2_hw_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth);

enum dlb2_wd_tmo {
	/* 40s watchdog timeout */
	DLB2_WD_TMO_40S,
	/* 10s watchdog timeout */
	DLB2_WD_TMO_10S,
	/* 1s watchdog timeout */
	DLB2_WD_TMO_1S,

	/* Must be last */
	NUM_DLB2_WD_TMOS,
};

int dlb2_hw_enable_wd_timer(struct dlb2_hw *hw, enum dlb2_wd_tmo tmo);

int dlb2_hw_enable_dir_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id);

int dlb2_hw_enable_ldb_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id);

void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw);

void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw);

void dlb2_hw_set_qe_arbiter_weights(struct dlb2_hw *hw, u8 weight[8]);

void dlb2_hw_set_qid_arbiter_weights(struct dlb2_hw *hw, u8 weight[8]);

int dlb2_hw_ldb_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id);

void dlb2_hw_ldb_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode);

int dlb2_hw_dir_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id);

void dlb2_hw_dir_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode);

/**
 * dlb2_enable_cq_weight() - Enable QE-weight based scheduling on an LDB port.
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: CQ weight enablement arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function enables QE-weight based scheduling on a load-balanced port's
 * CQ and configures the CQ's weight limit.
 *
 * This must be called after creating the port but before starting the
 * domain.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * Errors:
 * EINVAL - The domain or port is not configured, the domainhas already been
 *	    started, the requested limit exceeds the port's CQ depth, or this
 *	    feature is unavailable on the device.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_enable_cq_weight(struct dlb2_hw *hw,
			  u32 domain_id,
			  struct dlb2_enable_cq_weight_args *args,
			  struct dlb2_cmd_response *resp,
			  bool vdev_request,
			  unsigned int vdev_id);

int dlb2_cq_inflight_ctrl(struct dlb2_hw *hw,
			  u32 domain_id,
			  struct dlb2_cq_inflight_ctrl_args *args,
			  struct dlb2_cmd_response *resp,
			  bool vdev_req,
			  unsigned int vdev_id);

#define DLB2_GET_XSTATS_BASE(x)   ((x >> 16) & 0xFFFF)

int dlb2_get_xstats(struct dlb2_hw *hw,
		    struct dlb2_xstats_args *args,
		    bool vdev_req,
		    unsigned int vdev_id);
/**
 * dlb2_disable_ldb_sched_perf_ctrl() - disable DLB2 ldb perf counters.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function disables the ldb perf/idle counters by setting the
 * clear bit of ldb sched perf control register.
 */
void dlb2_disable_ldb_sched_perf_ctrl(struct dlb2_hw *hw);

/**
 * dlb2_enable_ldb_sched_perf_ctrl() - enable DLB2 ldb perf counters.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function enables the ldb perf/idle counters by setting the
 * enable bit of ldb sched perf control register.
 */
void dlb2_enable_ldb_sched_perf_ctrl(struct dlb2_hw *hw);

/**
 * dlb2_read_sched_idle_counts() - read the current DLB scheduling performance
 * and idle counter values.
 * @hw: dlb2_hw handle for a particular device.
 * @data: current scheduling counter values (output argument).
 * @counter_idx: index of the counter.
 *
 * This function returns the current values in counter registers.
 * These counters increase monotonically until the device is reset.
 */
void dlb2_read_sched_idle_counts(struct dlb2_hw *hw,
                                 struct dlb2_sched_idle_counts *data,
                                 int counter_idx);

void dlb2_hw_set_rate_limit(struct dlb2_hw *hw, int rate_limit);

void dlb2_hw_set_qidx_wrr_scheduler_weight(struct dlb2_hw *hw, int weight);

int dlb2_lm_pause_device(struct dlb2_hw *hw,
			 bool vdev_req,
			 unsigned int vdev_id,
			 struct dlb2_migration_state *state);

int dlb2_lm_restore_device(struct dlb2_hw *hw,
                           bool vdev_req,
                           unsigned int vdev_id,
			   struct dlb2_migration_state *state);

int dlb2_enable_live_migration(struct dlb2_hw *hw, uint8_t cq);

void dlb2_ldb_port_cq_enable(struct dlb2_hw *hw,
                             struct dlb2_ldb_port *port);

void dlb2_ldb_port_cq_disable(struct dlb2_hw *hw,
                              struct dlb2_ldb_port *port);

void dlb2_dir_port_cq_enable(struct dlb2_hw *hw,
                             struct dlb2_dir_pq_pair *port);

void dlb2_dir_port_cq_disable(struct dlb2_hw *hw,
                              struct dlb2_dir_pq_pair *port);

u32 dlb2_dir_cq_token_count(struct dlb2_hw *hw,
			   struct dlb2_dir_pq_pair *port);

u32 dlb2_ldb_cq_token_count(struct dlb2_hw *hw,
                            struct dlb2_ldb_port *port);

bool dlb2_port_find_slot_queue(struct dlb2_ldb_port *port,
                               enum dlb2_qid_map_state state,
                               struct dlb2_ldb_queue *queue,
                               int *slot);
#endif /* __DLB2_RESOURCE_H */
