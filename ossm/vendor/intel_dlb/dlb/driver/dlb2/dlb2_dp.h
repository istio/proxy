/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_DP_H
#define __DLB2_DP_H

#include <linux/errno.h>

#include "uapi/linux/dlb2_user.h"

/******************************/
/* DLB Common Data Structures */
/******************************/

struct dlb2_dp;
struct dlb2_domain_hdl;
struct dlb2_port_hdl;

/*!
 *  @brief QE Scheduling Types
 */
enum dlb2_event_sched_t {
	/*
	 * Atomic scheduling. Only valid if the destination queue is
	 * load-balanced.
	 */
	SCHED_ATOMIC = 0,
	/*
	 * Unordered scheduling. Only valid if the destination queue is
	 * load-balanced and was configured with zero sequence numbers.
	 */
	SCHED_UNORDERED,
	/*
	 * Ordered scheduling. Only valid if the destination queue is
	 * load-balanced and was configured with non-zero sequence numbers.
	 */
	SCHED_ORDERED,
	/*
	 * Directed scheduling. Only valid when the destination queue is
	 * directed.
	 */
	SCHED_DIRECTED
};

/*!
 *  @brief QE commands
 */
enum dlb2_event_cmd_t {
	/* NOOP */
	NOOP = 0,
	/* Batch token return */
	BAT_T,
	/* QE release */
	REL,
	/* QE release with a single token return */
	REL_T,

	/* Reserved */
	RSVD4,
	/* Reserved */
	RSVD5,
	/* Reserved */
	RSVD6,
	/* Reserved */
	RSVD7,

	/* New QE enqueue */
	NEW = 8,
	/* New QE enqueue with a single token return */
	NEW_T,
	/* Forward QE (NEW + REL) */
	FWD,
	/* Forward QE (NEW + REL) with a single token return */
	FWD_T,

	/* NUM_QE_CMD_TYPES must be last */
	NUM_QE_CMD_TYPES,
};

/*!
 *  @brief DLB event send structure
 */
struct dlb2_send {
	/* 64 bits of user data */
	u64 udata64;
	/* 16 bits of user data */
	u16 udata16;
	/* Destination queue ID */
	u8 queue_id;
	/* Scheduling type (use enum dlb2_event_sched_t) */
	u8 sched_type:2;
	/* Priority */
	u8 priority:3;
	/* Reserved */
	u8 rsvd0:3;
	/* Flow ID (valid for atomic scheduling) */
	u16 flow_id;
	/** Timestamp valid. To use this feature, the following must be
	 * satisfied:
	 * - The user has called FIXME (per-queue timestamp enable) for the
	 *   destination queue.
	 * - The user has called FIXME (per-port timestamp enable) for the
	 *   sending port.
	 *
	 * If those are satisfied and the user sets this bit, the device will
	 * place a timestamp in the corresponding received event. When
	 * timestamps are enabled, the user must clear this bit if they don't
	 * want hardware to timestamp the event.
	 */
	u8 ts_valid:1;
	/** Reserved */
	u8 rsvd1:7;
	/** Reserved */
	u8 rsvd2;
} __packed;

/*!
 *  @brief DLB event receive structure
 */
struct dlb2_recv {
	/* 64-bit event data */
	u64 udata64;
	union {
		/** 16 bits of user data */
		u16 udata16;
		/** Device timestamp. This field is set by the device if it has
		 * the qid_ts capability and the event's ts_valid bit is set.
		 */
		u16 timestamp;
	};
	/* Queue ID that this event was sent to (load-balanced only) */
	u8 queue_id;
	/* Scheduling type (enum dlb2_event_sched_t) */
	u8 sched_type:2;
	/* Priority */
	u8 priority:3;
	/* Reserved */
	u8 rsvd0:3;
	/* Flow ID */
	u16 flow_id;
	/** Device timestamp. This field is set by the device if the enqueued
	 * event's timestamp bit was set. This flag indicates the presence of a
	 * timestamp value in the dequeued event.
	 */
	u8 ts_valid:1;
	/** Reserved */
	u8 rsvd2:7;
	/** Reserved */
	u8 rsvd3:1;
	/** Queue depth indicator. This field is set by the device if it has
	 * the queue_dt capability. In queue_dt-capable devices, each queue is
	 * configured with a threshold, and the qdi is interpreted as follows:
	 * - 2’b11: queue depth > threshold
	 * - 2’b10: threshold >= queue depth > 0.75 * threshold
	 * - 2’b01: 0.75 * threshold >= queue depth > 0.5 * threshold
	 * - 2’b00: queue depth <= 0.5 * threshold
	 *
	 * The queue depth is measured when the event is scheduled to a port.
	 * (FIXME confirm.)
	 */
	u8 qdi:2;
	/** Reserved */
	u8 rsvd4:2;
	/** Flag set by hardware indicating an error in the event
	 *  (TODO: more info)
	 */
	u8 error:1;
	/** Reserved */
	u8 rsvd5:2;
} __packed;

/*!
 *  @brief Advanced DLB event send structure
 */
struct dlb2_adv_send {
	/* 64-bit event data */
	u64 udata64;
	/* 16 bits of user data */
	u16 udata16;
	/* Queue ID */
	u8 queue_id;
	/* Scheduling type (use enum dlb2_event_sched_t) */
	u8 sched_type:2;
	/* Priority */
	u8 priority:3;
	/* Reserved */
	u8 rsvd0:3;
	/**
	 * For the BAT_T command, the field indicates the number of tokens to
	 * return. For NEW/NEW_T and FWD/FWD_T commands, the flow_id field
	 * specifies the event's flow ID.
	 */
	union {
		/* Flow ID (valid for atomic scheduling) */
		u16 flow_id;
		/* Number of tokens to return, minus one */
		u16 num_tokens_minus_one;
	};
	/** Timestamp valid. To use this feature, the following must be
	 * satisfied:
	 * - The user has called FIXME (per-queue timestamp enable) for the
	 *   destination queue.
	 * - The user has called FIXME (per-port timestamp enable) for the
	 *   sending port.
	 *
	 * If those are satisfied and the user sets this bit, the device will
	 * place a timestamp in the corresponding received event. When
	 * timestamps are enabled, the user must clear this bit if they don't
	 * want hardware to timestamp the event.
	 */
	u8 ts_valid:1;
	/** Reserved */
	u8 rsvd1:7;
	/** Send command (use enum dlb2_event_cmd_t) */
	u8 cmd:4;
	/** Reserved */
	u8 rsvd2:4;
} __packed;

/*!
 *  @brief DLB event structure
 */
union dlb2_event {
	/* Structure for sending events */
	struct dlb2_send send;
	/* Structure for receiving events */
	struct dlb2_recv recv;
	/* Structure for sending events with the advanced send function */
	struct dlb2_adv_send adv_send;
};

/******************************/
/* DLB Device-Level Functions */
/******************************/

/*!
 * @fn int dlb2_open(
 *          int device_id,
 *          struct dlb2_dp **dlb2);
 *
 * @brief   Open the DLB device file and initialize the client library.
 *
 * @note    A DLB handle can be shared among kernel threads. Functions that
 *          take a DLB handle are MT-safe, unless otherwise noted. When
 *          dlb2_close() is called for a particular DLB handle, that handle can
 *          no longer be used.
 *
 * @param[in]  device_id: The ID of a DLB device file. That is, N in /dev/dlbN.
 * @param[out] hdl: Pointer to memory where a DLB handle will be stored.
 *
 * @retval  0 Success, hdl points to a valid DLB handle
 * @retval -ENOENT No DLB device found
 * @retval -EINVAL Invalid device ID or hdl pointer
 * @retval -EINVAL Client library version is incompatible with the installed
 *                   DLB driver
 */
int
dlb2_open(int device_id,
	  struct dlb2_dp **dlb2);

/*!
 * @fn int dlb2_close(
 *          struct dlb2_dp *dlb2);
 *
 * @brief   Clean up the client library and close the DLB device file
 *          associated with the DLB handle. The user must detach all scheduling
 *          domain handles attached with this handle before calling this
 *          function, else it will fail.
 *
 * @param[in] dlb2: Handle returned by a successful call to dlb2_open().
 *
 * @retval  0 Success
 * @retval -EEXIST There is at least one remaining attached domain handle
 * @retval -EINVAL Invalid DLB handle
 */
int
dlb2_close(struct dlb2_dp *dlb2);

/*!
 *  @brief DLB resources
 */
struct dlb2_resources {
	/* Number of available scheduling domains */
	u32 num_sched_domains;
	/*
	 * Number of available load-balanced queues. Each event sent to a
	 * load-balanced queue is scheduled among its linked ports according to
	 * one of three scheduling types: atomic, ordered, or unordered.
	 */
	u32 num_ldb_queues;
	/*
	 * Number of available load-balanced ports. A load-balanced port can
	 * link to up to 8 load-balanced queues to receive events, and can send
	 * events to any queue in its scheduling domain.
	 */
	u32 num_ldb_ports;
	/*
	 * Number of available directed ports. A directed port receives events
	 * from a single (directed) queue, and can send events to any queue in
	 * its scheduling domain.
	 */
	u32 num_dir_ports;
	/*
	 * Load-balanced event state entries. Each event scheduled to a
	 * load-balanced port uses an event state entry until the application
	 * releases it. The scheduling domain's event state entries are
	 * statically divided among its ports.
	 */
	u32 num_ldb_event_state_entries;
	/*
	 * Load-balanced event state storage is allocated to a scheduling
	 * domain in a contiguous chunk, because each port must be configured
	 * with a contiguous chunk of state entries. This is the largest
	 * available contiguous range of load-balanced event state entries.
	 */
	u32 max_contiguous_ldb_event_state_entries;
	/*
	 * Number of available load-balanced credits. A load-balanced credit is
	 * spent when a port enqueues to a load-balanced queue.
	 */
	u32 num_ldb_credits;
	/*
	 * Number of available directed credits. A directed credit is spent
	 * when a port enqueues to a directed queue.
	 */
	u32 num_dir_credits;
	/*
	 * Number of available load-balanced credit pools. For most cases, one
	 * pool per scheduling domain is recommended. In this case, all the
	 * domain's producer ports will pull from the same reservoir of credits
	 * when sending to load-balanced queues. Using multiple pools in a
	 * scheduling domain raises the possibility of credit starvation
	 * deadlock (@see dlb2_send for details on avoiding deadlock).
	 */
	u32 num_ldb_credit_pools;
	/*
	 * Number of available directed credit pools. For most cases, one pool
	 * per scheduling domain is recommended. In this case, all the domain's
	 * producer ports will pull from the same reservoir of credits when
	 * sending to directed queues. Using multiple pools in a scheduling
	 * domain raises the possibility of credit starvation deadlock (@see
	 * dlb2_send for details on avoiding deadlock).
	 */
	u32 num_dir_credit_pools;
	u32 num_sn_slots[2];

};

/*!
 * @fn int dlb2_get_num_resources(
 *          struct dlb2_dp *dlb2,
 *          struct dlb2_resources *rsrcs);
 *
 * @brief   Get the current number of available DLB resources. These
 *          resources can be assigned to a scheduling domain with
 *          dlb2_create_sched_domain() (if there are any available domains).
 *
 * @param[in]  dlb2: Handle returned by a successful call to dlb2_open().
 * @param[out] rsrcs: Pointer to memory where the resource information will be
 *                    written.
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid DLB handle or rsrcs pointer
 */
int
dlb2_get_num_resources(struct dlb2_dp *dlb2,
		       struct dlb2_resources *rsrcs);

/*********************************************/
/* Scheduling Domain Configuration Functions */
/*********************************************/

/*!
 *  @brief DLB scheduling domain creation arguments
 *
 *  @see struct dlb2_resources
 */
struct dlb2_create_sched_domain {
	/* Number of load-balanced queues */
	u32 num_ldb_queues;
	/* Number of load-balanced ports */
	u32 num_ldb_ports;
	/* Number of directed ports */
	u32 num_dir_ports;
	/*
	 * Number of load-balanced event state entries. Recommend giving each
	 * load-balanced port twice its CQ depth of event state entries, so the
	 * recommended allocation is 2*SUM(all load-balanced CQ depths).
	 */
	u32 num_ldb_event_state_entries;
	/* Number of load-balanced credits */
	u32 num_ldb_credits;
	/* Number of directed credits */
	u32 num_dir_credits;
	/* Number of load-balanced credit pools */
	u32 num_ldb_credit_pools;
	/* Number of directed credit pools */
	u32 num_dir_credit_pools;
	u32 num_sn_slots[2];
};

/*!
 * @fn int dlb2_create_sched_domain(
 *          struct dlb2_dp *dlb2,
 *          struct dlb2_create_sched_domain *args);
 *
 * @brief   Create a scheduling domain with the resources specified by args. If
 *          successful, the function returns the domain ID.
 *
 * @param[in] dlb2: Handle returned by a successful call to dlb2_open().
 * @param[in] args: Pointer to struct dlb2_create_sched_domain structure.
 *
 * @retval >=0 Scheduling domain ID
 * @retval -EINVAL Insufficient DLB resources to satisfy the request
 */
int
dlb2_create_sched_domain(struct dlb2_dp *dlb2,
			 struct dlb2_create_sched_domain *args);

/*!
 * @fn struct dlb2_domain_hdl *dlb2_attach_sched_domain(
 *                          struct dlb2_dp *dlb2,
 *                          int domain_id);
 *
 * @brief   Attach to a previously created scheduling domain.
 *
 * @note    A domain handle can be shared among kernel threads. Functions that
 *          take a domain handle are MT-safe, unless otherwise noted. When
 *          dlb2_detach_sched_domain() is called for a particular DLB handle,
 *          that handle can no longer be used.
 *
 * @param[in] dlb2: Handle returned by a successful call to dlb2_open().
 * @param[in] domain_id: Domain ID returned by a successful call to
 *                       dlb2_create_sched_domain().
 *
 * @retval Non-NULL  Success, returned a domain handle
 * @retval NULL Invalid domain ID or the scheduling domain is not configured
 */
struct dlb2_domain_hdl *
dlb2_attach_sched_domain(struct dlb2_dp *dlb2, int domain_id);

/*!
 * @fn int dlb2_detach_sched_domain(
 *          struct dlb2_domain_hdl *hdl);
 *
 * @brief   Detach a scheduling domain handle. The handle can no longer be used
 *          after it is detached. All port handles from a domain must be
 *          detached before detaching any domain handles.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid domain handle
 * @retval -EEXIST There is at least one remaining attached port handle
 */
int
dlb2_detach_sched_domain(struct dlb2_domain_hdl *hdl);

/*!
 * @fn int dlb2_create_ldb_pool(
 *          struct dlb2_domain_hdl *hdl,
 *          int num_credits);
 *
 * @brief   Create a load-balanced credit pool. These credits are used by ports
 *          to enqueue events to load-balanced queues.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] num_credits: Number of credits in the pool.
 *
 * @retval >=0 Pool ID
 * @retval -EINVAL Invalid domain handle
 * @retval -EINVAL Insufficient load-balanced credits available in the domain
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_ldb_credit_pool(struct dlb2_domain_hdl *hdl,
			    int num_credits);

/*!
 * @fn int dlb2_create_dir_pool(
 *          struct dlb2_domain_hdl *hdl,
 *          int num_credits);
 *
 * @brief   Create a directed credit pool. These credits are used by ports
 *          to enqueue events to directed queues.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] num_credits: Number of credits in the pool.
 *
 * @retval >=0 Pool ID
 * @retval -EINVAL Invalid domain handle
 * @retval -EINVAL Insufficient directed credits available in the domain
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_dir_credit_pool(struct dlb2_domain_hdl *hdl,
			    int num_credits);

/*
 * @brief Load-balanced queue configuration
 *
 * A load-balanced queue can support atomic and ordered scheduling or atomic and
 * unordered scheduling, but not all three.
 *
 * If the user configures the queue with a non-zero num_sequence_numbers, it is
 * configured for atomic and ordered scheduling. In this case, SCHED_ORDERED
 * is supported but SCHED_UNORDERED is not. Conversely, if the user configures
 * the queue with zero num_sequence_numbers, it is configured for atomic and
 * unordered scheduling. In this case, the queue cannot schedule SCHED_ORDERED
 * events but can schedule SCHED_UNORDERED events.
 *
 * The num_sequence_numbers field sets the ordered queue's reorder buffer size.
 * DLB has 2 groups of ordered queues, where each group is configured to
 * contain either 1 queue with 1024 reorder entries, 2 queues with 512 reorder
 * entries, and so on down to 16 queues with 64 entries. These groups are
 * configured through the sysfs interface, and once the first ordered queue is
 * configured then the four group configurations are fixed, for all domains and
 * VFs, until the device is reset.
 *
 * If the user requests num_sequence_numbers of, for example, 64, there must be
 * at least one ordered queue group configured for 16 queues of 64 entries each
 * and at least one available slot in the group. Otherwise, the queue
 * configuration will fail. All four queue groups default to 16 queues with 64
 * reorder entries, and these groups can be configured through the DLB sysfs
 * interface.
 *
 * (TODO: Add an ioctl to control this)
 */
struct dlb2_create_ldb_queue {
	/**
	 * Number of sequence numbers. Valid configurations are as power-of-two
	 * numbers between 32 and 1024, inclusive. If 0, the queue will not
	 * supported ordered traffic.
	 */
	u32 num_sequence_numbers;
	/**
	 * Lock ID compression level. A non-zero value specifies the number of
	 * unique lock IDs the queue should compress down to, and valid values
	 * are 64, 128, 256, 512, 1024, 2048, 4096, and 65536. If
	 * lock_id_comp_level is 0, the queue will not compress its lock IDs.
	 */
	u32 lock_id_comp_level;
};

/*!
 * @fn int dlb2_create_ldb_queue(
 *          struct dlb2_domain_hdl *hdl,
 *          struct dlb2_create_ldb_queue *args);
 *
 * @brief   Create a load-balanced queue. events sent to this queue are
 *          load-balanced across the ports that have linked the queue.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] args: Pointer to queue configuration arguments.
 *
 * @retval >=0 Queue ID
 * @retval -EINVAL Invalid domain handle
 * @retval -EINVAL Insufficient available sequence numbers.
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_ldb_queue(struct dlb2_domain_hdl *hdl,
		      struct dlb2_create_ldb_queue *args);

/*!
 * @fn int dlb2_create_dir_queue(
 *          struct dlb2_domain_hdl *hdl,
 *          int port_id);
 *
 * @brief   Create a directed queue. events sent to this queue are enqueued
 *          directly to a single port.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] port_id: Directed port ID. If the queue's corresponding directed
 *                     port is already created, specify its ID here. Else this
 *                     argument must be -1 to indicate that the queue is being
 *                     created before the port.
 *
 * @retval >=0 Queue ID. If port_id is not -1, the queue ID will be the same as
 *             the port ID.
 * @retval -EINVAL Invalid domain handle or port ID
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_dir_queue(struct dlb2_domain_hdl *hdl,
		      int port_id);

struct dlb2_create_port {
	/* Load-balanced credit pool ID */
	u32 ldb_credit_pool_id;
	/* Directed credit pool ID */
	u32 dir_credit_pool_id;
	/*
	 * Number of load-balanced credits. If 0, the port cannot send
	 * load-balanced events.
	 */
	u16 num_ldb_credits;
	/*
	 * Number of directed credits. If 0, the port cannot send
	 * directed events.
	 */
	u16 num_dir_credits;
	/*
	 * Depth of the port's consumer queue. Must be a power-of-2 number
	 * between 8 and 1024, inclusive.
	 *
	 * For load-balanced CQs, smaller CQ depths (8-16) are recommended for
	 * best load-balancing. For directed CQs, larger depths (128+) are
	 * recommended.
	 */
	u16 cq_depth;
	/*
	 * Load-balanced event state storage. Must be a power-of-2 number
	 * greater than or equal to the CQ depth. Applicable to load-balanced
	 * ports only.
	 *
	 * Recommended setting: 2*cq_depth.
	 */
	u32 num_ldb_event_state_entries;
};

/*!
 * @fn int dlb2_create_ldb_port(
 *          struct dlb2_domain_hdl *hdl,
 *          struct dlb2_create_port *args);
 *
 * @brief   Create a load-balanced port. This port can be used to enqueue events
 *          into load-balanced or directed queues, and to receive events from up
 *          to 8 load-balanced queues.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] args: Pointer to port configuration arguments.
 *
 * @retval >=0 Port ID
 * @retval -EINVAL Invalid domain handle
 * @retval -EINVAL NULL args pointer
 * @retval -EINVAL Name is in use, invalid credit pool ID, insufficient
 *                 available credits, or invalid CQ depth
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_ldb_port(struct dlb2_domain_hdl *hdl,
		     struct dlb2_create_port *args);

/*!
 * @fn int dlb2_create_dir_port(
 *          struct dlb2_domain_hdl *hdl,
 *          struct dlb2_create_port *args,
 *          int queue_id);
 *
 * @brief   Create a directed port. This port can be used to enqueue events into
 *          load-balanced or directed queues, and to receive events from a
 *          directed queue.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] args: Pointer to port configuration arguments.
 * @param[in] queue_id: Directed queue ID. If the port's corresponding directed
 *                      port is already created, specify its ID here. Else this
 *                      argument must be -1 to indicate that the port is being
 *                      created before the queue.
 *
 * @retval >=0 Port ID
 * @retval -EINVAL Invalid domain handle
 * @retval -EINVAL NULL args pointer
 * @retval -EINVAL Name is in use, invalid credit pool ID, insufficient
 *                 available credits, or invalid CQ depth
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_create_dir_port(struct dlb2_domain_hdl *hdl,
		     struct dlb2_create_port *args,
		     int queue_id);

/*!
 * @fn struct dlb2_port_hdl *dlb2_attach_ldb_port(
 *          struct dlb2_domain_hdl *hdl,
 *          int port_id);
 *
 * @brief   Attach to a previously created load-balanced port. The handle
 *          returned is used to send and receive events.
 *
 * @note    Nearly all functions that take a port handle are *not* MT-safe
 *          (these functions are documented as such). It is invalid to
 *          simultaneously run *any* non-MT-safe datapath functions (e.g.,
 *          dlb2_send and dlb2_recv()) with the same port handle. Multiple
 *          threads within a single process can share a port handle, so long as
 *          the application provides mutual exclusion around the datapath
 *          functions.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] port_id: load-balanced port ID.
 *
 * @retval Non-NULL  Success, returned a port handle
 * @retval NULL Invalid domain handle or port ID
 */
struct dlb2_port_hdl *
dlb2_attach_ldb_port(struct dlb2_domain_hdl *hdl,
		     int port_id);

/*!
 * @fn struct dlb2_port_hdl *dlb2_attach_dir_port(
 *          struct dlb2_domain_hdl *hdl,
 *          int port_id);
 *
 * @brief   Attach to a previously created directed port. The handle returned
 *          is used to send and receive events.
 *
 * @note    A port can be attached to by only one process.
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 * @param[in] port_id: directed port ID.
 *
 * @retval Non-NULL  Success, returned a port handle
 * @retval NULL Invalid domain handle or port ID
 */
struct dlb2_port_hdl *
dlb2_attach_dir_port(struct dlb2_domain_hdl *hdl,
		     int port_id);

/*!
 * @fn int dlb2_detach_port(
 *          struct dlb2_port_hdl *hdl);
 *
 * @brief   Detach a previously attached port handle.
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid port handle
 */
int
dlb2_detach_port(struct dlb2_port_hdl *hdl);

/*!
 * @fn int dlb2_link_queue(
 *          struct dlb2_port_hdl *hdl,
 *          int queue_id,
 *          int priority);
 *
 * @brief   Link a load-balanced queue to a load-balanced port. Each
 *          load-balanced port can link up to 8 queues.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb2_start_domain().
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port().
 * @param[in] qid: Load-balanced queue ID.
 * @param[in] priority: Priority. Must be between 0 and 8, inclusive, where 0
 *                      is the highest priority.
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid port handle, queue ID, or priority
 * @retval -EINVAL The port is already linked to 8 queues.
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_link_queue(struct dlb2_port_hdl *hdl,
		int queue_id,
		int priority);

/*!
 * @fn int dlb2_unlink_queue(
 *          struct dlb2_port_hdl *hdl,
 *          int queue_id);
 *
 * @brief   Unlink a load-balanced queue from a load-balanced port.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb2_start_sched_domain().
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port().
 * @param[in] qid: Load-balanced queue ID.
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid port handle or queue ID
 * @retval -EINVAL The queue ID was not linked to the port
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_unlink_queue(struct dlb2_port_hdl *hdl,
		  int queue_id);

/*!
 * @fn int dlb2_enable_port(
 *          struct dlb2_port_hdl *hdl);
 *
 * @brief   Enable event scheduling to the port. By default, ports are enabled.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb2_start_sched_domain().
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid port handle
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_enable_port(struct dlb2_port_hdl *hdl);

/*!
 * @fn int dlb2_disable_port(
 *          struct dlb2_port_hdl *hdl);
 *
 * @brief   Disable event scheduling to the port and prevent threads from
 *          blocking on dequeueing events from the port. By default, ports are
 *          enabled. A disabled port can continue to enqueue events and dequeue
 *          any events remaining in the port's CQ, but the DLB will not
 *          schedule any further events to the port until it is re-enabled.
 *
 * @note    If a thread is blocked in a DLB receive function call when the
 *          port is disabled, the thread will be unblocked and return.
 *
 * @note    After disabling a port, it may still have events in its CQ. It is
 *          the application's responsibility to drain the port's CQ after
 *          disabling it.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb2_start_sched_domain().
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid port handle
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 */
int
dlb2_disable_port(struct dlb2_port_hdl *hdl);

/*!
 * @fn int dlb2_start_sched_domain(
 *          dlb2_domain_hdl *hdl);
 *
 * @brief   This function is called to indicated the end of the DLB
 *          configuration phase and beginning of the dataflow phase. Until this
 *          function is called, ports cannot send events, and after it is called
 *          the domain's resources cannot be configured (unless otherwise noted
 *          in the documentation).
 *
 * @param[in] domain: domain handle returned by a successful call to
 *                    dlb2_attach_sched_domain().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid domain handle
 * @retval -EINTR The DLB was unexpectedly reset. User must detach ports
 *		  then detach and reset scheduling domains, then can
 *		  attempt to re-create their scheduling domain(s).
 * @retval -ESRCH The domain alert thread hasn't launched. The application
 *		  must call dlb2_launch_domain_alert_thread() before starting
 *		  the domain.
 *
 * @see dlb2_launch_domain_alert_thread()
 */
int
dlb2_start_sched_domain(struct dlb2_domain_hdl *hdl);

/*!
 * @fn int dlb2_reset_sched_domain(
 *          struct dlb2_dp *dlb2,
 *          int domain_id);
 *
 * @brief   This function resets a configured scheduling domain, effectively
 *          undoing the effects of dlb2_create_sched_domain(). The domain's
 *          resources (ports, queues, pools, etc.) are relinquished to the DLB
 *          driver, making them available to be re-allocated and reconfigured.
 *
 *          Prior to calling this function, all port and domain handles must be
 *          detached. This function wakes any threads blocked reading the
 *          domain's device file, causing them to return errno ENODEV.
 *
 * @param[in] domain_id: Domain ID returned by a successful call to
 *                       dlb2_create_sched_domain().
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid domain ID
 * @retval -EEXIST There is at least one remaining attached domain or port
 *                 handle
 */
int
dlb2_reset_sched_domain(struct dlb2_dp *dlb2,
			int domain_id);

/****************************/
/* Scheduling Domain Alerts */
/****************************/

/*
 *  @brief DLB alert IDs
 */
enum dlb2_alert_id {
	/** The device containing this domain is being reset. The application
	 * needs to detach all its handles and call dlb2_close(), or simply
	 * exit and restart. The alert data doesn't contain any information for
	 * this alert.
	 */
	DLB2_ALERT_DEVICE_RESET,
	/** The domain is being reset, triggered by a call to
	 * dlb2_reset_sched_domain(). The application doesn't need to take any
	 * action in response to this alert. The alert data doesn't contain any
	 * information for this alert.
	 */
	DLB2_ALERT_DOMAIN_RESET,
};

/*!
 *  @brief DLB alert information
 */
struct dlb2_dp_domain_alert {
	enum dlb2_alert_id id;
	u64 data;
};

/*!
 * @fn int dlb2_launch_domain_alert_thread(
 *	    struct dlb2_domain_hdl *hdl,
 *	    void (*callback)(struct dlb2_dp_domain_alert *alert,
 *			     int domain_id,
 *			     void *arg),
 *	    void *callback_arg)
 *
 * @brief   This function launches a background thread that blocks waiting
 *          for scheduling domain alerts. The thread calls *callback* (if
 *          non-NULL) for every alert it receives.
 *
 *          Each kernel dlb2 user must launch a domain alert thread before
 *          starting its domain, and only one alert thread per domain is
 *          supported.
 *
 *          While blocking, the thread is put to sleep by the kernel and thus
 *          doesn't consume any CPU. Alerts are uncommon -- typically only when
 *          an error occurs or during a reset -- so this thread should not
 *          affect datapath performance.
 *
 *          The alert thread is destroyed when the scheduling domain is reset,
 *          or the application exits.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb2_attach_sched_domain().
 * @param[out] callback: Callback function pointer. Can be NULL.
 * @param[out] callback_arg: Callback function argument. Can be NULL.
 *
 * @retval  0 Success
 * @retval -EINVAL Invalid domain handle
 * @retval -EEXIST The alert thread is already launched
 *
 * @see enum dlb2_alert_id
 */
int
dlb2_launch_domain_alert_thread(struct dlb2_domain_hdl *hdl,
				void (*cb)(struct dlb2_dp_domain_alert *alert,
					   int domain_id,
					   void *arg),
				void *cb_arg);

/****************************************/
/* Scheduling Domain Datapath Functions */
/****************************************/

/*!
 * @fn int dlb2_send(
 *          struct dlb2_port_hdl *hdl,
 *          u32 num,
 *          union dlb2_event *events,
 *          int *error);
 *
 * @brief   Send one or more new events.
 *
 *          If the port has insufficient credits to send all num events, it will
 *          return early. Typically insufficient credits is a transient
 *          condition and the send should be retried; however, certain pipeline
 *          architectures and credit pool configurations can lead to deadlock.
 *          Consequently, it is strongly recommended that the application have a
 *          finite retry count and if necessary release the events and continue
 *          processing the port's consumer queue. (Credits aren't consumed when
 *          releasing events.)
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] num: Number of events pointed to by events.
 * @param[in] events: Pointer to an array of dlb2_event structures.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of enqueued events. Events
 *             are enqueued in array order. Fewer than num events will be
 *             enqueued if there are insufficient credits (error will be set to
 *             0), or an error occurred (and error will be set according to the
 *             following exceptions).
 * @exception -EINVAL events pointer is NULL
 * @exception -EINVAL Invalid port handle, or invalid queue ID in events
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EINTR  The DLB was unexpectedly reset. User must detach ports
 *		      then detach and reset scheduling domains, then can
 *		      attempt to re-create their scheduling domain(s).
 */
int
dlb2_send(struct dlb2_port_hdl *hdl,
	  u32 num,
	  union dlb2_event *events,
	  int *error);

/*!
 * @fn int dlb2_release(
 *          struct dlb2_port_hdl *hdl,
 *          u32 num,
 *          int *error);
 *
 * @brief   Release one or more events. Every event received by a
 *          load-balanced port has to be released; this tells the DLB that the
 *          port is done processing the event. Releases apply to events in the
 *          order in which events are received; that is, each release applies
 *          to the oldest unreleased event received on the port that issues
 *          the release.
 *
 *          Releases sent from a directed port are dropped.
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] num: Number of releases to issue.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of released events. Fewer
 *	       than num events will be released if there are insufficient
 *	       credits (error will be set to 0), or an error occurred (and
 *	       error will be set according to the following exceptions).
 * @exception -EINVAL Invalid port handle
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EINTR  The DLB was unexpectedly reset. User must detach ports
 *		      then detach and reset scheduling domains, then can
 *		      attempt to re-create their scheduling domain(s).
 */
int
dlb2_release(struct dlb2_port_hdl *hdl,
	     u32 num,
	     int *error);

/*!
 * @fn int dlb2_forward(
 *          struct dlb2_port_hdl *hdl,
 *          u32 num,
 *          union dlb2_event *events,
 *          int *error);
 *
 * @brief   Forward one or more events. A forward is equivalent to sending a new
 *          event followed by a release.
 *
 *          If the port has insufficient credits to send all num events, it will
 *          return early. Typically insufficient credits is a transient
 *          condition and the send should be retried; however, certain pipeline
 *          architectures can lead to deadlock. For such pipelines, the
 *          application, should have a finite retry count and if necessary
 *          release the events and continue processing the port's consumer
 *          queue.
 *
 *          TODO: Add a description of the deadlock scenarios. At least a
 *          warning.
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] num: Number of events pointed to by events.
 * @param[in] events: Pointer to an array of dlb2_event structures.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of enqueued events. Events
 *             are enqueued in array order. Fewer than num events will be
 *             enqueued if there are insufficient credits (error will be set to
 *             0), or an error occurred (and error will be set according to the
 *             following exceptions).
 * @exception -EINVAL events pointer is NULL
 * @exception -EINVAL Invalid port handle, or invalid queue ID in events
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EINTR  The DLB was unexpectedly reset. User must detach ports
 *		      then detach and reset scheduling domains, then can
 *		      attempt to re-create their scheduling domain(s).
 */
int
dlb2_forward(struct dlb2_port_hdl *hdl,
	     u32 num,
	     union dlb2_event *events,
	     int *error);

/*!
 * @fn int dlb2_pop_cq(
 *          struct dlb2_port_hdl *hdl,
 *          u32 num,
 *          int *error);
 *
 * @brief   Pop the CQ one or more times. Every event received for a port
 *          occupies a location in its CQ, and the pop clears space for more
 *          events to be scheduled to the port. Each pop is applied to the
 *          oldest event in the CQ.
 *
 * @note    dlb2_recv() automatically pops the CQ for every received event,
 *          but dlb2_recv_no_pop() does not. dlb2_pop_cq() should only be
 *          used in conjunction with dlb2_recv_no_pop().
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] port: Port handle returned by a successful call to
 *                  dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] num: Number of CQ pop operations to perform.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of released events. Fewer
 *	       than num events will be released if there are insufficient
 *	       credits (error will be set to 0), or an error occurred (and
 *	       error will be set according to the following exceptions).
 * @exception -EINVAL Invalid port handle
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EINTR The DLB was unexpectedly reset. User must detach ports
 *		     then detach and reset scheduling domains, then can
 *		     attempt to re-create their scheduling domain(s).
 */
int
dlb2_pop_cq(struct dlb2_port_hdl *hdl,
	    u32 num,
	    int *error);

/*!
 * @fn int dlb2_recv(
 *          struct dlb2_port_hdl *hdl,
 *          u32 max,
 *          bool wait,
 *          union dlb2_event *events,
 *	    int *err);
 *
 * @brief   Receive one or more events, and pop the CQ for each event received.
 *          The wait argument determines whether the function waits for at
 *          least one event or not. Received events must be processed in order.
 *
 * @note    All events received on a load-balanced port must be released with
 *          dlb2_release() or dlb2_forward().
 *          The advanced send function can release the oldest event with the
 *          REL/REL_T commands, or can simultaneously release the oldest
 *          event and send a event with the FWD/FWD_T commands.
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] max: Maximum number of events to receive.
 * @param[in] wait: If false, the function receives as many events as it can,
 *                  up to max or until the CQ is empty. If true, the function
 *                  waits until the CQ contains at least one event, then
 *                  receives as many events as it can, up to max or until the
 *                  CQ is empty.
 * @param[out] events: Pointer to an array of dlb2_event structures.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of dequeued events. If
 *	       fewer than num events are dequeued, either there were not num
 *	       events available (error will be set to 0), or an error occurred
 *	       (and error will be set according to the following exceptions).
 * @exception -EINVAL Invalid port handle or NULL events pointer
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EACCES The port is disabled and there are no events in its CQ
 * @exception -EINTR  The DLB was unexpectedly reset. User must detach ports
 *		      then detach and reset scheduling domains, then can
 *		      attempt to re-create their scheduling domain(s).
 */
int
dlb2_recv(struct dlb2_port_hdl *hdl,
	  u32 max,
	  bool wait,
	  union dlb2_event *events,
	  int *err);

/*!
 * @fn int dlb2_recv_no_pop(
 *          struct dlb2_port_hdl *hdl,
 *          u32 max,
 *          bool wait,
 *          union dlb2_event *events,
 *	    int *err);
 *
 * @brief   Receive one or more events. The wait argument determines whether
 *          the function waits for at least one event or not. Received events
 *          must be processed in order.
 *
 * @brief   Receive one or more events. The wait argument determines whether the
 *          function waits for at least one event or not. Received events must
 *          be processed in order.
 *
 * @note    All received events occupy space in the CQ that needs to be cleared
 *          to allow more events to be scheduled to the port.
 *          dlb2_recv_no_pop() does not automatically pop the event; the user
 *          must call dlb2_pop_cq() to do so. Alternatively, the advanced send
 *          function has ways to pop the CQ:
 *          - By sending a batch token return command (BAT_T)
 *          - By piggy-backing a single token return onto another enqueue
 *            command (NEW_T, FWD_T, COMP_T, or FRAG_T)
 *
 * @note    All events received on a load-balanced port must be released with
 *          dlb2_release() or dlb2_forward().
 *          The advanced send function can release the oldest event with the
 *          REL/REL_T commands, or can simultaneously release the oldest
 *          event and send a event with the FWD/FWD_T commands.
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb2_attach_ldb_port() or dlb2_attach_dir_port().
 * @param[in] max: Maximum number of events to receive.
 * @param[in] wait: If false, the function receives as many events as it can,
 *                  up to max or until the CQ is empty. If true, the function
 *                  waits until the CQ contains at least one event, then
 *                  receives as many events as it can, up to max or until the
 *                  CQ is empty.
 * @param[out] events: Pointer to an array of dlb2_event structures.
 * @param[out] error: If an error is detected, this is set according to the
 *		      following exception values. Otherwise, it is set to 0.
 *                    If NULL, the function will not record an error value.
 *
 * @retval >=0 The return value indicates the number of dequeued events. If
 *	       fewer than num events are dequeued, either there were not num
 *	       events available (error will be set to 0), or an error occurred
 *	       (and error will be set according to the following exceptions).
 * @exception -EINVAL Invalid port handle or NULL events pointer
 * @exception -EPERM  The scheduling domain isn't started
 * @exception -EACCES The port is disabled and there are no events in its CQ
 * @exception -EINTR  The DLB was unexpectedly reset. User must detach ports
 *		      then detach and reset scheduling domains, then can
 *		      attempt to re-create their scheduling domain(s).
 */
int
dlb2_recv_no_pop(struct dlb2_port_hdl *hdl,
		 u32 max,
		 bool wait,
		 union dlb2_event *events,
		 int *err);

#endif /* __DLB2_DP_H */
