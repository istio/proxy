/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

/*! @file      dlb_adv.h
 *
 *  @brief     DLB Client API (Advanced Functions)
 *
 *  @details   This header file defines advanced datapath functions. These
 *             are more difficult to use, but their flexibility can lead to
 *             better performance compared to the standard datapath functions.
 *
 *             Use these at your own risk.
 */

#ifndef __DLB_ADV_H__
#define __DLB_ADV_H__

#ifdef __cplusplus
extern "C" {
#endif


/*******************************/
/* Advanced Datapath Functions */
/*******************************/


/*!
 * @fn int dlb_adv_send(
 *          dlb_port_hdl_t hdl,
 *          uint32_t num,
 *          dlb_event_t *events);
 *
 * @brief   Send one or more events. If the port has insufficient credits to
 *          send all num events, it will return early. Typically insufficient
 *          credits is a transient condition and the send should be retried;
 *          however, certain pipeline architectures and credit pool
 *          configurations can lead to deadlock. Consequently, it is strongly
 *          recommended that the application have a finite retry count and if
 *          necessary release the events and continue processing the port's
 *          consumer queue. (Credits aren't consumed when releasing events.)
 *
 *          TODO: Add more detail about the deadlock scenarios.
 *
 * @note    This function is not MT-safe.
 *
 * @param[in] hdl: Port handle returned by a successful call to
 *                 dlb_attach_ldb_port() or dlb_attach_dir_port().
 * @param[in] num: Number of events pointed to by events.
 * @param[in] events: Pointer to an array of dlb_send_t structures.
 *
 * @retval >=0 The return value indicates the number of enqueued events. events
 *             are enqueued in array order.
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL events pointer is NULL
 * @exception EINVAL Invalid port handle, excess releases, or excess token pops
 * @exception EPERM  The scheduling domain isn't started
 */
int
dlb_adv_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *events);

/*! Forward declaration */
struct dlb_create_sched_domain;

/*!
 *  @brief Advanced DLB scheduling domain creation arguments
 *
 *  @see dlb_resources_t
 */
typedef struct {
    /** Number of load-balanced ports from the four classes-of-service. These
     * are allocated in addition to those requested with struct
     * dlb_create_sched_domain's num_ldb_ports field, which can come from any
     * class-of-service.
     *
     * This field is ignored if the device doesn't support load-balanced port
     * classes of service (indicated by the port_cos capability flag).
     */
    uint32_t num_cos_ldb_ports[4];
} dlb_adv_create_sched_domain_t;

/*!
 * @fn int dlb_adv_create_sched_domain(
 *          dlb_hdl_t hdl,
 *          struct dlb_create_sched_domain *args,
 *          dlb_adv_create_sched_domain_t *adv_args);
 *
 * @brief   Create a scheduling domain with the resources specified by args and
 *          adv_args. If successful, the function returns the domain ID.
 *
 *          Only the process that created the domain can reset it, and this
 *          process must remain active for the duration of the scheduling
 *          domain's lifetime.
 *
 * @param[in] hdl: Handle returned by a successful call to dlb_open().
 * @param[in] args: Pointer to struct dlb_create_sched_domain structure.
 * @param[in] adv_args: Pointer to dlb_adv_create_sched_domain_t structure.
 *
 * @retval >=0 Scheduling domain ID
 * @retval -1  Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Insufficient DLB resources to satisfy the request
 * @exception EPERM  Unable to create or size a shared memory file
 * @exception EPERM  Unable to unlink a previously created shared memory file
 * @exception ENOMEM Unable to mmap the shared memory file
 *
 * @see dlb_create_sched_domain()
 */
int
dlb_adv_create_sched_domain(
    dlb_hdl_t hdl,
    struct dlb_create_sched_domain *args,
    dlb_adv_create_sched_domain_t *adv_args);

/*!
 *  @brief DLB queue depth levels. Each level is defined relative to the
 *         scheduling domain's credits. For example for a load-balanced queue,
 *         the levels are relative to num_ldb_credits field passed to
 *         dlb_create_sched_domain().
 */
typedef enum {
    /** Level 0: queue depth <= 1/3 * domain credits */
    DLB_QUEUE_DEPTH_LEVEL_0,
    /** Level 1: 0.33 * domain credits < queue depth <= 0.5 * domain credits */
    DLB_QUEUE_DEPTH_LEVEL_1,
    /** Level 2: 0.5 * domain credits < queue depth <= 0.66 * domain credits */
    DLB_QUEUE_DEPTH_LEVEL_2,
    /** Level 3: queue depth > 0.66 * domain credits */
    DLB_QUEUE_DEPTH_LEVEL_3,

    NUM_DLB_QUEUE_DEPTH_LEVELS
} dlb_queue_depth_levels_t;

/*!
 * @fn int64_t dlb_adv_read_queue_depth_counter(
 *          dlb_domain_hdl_t hdl,
 *          int queue_id,
 *          bool is_dir,
 *          dlb_queue_depth_levels_t level);
 *
 * @brief   Read the queue depth level counter. The count is the number of
 *          events received when the queue depth was at the specified level.
 *
 * @note    Only supported on devices with the queue depth threshold capability
 *          (indicated by the queue_dt capability flag).
 *
 * @note    This interface is deprecated and will be removed in the future.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] queue_id: Queue ID.
 * @param[in] is_dir: True if a directed queue, false if load-balanced.
 * @param[in] level: Queue depth level.
 *
 * @retval >=0 queue depth level counter
 * @retval -1  Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Device lacks queue_dt capability
 * @exception EINVAL Invalid queue ID or level
 */
int64_t
dlb_adv_read_queue_depth_counter(
    dlb_domain_hdl_t hdl,
    int queue_id,
    bool is_dir,
    dlb_queue_depth_levels_t level);

/*!
 * @fn int dlb_adv_reset_queue_depth_counter(
 *          dlb_domain_hdl_t hdl,
 *          int queue_id,
 *          bool is_dir,
 *          dlb_queue_depth_levels_t level);
 *
 * @brief   Reset the queue depth level counter.
 *
 * @note    Only supported on devices with the queue depth threshold capability
 *          (indicated by the queue_dt capability flag).
 *
 * @note    This interface is deprecated and will be removed in the future.
 *
 * @param[in] hdl: domain handle returned by a successful call to
 *                 dlb_attach_sched_domain().
 * @param[in] queue_id: Queue ID.
 * @param[in] is_dir: true if a directed queue, false if load-balanced.
 * @param[in] level: Queue depth level.
 *
 * @retval  0 Success
 * @retval -1 Failure, and errno is set according to the exceptions listed below
 * @exception EINVAL Device lacks queue_dt capability
 * @exception EINVAL Invalid queue ID or level
 */
int
dlb_adv_reset_queue_depth_counter(
    dlb_domain_hdl_t hdl,
    int queue_id,
    bool is_dir,
    dlb_queue_depth_levels_t level);

#ifdef __cplusplus
}
#endif

#endif /* __DLB_H__ */
