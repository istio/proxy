/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

/* TODO: flesh out error checking and error codes documentation */

/*! @file      dlb.h
 *
 *  @brief     DLB Client API
 *
 *  @details   This API supports enables the configuration and use of the DLB
 *             for hardware-accelerated queue scheduling and core-to-core
 *             communication.
 */

#ifndef __DLB_H__
#define __DLB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "dlb_common.h"
#include "dlb_adv.h"

/******************************/
/* DLB Device-Level Functions */
/******************************/


/*!
 * @fn int dlb_open(
 *          int device_id,
 *          dlb_hdl_t *hdl);
 *
 * @brief   Open the DLB device file and initialize the client library.
 *
 * @note    A DLB handle can be shared among threads within a process, but
 *          cannot be used by multiple processes. Functions that take a DLB
 *          handle are MT-safe, unless otherwise noted. When dlb_close() is
 *          called for a particular DLB handle, that handle can no longer be
 *          used.
 *
 * @param[in]  device_id: The ID of a DLB device file. That is, N in /dev/dlbN.
 * @param[out] hdl: Pointer to memory where a DLB handle will be stored.
 *
 * @retval  0 Success, hdl points to a valid DLB handle
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception ENOENT No DLB device found
 * @exception EINVAL Invalid device ID or hdl pointer
 * @exception EINVAL Client library version is incompatible with the installed
 *                   DLB driver
 */
int
dlb_open(
    int device_id,
    dlb_hdl_t *hdl);

/*!
 * @fn int dlb_close(
 *          dlb_hdl_t hdl);
 *
 * @brief   Clean up the client library and close the DLB device file
 *          associated with the DLB handle. The user must detach all scheduling
 *          domain handles attached with this handle before calling this
 *          function, else it will fail.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EEXIST There is at least one remaining attached domain handle
 * @exception EINVAL Invalid DLB handle
 */
int
dlb_close(
    dlb_hdl_t hdl);

/*!
 *  @brief DLB device capability flags
 */
typedef struct {
    /** The device places the flow ID in the dequeued load-balanced event. When
     * this flag is set, the device fills dlb_ldb_recv_t's flow_id field with
     * the enqueued flow ID.
     */
    uint64_t ldb_deq_event_fid:1;
    /** The device supports multiple load-balanced port scheduling classes-of-
     * service (CoS). The device defines four classes, each with a percentage
     * of guaranteed scheduling bandwidth (25% per class by default) with any
     * unreserved bandwidth shared between all classes.
     *
     * For devices with this capability, ports from specific classes of service
     * can be reserved when creating the a scheduling domain and a desired CoS
     * can be specified when creating a load-balanced port.
     */
    uint64_t port_cos:1;
    /** The device supports queue depth threshold indicators. When this flag is
     * set, each queue can be configured with a threshold, and the dequeue
     * event's qdi field indicates the queue's depth relative to that
     * threshold.
     */
    uint64_t queue_dt:1;
    /** The device supports lock ID compression. When this flag is set, each
     * queue can be configured to hash and compress lock IDs in order to fit
     * more concurrent flows in the device's atomic storage area, at the
     * (potential) cost of increased average flow pinning duration.
     */
    uint64_t lock_id_comp:1;
    /** The device supports a combined load-balanced and directed credit pool.
     * When this flag is set, the user only needs a single credit pool for
     * sending both load-balanced and directed events.
     */
    uint64_t combined_credits:1;
    /** The device supports weight-based scheduling. This must be enabled for
     * each port by calling dlb_enable_cq_weight().
     *
     * @see dlb_enable_cq_weight for more details
     */
    uint64_t qe_weight:1;
    /** The device supports event fragmentation. When a thread is processing an
     * ordered event, it can perform up to 16 partial enqueues, which allows a
     * single received ordered event to result in multiple reordered events.
     */
    uint64_t op_frag:1;
} dlb_dev_cap_t;

/*!
 * @fn int dlb_get_dev_capabilities(
 *          dlb_hdl_t hdl,
 *          dlb_dev_cap_t *cap);
 *
 * @brief   Get the capabilities of the DLB device represented by hdl. These
 *          capabilities indicate what device-specific features are usable.
 *
 * @param[in]  hdl: Handle returned by a successful call to dlb_open().
 * @param[out] cap: Pointer to memory where the capability information will be
 *                  written.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle or cap pointer
 */
int
dlb_get_dev_capabilities(
    dlb_hdl_t hdl,
    dlb_dev_cap_t *cap);

/*!
 *  @brief DLB resources
 */
typedef struct {
    /** Number of available scheduling domains */
    uint32_t num_sched_domains;
    /** Number of available load-balanced queues. Each event sent to a
     * load-balanced queue is scheduled among its linked ports according to one
     * of three scheduling types: atomic, ordered, or unordered.
     */
    uint32_t num_ldb_queues;
    /** Number of available load-balanced ports. A load-balanced port can link
     * to up to 8 load-balanced queues to receive events, and can send events
     * to any queue in its scheduling domain.
     */
    uint32_t num_ldb_ports;
    /** Number of load-balanced ports belonging to each of the four
     * classes-of-service. The sum of this array is equal to num_ldb_ports.
     *
     * This field is not valid if the device doesn't support load-balanced port
     * classes of service (indicated by the port_cos capability flag).
     */
    uint32_t num_ldb_ports_per_cos[4];
    /** Number of available directed ports. A directed port receives events from
     * a single (directed) queue, and can send events to any queue in its
     * scheduling domain.
     */
    uint32_t num_dir_ports;
    /**
     * Load-balanced event state entries. Each event scheduled to a
     * load-balanced port uses an event state entry until the application
     * releases it. The scheduling domain's event state entries are statically
     * divided among its ports.
     */
    uint32_t num_ldb_event_state_entries;
    /** Load-balanced event state storage is allocated to a scheduling domain in
     * a contiguous chunk, because each port must be configured with a
     * contiguous chunk of state entries. This is the largest available
     * contiguous range of load-balanced event state entries.
     */
    uint32_t max_contiguous_ldb_event_state_entries;
    union {
        /** Load-balanced and directed credit info. Used by devices without the
         * combined_credits capability.
         */
        struct {
            /** Number of available load-balanced credits. A load-balanced
             * credit is spent when a port enqueues to a load-balanced queue.
             */
            uint32_t num_ldb_credits;
            /** Load-balanced credits are allocated to a scheduling domain in a
             * contiguous chunk, because each pool must be configured with a
             * contiguous chunk of credits. This is the largest available
             * contiguous range of load-balanced credits.
             */
            uint32_t max_contiguous_ldb_credits;
            /** Number of available directed credits. A directed credit is
             * spent when a port enqueues to a directed queue.
             */
            uint32_t num_dir_credits;
            /** Directed credits are allocated to a scheduling domain in a
             * contiguous chunk, because each pool must be configured with a
             * contiguous chunk of credits. This is the largest available
             * contiguous range of directed credits.
             */
            uint32_t max_contiguous_dir_credits;
            /** Number of available load-balanced credit pools. For most cases,
             * one pool per scheduling domain is recommended. In this case, all
             * the domain's producer ports will pull from the same reservoir of
             * credits when sending to load-balanced queues. Using multiple
             * pools in a scheduling domain raises the possibility of credit
             * starvation deadlock (@see dlb_send for details on avoiding
             * deadlock).
             */
            uint32_t num_ldb_credit_pools;
            /** Number of available directed credit pools. For most cases, one
             * pool per scheduling domain is recommended. In this case, all the
             * domain's producer ports will pull from the same reservoir of
             * credits when sending to directed queues. Using multiple pools in
             * a scheduling domain raises the possibility of credit starvation
             * deadlock (@see dlb_send for details on avoiding deadlock).
             */
            uint32_t num_dir_credit_pools;
        };
        /** Combined credit info. Used by devices with the combined_credits
         * capability.
         */
        struct {
            /** Number of available credits. A credit is spent when a port
             * enqueues an event to a queue.
             */
            uint32_t num_credits;
            /** Number of available credit pools. For most cases, one pool per
             * scheduling domain is recommended. In this case, all the domain's
             * producer ports will pull from the same reservoir of credits when
             * sending to an event queue. Using multiple pools in a scheduling
             * domain raises the possibility of credit starvation deadlock
             * (@see dlb_send for details on avoiding deadlock).
             */
            uint32_t num_credit_pools;
        };
    };
    /** Number of available sequence number slots in each of the 2 SN groups.
     */
    uint32_t num_sn_slots[2];
} dlb_resources_t;

/*!
 * @fn int dlb_get_num_resources(
 *          dlb_hdl_t hdl,
 *          dlb_resources_t *rsrcs);
 *
 * @brief   Get the current number of available DLB resources. These
 *          resources can be assigned to a scheduling domain with
 *          dlb_create_sched_domain() (if there are any available domains).
 *
 * @param[in]  hdl: Handle returned by a successful call to dlb_open().
 * @param[out] rsrcs: Pointer to memory where the resource information will be
 *                    written.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle or rsrcs pointer
 */
int
dlb_get_num_resources(
    dlb_hdl_t hdl,
    dlb_resources_t *rsrcs);

/*!
 * @fn int dlb_set_ldb_sequence_number_allocation(
 *          dlb_hdl_t hdl,
 *          unsigned int group,
 *          unsigned int num);
 *
 * @brief   Set the number of sequence numbers (SNs) per queue in an SN group.
 *          When a load-balanced queue is configured with N (N > 0) SNs, it is
 *          given a slot in a sequence number group configured for N SNs per
 *          queue. Each group has a total of 1024 SNs, so the number of slots
 *          in a group depends on the group's SN allocations. For example:
 *          - 64 SNs per queue => 16 slots
 *          - 128 SNs per queue => 8 slots
 *          ...
 *          - 1024 SNs per queue => 1 slot
 *
 *          If a queue is configured with N SNs, but the SN groups are either
 *          not configured for N SNs per queue or have no available slots, the
 *          queue configuration will fail.
 *
 * @note    A group's sequence number allocations cannot be altered if 1+ of
 *          the group's slots are in use, therefore, this function can only be
 *          called before any slots of the group is assigned to a domain (when
 *          a domain is created, for example) or VF/VDEV.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] group: Sequence number group ID.
 * @param[in] num: Sequence numbers per queue.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle
 * @exception EINVAL Invalid group ID or sequence number allocation
 * @exception EPERM  The sequence number allocation is locked and cannot be
 *                   changed
 */
int
dlb_set_ldb_sequence_number_allocation(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int num);

/*!
 * @fn int dlb_get_ldb_sequence_number_allocation(
 *          dlb_hdl_t hdl,
 *          unsigned int group,
 *          unsigned int *num);
 *
 * @brief   Get the number of sequence number groups.
 *
 * @see dlb_set_ldb_sequence_number_allocation
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] group: Sequence number group ID.
 * @param[out] num: Pointer to the sequence numbers per queue.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle
 * @exception EINVAL Invalid group ID or num is NULL
 */
int
dlb_get_ldb_sequence_number_allocation(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int *num);

/*!
 * @fn int dlb_get_ldb_sequence_number_occupancy(
 *          dlb_hdl_t hdl,
 *          unsigned int group,
 *          unsigned int *num);
 *
 * @brief   Get the occupancy (the number of in-use slots) of a sequence number
 *          group. Each sequence number (SN) group has one or more slots,
 *          depending on its configuration. I.e.:
 *
 *          - If configured for 1024 SNs per queue, the group has 1 slot
 *          - If configured for 512 SNs per queue, the group has 2 slots
 *            ...
 *          - If configured for 32 SNs per queue, the group has 32 slots
 *
 *          A sequence number group slot is used when it is assigned to a
 *          domain in either PF or VM.
 *
 * @see dlb_set_ldb_sequence_number_allocation
 * @see dlb_get_ldb_sequence_number_allocation
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] group: Sequence number group ID.
 * @param[out] num: Pointer to the group occupancy.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle
 * @exception EINVAL Invalid group ID or num is NULL
 */
int
dlb_get_ldb_sequence_number_occupancy(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int *num);

/*!
 * @fn int dlb_get_num_ldb_sequence_number_groups(
 *          dlb_hdl_t hdl);
 *
 * @brief   Get the number of sequence number groups.
 *
 * @see dlb_set_ldb_sequence_number_allocation
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 *
 * @retval >=0 Number of sequence number groups
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle
 */
int
dlb_get_num_ldb_sequence_number_groups(
    dlb_hdl_t hdl);

/*!
 * @fn int dlb_get_min_ldb_sequence_number_allocation(
 *          dlb_hdl_t hdl);
 *
 * @brief   Get the minimum configurable sequence numbers (SNs) per queue
 *          allowed by the device.
 *
 * @see dlb_set_ldb_sequence_number_allocation
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 *
 * @retval >=0 Minimum configurable sequence numbers per queue allowed
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid DLB handle
 */
int
dlb_get_min_ldb_sequence_number_allocation(
    dlb_hdl_t hdl);


/*********************************************/
/* Scheduling Domain Configuration Functions */
/*********************************************/


/*!
 *  @brief DLB scheduling domain creation arguments
 *
 *  @see dlb_resources_t
 */
typedef struct dlb_create_sched_domain {
    /** Number of load-balanced queues */
    uint32_t num_ldb_queues;
    /** Number of load-balanced ports */
    uint32_t num_ldb_ports;
    /** Number of directed ports */
    uint32_t num_dir_ports;
    /** Number of load-balanced event state entries. Recommend giving each
     * load-balanced port twice its CQ depth of event state entries, so the
     * recommended allocation is 2*SUM(all load-balanced CQ depths).
     */
    uint32_t num_ldb_event_state_entries;
    /** Number of sequence number slots (per SN group) assigned to a domain.
     */
    uint32_t num_sn_slots[2];
    union {
        /** Load-balanced and directed credit configuration. Used by devices
         * without the combined_credits capability.
         */
        struct {
            /** Number of load-balanced credits */
            uint32_t num_ldb_credits;
            /** Number of directed credits */
            uint32_t num_dir_credits;
            /** Number of load-balanced credit pools */
            uint32_t num_ldb_credit_pools;
            /** Number of directed credit pools */
            uint32_t num_dir_credit_pools;
        };
        /** Combined credit configuration. Used by devices with the
         * combined_credits capability.
         */
        struct {
            /** Number of credits */
            uint32_t num_credits;
            /** Number of credit pools */
            uint32_t num_credit_pools;
        };
    };
    /** Mask of cores on which producer threads are running.*/
    uint64_t producer_coremask[2];
} dlb_create_sched_domain_t;

/*!
 * @fn int dlb_create_sched_domain(
 *          dlb_hdl_t hdl,
 *          dlb_create_sched_domain_t *args);
 *
 * @brief   Create a scheduling domain with the resources specified by args. If
 *          successful, the function returns the domain ID.
 *
 *          Only the process that created the domain can reset it, and this
 *          process must remain active for the duration of the scheduling
 *          domain's lifetime.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] args: Pointer to dlb_create_sched_domain_t structure.
 *
 * @retval >=0 Scheduling domain ID
 * @retval -1  Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Insufficient DLB resources to satisfy the request
 * @exception EPERM  Unable to create or size a shared memory file
 * @exception EPERM  Unable to unlink a previously created shared memory file
 * @exception ENOMEM Unable to mmap the shared memory file
 */
int
dlb_create_sched_domain(
    dlb_hdl_t hdl,
    dlb_create_sched_domain_t *args);

/*!
 * @fn dlb_domain_hdl_t dlb_attach_sched_domain(
 *                          dlb_hdl_t hdl,
 *                          int domain_id);
 *
 * @brief   Attach to a previously created scheduling domain.
 *
 * @note    A process can attach to a domain created by another process. A
 *          domain handle cannot be shared among processes, but can be shared
 *          among threads within the caller process.
 *
 * @note    Functions that take a domain handle are MT-safe, unless otherwise
 *          noted. When dlb_detach_sched_domain() is called for a particular
 *          DLB handle, that handle can no longer be used.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] domain_id: Domain ID returned by a successful call to
 *                       dlb_create_sched_domain().
 *
 * @retval > 0  Success, returned a domain handle
 * @retval NULL Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain ID or the scheduling domain is not configured
 * @exception EPERM  Unable to open or map the domain's shared memory file
 */
dlb_domain_hdl_t
dlb_attach_sched_domain(
    dlb_hdl_t hdl,
    int domain_id);

/*!
 * @fn int dlb_detach_sched_domain(
 *          dlb_domain_hdl_t hdl);
 *
 * @brief   Detach a scheduling domain handle. The handle can no longer be used
 *          after it is detached. All port handles from a domain must be
 *          detached before detaching any domain handles.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EEXIST There is at least one remaining attached port handle
 */
int
dlb_detach_sched_domain(
    dlb_domain_hdl_t hdl);

/*!
 * @fn int dlb_create_ldb_credit_pool(
 *          dlb_domain_hdl_t hdl,
 *          int num_credits);
 *
 * @brief   Create a load-balanced credit pool. These credits are used by ports
 *          to enqueue events to load-balanced queues.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] num_credits: Number of credits in the pool.
 *
 * @retval >=0 Pool ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Insufficient load-balanced credits available in the domain
 * @exception EINVAL Function not supported for devices with the
 *                   combined_credits capability
 */
int
dlb_create_ldb_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits);

/*!
 * @fn int dlb_create_dir_credit_pool(
 *          dlb_domain_hdl_t hdl,
 *          int num_credits);
 *
 * @brief   Create a directed credit pool. These credits are used by ports
 *          to enqueue events to directed queues.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] num_credits: Number of credits in the pool.
 *
 * @retval >=0 Pool ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Insufficient directed credits available in the domain
 * @exception EINVAL Function not supported for devices with the
 *                   combined_credits capability
 */
int
dlb_create_dir_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits);

/*!
 * @fn int dlb_create_credit_pool(
 *          dlb_domain_hdl_t hdl,
 *          int num_credits);
 *
 * @brief   Create a credit pool. These credits are used by ports to enqueue
 *          events to queues.
 *
 * @note    This function is only usable by devices with the combined_credits
 *          capability.
 *
 *          @see dlb_get_dev_capabilities, dlb_dev_cap_t
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] num_credits: Number of credits in the pool.
 *
 * @retval >=0 Pool ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Insufficient credits available in the domain
 * @exception EINVAL Function not supported for devices without the
 *                   combined_credits capability
 */
int
dlb_create_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits);

/**
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
 * DLB divides its reorder buffer storage into groups, where each group is
 * configured to contain either 1 queue with 1024 reorder entries, 2 queues
 * with 512 reorder entries, and so on down to 32 queues with 32 entries.
 * For more information, @see dlb_set_ldb_sequence_number_allocation.
 *
 * When a load-balanced queue is created with dlb_create_ldb_queue(), libdlb
 * will configure a new sequence number group on-demand if num_sequence_numbers
 * does not match a pre-existing group with available reorder buffer entries. If
 * all sequence number groups are in use, no new group will be created and
 * queue configuration will fail. (Note that when libdlb is used with a
 * virtual DLB device, it cannot change the sequence number configuration.)
 */
typedef struct {
    /**
     * Number of sequence numbers. Valid configurations are as power-of-two
     * numbers between 32 and 1024, inclusive. If 0, the queue will not
     * supported ordered traffic.
     */
    uint32_t num_sequence_numbers;
    /**
     * Lock ID compression level. A non-zero value specifies the number of
     * unique lock IDs the queue should compress down to, and valid values are
     * 64, 128, 256, 512, 1024, 2048, 4096, and 65536. If lock_id_comp_level is
     * 0, the queue will not compress its lock IDs.
     *
     * This field is ignored if the device doesn't support lock ID compression
     * (indicated by the lock_id_comp capability flag).
     */
    uint32_t lock_id_comp_level;
} dlb_create_ldb_queue_t;

/*!
 * @fn int dlb_create_ldb_queue(
 *          dlb_domain_hdl_t hdl,
 *          dlb_create_ldb_queue_t *args);
 *
 * @brief   Create a load-balanced queue. events sent to this queue are
 *          load-balanced across the ports that have linked the queue.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] args: Pointer to queue configuration arguments.
 *
 * @retval >=0 Queue ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Insufficient available sequence numbers.
 */
int
dlb_create_ldb_queue(
    dlb_domain_hdl_t hdl,
    dlb_create_ldb_queue_t *args);

/*!
 * @fn int dlb_create_dir_queue(
 *          dlb_domain_hdl_t hdl,
 *          int port_id);
 *
 * @brief   Create a directed queue. events sent to this queue are enqueued
 *          directly to a single port.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] port_id: Directed port ID. If the queue's corresponding directed
 *                     port is already created, specify its ID here. Else this
 *                     argument must be -1 to indicate that the queue is being
 *                     created before the port.
 *
 * @retval >=0 Queue ID. If port_id is not -1, the queue ID will be the same as
 *             the port ID.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle or port ID
 */
int
dlb_create_dir_queue(
    dlb_domain_hdl_t hdl,
    int port_id);

/*!
 *  @brief Load-balanced port class-of-service IDs
 */
typedef enum {
    /** Class-of-service 0 */
    DLB_PORT_COS_ID_0,
    /** Class-of-service 1 */
    DLB_PORT_COS_ID_1,
    /** Class-of-service 2 */
    DLB_PORT_COS_ID_2,
    /** Class-of-service 3 */
    DLB_PORT_COS_ID_3,
    /** Any class-of-service */
    DLB_PORT_COS_ID_ANY,
} dlb_port_cos_ids_t;

/**
 * Port creation configuration structure.
 */
typedef struct {
    union {
        /** Load-balanced and directed credit configuration. Used by devices
         * without the combined_credits capability.
         */
        struct {
            /** Load-balanced credit pool ID */
            uint32_t ldb_credit_pool_id;
            /** Directed credit pool ID */
            uint32_t dir_credit_pool_id;
        };
        /** Combined credit configuration. Used by devices with the
         * combined_credits capability.
         */
        struct {
            /** Credit pool ID */
            uint32_t credit_pool_id;
        };
    };
    /** Depth of the port's consumer queue. Must be a power-of-2 number between
     * 8 and 1024, inclusive.
     *
     * For load-balanced CQs, smaller CQ depths (8-16) are recommended for
     * best load-balancing. For directed CQs, larger depths (128+) are
     * recommended.
     */
    uint16_t cq_depth;
    /**
     * Load-balanced event state storage. Must be a power-of-2 number greater
     * than or equal to the CQ depth. Applicable to load-balanced ports only.
     *
     * Recommended setting: 2*cq_depth.
     */
    uint32_t num_ldb_event_state_entries;
    union {
        /**
         * Load-balanced port scheduling class-of-service this port should
         * belong to. Applicable to load-balanced ports only.
         *
         * If in doubt, select DLB_PORT_COS_ID_ANY.
         *
         * This field is ignored if the device lacks the port_cos capability.
         */
        dlb_port_cos_ids_t cos_id;

        /**
         * Hint for DLB that the port is being used only for enqueueing.
         * Applicable only for directed ports.
         */
        bool is_producer;
    };
} dlb_create_port_t;

/*!
 * @fn int dlb_create_ldb_port(
 *          dlb_domain_hdl_t hdl,
 *          dlb_create_port_t *args);
 *
 * @brief   Create a load-balanced port. This port can be used to enqueue events
 *          into load-balanced or directed queues, and to receive events from up
 *          to 8 load-balanced queues.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] args: Pointer to port configuration arguments.
 *
 * @retval >=0 Port ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL NULL args pointer
 * @exception EINVAL Invalid credit pool ID, or
 *                   invalid CQ depth
 */
int
dlb_create_ldb_port(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args);

/*!
 * @fn int dlb_create_dir_port(
 *          dlb_domain_hdl_t hdl,
 *          dlb_create_port_t *args,
 *          int queue_id);
 *
 * @brief   Create a directed port. This port can be used to enqueue events into
 *          load-balanced or directed queues, and to receive events from a
 *          directed queue.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] args: Pointer to port configuration arguments.
 * @param[in] queue_id: Directed queue ID. If the port's corresponding directed
 *                      port is already created, specify its ID here. Else this
 *                      argument must be -1 to indicate that the port is being
 *                      created before the queue.
 *
 * @retval >=0 Port ID
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL NULL args pointer
 * @exception EINVAL Invalid credit pool ID, or
 *                   invalid CQ depth
 */
int
dlb_create_dir_port(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    int queue_id);

/*!
 * @fn dlb_port_hdl_t dlb_attach_ldb_port(
 *          dlb_domain_hdl_t hdl,
 *          int port_id);
 *
 * @brief   Attach to a previously created load-balanced port. The handle
 *          returned is used to send and receive events.
 *
 * @note    A process can attach to a port created by another process. A port
 *          handle cannot be shared among processes, but can be shared among
 *          threads within the caller process.
 *
 * @note    Nearly all functions that take a port handle are *not* MT-safe
 *          (these functions are documented as such). It is invalid to
 *          simultaneously run *any* non-MT-safe datapath functions (e.g.,
 *          dlb_send and dlb_recv()) with the same port handle. Multiple
 *          threads within a single process can share a port handle, so long as
 *          the application provides mutual exclusion around the datapath
 *          functions.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] port_id: load-balanced port ID.
 *
 * @retval > 0  Success, returned a port handle
 * @retval NULL Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Invalid port ID
 */
dlb_port_hdl_t
dlb_attach_ldb_port(
    dlb_domain_hdl_t hdl,
    int port_id);

/*!
 * @fn dlb_port_hdl_t dlb_attach_dir_port(
 *          dlb_domain_hdl_t hdl,
 *          int port_id);
 *
 * @brief   Attach to a previously created directed port. The handle returned
 *          is used to send and receive events.
 *
 * @note    A process can attach to a port created by another process. A port
 *          handle cannot be shared among processes, but can be shared among
 *          threads within the caller process.
 *
 * @note    Nearly all functions that take a port handle are *not* MT-safe
 *          (these functions are documented as such). It is invalid to
 *          simultaneously run *any* non-MT-safe datapath functions (e.g.,
 *          dlb_send and dlb_recv()) with the same port handle. Multiple
 *          threads within a single process can share a port handle, so long as
 *          the application provides mutual exclusion around the datapath
 *          functions.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] port_id: directed port ID.
 *
 * @retval > 0  Success, returned a port handle
 * @retval NULL Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EINVAL Invalid port ID
 */
dlb_port_hdl_t
dlb_attach_dir_port(
    dlb_domain_hdl_t hdl,
    int port_id);

/*!
 * @fn int dlb_detach_port(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Detach a previously attached port handle.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 */
int
dlb_detach_port(
    dlb_port_hdl_t hdl);

/*!
 *  @brief API classes
 */
typedef enum {
    /** Applies to all functions prefixed with dlb_recv. */
    DLB_API_CLASS_RECV,

    DLB_NUM_API_CLASSES
} dlb_api_class_t;

/*!
 *  @brief Wait profile wait types.
 */
typedef enum {
    /** Suspend the thread until the interrupt fires or the port is disabled. */
    DLB_WAIT_INTR,
    /** Put the core in low-power mode until the interrupt fires, a timeout
     * is reached, or the port is disabled. To maximize power savings, the user
     * should configure their system to minimize interrupts (timer, network,
     * etc.) on this core.
     */
    DLB_WAIT_INTR_LOW_POWER,
    /** Continuously poll until an event is available, the timeout is reached,
     * or the port is disabled.
     */
    DLB_WAIT_TIMEOUT_HARD_POLL,
    /** Poll until an event is available, the timeout is reached, or the port
     * is disabled, sleeping for a user-specified duration after every
     * unsuccessful poll.
     */
    DLB_WAIT_TIMEOUT_SLEEP_POLL,

    DLB_NUM_WAIT_TYPES
} dlb_wait_profile_type_t;

/*!
 *  @brief Wait profile.
 */
typedef struct {
    /** Wait profile type */
    dlb_wait_profile_type_t type;
    /** Duration, in nanoseconds, to wait before timing out. Valid only for
     * profile types DLB_WAIT_TIMEOUT_* and DLB_WAIT_INTR_LOW_POWER, ignored
     * otherwise.
     */
    uint64_t timeout_value_ns;
    /** Duration, in nanoseconds, of the sleep. Valid only for profile type
     * DLB_WAIT_TIMEOUT_SLEEP_POLL, ignored otherwise. Note that the OS may
     * sleep the thread for longer than the requested duration.
     */
    uint64_t sleep_duration_ns;
} dlb_wait_profile_t;

/*!
 * @fn int dlb_set_wait_profile(
 *          dlb_port_hdl_t hdl,
 *          dlb_api_class_t api_class,
 *          dlb_wait_profile_t profile)
 *
 * @brief   Set the port handle's behavior when it blocks in a datapath
 *          function.
 *
 * @note    The default wait profiles are:
 *          - DLB_API_CLASS_RECV: DLB_WAIT_INTR
 *
 * @note    This function is not MT-safe with respect to datapath functions.
 *          Unexpected behavior may result if the wait profile is changed
 *          while the port handle is used in a datapath function.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] api_class: the api_class of functions this profile will apply to.
 * @param[in] profile: a structure specifying the manner in which blocking
 *                     functions will behave when they block.
 *
 * @retval 0  Success
 * @retval -1 Failure, and errno is set according to the exceptions below
 * @exception EINVAL Invalid port handle, api_class, or profile
 */
int
dlb_set_wait_profile(
    dlb_port_hdl_t hdl,
    dlb_api_class_t api_class,
    dlb_wait_profile_t profile);

/*!
 * @fn int dlb_get_wait_profile(
 *          dlb_port_hdl_t hdl,
 *          dlb_api_class_t api_class,
 *          dlb_wait_profile_t *profile)
 *
 * @brief   Get the port handle's wait profile.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] api_class: the api_class of functions this profile applies to.
 * @param[in] profile: a pointer to memory where the profile will be written.
 *
 * @retval 0  Success
 * @retval -1 Failure, and errno is set according to the exceptions below
 * @exception EINVAL Invalid port handle, api_class, or profile pointer
 */
int
dlb_get_wait_profile(
    dlb_port_hdl_t hdl,
    dlb_api_class_t api_class,
    dlb_wait_profile_t *profile);

/*!
 * @fn int dlb_enable_cq_weight(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Enable weight-based scheduling for this port. If enabled,
 *          the event weight field allows the event sender to specify
 *          the number of CQ slots (1, 2, 4, or 8) that the event will
 *          occupy.
 *
 *          By default, weight-based scheduling is disabled for newly created
 *          ports.
 *
 * @note    This function cannot be called after the domain is started
 *
 * @note    This function is only usable by devices with the qe_weight
 *          capability.
 *
 *          @see dlb_get_dev_capabilities, dlb_dev_cap_t
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 * @exception EINVAL The domain is already started
 * @exception EINVAL Function not supported for devices without the
 *                   qe_weight capability
 */
int
dlb_enable_cq_weight(
    dlb_port_hdl_t hdl);

/*!
 * @fn int dlb_link_queue(
 *          dlb_port_hdl_t hdl,
 *          int queue_id,
 *          int priority);
 *
 * @brief   Link a load-balanced queue to a load-balanced port. Each
 *          load-balanced port can link up to 8 queues.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port().
 * @param[in] queue_id: Load-balanced queue ID.
 * @param[in] priority: Priority. Must be between 0 and 8, inclusive, where 0
 *                      is the highest priority.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle, queue ID, or priority
 * @exception EINVAL The port is already linked to 8 queues.
 */
int
dlb_link_queue(
    dlb_port_hdl_t hdl,
    int queue_id,
    int priority);

/*!
 * @fn int dlb_unlink_queue(
 *          dlb_port_hdl_t hdl,
 *          int queue_id);
 *
 * @brief   Unlink a load-balanced queue from a load-balanced port.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port().
 * @param[in] queue_id: Load-balanced queue ID.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle or queue ID
 * @exception EINVAL The queue ID was not linked to the port
 */
int
dlb_unlink_queue(
    dlb_port_hdl_t hdl,
    int queue_id);

/*!
 * @fn int dlb_enable_port_sched(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Enable event scheduling to the port (enabled by default).
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 */
int
dlb_enable_port_sched(
    dlb_port_hdl_t hdl);

/*!
 * @fn int dlb_disable_port_sched(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Disable event scheduling to the port. A disabled port can continue
 *          to enqueue events and dequeue any events remaining in the port's
 *          CQ when it was disabled, but the DLB will not schedule any
 *          additional events to the port until it is re-enabled.
 *
 * @note    By default, ports are enabled.
 *
 * @note    If a thread is blocked in a DLB receive function call when the
 *          port is disabled, the thread will be unblocked and return.
 *
 * @note    After disabling a port, it may still have events in its CQ. It is
 *          the application's responsibility to drain the port's CQ after
 *          disabling it.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 */
int
dlb_disable_port_sched(
    dlb_port_hdl_t hdl);

/*!
 * @fn int dlb_enable_port(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Enable the port (enabled by default).
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 */
int
dlb_enable_port(
    dlb_port_hdl_t hdl);

/*!
 * @fn int dlb_disable_port(
 *          dlb_port_hdl_t hdl);
 *
 * @brief   Disable event scheduling to the port and prevent threads from
 *          enqueueing to or dequeueing from the port. A disabled port can
 *          continue to dequeue any events remaining in the port's CQ when it
 *          was disabled, but the DLB will not schedule any additional events
 *          to the port until it is re-enabled. Attempts to dequeue from an
 *          empty disabled port will return an error.
 *
 * @note    By default, ports are enabled.
 *
 * @note    If a thread is blocked in a DLB receive function call when the
 *          port is disabled, the thread will be unblocked and return.
 *
 * @note    This API can be called dynamically, i.e. after calling
 *          dlb_start_sched_domain().
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 */
int
dlb_disable_port(
    dlb_port_hdl_t hdl);

/*!
 * @fn int dlb_start_sched_domain(
 *          dlb_domain_hdl_t hdl);
 *
 * @brief   This function is called to indicated the end of the DLB
 *          configuration phase and beginning of the dataflow phase. Until this
 *          function is called, ports cannot send events, and after it is called
 *          the domain's resources cannot be configured (unless otherwise noted
 *          in the documentation).
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception ESRCH The domain alert thread hasn't launched. The application
 *                  must call dlb_launch_domain_alert_thread() before starting
 *                  the domain
 *
 * @see dlb_launch_domain_alert_thread()
 */
int
dlb_start_sched_domain(
    dlb_domain_hdl_t hdl);

/*!
 * @fn int dlb_reset_sched_domain(
 *          dlb_hdl_t hdl,
 *          int domain_id);
 *
 * @brief   This function resets a configured scheduling domain, effectively
 *          undoing the effects of dlb_create_sched_domain(). The domain's
 *          resources (ports, queues, pools, etc.) are relinquished to the DLB
 *          driver, making them available to be re-allocated and reconfigured.
 *
 *          Prior to calling this function, all port and domain handles must be
 *          detached. This function wakes any threads blocked reading the
 *          domain's device file, causing them to return errno ENODEV.
 *
 *          Only the process that created the domain can reset it.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] domain_id: Domain ID returned by a successful call to
 *                       dlb_create_sched_domain().
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain ID
 * @exception EPERM The calling process did not create the domain
 * @exception EEXIST There is at least one remaining attached domain or port
 *                   handle
 */
int
dlb_reset_sched_domain(
    dlb_hdl_t hdl,
    int domain_id);


/****************************/
/* Scheduling Domain Alerts */
/****************************/


/*!
 *  @brief DLB alert IDs
 */
typedef enum {
    /** The device containing this domain is being reset. When this occurs,
     * libdlb disables all ports, blocking the application from enqueueing or
     * dequeueing (indicated by error code EACCES). The application needs to
     * detach all its handles and call dlb_close(), or simply exit and restart.
     *
     * The alert data doesn't contain any information for this alert.
     */
    DLB_ALERT_DEVICE_RESET,
    /** The domain is being reset, triggered by a call to
     * dlb_reset_sched_domain(). The application doesn't need to take any
     * action in response to this alert. The alert data doesn't contain any
     * information for this alert.
     */
    DLB_ALERT_DOMAIN_RESET,
    /* The watchdog timer fired for the specified port. This occurs if its
     * CQ was not serviced for a large amount of time, likely indicating a hung
     * thread. The watchdog timer must be enabled via the kernel driver.
     * Alert data[7:0] contains the port ID, and data[15:8] contains a flag
     * indicating whether the port is load-balanced (1) or directed (0).
     */
    DLB_ALERT_CQ_WATCHDOG_TIMEOUT,
} dlb_alert_id_t;

/*!
 *  @brief DLB alert information
 */
typedef struct {
    /** Alert ID */
    dlb_alert_id_t id;
    /** Alert data */
    uint64_t data;
} dlb_alert_t;

/*!
 *  @brief DLB alert callback function type
 */
typedef void (*domain_alert_callback_t)(dlb_alert_t *alert,
                                        int domain_id,
                                        void *arg);

/*!
 * @fn int dlb_launch_domain_alert_thread(
 *          dlb_domain_hdl_t hdl,
 *          domain_alert_callback_t callback,
 *          void *callback_arg);
 *
 * @brief   This function launches a background thread that blocks waiting
 *          for scheduling domain alerts. The thread calls *callback* (if
 *          non-NULL) for every alert it receives.
 *
 *          Each libdlb application must launch a domain alert thread before
 *          starting the domain, and only one alert thread per domain is
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
 *          Only the process that created the domain may launch the alert
 *          thread.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[out] callback: Callback function pointer. Can be NULL.
 * @param[out] callback_arg: Callback function argument. Can be NULL.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid domain handle
 * @exception EPERM The calling process did not create the domain
 * @exception EEXIST The alert thread is already launched
 *
 * @see dlb_alert_id_t
 */
int
dlb_launch_domain_alert_thread(
    dlb_domain_hdl_t hdl,
    domain_alert_callback_t callback,
    void *callback_arg);


/****************************************/
/* Scheduling Domain Datapath Functions */
/****************************************/


/*!
 * @fn int dlb_send(
 *          dlb_port_hdl_t hdl,
 *          uint32_t num,
 *          dlb_event_t *events);
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
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] num: Number of events pointed to by events.
 * @param[in] events: Pointer to an array of dlb_event_t structures.
 *
 * @retval >=0 The return value indicates the number of enqueued events. Events
 *             are enqueued in array order.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL events pointer is NULL
 * @exception EINVAL Invalid port handle, or invalid queue ID in events
 * @exception EPERM  The scheduling domain isn't started
 * @exception EACCES The port is disabled
 */
int
dlb_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *events);

/*!
 * @fn int dlb_release(
 *          dlb_port_hdl_t hdl,
 *          uint32_t num);
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
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] num: Number of releases to issue.
 *
 * @retval >=0 The return value indicates the number of released events.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 * @exception EPERM  The scheduling domain isn't started
 * @exception EACCES The port is disabled
 */
int
dlb_release(
    dlb_port_hdl_t hdl,
    uint32_t num);

/*!
 * @fn int dlb_forward(
 *          dlb_port_hdl_t hdl,
 *          uint32_t num,
 *          dlb_event_t *events);
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
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] num: Number of events pointed to by events.
 * @param[in] events: Pointer to an array of dlb_event_t structures.
 *
 * @retval >=0 The return value indicates the number of forwarded events. Events
 *             are enqueued in array order.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL events pointer is NULL
 * @exception EINVAL Invalid port handle
 * @exception EPERM  The scheduling domain isn't started
 * @exception EACCES The port is disabled
 */
int
dlb_forward(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *events);

/*!
 * @fn int dlb_pop_cq(
 *          dlb_port_hdl_t hdl,
 *          uint32_t num);
 *
 * @brief   Pop the CQ one or more times. Every event received for a port
 *          occupies a location in its CQ, and the pop clears space for more
 *          events to be scheduled to the port. Each pop is applied to the
 *          oldest event in the CQ.
 *
 * @note    dlb_recv() automatically pops the CQ for every received event,
 *          but dlb_recv_no_pop() does not. dlb_pop_cq() should only be used in
 *          conjunction with dlb_recv_no_pop().
 *
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] num: Number of CQ pop operations to perform.
 *
 * @retval >=0 The return value indicates the number of successful pops.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle
 * @exception EACCES The port is disabled
 */
int
dlb_pop_cq(
    dlb_port_hdl_t hdl,
    uint32_t num);


/*!
 * @fn int dlb_recv(
 *          dlb_port_hdl_t hdl,
 *          uint32_t max,
 *          bool wait,
 *          dlb_event_t *events);
 *
 * @brief   Receive one or more events, and pop the CQ for each event received.
 *          The wait argument determines whether the function waits for at
 *          least one event or not. Received events must be processed in order.
 *
 * @note    All events received on a load-balanced port must be released with
 *          dlb_release() or dlb_forward().
 *          The advanced send function can release the oldest event with the
 *          REL/REL_T commands, or can simultaneously release the oldest
 *          event and send a event with the FWD/FWD_T commands.
 *
 * @note    When wait is true and no events are available to receive, this
 *          handle's wait profile determines the manner in which the thread
 *          blocks.
 *
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] max: Maximum number of events to receive.
 * @param[in] wait: If false, the function receives as many events as it can,
 *                  up to max or until the CQ is empty. If true, the function
 *                  waits until the CQ contains at least one event, then
 *                  receives as many events as it can, up to max or until the
 *                  CQ is empty.
 * @param[out] events: Pointer to an array of dlb_event_t structures.
 *
 * @retval >=0 The return value indicates the number of dequeued events.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle or NULL events pointer
 * @exception EPERM  The scheduling domain isn't started
 * @exception EACCES The port is disabled and there are no events in its CQ
 * @exception EINTR  The thread blocked on a CQ interrupt and returned early
 *                   because the DLB was unexpectedly reset
 */
int
dlb_recv(
    dlb_port_hdl_t hdl,
    uint32_t max,
    bool wait,
    dlb_event_t *events);

/*!
 * @fn int dlb_recv_no_pop(
 *          dlb_port_hdl_t hdl,
 *          uint32_t max,
 *          bool wait,
 *          dlb_event_t *events);
 *
 * @brief   Receive one or more events. The wait argument determines whether
 *          the function waits for at least one event or not. Received events
 *          must be processed in order.
 *
 * @brief   Receive one or more events. The wait argument determines whether the
 *          function waits for at least one event or not. Received events must
 *          be processed in order.
 *
 * @note    The application may hold onto owed tokens to cause the CQ to have
 *          an effective depth less than its configured depth. However, if
 *          the port's owed CQ tokens > (CQ depth - 4), the DLB will NOT
 *          schedule any more events to the CQ. If this state is reached, the
 *          application must release tokens until owed tokens <= (CQ depth - 4).
 *
 * @note    All received events occupy space in the CQ that needs to be cleared
 *          to allow more events to be scheduled to the port. dlb_recv_no_pop()
 *          does not automatically pop the event; the user must call
 *          dlb_pop_cq() to do so.  Alternatively, the advanced send function
 *          has ways to pop the CQ:
 *          - By sending a batch token return command (BAT_T)
 *          - By piggy-backing a single token return onto another enqueue
 *            command (NEW_T, FWD_T, COMP_T, or FRAG_T)
 *
 * @note    All events received on a load-balanced port must be released with
 *          dlb_release() or dlb_forward().
 *          The advanced send function can release the oldest event with the
 *          REL/REL_T commands, or can simultaneously release the oldest
 *          event and send a event with the FWD/FWD_T commands.
 *
 * @note    When wait is true and no events are available to receive, this
 *          handle's wait profile determines the manner in which the thread
 *          blocks.
 *
 * @note    Datapath functions (dlb_send, dlb_recv, etc.) are not MT-safe on the
 *          same port. Datapath functions can be run simultaneously on multiple
 *          threads if each thread is using a unique port.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] max: Maximum number of events to receive.
 * @param[in] wait: If false, the function receives as many events as it can,
 *                  up to max or until the CQ is empty. If true, the function
 *                  waits until the CQ contains at least one event, then
 *                  receives as many events as it can, up to max or until the
 *                  CQ is empty.
 * @param[out] events: Pointer to an array of dlb_event_t structures.
 *
 * @retval >=0 The return value indicates the number of dequeued events.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Invalid port handle or NULL events pointer
 * @exception EPERM  The domain isn't started
 * @exception EACCES The port is disabled and there are no events in its CQ
 */
int
dlb_recv_no_pop(
    dlb_port_hdl_t hdl,
    uint32_t max,
    bool wait,
    dlb_event_t *events);


/*!
 * @fn int dlb_enable_cq_epoll(
 *          dlb_port_hdl_t hdl,
 *          bool is_ldb,
 *          int efd);
 *
 * @brief  Enable epoll support to monitor event file descriptors created for
 *         directed and load-balanced port's CQs. Kernel notifies user-space
 *         of events through the eventfds.
 *
 *         Application must create eventfds per CQ with EFD_NONBLOCK flag.
 *
 * @param[in] hdl: Port handle.
 * @param[in] is_ldb: True for load-balanced port and false for directed port.
 * @param[in] efd: Event file descriptor for the CQ.
 *
 * @retval 0 Success
 * @retval -1 Failure, and errno is set according to the exception listed below
 * @exception EINVAL Invalid port handle
 */
int dlb_enable_cq_epoll(
          dlb_port_hdl_t hdl,
	  bool is_ldb,
          int efd);

/*!
 * @fn int dlb_get_xstats(
 *	dlb_hdl_t hdl,
 *	uint32_t type,
 *	uint32_t id,
 *	uint64_t *val);
 *
 * @brief  Get xstats from driver
 *
 * @param[in] hdl: Port handle.
 * @param[in] type: xstats type
 * @param[in] id: xstats id
 * @param[out] val: xstats val
 *
 * @retval 0 Success
 * @retval -1 Failure
 * @exception EINVAL Invalid port handle
 */
int
dlb_get_xstats(
	dlb_hdl_t hdl,
	uint32_t type,
	uint32_t id,
	uint64_t *val);
#ifdef __cplusplus
}
#endif

#endif /* __DLB_H__ */
