/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_USER_H
#define __DLB2_USER_H

#define DLB2_MAX_NAME_LEN 64

#include <linux/types.h>

#ifndef __KERNEL__
#define BITS_PER_LONG __BITS_PER_LONG
#endif

enum DLB2_PERF_COUNTERS {
        DLB2_LDB_PERF_SCHED_CNT = 0,
        DLB2_LDB_PERF_NOWORK_IDLE_CNT,
        DLB2_LDB_PERF_NOSPACE_IDLE_CNT,
        DLB2_LDB_PERF_PFRICTION_IDLE_CNT,
        DLB2_LDB_PERF_IFLIMIT_IDLE_CNT,
        DLB2_LDB_PERF_FIDLIMIT_IDLE_CNT,
        DLB2_PERF_PROC_ON_CNT,
        DLB2_PERF_CLK_ON_CNT,
	DLB2_HW_ERR_CNT,
	DLB2_MAX_NUM_CNTRS
};

struct dlb2_sched_idle_counts {
        __u64 ldb_perf_counters[DLB2_MAX_NUM_CNTRS];
};

enum dlb2_error {
	DLB2_ST_SUCCESS = 0,
	DLB2_ST_NAME_EXISTS,
	DLB2_ST_DOMAIN_UNAVAILABLE,
	DLB2_ST_LDB_PORTS_UNAVAILABLE,
	DLB2_ST_DIR_PORTS_UNAVAILABLE,
	DLB2_ST_LDB_QUEUES_UNAVAILABLE,
	DLB2_ST_LDB_CREDITS_UNAVAILABLE,
	DLB2_ST_DIR_CREDITS_UNAVAILABLE,
	DLB2_ST_SEQUENCE_NUMBERS_UNAVAILABLE,
	DLB2_ST_INVALID_DOMAIN_ID,
	DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION,
	DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE,
	DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE,
	DLB2_ST_INVALID_LDB_QUEUE_ID,
	DLB2_ST_INVALID_CQ_DEPTH,
	DLB2_ST_INVALID_CQ_VIRT_ADDR,
	DLB2_ST_INVALID_PORT_ID,
	DLB2_ST_INVALID_QID,
	DLB2_ST_INVALID_PRIORITY,
	DLB2_ST_NO_QID_SLOTS_AVAILABLE,
	DLB2_ST_INVALID_DIR_QUEUE_ID,
	DLB2_ST_DIR_QUEUES_UNAVAILABLE,
	DLB2_ST_DOMAIN_NOT_CONFIGURED,
	DLB2_ST_INTERNAL_ERROR,
	DLB2_ST_DOMAIN_IN_USE,
	DLB2_ST_DOMAIN_NOT_FOUND,
	DLB2_ST_QUEUE_NOT_FOUND,
	DLB2_ST_DOMAIN_STARTED,
	DLB2_ST_DOMAIN_NOT_STARTED,
	DLB2_ST_LDB_PORT_REQUIRED_FOR_LDB_QUEUES,
	DLB2_ST_DOMAIN_RESET_FAILED,
	DLB2_ST_MBOX_ERROR,
	DLB2_ST_INVALID_HIST_LIST_DEPTH,
	DLB2_ST_NO_MEMORY,
	DLB2_ST_INVALID_LOCK_ID_COMP_LEVEL,
	DLB2_ST_INVALID_COS_ID,
	DLB2_ST_INVALID_CQ_WEIGHT_LIMIT,
	DLB2_ST_SN_SLOTS_UNAVAILABLE,
	DLB2_ST_FEATURE_UNAVAILABLE,
};

static const char dlb2_error_strings[][128] = {
	"DLB2_ST_SUCCESS",
	"DLB2_ST_NAME_EXISTS",
	"DLB2_ST_DOMAIN_UNAVAILABLE",
	"DLB2_ST_LDB_PORTS_UNAVAILABLE",
	"DLB2_ST_DIR_PORTS_UNAVAILABLE",
	"DLB2_ST_LDB_QUEUES_UNAVAILABLE",
	"DLB2_ST_LDB_CREDITS_UNAVAILABLE",
	"DLB2_ST_DIR_CREDITS_UNAVAILABLE",
	"DLB2_ST_SEQUENCE_NUMBERS_UNAVAILABLE",
	"DLB2_ST_INVALID_DOMAIN_ID",
	"DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION",
	"DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE",
	"DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE",
	"DLB2_ST_INVALID_LDB_QUEUE_ID",
	"DLB2_ST_INVALID_CQ_DEPTH",
	"DLB2_ST_INVALID_CQ_VIRT_ADDR",
	"DLB2_ST_INVALID_PORT_ID",
	"DLB2_ST_INVALID_QID",
	"DLB2_ST_INVALID_PRIORITY",
	"DLB2_ST_NO_QID_SLOTS_AVAILABLE",
	"DLB2_ST_INVALID_DIR_QUEUE_ID",
	"DLB2_ST_DIR_QUEUES_UNAVAILABLE",
	"DLB2_ST_DOMAIN_NOT_CONFIGURED",
	"DLB2_ST_INTERNAL_ERROR",
	"DLB2_ST_DOMAIN_IN_USE",
	"DLB2_ST_DOMAIN_NOT_FOUND",
	"DLB2_ST_QUEUE_NOT_FOUND",
	"DLB2_ST_DOMAIN_STARTED",
	"DLB2_ST_DOMAIN_NOT_STARTED",
	"DLB2_ST_LDB_PORT_REQUIRED_FOR_LDB_QUEUES",
	"DLB2_ST_DOMAIN_RESET_FAILED",
	"DLB2_ST_MBOX_ERROR",
	"DLB2_ST_INVALID_HIST_LIST_DEPTH",
	"DLB2_ST_NO_MEMORY",
	"DLB2_ST_INVALID_LOCK_ID_COMP_LEVEL",
	"DLB2_ST_INVALID_COS_ID",
	"DLB2_ST_INVALID_CQ_WEIGHT_LIMIT",
	"DLB2_ST_SN_SLOTS_UNAVAILABLE",
	"DLB2_ST_FEATURE_UNAVAILABLE",
};

#define DLB2_DRIVER_VERSION	"8.8.0"

struct dlb2_cmd_response {
	__u32 status; /* Interpret using enum dlb2_error */
	__u32 id;
};

/********************************/
/* 'dlb2' device file commands */
/********************************/

#define DLB2_DEVICE_VERSION(x) (((x) >> 8) & 0xFF)
#define DLB2_DEVICE_REVISION(x) ((x) & 0xFF)

enum dlb2_revisions {
	DLB2_REV_A0 = 0,
};

/*
 * DLB2_CMD_GET_DEVICE_VERSION: Query the DLB device version.
 *
 *	This ioctl interface is the same in all driver versions and is always
 *	the first ioctl.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id[7:0]: Device revision.
 * - response.id[15:8]: Device version.
 */

struct dlb2_get_device_version_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
};

#define DLB2_MAX_CPU_CORES 128
/*
 * DLB2_CMD_CREATE_SCHED_DOMAIN: Create a DLB 2.0 scheduling domain and reserve
 *	its hardware resources. This command returns the newly created domain
 *	ID and a file descriptor for accessing the domain.
 *
 * Input parameters:
 * - num_ldb_queues: Number of load-balanced queues.
 * - num_ldb_ports: Number of load-balanced ports that can be allocated from
 *	from any class-of-service with available ports.
 * - num_cos_ldb_ports[4]: Number of load-balanced ports from
 *	classes-of-service 0-3.
 * - num_dir_ports: Number of directed ports. A directed port has one directed
 *	queue, so no num_dir_queues argument is necessary.
 * - num_atomic_inflights: This specifies the amount of temporary atomic QE
 *	storage for the domain. This storage is divided among the domain's
 *	load-balanced queues that are configured for atomic scheduling.
 * - num_hist_list_entries: Amount of history list storage. This is divided
 *	among the domain's CQs.
 * - num_ldb_credits: Amount of load-balanced QE storage (QED). QEs occupy this
 *	space until they are scheduled to a load-balanced CQ. One credit
 *	represents the storage for one QE.
 * - num_dir_credits: Amount of directed QE storage (DQED). QEs occupy this
 *	space until they are scheduled to a directed CQ. One credit represents
 *	the storage for one QE.
 * - cos_strict: If set, return an error if there are insufficient ports in
 *	class-of-service N to satisfy the num_ldb_ports_cosN argument. If
 *	unset, attempt to fulfill num_ldb_ports_cosN arguments from other
 *	classes-of-service if class N does not contain enough free ports.
 * - num_sn_slots[2]: number of sequence number slots from group 0 and 1.
 * -  pcore_mask: Producer coremask for the domain. Bit map of cores on which
 *   producer threads for this domain will run.
 * -  core_mask: EAL coremask
 * - padding1: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: domain ID.
 * - domain_fd: file descriptor for performing the domain's ioctl operations
 * - padding0: Reserved for future use.
 */
struct dlb2_create_sched_domain_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	__u32 domain_fd;
	__u32 padding0;
	/* Input parameters */
	__u32 num_ldb_queues;
	__u32 num_ldb_ports;
	__u32 num_cos_ldb_ports[4];
	__u32 num_dir_ports;
	__u32 num_atomic_inflights;
	__u32 num_hist_list_entries;
	__u32 num_ldb_credits;
	__u32 num_dir_credits;
	__u8 cos_strict;
	__u8 padding1[3];
	__u32 num_sn_slots[2];
	__u64 pcore_mask[DLB2_MAX_CPU_CORES/BITS_PER_LONG];
	__u64 core_mask[DLB2_MAX_CPU_CORES/BITS_PER_LONG];
};

/*
 * DLB2_CMD_GET_NUM_RESOURCES: Return the number of available resources
 *	(queues, ports, etc.) that this device owns.
 *
 * Output parameters:
 * - num_domains: Number of available scheduling domains.
 * - num_ldb_queues: Number of available load-balanced queues.
 * - num_ldb_ports: Total number of available load-balanced ports.
 * - num_cos_ldb_ports[4]: Number of available load-balanced ports from
 *	classes-of-service 0-3.
 * - num_dir_ports: Number of available directed ports. There is one directed
 *	queue for every directed port.
 * - num_atomic_inflights: Amount of available temporary atomic QE storage.
 * - num_hist_list_entries: Amount of history list storage.
 * - max_contiguous_hist_list_entries: History list storage is allocated in
 *	a contiguous chunk, and this return value is the longest available
 *	contiguous range of history list entries.
 * - num_ldb_credits: Amount of available load-balanced QE storage.
 * - num_dir_credits: Amount of available directed QE storage.
 * - num_sn_slots[2]: number of available sequence number slots from group
 *      0 and 1.
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_get_num_resources_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	__u32 num_sched_domains;
	__u32 num_ldb_queues;
	__u32 num_ldb_ports;
	__u32 num_cos_ldb_ports[4];
	__u32 num_dir_ports;
	__u32 num_atomic_inflights;
	__u32 num_hist_list_entries;
	__u32 max_contiguous_hist_list_entries;
	__u32 num_ldb_credits;
	__u32 num_dir_credits;
	__u32 num_sn_slots[2];
};

/*
 * DLB2_CMD_SET_SN_ALLOCATION: Configure a sequence number group (PF only)
 *
 * Input parameters:
 * - group: Sequence number group ID.
 * - num: Number of sequence numbers per queue.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_set_sn_allocation_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 group;
	__u32 num;
};

/*
 * DLB2_CMD_GET_SN_ALLOCATION: Get a sequence number group's configuration
 *
 * Input parameters:
 * - group: Sequence number group ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Specified group's number of sequence numbers per queue.
 */
struct dlb2_get_sn_allocation_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 group;
	__u32 padding0;
};

/*
 * DLB2_CMD_SET_COS_BW: Set a bandwidth allocation percentage for a
 *	load-balanced port class-of-service (PF only).
 *
 * Input parameters:
 * - cos_id: class-of-service ID, between 0 and 3 (inclusive).
 * - bandwidth: class-of-service bandwidth percentage. Total bandwidth
 *		percentages across all 4 classes cannot exceed 100%.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_set_cos_bw_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 cos_id;
	__u32 bandwidth;
};

/*
 * DLB2_CMD_GET_COS_BW: Get the bandwidth allocation percentage for a
 *	load-balanced port class-of-service.
 *
 * Input parameters:
 * - cos_id: class-of-service ID, between 0 and 3 (inclusive).
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Specified class's bandwidth percentage.
 */
struct dlb2_get_cos_bw_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 cos_id;
	__u32 padding0;
};

/*
 * DLB2_CMD_GET_SN_OCCUPANCY: Get a sequence number group's occupancy
 *
 * Each sequence number group has one or more slots, depending on its
 * configuration. I.e.:
 * - If configured for 1024 sequence numbers per queue, the group has 1 slot
 * - If configured for 512 sequence numbers per queue, the group has 2 slots
 *   ...
 * - If configured for 32 sequence numbers per queue, the group has 32 slots
 *
 * This ioctl returns the group's number of in-use slots. If its occupancy is
 * 0, the group's sequence number allocation can be reconfigured.
 *
 * Input parameters:
 * - group: Sequence number group ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Specified group's number of used slots.
 */
struct dlb2_get_sn_occupancy_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 group;
	__u32 padding0;
};

enum dlb2_cq_poll_modes {
	DLB2_CQ_POLL_MODE_STD,
	DLB2_CQ_POLL_MODE_SPARSE,

	/* NUM_DLB2_CQ_POLL_MODE must be last */
	NUM_DLB2_CQ_POLL_MODE,
};

/*
 * DLB2_CMD_QUERY_CQ_POLL_MODE: Query the CQ poll mode setting
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: CQ poll mode (see enum dlb2_cq_poll_modes).
 */
struct dlb2_query_cq_poll_mode_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
};

/*
 * DLB2_CMD_GET_HW_REG: Read the contents of a HW register
 *
 * Input parameters:
 * - reg_addr: 32 bit address of HW register
 *
 * Output parameters:
 * - reg_val: Contents of a HW register
 * - response.status: Detailed error code. In certain cases, such as if the
 *     ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_xstats_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	__u64 xstats_val;
	/* Input parameters */
	__u32 xstats_type;
	__u32 xstats_id;
};
enum dlb2_xtats_type {
	DEVICE_XSTATS= 0x0,
	LDB_QUEUE_XSTATS,
	LDB_PORT_XSTATS,
	DIR_PQ_XSTATS,
	MAX_XSTATS
};
#define XSTATS_BASE(id) (id << 16)

enum dlb2_ldb_queue_xstats {
	DLB_CFG_QID_LDB_INFLIGHT_COUNT = XSTATS_BASE(LDB_QUEUE_XSTATS),
	DLB_CFG_QID_LDB_INFLIGHT_LIMIT,
	DLB_CFG_QID_ATM_ACTIVE,
	DLB_CFG_QID_ATM_DEPTH_THRSH,
	DLB_CFG_QID_NALB_DEPTH_THRSH,
	DLB_CFG_QID_ATQ_ENQ_CNT,
	DLB_CFG_QID_LDB_ENQ_CNT,
};

enum dlb2_ldb_port_xstats {
	DLB_CFG_CQ_LDB_DEPTH = XSTATS_BASE(LDB_PORT_XSTATS),
	DLB_CFG_CQ_LDB_TOKEN_COUNT,
	DLB_CFG_CQ_LDB_TOKEN_DEPTH_SELECT,
	DLB_CFG_CQ_LDB_INFLIGHT_COUNT,
};

enum dlb2_dir_pq_xstats {
	DLB_CFG_CQ_DIR_TOKEN_DEPTH_SELECT = XSTATS_BASE(DIR_PQ_XSTATS),
	DLB_CFG_CQ_DIR_DEPTH,
	DLB_CFG_QID_DIR_DEPTH_THRSH,
	DLB_CFG_QID_DIR_ENQ_CNT,
};

enum dlb2_user_interface_commands {
	DLB2_CMD_GET_DEVICE_VERSION,
	DLB2_CMD_CREATE_SCHED_DOMAIN,
	DLB2_CMD_GET_NUM_RESOURCES,
	DLB2_CMD_RESERVED1,
	DLB2_CMD_RESERVED2,
	DLB2_CMD_SET_SN_ALLOCATION,
	DLB2_CMD_GET_SN_ALLOCATION,
	DLB2_CMD_SET_COS_BW,
	DLB2_CMD_GET_COS_BW,
	DLB2_CMD_GET_SN_OCCUPANCY,
	DLB2_CMD_QUERY_CQ_POLL_MODE,
	DLB2_CMD_GET_XSTATS,

	/* NUM_DLB2_CMD must be last */
	NUM_DLB2_CMD,
};

/*******************************/
/* 'domain' device file alerts */
/*******************************/

/*
 * Scheduling domain device files can be read to receive domain-specific
 * notifications, for alerts such as hardware errors or device reset.
 *
 * Each alert is encoded in a 16B message. The first 8B contains the alert ID,
 * and the second 8B is optional and contains additional information.
 * Applications should cast read data to a struct dlb2_domain_alert, and
 * interpret the struct's alert_id according to dlb2_domain_alert_id. The read
 * length must be 16B, or the function will return -EINVAL.
 *
 * Reads are destructive, and in the case of multiple file descriptors for the
 * same domain device file, an alert will be read by only one of the file
 * descriptors.
 *
 * The driver stores alerts in a fixed-size alert ring until they are read. If
 * the alert ring fills completely, subsequent alerts will be dropped. It is
 * recommended that DLB2 applications dedicate a thread to perform blocking
 * reads on the device file.
 */
enum dlb2_domain_alert_id {
	/*
	 * Software issued an illegal enqueue for a port in this domain. An
	 * illegal enqueue could be:
	 * - Illegal (excess) completion
	 * - Illegal fragment
	 * - Insufficient credits
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_PP_ILLEGAL_ENQ,
	/*
	 * Software issued excess CQ token pops for a port in this domain.
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_PP_EXCESS_TOKEN_POPS,
	/*
	 * A enqueue contained either an invalid command encoding or a REL,
	 * REL_T, RLS, FWD, FWD_T, FRAG, or FRAG_T from a directed port.
	 *
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_ILLEGAL_HCW,
	/*
	 * The QID must be valid and less than 128.
	 *
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_ILLEGAL_QID,
	/*
	 * An enqueue went to a disabled QID.
	 *
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_DISABLED_QID,
	/*
	 * The device containing this domain was reset. All applications using
	 * the device need to exit for the driver to complete the reset
	 * procedure.
	 *
	 * aux_alert_data doesn't contain any information for this alert.
	 */
	DLB2_DOMAIN_ALERT_DEVICE_RESET,
	/*
	 * User-space has enqueued an alert.
	 *
	 * aux_alert_data contains user-provided data.
	 */
	DLB2_DOMAIN_ALERT_USER,
	/*
	 * The watchdog timer fired for the specified port. This occurs if its
	 * CQ was not serviced for a large amount of time, likely indicating a
	 * hung thread.
	 * aux_alert_data[7:0] contains the port ID, and aux_alert_data[15:8]
	 * contains a flag indicating whether the port is load-balanced (1) or
	 * directed (0).
	 */
	DLB2_DOMAIN_ALERT_CQ_WATCHDOG_TIMEOUT,

	/* Number of DLB2 domain alerts */
	NUM_DLB2_DOMAIN_ALERTS
};

static const char dlb2_domain_alert_strings[][128] = {
	"DLB2_DOMAIN_ALERT_PP_ILLEGAL_ENQ",
	"DLB2_DOMAIN_ALERT_PP_EXCESS_TOKEN_POPS",
	"DLB2_DOMAIN_ALERT_ILLEGAL_HCW",
	"DLB2_DOMAIN_ALERT_ILLEGAL_QID",
	"DLB2_DOMAIN_ALERT_DISABLED_QID",
	"DLB2_DOMAIN_ALERT_DEVICE_RESET",
	"DLB2_DOMAIN_ALERT_USER",
	"DLB2_DOMAIN_ALERT_CQ_WATCHDOG_TIMEOUT",
};

struct dlb2_domain_alert {
	__u64 alert_id;
	__u64 aux_alert_data;
};

/*********************************/
/* 'domain' device file commands */
/*********************************/

/*
 * DLB2_DOMAIN_CMD_CREATE_LDB_QUEUE: Configure a load-balanced queue.
 * Input parameters:
 * - num_atomic_inflights: This specifies the amount of temporary atomic QE
 *	storage for this queue. If zero, the queue will not support atomic
 *	scheduling.
 * - num_sequence_numbers: This specifies the number of sequence numbers used
 *	by this queue. If zero, the queue will not support ordered scheduling.
 *	If non-zero, the queue will not support unordered scheduling.
 * - num_qid_inflights: The maximum number of QEs that can be inflight
 *	(scheduled to a CQ but not completed) at any time. If
 *	num_sequence_numbers is non-zero, num_qid_inflights must be set equal
 *	to num_sequence_numbers.
 * - lock_id_comp_level: Lock ID compression level. Specifies the number of
 *	unique lock IDs the queue should compress down to. Valid compression
 *	levels: 0, 64, 128, 256, 512, 1k, 2k, 4k, 64k. If lock_id_comp_level is
 *	0, the queue won't compress its lock IDs.
 * - depth_threshold: DLB sets two bits in the received QE to indicate the
 *	depth of the queue relative to the threshold before scheduling the
 *	QE to a CQ:
 *	- 2’b11: depth > threshold
 *	- 2’b10: threshold >= depth > 0.75 * threshold
 *	- 2’b01: 0.75 * threshold >= depth > 0.5 * threshold
 *	- 2’b00: depth <= 0.5 * threshold
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Queue ID.
 */
struct dlb2_create_ldb_queue_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 num_sequence_numbers;
	__u32 num_qid_inflights;
	__u32 num_atomic_inflights;
	__u32 lock_id_comp_level;
	__u32 depth_threshold;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_CREATE_DIR_QUEUE: Configure a directed queue.
 * Input parameters:
 * - port_id: Port ID. If the corresponding directed port is already created,
 *	specify its ID here. Else this argument must be 0xFFFFFFFF to indicate
 *	that the queue is being created before the port.
 * - depth_threshold: DLB sets two bits in the received QE to indicate the
 *	depth of the queue relative to the threshold before scheduling the
 *	QE to a CQ:
 *	- 2’b11: depth > threshold
 *	- 2’b10: threshold >= depth > 0.75 * threshold
 *	- 2’b01: 0.75 * threshold >= depth > 0.5 * threshold
 *	- 2’b00: depth <= 0.5 * threshold
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Queue ID.
 */
struct dlb2_create_dir_queue_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__s32 port_id;
	__u32 depth_threshold;
};

/*
 * DLB2_DOMAIN_CMD_CREATE_LDB_PORT: Configure a load-balanced port.
 * Input parameters:
 * - cq_depth: Depth of the port's CQ. Must be a power-of-two between 8 and
 *	1024, inclusive.
 * - cq_depth_threshold: CQ depth interrupt threshold. A value of N means that
 *	the CQ interrupt won't fire until there are N or more outstanding CQ
 *	tokens.
 * - num_hist_list_entries: Number of history list entries. This must be
 *	greater than or equal cq_depth.
 * - cos_id: class-of-service to allocate this port from. Must be between 0 and
 *	3, inclusive.
 * - cos_strict: If set, return an error if there are no available ports in the
 *	requested class-of-service. Else, allocate the port from a different
 *	class-of-service if the requested class has no available ports.
 *
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: port ID.
 */

struct dlb2_create_ldb_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u16 cq_depth;
	__u16 cq_depth_threshold;
	__u16 cq_history_list_size;
	__u8 cos_id;
	__u8 cos_strict;
	__u8 enable_inflight_ctrl;
	__u16 inflight_threshold;
};

/*
 * DLB2_DOMAIN_CMD_CREATE_DIR_PORT: Configure a directed port.
 * Input parameters:
 * - cq_depth: Depth of the port's CQ. Must be a power-of-two between 8 and
 *	1024, inclusive.
 * - cq_depth_threshold: CQ depth interrupt threshold. A value of N means that
 *	the CQ interrupt won't fire until there are N or more outstanding CQ
 *	tokens.
 * - qid: Queue ID. If the corresponding directed queue is already created,
 *	specify its ID here. Else this argument must be 0xFFFFFFFF to indicate
 *	that the port is being created before the queue.
 * - is_producer: If this port is used as a producer i.e., events will be
 *   primarily enqueued from this port
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: Port ID.
 */
struct dlb2_create_dir_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u16 cq_depth;
	__u16 cq_depth_threshold;
	__s32 queue_id;
	__u8 is_producer;
};

/*
 * DLB2_DOMAIN_CMD_START_DOMAIN: Mark the end of the domain configuration. This
 *	must be called before passing QEs into the device, and no configuration
 *	ioctls can be issued once the domain has started. Sending QEs into the
 *	device before calling this ioctl will result in undefined behavior.
 * Input parameters:
 * - (None)
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_start_domain_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
};

/*
 * DLB2_DOMAIN_CMD_STOP_DOMAIN: Stop scheduling of a domain. Scheduling can be
 *     resumed by calling DLB2_DOMAIN_CMD_START_DOMAIN. Sending QEs into the
 *     device after calling this ioctl will result in undefined behavior.
 * Input parameters:
 * - (None)
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_stop_domain_args {
       /* Output parameters */
       struct dlb2_cmd_response response;
};

/*
 * DLB2_DOMAIN_CMD_MAP_QID: Map a load-balanced queue to a load-balanced port.
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - qid: Load-balanced queue ID.
 * - priority: Queue->port service priority.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_map_qid_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 qid;
	__u32 priority;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_UNMAP_QID: Unmap a load-balanced queue to a load-balanced
 *	port.
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - qid: Load-balanced queue ID.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_unmap_qid_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 qid;
};

/*
 * DLB2_DOMAIN_CMD_ENABLE_LDB_PORT: Enable scheduling to a load-balanced port.
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_enable_ldb_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_ENABLE_DIR_PORT: Enable scheduling to a directed port.
 * Input parameters:
 * - port_id: Directed port ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_enable_dir_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
};

/*
 * DLB2_DOMAIN_CMD_DISABLE_LDB_PORT: Disable scheduling to a load-balanced
 *	port.
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_disable_ldb_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_DISABLE_DIR_PORT: Disable scheduling to a directed port.
 * Input parameters:
 * - port_id: Directed port ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_disable_dir_port_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_BLOCK_ON_CQ_INTERRUPT: Block on a CQ interrupt until a QE
 *	arrives for the specified port. If a QE is already present, the ioctl
 *	will immediately return.
 *
 *	Note: Only one thread can block on a CQ's interrupt at a time. Doing
 *	otherwise can result in hung threads.
 *
 * Input parameters:
 * - port_id: Port ID.
 * - is_ldb: True if the port is load-balanced, false otherwise.
 * - arm: Tell the driver to arm the interrupt.
 * - cq_gen: Current CQ generation bit.
 * - padding0: Reserved for future use.
 * - cq_va: VA of the CQ entry where the next QE will be placed.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_block_on_cq_interrupt_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u8 is_ldb;
	__u8 arm;
	__u8 cq_gen;
	__u8 padding0;
	__u64 cq_va;
};

/*
 * DLB2_DOMAIN_CMD_ENQUEUE_DOMAIN_ALERT: Enqueue a domain alert that will be
 *	read by one reader thread.
 *
 * Input parameters:
 * - aux_alert_data: user-defined auxiliary data.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_enqueue_domain_alert_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u64 aux_alert_data;
};

/*
 * DLB2_DOMAIN_CMD_GET_LDB_QUEUE_DEPTH: Get a load-balanced queue's depth.
 * Input parameters:
 * - queue_id: The load-balanced queue ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: queue depth.
 */
struct dlb2_get_ldb_queue_depth_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 queue_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_DIR_QUEUE_DEPTH: Get a directed queue's depth.
 * Input parameters:
 * - queue_id: The directed queue ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: queue depth.
 */
struct dlb2_get_dir_queue_depth_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 queue_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_PENDING_PORT_UNMAPS: Get number of queue unmap operations in
 *	progress for a load-balanced port.
 *
 *	Note: This is a snapshot; the number of unmap operations in progress
 *	is subject to change at any time.
 *
 * Input parameters:
 * - port_id: Load-balanced port ID.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: number of unmaps in progress.
 */
struct dlb2_pending_port_unmaps_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 padding0;
};

/*
 * DLB2_CMD_GET_LDB_PORT_PP_FD: Get file descriptor to mmap a load-balanced
 *	port's producer port (PP).
 * DLB2_CMD_GET_LDB_PORT_CQ_FD: Get file descriptor to mmap a load-balanced
 *	port's consumer queue (CQ).
 *
 *	The load-balanced port must have been previously created with the ioctl
 *	DLB2_CMD_CREATE_LDB_PORT. The fd is used to mmap the PP/CQ region.
 *
 * DLB2_CMD_GET_DIR_PORT_PP_FD: Get file descriptor to mmap a directed port's
 *	producer port (PP).
 * DLB2_CMD_GET_DIR_PORT_CQ_FD: Get file descriptor to mmap a directed port's
 *	consumer queue (CQ).
 *
 *	The directed port must have been previously created with the ioctl
 *	DLB2_CMD_CREATE_DIR_PORT. The fd is used to mmap PP/CQ region.
 *
 * Input parameters:
 * - port_id: port ID.
 * - padding0: Reserved for future use.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: fd.
 */
struct dlb2_get_port_fd_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 padding0;
};

/*
 * DLB2_DOMAIN_CMD_ENABLE_CQ_WEIGHT: Enable QE-weight based scheduling on a
 *	load-balanced port's CQ and configures the CQ's weight limit.
 *
 *	This must be called after creating the port but before starting the
 *	domain. The QE weight limit must be non-zero and cannot exceed the
 *	CQ's depth.
 *
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - limit: QE weight limit.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *	ioctl request arg is invalid, the driver won't set status.
 * - response.id: number of unmaps in progress.
 */
struct dlb2_enable_cq_weight_args {
	/* Output parameters */
	struct dlb2_cmd_response response;
	/* Input parameters */
	__u32 port_id;
	__u32 limit;
};

/*
 * DLB2_DOMAIN_CMD_ENABLE_CQ_EPOLL: Enable epoll support to monitor event
 *      file descriptors created for directed and load-balanced port's CQs.
 *	Kernel notifies user-space of events through the eventfds.
 *
 *	This must be called after creating the port. It can be called after
 *	starting the domain.
 *
 * Input parameters:
 * - port_id: Directed or Load-balanced port ID.
 * - process_id: Process id of the user space application.
 * - event_fd: Event file descriptor.
 * - is_ldb: True for load-balanced port and false for directed port.
 *
 * Output parameters:
 * - response.status
 * - response.id: port_id
 */
struct dlb2_enable_cq_epoll_args {
       /* Output parameters */
       struct dlb2_cmd_response response;
       /* Input parameters */
       __u32 port_id;
       __u32 process_id;
       __u32 event_fd;
       __u8 is_ldb;
       __u8 padding0[3];
};

/*
 * DLB2_DOMAIN_CMD_SET_CQ_INFLIGHT_CTRL: Set Per-CQ inflight control for
 *     {ATM,UNO,ORD} QEs.
 *
 * Input parameters:
 * - port_id: Load-balanced port ID.
 * - enable: True if inflight control is enabled. False otherwise
 * - threshold: Per CQ inflight threshold.
 *
 * Output parameters:
 * - response.status: Detailed error code. In certain cases, such as if the
 *     ioctl request arg is invalid, the driver won't set status.
 */
struct dlb2_cq_inflight_ctrl_args {
       /* Output parameters */
       struct dlb2_cmd_response response;
       /* Input parameters */
       __u32 port_id;
       __u16 enable;
       __u16 threshold;
};

enum dlb2_domain_user_interface_commands {
	DLB2_DOMAIN_CMD_CREATE_LDB_QUEUE,
	DLB2_DOMAIN_CMD_CREATE_DIR_QUEUE,
	DLB2_DOMAIN_CMD_CREATE_LDB_PORT,
	DLB2_DOMAIN_CMD_CREATE_DIR_PORT,
	DLB2_DOMAIN_CMD_START_DOMAIN,
	DLB2_DOMAIN_CMD_MAP_QID,
	DLB2_DOMAIN_CMD_UNMAP_QID,
	DLB2_DOMAIN_CMD_ENABLE_LDB_PORT,
	DLB2_DOMAIN_CMD_ENABLE_DIR_PORT,
	DLB2_DOMAIN_CMD_DISABLE_LDB_PORT,
	DLB2_DOMAIN_CMD_DISABLE_DIR_PORT,
	DLB2_DOMAIN_CMD_BLOCK_ON_CQ_INTERRUPT,
	DLB2_DOMAIN_CMD_ENQUEUE_DOMAIN_ALERT,
	DLB2_DOMAIN_CMD_GET_LDB_QUEUE_DEPTH,
	DLB2_DOMAIN_CMD_GET_DIR_QUEUE_DEPTH,
	DLB2_DOMAIN_CMD_PENDING_PORT_UNMAPS,
	DLB2_DOMAIN_CMD_GET_LDB_PORT_PP_FD,
	DLB2_DOMAIN_CMD_GET_LDB_PORT_CQ_FD,
	DLB2_DOMAIN_CMD_GET_DIR_PORT_PP_FD,
	DLB2_DOMAIN_CMD_GET_DIR_PORT_CQ_FD,
	DLB2_DOMAIN_CMD_ENABLE_CQ_WEIGHT,
	DLB2_DOMAIN_CMD_ENABLE_CQ_EPOLL,
	DLB2_DOMAIN_CMD_SET_CQ_INFLIGHT_CTRL,
	DLB2_DOMAIN_CMD_STOP_DOMAIN,

	/* NUM_DLB2_DOMAIN_CMD must be last */
	NUM_DLB2_DOMAIN_CMD,
};
/*
 * Mapping sizes for memory mapping the consumer queue (CQ) memory space, and
 * producer port (PP) MMIO space.
 */
#define DLB2_CQ_SIZE 65536
#define DLB2_PP_SIZE 4096

/********************/
/* dlb2 ioctl codes */
/********************/

#define DLB2_IOC_MAGIC  'h'

#define DLB2_IOC_GET_DEVICE_VERSION				\
		_IOR(DLB2_IOC_MAGIC,				\
		     DLB2_CMD_GET_DEVICE_VERSION,		\
		     struct dlb2_get_device_version_args)
#define DLB2_IOC_CREATE_SCHED_DOMAIN				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_CREATE_SCHED_DOMAIN,		\
		      struct dlb2_create_sched_domain_args)
#define DLB2_IOC_GET_NUM_RESOURCES				\
		_IOR(DLB2_IOC_MAGIC,				\
		     DLB2_CMD_GET_NUM_RESOURCES,		\
		     struct dlb2_get_num_resources_args)
#define DLB2_IOC_SET_SN_ALLOCATION				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_SET_SN_ALLOCATION,		\
		      struct dlb2_set_sn_allocation_args)
#define DLB2_IOC_GET_SN_ALLOCATION				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_GET_SN_ALLOCATION,		\
		      struct dlb2_get_sn_allocation_args)
#define DLB2_IOC_SET_COS_BW					\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_SET_COS_BW,			\
		      struct dlb2_set_cos_bw_args)
#define DLB2_IOC_GET_COS_BW					\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_GET_COS_BW,			\
		      struct dlb2_get_cos_bw_args)
#define DLB2_IOC_GET_SN_OCCUPANCY				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_GET_SN_OCCUPANCY,		\
		      struct dlb2_get_sn_occupancy_args)
#define DLB2_IOC_QUERY_CQ_POLL_MODE				\
		_IOR(DLB2_IOC_MAGIC,				\
		     DLB2_CMD_QUERY_CQ_POLL_MODE,		\
		     struct dlb2_query_cq_poll_mode_args)
#define DLB2_IOC_CREATE_LDB_QUEUE				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_CREATE_LDB_QUEUE,		\
		      struct dlb2_create_ldb_queue_args)
#define DLB2_IOC_CREATE_DIR_QUEUE				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_CREATE_DIR_QUEUE,		\
		      struct dlb2_create_dir_queue_args)
#define DLB2_IOC_CREATE_LDB_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_CREATE_LDB_PORT,		\
		      struct dlb2_create_ldb_port_args)
#define DLB2_IOC_CREATE_DIR_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_CREATE_DIR_PORT,		\
		      struct dlb2_create_dir_port_args)
#define DLB2_IOC_START_DOMAIN					\
		_IOR(DLB2_IOC_MAGIC,				\
		     DLB2_DOMAIN_CMD_START_DOMAIN,		\
		     struct dlb2_start_domain_args)
#define DLB2_IOC_MAP_QID					\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_MAP_QID,			\
		      struct dlb2_map_qid_args)
#define DLB2_IOC_UNMAP_QID					\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_UNMAP_QID,		\
		      struct dlb2_unmap_qid_args)
#define DLB2_IOC_ENABLE_LDB_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_ENABLE_LDB_PORT,		\
		      struct dlb2_enable_ldb_port_args)
#define DLB2_IOC_ENABLE_DIR_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_ENABLE_DIR_PORT,		\
		      struct dlb2_enable_dir_port_args)
#define DLB2_IOC_DISABLE_LDB_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_DISABLE_LDB_PORT,		\
		      struct dlb2_disable_ldb_port_args)
#define DLB2_IOC_DISABLE_DIR_PORT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_DISABLE_DIR_PORT,		\
		      struct dlb2_disable_dir_port_args)
#define DLB2_IOC_BLOCK_ON_CQ_INTERRUPT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_BLOCK_ON_CQ_INTERRUPT,	\
		      struct dlb2_block_on_cq_interrupt_args)
#define DLB2_IOC_ENQUEUE_DOMAIN_ALERT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_ENQUEUE_DOMAIN_ALERT,	\
		      struct dlb2_enqueue_domain_alert_args)
#define DLB2_IOC_GET_LDB_QUEUE_DEPTH				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_LDB_QUEUE_DEPTH,	\
		      struct dlb2_get_ldb_queue_depth_args)
#define DLB2_IOC_GET_DIR_QUEUE_DEPTH				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_DIR_QUEUE_DEPTH,	\
		      struct dlb2_get_dir_queue_depth_args)
#define DLB2_IOC_PENDING_PORT_UNMAPS				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_PENDING_PORT_UNMAPS,	\
		      struct dlb2_pending_port_unmaps_args)
#define DLB2_IOC_GET_LDB_PORT_PP_FD				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_LDB_PORT_PP_FD,	\
		      struct dlb2_get_port_fd_args)
#define DLB2_IOC_GET_LDB_PORT_CQ_FD				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_LDB_PORT_CQ_FD,	\
		      struct dlb2_get_port_fd_args)
#define DLB2_IOC_GET_DIR_PORT_PP_FD				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_DIR_PORT_PP_FD,	\
		      struct dlb2_get_port_fd_args)
#define DLB2_IOC_GET_DIR_PORT_CQ_FD				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_GET_DIR_PORT_CQ_FD,	\
		      struct dlb2_get_port_fd_args)
#define DLB2_IOC_ENABLE_CQ_EPOLL				\
               _IOWR(DLB2_IOC_MAGIC,				\
                     DLB2_DOMAIN_CMD_ENABLE_CQ_EPOLL,		\
                     struct dlb2_enable_cq_epoll_args)
#define DLB2_IOC_ENABLE_CQ_WEIGHT				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_ENABLE_CQ_WEIGHT,		\
		      struct dlb2_enable_cq_weight_args)
#define DLB2_IOC_SET_CQ_INFLIGHT_CTRL				\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_DOMAIN_CMD_SET_CQ_INFLIGHT_CTRL,	\
		      struct dlb2_cq_inflight_ctrl_args)
#define DLB2_IOC_GET_XSTATS					\
		_IOWR(DLB2_IOC_MAGIC,				\
		      DLB2_CMD_GET_XSTATS,			\
		      struct dlb2_xstats_args)
#endif /* __DLB2_USER_H */
