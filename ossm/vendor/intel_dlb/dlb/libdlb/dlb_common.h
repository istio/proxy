/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

/*! @file      dlb_common.h
 *
 *  @brief     DLB Client API Common Data Structures
 */

#ifndef __DLB_COMMON_H__
#define __DLB_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque DLB handle */
typedef void *dlb_hdl_t;
/** Opaque DLB scheduling domain handle */
typedef void *dlb_domain_hdl_t;
/** Opaque DLB port handle */
typedef void *dlb_port_hdl_t;

/*!
 *  @brief Event Scheduling Types
 */
enum dlb_event_sched_t {
    /** Atomic scheduling. Only valid if the destination queue is
     * load-balanced.
     */
    SCHED_ATOMIC = 0,
    /** Unordered scheduling. Only valid if the destination queue is
     * load-balanced and was configured with zero sequence numbers.
     */
    SCHED_UNORDERED,
    /** Ordered scheduling. Only valid if the destination queue is load-balanced
     * and was configured with non-zero sequence numbers.
     */
    SCHED_ORDERED,
    /** Directed scheduling. Only valid when the destination queue is
     * directed.
     */
    SCHED_DIRECTED
};

/*!
 *  @brief Event commands
 */
enum dlb_event_cmd_t {
    /** NOOP */
    NOOP = 0,
    /** Batch token return */
    BAT_T,
    /** Event release */
    REL,
    /** Event release with a single token return */
    REL_T,

    /** Reserved */
    RSVD4,
    /** Reserved */
    RSVD5,
    /** Reserved */
    RSVD6,
    /** Reserved */
    RSVD7,

    /** New event enqueue */
    NEW = 8,
    /** New event enqueue with a single token return */
    NEW_T,
    /** Forward event (NEW + REL) */
    FWD,
    /** Forward event (NEW + REL) with a single token return */
    FWD_T,
    /* Fragment */
    FRAG,
    /* Fragment with a single token return */
    FRAG_T,

    /** NUM_EVENT_CMD_TYPES must be last */
    NUM_EVENT_CMD_TYPES,
};

/*!
 *  @brief DLB event send structure
 *
 *  Some of the fields depend on the device's capabilities.
 *
 *  @see dlb_get_dev_capabilities, dlb_dev_cap_t
 */
typedef struct {
    /** 64 bits of user data */
    uint64_t udata64;
    /** 16 bits of user data */
    uint16_t udata16;
    /** Destination queue ID */
    uint8_t queue_id;
    /** Scheduling type (use enum dlb_event_sched_t) */
    uint8_t sched_type:2;
    /** Priority */
    uint8_t priority:3;
    /** Reserved */
    uint8_t rsvd0:3;
    /** Flow ID (valid for atomic scheduling) */
    uint16_t flow_id;
    /** Reserved */
    uint8_t rsvd1:1;
    /**
     * The value of the weight field allows the event to effectively occupy
     * more slots in the recipient CQ. The map of weight values to CQ slots
     * are:
     * - 0: 1 CQ slot
     * - 1: 2 CQ slots
     * - 2: 4 CQ slots
     * - 3: 8 CQ slots
     *
     * For example, one QE with weight 3 will fill a CQ with a depth of 8.
     *
     * This field is only used if the recipient CQ has weight-based scheduling
     * enabled through dlb_enable_cq_weight().
     *
     * This field is supported in devices with the qe_weight capability,
     * otherwise it is reserved.
     */
    uint8_t weight:2;
    /** Reserved */
    uint8_t rsvd2:5;
    /** Reserved */
    uint8_t rsvd3;
} __attribute((packed)) dlb_send_t;

/*!
 *  @brief DLB event receive structure
 *
 *  Some of the fields depend on the device's capabilities.
 *
 *  @see dlb_get_dev_capabilities, dlb_dev_cap_t
 */
typedef struct {
    /** 64-bit event data */
    uint64_t udata64;
    /** 16 bits of user data */
    uint16_t udata16;
    /** Queue ID that this event was sent to (load-balanced only) */
    uint8_t queue_id;
    /** Scheduling type (enum dlb_event_sched_t) (load-balanced only). */
    uint8_t sched_type:2;
    /** Priority */
    uint8_t priority:3;
    /** Reserved */
    uint8_t rsvd0:3;
    /** Flow ID. Set if the device has the ldb_deq_event_fid capability. */
    uint16_t flow_id;
    /** Reserved */
    uint8_t rsvd2;
    /** Reserved */
    uint8_t rsvd3:5;
    /** Flag set by hardware indicating an error in the event
     *  (TODO: more info)
     */
    uint8_t error:1;
    /** Reserved */
    uint8_t rsvd4:2;
} __attribute((packed)) dlb_recv_t;

/*!
 *  @brief Advanced DLB event send structure. For use with dlb_adv_send().
 */
typedef struct {
    /** 64-bit event data */
    uint64_t udata64;
    /** 16 bits of user data */
    uint16_t udata16;
    /** Queue ID */
    uint8_t queue_id;
    /** Scheduling type (use enum dlb_event_sched_t) */
    uint8_t sched_type:2;
    /** Priority */
    uint8_t priority:3;
    /** Reserved */
    uint8_t rsvd0:3;
    /**
     * For the BAT_T command, the field indicates the number of tokens to
     * return. For NEW/NEW_T and FWD/FWD_T commands, the flow_id field
     * specifies the event's flow ID.
     */
    union {
        /** Flow ID (valid for atomic scheduling) */
        uint16_t flow_id;
        /** Number of tokens to return, minus one */
        uint16_t num_tokens_minus_one;
    };
    /** Reserved */
    uint8_t rsvd1:1;
    /** Event weight (@see dlb_send_t) */
    uint8_t weight:2;
    /** Reserved */
    uint8_t rsvd2:5;
    /** Send command (use enum dlb_event_cmd_t) */
    uint8_t cmd:4;
    /** Reserved */
    uint8_t rsvd3:4;
} dlb_adv_send_t;

/*!
 *  @brief DLB event structure
 */
typedef union {
    /** Structure for sending events */
    dlb_send_t send;
    /** Structure for receiving events */
    dlb_recv_t recv;
    /** Structure for sending events with the advanced send function */
    dlb_adv_send_t adv_send;
} dlb_event_t;

#ifdef __cplusplus
}
#endif

#endif /* __DLB_COMMON_H__ */
