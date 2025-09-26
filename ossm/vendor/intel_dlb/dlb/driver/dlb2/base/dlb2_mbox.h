/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_BASE_DLB2_MBOX_H
#define __DLB2_BASE_DLB2_MBOX_H

#include "dlb2_osdep_types.h"
#include "dlb2_regs.h"

/*
 * Version 2: Added DLB2_MBOX_CMD_ENABLE_CQ_WEIGHT
 * Version 3: Added sn_slots in resource management
 * Version 4: Added DLB2_MBOX_CMD_CQ_INFLIGHT_CTRL and DLB2_MBOX_CMD_GET_XSTATS
 *            Also, added inflight ctrl params to port create mailbox command
 * Version 5: Added DLB2_MBOX_CMD_STOP_DOMAIN
 */
#define DLB2_MBOX_INTERFACE_VERSION 5

/* Set minimum supported version as the last released stable version */
#define DLB2_MBOX_MIN_INTERFACE_VERSION 3

/*
 * The PF uses its PF->VF mailbox to send responses to VF requests, as well as
 * to send requests of its own (e.g. notifying a VF of an impending FLR).
 * To avoid communication race conditions, e.g. the PF sends a response and then
 * sends a request before the VF reads the response, the PF->VF mailbox is
 * divided into two sections:
 * - Bytes 0-47: PF responses
 * - Bytes 48-63: PF requests
 *
 * Partitioning the PF->VF mailbox allows responses and requests to occupy the
 * mailbox simultaneously.
 *
 * IMPORTANT: Add new fields at the end of structure to maintain compatibility
 * with older driver versions. If changes result in structure field offset
 * change, update mailbox version compatibility.
 */
#define DLB2_PF2VF_RESP_BYTES	  48
#define DLB2_PF2VF_RESP_BASE	  0
#define DLB2_PF2VF_RESP_BASE_WORD (DLB2_PF2VF_RESP_BASE / 4)

#define DLB2_PF2VF_REQ_BYTES	  16
#define DLB2_PF2VF_REQ_BASE	  (DLB2_PF2VF_RESP_BASE + DLB2_PF2VF_RESP_BYTES)
#define DLB2_PF2VF_REQ_BASE_WORD  (DLB2_PF2VF_REQ_BASE / 4)

/*
 * Similarly, the VF->PF mailbox is divided into two sections:
 * - Bytes 0-239: VF requests
 * -- (Bytes 0-3 are unused due to a hardware errata)
 * - Bytes 240-255: VF responses
 */
#define DLB2_VF2PF_REQ_BYTES	 236
#define DLB2_VF2PF_REQ_BASE	 4
#define DLB2_VF2PF_REQ_BASE_WORD (DLB2_VF2PF_REQ_BASE / 4)

#define DLB2_VF2PF_RESP_BYTES	  16
#define DLB2_VF2PF_RESP_BASE	  (DLB2_VF2PF_REQ_BASE + DLB2_VF2PF_REQ_BYTES)
#define DLB2_VF2PF_RESP_BASE_WORD (DLB2_VF2PF_RESP_BASE / 4)

/*
 * VF-initiated commands
 * IMPORTANT: Do not change the ordering as that will break backward
 * compatibility between PF<->VF drivers. New commands should be added
 * at the end.
 */
enum dlb2_mbox_cmd_type {
	DLB2_MBOX_CMD_REGISTER,
	DLB2_MBOX_CMD_UNREGISTER,
	DLB2_MBOX_CMD_GET_NUM_RESOURCES,
	DLB2_MBOX_CMD_CREATE_SCHED_DOMAIN,
	DLB2_MBOX_CMD_RESET_SCHED_DOMAIN,
	DLB2_MBOX_CMD_CREATE_LDB_QUEUE,
	DLB2_MBOX_CMD_CREATE_DIR_QUEUE,
	DLB2_MBOX_CMD_CREATE_LDB_PORT,
	DLB2_MBOX_CMD_CREATE_DIR_PORT,
	DLB2_MBOX_CMD_ENABLE_LDB_PORT,
	DLB2_MBOX_CMD_DISABLE_LDB_PORT,
	DLB2_MBOX_CMD_ENABLE_DIR_PORT,
	DLB2_MBOX_CMD_DISABLE_DIR_PORT,
	DLB2_MBOX_CMD_LDB_PORT_OWNED_BY_DOMAIN,
	DLB2_MBOX_CMD_DIR_PORT_OWNED_BY_DOMAIN,
	DLB2_MBOX_CMD_MAP_QID,
	DLB2_MBOX_CMD_UNMAP_QID,
	DLB2_MBOX_CMD_START_DOMAIN,
	DLB2_MBOX_CMD_ENABLE_LDB_PORT_INTR,
	DLB2_MBOX_CMD_ENABLE_DIR_PORT_INTR,
	DLB2_MBOX_CMD_ARM_CQ_INTR,
	DLB2_MBOX_CMD_GET_NUM_USED_RESOURCES,
	DLB2_MBOX_CMD_GET_SN_ALLOCATION,
	DLB2_MBOX_CMD_GET_LDB_QUEUE_DEPTH,
	DLB2_MBOX_CMD_GET_DIR_QUEUE_DEPTH,
	DLB2_MBOX_CMD_PENDING_PORT_UNMAPS,
	DLB2_MBOX_CMD_GET_COS_BW,
	DLB2_MBOX_CMD_GET_SN_OCCUPANCY,
	DLB2_MBOX_CMD_QUERY_CQ_POLL_MODE,
	DLB2_MBOX_CMD_DEV_RESET,
	DLB2_MBOX_CMD_ENABLE_CQ_WEIGHT,
	DLB2_MBOX_CMD_CQ_INFLIGHT_CTRL,
	DLB2_MBOX_CMD_GET_XSTATS,
	DLB2_MBOX_CMD_STOP_DOMAIN,

	/* NUM_QE_CMD_TYPES must be last */
	NUM_DLB2_MBOX_CMD_TYPES,
};

/*
 * Each MBOX command needs to populate its supported version in this structure.
 * VF driver will check this against the MBOX version of PF driver and only
 * allow commands whose version are <= PF driver's supported MBOX version.
 */
static const int dlb2_mbox_cmd_version [] = {
	[DLB2_MBOX_CMD_REGISTER] = 0,
	[DLB2_MBOX_CMD_UNREGISTER ... DLB2_MBOX_CMD_DEV_RESET] = 1,
	[DLB2_MBOX_CMD_ENABLE_CQ_WEIGHT] = 2,
	[DLB2_MBOX_CMD_CQ_INFLIGHT_CTRL] = 4,
	[DLB2_MBOX_CMD_GET_XSTATS] = 4,
	[DLB2_MBOX_CMD_STOP_DOMAIN] = 5,
};

static const char dlb2_mbox_cmd_type_strings[][128] = {
	"DLB2_MBOX_CMD_REGISTER",
	"DLB2_MBOX_CMD_UNREGISTER",
	"DLB2_MBOX_CMD_GET_NUM_RESOURCES",
	"DLB2_MBOX_CMD_CREATE_SCHED_DOMAIN",
	"DLB2_MBOX_CMD_RESET_SCHED_DOMAIN",
	"DLB2_MBOX_CMD_CREATE_LDB_QUEUE",
	"DLB2_MBOX_CMD_CREATE_DIR_QUEUE",
	"DLB2_MBOX_CMD_CREATE_LDB_PORT",
	"DLB2_MBOX_CMD_CREATE_DIR_PORT",
	"DLB2_MBOX_CMD_ENABLE_LDB_PORT",
	"DLB2_MBOX_CMD_DISABLE_LDB_PORT",
	"DLB2_MBOX_CMD_ENABLE_DIR_PORT",
	"DLB2_MBOX_CMD_DISABLE_DIR_PORT",
	"DLB2_MBOX_CMD_LDB_PORT_OWNED_BY_DOMAIN",
	"DLB2_MBOX_CMD_DIR_PORT_OWNED_BY_DOMAIN",
	"DLB2_MBOX_CMD_MAP_QID",
	"DLB2_MBOX_CMD_UNMAP_QID",
	"DLB2_MBOX_CMD_START_DOMAIN",
	"DLB2_MBOX_CMD_ENABLE_LDB_PORT_INTR",
	"DLB2_MBOX_CMD_ENABLE_DIR_PORT_INTR",
	"DLB2_MBOX_CMD_ARM_CQ_INTR",
	"DLB2_MBOX_CMD_GET_NUM_USED_RESOURCES",
	"DLB2_MBOX_CMD_GET_SN_ALLOCATION",
	"DLB2_MBOX_CMD_GET_LDB_QUEUE_DEPTH",
	"DLB2_MBOX_CMD_GET_DIR_QUEUE_DEPTH",
	"DLB2_MBOX_CMD_PENDING_PORT_UNMAPS",
	"DLB2_MBOX_CMD_GET_COS_BW",
	"DLB2_MBOX_CMD_GET_SN_OCCUPANCY",
	"DLB2_MBOX_CMD_QUERY_CQ_POLL_MODE",
	"DLB2_MBOX_CMD_DEV_RESET",
	"DLB2_MBOX_CMD_ENABLE_CQ_WEIGHT",
	"DLB2_MBOX_CMD_CQ_INFLIGHT_CTRL",
	"DLB2_MBOX_CMD_GET_XSTATS",
	"DLB2_MBOX_CMD_STOP_DOMAIN",
};

/* PF-initiated commands */
enum dlb2_mbox_vf_cmd_type {
	DLB2_MBOX_VF_CMD_DOMAIN_ALERT,
	DLB2_MBOX_VF_CMD_NOTIFICATION,
	DLB2_MBOX_VF_CMD_IN_USE,

	/* NUM_DLB2_MBOX_VF_CMD_TYPES must be last */
	NUM_DLB2_MBOX_VF_CMD_TYPES,
};

static const char dlb2_mbox_vf_cmd_type_strings[][128] = {
	"DLB2_MBOX_VF_CMD_DOMAIN_ALERT",
	"DLB2_MBOX_VF_CMD_NOTIFICATION",
	"DLB2_MBOX_VF_CMD_IN_USE",
};

#define DLB2_MBOX_CMD_TYPE(hdr) \
	(((struct dlb2_mbox_req_hdr *)hdr)->type)

enum dlb2_mbox_status_type {
	DLB2_MBOX_ST_SUCCESS,
	DLB2_MBOX_ST_INVALID_CMD_TYPE,
	DLB2_MBOX_ST_VERSION_MISMATCH,
	DLB2_MBOX_ST_INVALID_OWNER_VF,
	DLB2_MBOX_ST_INVALID_DATA,

	/* NUM_DLB2_MBOX_STATUS_TYPES must be last */
	NUM_DLB2_MBOX_STATUS_TYPES,
};

static const char dlb2_mbox_status_type_strings[][128] = {
	"DLB2_MBOX_ST_SUCCESS",
	"DLB2_MBOX_ST_INVALID_CMD_TYPE",
	"DLB2_MBOX_ST_VERSION_MISMATCH",
	"DLB2_MBOX_ST_INVALID_OWNER_VF",
	"DLB2_MBOX_ST_INVALID_DATA",
};

/* This structure is always the first field in a request structure */
struct dlb2_mbox_req_hdr {
	u32 type;
};

/* This structure is always the first field in a response structure */
struct dlb2_mbox_resp_hdr {
	u32 status;
};

static inline const char*
dlb2_mbox_cmd_string(struct dlb2_mbox_req_hdr *hdr)
{
	if (hdr->type >= NUM_DLB2_MBOX_CMD_TYPES)
		return "(invalid request)";
	else
		return dlb2_mbox_cmd_type_strings[hdr->type];
}

static inline const char*
dlb2_mbox_st_string(struct dlb2_mbox_resp_hdr *hdr)
{
	if (hdr->status >= NUM_DLB2_MBOX_STATUS_TYPES)
		return "(invalid response)";
	else
		return dlb2_mbox_status_type_strings[hdr->status];
}

enum dlb2_mbox_error_code {
	DLB2_MBOX_SUCCESS,
	DLB2_MBOX_EINVAL,	/* Invalid argument */
	DLB2_MBOX_EFAULT,	/* Internal error */
	DLB2_MBOX_EPERM,	/* The requested resource is locked */
	DLB2_MBOX_ETIMEDOUT,	/* Operation timed out */
};

struct dlb2_mbox_register_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u16 min_interface_version;
	u16 max_interface_version;
};

#define DLB2_MBOX_FLAG_IS_AUX_VF  BIT(0)
#define DLB2_MBOX_FLAG_MBOX_RESET BIT(1)

struct dlb2_mbox_register_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 interface_version;
	u8 pf_id;
	u8 vf_id;
	u8 primary_vf_id;
	u8 padding;
	u32 flags;
};

struct dlb2_mbox_unregister_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_unregister_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_get_num_resources_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_get_num_resources_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u16 num_sched_domains;
	u16 num_ldb_queues;
	u16 num_ldb_ports;
	u16 num_cos_ldb_ports[4];
	u16 num_dir_ports;
	u32 num_atomic_inflights;
	u32 num_hist_list_entries;
	u32 max_contiguous_hist_list_entries;
	u16 num_ldb_credits;
	u16 num_dir_credits;
	u16 num_sn_slots[2];
};

struct dlb2_mbox_create_sched_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 num_ldb_queues;
	u32 num_ldb_ports;
	u32 num_cos_ldb_ports[4];
	u32 num_dir_ports;
	u32 num_atomic_inflights;
	u32 num_hist_list_entries;
	u32 num_ldb_credits;
	u32 num_dir_credits;
	u8 cos_strict;
	u8 padding0[3];
	u32 num_sn_slots[2];
	u32 padding1;
};

struct dlb2_mbox_create_sched_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_reset_sched_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 id;
};

struct dlb2_mbox_reset_sched_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
};

struct dlb2_mbox_create_ldb_queue_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 num_sequence_numbers;
	u32 num_qid_inflights;
	u32 num_atomic_inflights;
	u32 lock_id_comp_level;
	u32 depth_threshold;
	u32 padding;
};

struct dlb2_mbox_create_ldb_queue_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_create_dir_queue_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 depth_threshold;
};

struct dlb2_mbox_create_dir_queue_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_create_ldb_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u16 cq_depth;
	u16 cq_history_list_size;
	u8 cos_id;
	u8 cos_strict;
	u16 inflight_threshold;
	u64 cq_base_address;
	u8 enable_inflight_ctrl;
	u8 pad[3];
};

struct dlb2_mbox_create_ldb_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_create_dir_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u64 cq_base_address;
	u16 cq_depth;
	u16 padding0;
	s32 queue_id;
};

struct dlb2_mbox_create_dir_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_enable_ldb_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_enable_ldb_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_disable_ldb_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_disable_ldb_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_enable_dir_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_enable_dir_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_disable_dir_port_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_disable_dir_port_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_ldb_port_owned_by_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_ldb_port_owned_by_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	s32 owned;
};

struct dlb2_mbox_dir_port_owned_by_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_dir_port_owned_by_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	s32 owned;
};

struct dlb2_mbox_map_qid_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 qid;
	u32 priority;
	u32 padding0;
};

struct dlb2_mbox_map_qid_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 id;
};

struct dlb2_mbox_unmap_qid_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 qid;
};

struct dlb2_mbox_unmap_qid_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_start_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
};

struct dlb2_mbox_start_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_stop_domain_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
};

struct dlb2_mbox_stop_domain_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_enable_ldb_port_intr_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u16 port_id;
	u16 thresh;
	u16 vector;
	u16 owner_vf;
	u16 reserved[2];
};

struct dlb2_mbox_enable_ldb_port_intr_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_enable_dir_port_intr_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u16 port_id;
	u16 thresh;
	u16 vector;
	u16 owner_vf;
	u16 reserved[2];
};

struct dlb2_mbox_enable_dir_port_intr_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_arm_cq_intr_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 is_ldb;
};

struct dlb2_mbox_arm_cq_intr_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding0;
};

/*
 * The alert_id and aux_alert_data follows the format of the alerts defined in
 * dlb2_types.h. The alert id contains an enum dlb2_domain_alert_id value, and
 * the aux_alert_data value varies depending on the alert.
 */
struct dlb2_mbox_vf_alert_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 alert_id;
	u32 aux_alert_data;
};

enum dlb2_mbox_vf_notification_type {
	DLB2_MBOX_VF_NOTIFICATION_PRE_RESET,

	/* NUM_DLB2_MBOX_VF_NOTIFICATION_TYPES must be last */
	NUM_DLB2_MBOX_VF_NOTIFICATION_TYPES,
};

struct dlb2_mbox_vf_notification_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 notification;
};

struct dlb2_mbox_vf_in_use_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_vf_in_use_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 in_use;
};

struct dlb2_mbox_get_sn_allocation_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 group_id;
};

struct dlb2_mbox_get_sn_allocation_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 num;
};

struct dlb2_mbox_get_ldb_queue_depth_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 queue_id;
	u32 padding;
};

struct dlb2_mbox_get_ldb_queue_depth_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 depth;
};

struct dlb2_mbox_get_dir_queue_depth_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 queue_id;
	u32 padding;
};

struct dlb2_mbox_get_dir_queue_depth_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 depth;
};

struct dlb2_mbox_pending_port_unmaps_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 padding;
};

struct dlb2_mbox_pending_port_unmaps_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 num;
};

struct dlb2_mbox_get_cos_bw_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 cos_id;
};

struct dlb2_mbox_get_cos_bw_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 num;
};

struct dlb2_mbox_get_sn_occupancy_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 group_id;
};

struct dlb2_mbox_get_sn_occupancy_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 num;
};

struct dlb2_mbox_query_cq_poll_mode_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_query_cq_poll_mode_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 mode;
};

struct dlb2_mbox_dev_reset_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 padding;
};

struct dlb2_mbox_dev_reset_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
};

struct dlb2_mbox_enable_cq_weight_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u32 limit;
};

struct dlb2_mbox_enable_cq_weight_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_cq_inflight_ctrl_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 domain_id;
	u32 port_id;
	u8 enable;
	u16 threshold;
};

struct dlb2_mbox_cq_inflight_ctrl_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u32 padding;
};

struct dlb2_mbox_get_xstats_cmd_req {
	struct dlb2_mbox_req_hdr hdr;
	u32 xstats_type;
	u32 xstats_id;
};

struct dlb2_mbox_get_xstats_cmd_resp {
	struct dlb2_mbox_resp_hdr hdr;
	u32 error_code;
	u32 status;
	u64 xstats_val;
};
#endif /* __DLB2_BASE_DLB2_MBOX_H */
