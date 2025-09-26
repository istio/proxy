/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "dlb2_ioctl.h"

#define DLB2_LOG_IOCTL_ERROR(_ret, _status)          \
do {                                                 \
    if (_ret && _status) {                           \
        printf("[%s()] Error: %s\n", __func__,       \
               dlb2_error_strings[_status]);         \
    } else if (_ret) {                               \
        printf("%s: ioctl failed with errno = %d\n", \
               __func__, errno);                     \
    }                                                \
} while (0)

void dlb2_ioctl_get_device_version(int fd, uint8_t *ver, uint8_t *rev)
{
    struct dlb2_get_device_version_args ioctl_args = {0};

    int ret = ioctl(fd, DLB2_IOC_GET_DEVICE_VERSION, (unsigned long)&ioctl_args);

    *ver = DLB2_DEVICE_VERSION(ioctl_args.response.id);
    *rev = DLB2_DEVICE_REVISION(ioctl_args.response.id);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);
}

int dlb2_ioctl_create_sched_domain(
    int fd,
    dlb_create_sched_domain_t *args,
    dlb_adv_create_sched_domain_t *adv_args,
    int *domain_fd,
    bool is_2_5)
{
    struct dlb2_create_sched_domain_args ioctl_args = {0};
    int ret;

    ioctl_args.num_ldb_queues = args->num_ldb_queues;
    ioctl_args.num_ldb_ports = args->num_ldb_ports;
    ioctl_args.num_cos_ldb_ports[0] = adv_args->num_cos_ldb_ports[0];
    ioctl_args.num_cos_ldb_ports[1] = adv_args->num_cos_ldb_ports[1];
    ioctl_args.num_cos_ldb_ports[2] = adv_args->num_cos_ldb_ports[2];
    ioctl_args.num_cos_ldb_ports[3] = adv_args->num_cos_ldb_ports[3];
    ioctl_args.cos_strict = 1;
    ioctl_args.num_dir_ports = args->num_dir_ports;
    ioctl_args.num_atomic_inflights =
        args->num_ldb_queues * NUM_V2_ATM_INFLIGHTS_PER_LDB_QUEUE;
    ioctl_args.num_hist_list_entries = args->num_ldb_event_state_entries;
    if (!is_2_5) {
        ioctl_args.num_ldb_credits = args->num_ldb_credits;
        ioctl_args.num_dir_credits = args->num_dir_credits;
    } else {
        ioctl_args.num_ldb_credits = args->num_credits;
    }

    ioctl_args.num_sn_slots[0] = args->num_sn_slots[0];
    ioctl_args.num_sn_slots[1] = args->num_sn_slots[1];

    ioctl_args.pcore_mask[0] = args->producer_coremask[0];
    ioctl_args.pcore_mask[1] = args->producer_coremask[1];

    ret = ioctl(fd, DLB2_IOC_CREATE_SCHED_DOMAIN, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    if (ret == 0)
        *domain_fd = ioctl_args.domain_fd;

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_get_num_resources(
    int fd,
    dlb_resources_t *rsrcs,
    bool is_2_5)
{
    struct dlb2_get_num_resources_args ioctl_args = {0};
    int ret;

    ret = ioctl(fd, DLB2_IOC_GET_NUM_RESOURCES, (unsigned long) &ioctl_args);

    rsrcs->num_sched_domains = ioctl_args.num_sched_domains;
    rsrcs->num_ldb_queues = ioctl_args.num_ldb_queues;
    rsrcs->num_ldb_ports = ioctl_args.num_ldb_ports;
    rsrcs->num_ldb_ports_per_cos[0] = ioctl_args.num_cos_ldb_ports[0];
    rsrcs->num_ldb_ports_per_cos[1] = ioctl_args.num_cos_ldb_ports[1];
    rsrcs->num_ldb_ports_per_cos[2] = ioctl_args.num_cos_ldb_ports[2];
    rsrcs->num_ldb_ports_per_cos[3] = ioctl_args.num_cos_ldb_ports[3];

    rsrcs->num_sn_slots[0] = ioctl_args.num_sn_slots[0];
    rsrcs->num_sn_slots[1] = ioctl_args.num_sn_slots[1];

    rsrcs->num_dir_ports = ioctl_args.num_dir_ports;
    rsrcs->num_ldb_event_state_entries =
        ioctl_args.num_hist_list_entries;
    rsrcs->max_contiguous_ldb_event_state_entries =
        ioctl_args.num_hist_list_entries;
    if (is_2_5) {
        rsrcs->num_credits = ioctl_args.num_ldb_credits;
        rsrcs->num_credit_pools = MAX_NUM_LDB_CREDIT_POOLS;
    } else {
        rsrcs->num_ldb_credits = ioctl_args.num_ldb_credits;
        rsrcs->max_contiguous_ldb_credits = ioctl_args.num_ldb_credits;
        rsrcs->num_dir_credits = ioctl_args.num_dir_credits;
        rsrcs->max_contiguous_dir_credits = ioctl_args.num_dir_credits;
        rsrcs->num_ldb_credit_pools = MAX_NUM_LDB_CREDIT_POOLS;
        rsrcs->num_dir_credit_pools = MAX_NUM_DIR_CREDIT_POOLS;
    }

    return ret;
}

int dlb2_ioctl_create_ldb_queue(
    int fd,
    dlb_create_ldb_queue_t *args,
    int depth_threshold)
{
    struct dlb2_create_ldb_queue_args ioctl_args = {0};
    int ret;

    ioctl_args.num_sequence_numbers = args->num_sequence_numbers;
    ioctl_args.num_atomic_inflights = NUM_V2_ATM_INFLIGHTS_PER_LDB_QUEUE;
    ioctl_args.lock_id_comp_level = args->lock_id_comp_level;
    ioctl_args.depth_threshold = depth_threshold;
    if (args->num_sequence_numbers > 0)
        ioctl_args.num_qid_inflights = args->num_sequence_numbers;
    else
        /* Give each queue half of the QID inflights. Intent is to support high
         * fan-out queues without allowing one or two queues to use all the
         * inflights.
         */
        ioctl_args.num_qid_inflights = NUM_QID_INFLIGHTS / 4;

    ret = ioctl(fd, DLB2_IOC_CREATE_LDB_QUEUE, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_create_dir_queue(
    int fd,
    int port_id,
    int depth_threshold)
{
    struct dlb2_create_dir_queue_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.depth_threshold = depth_threshold;

    ret = ioctl(fd, DLB2_IOC_CREATE_DIR_QUEUE, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_create_ldb_port(
    int fd,
    dlb_create_port_t *args,
    uint16_t rsvd_tokens)
{
    struct dlb2_create_ldb_port_args ioctl_args = {0};
    int ret;

    ioctl_args.cq_depth = args->cq_depth;
    ioctl_args.cq_depth_threshold = rsvd_tokens;
    ioctl_args.cq_history_list_size = args->num_ldb_event_state_entries;
    ioctl_args.cos_id = args->cos_id;
    ioctl_args.cos_strict = 1;

    if (args->cos_id == DLB_PORT_COS_ID_ANY) {
        ioctl_args.cos_id = 0;
        ioctl_args.cos_strict = 0;
    }

    ret = ioctl(fd, DLB2_IOC_CREATE_LDB_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_create_dir_port(
    int fd,
    dlb_create_port_t *args,
    int queue_id,
    uint16_t rsvd_tokens)
{
    struct dlb2_create_dir_port_args ioctl_args = {0};
    int ret;

    ioctl_args.cq_depth = args->cq_depth;
    ioctl_args.cq_depth_threshold = rsvd_tokens;

    ioctl_args.queue_id = queue_id;

    ret = ioctl(fd, DLB2_IOC_CREATE_DIR_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_start_domain(int fd)
{
    struct dlb2_start_domain_args ioctl_args = {0};
    int ret;

    ret = ioctl(fd, DLB2_IOC_START_DOMAIN, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_link_qid(
    int fd,
    int port_id,
    int queue_id,
    int priority)
{
    struct dlb2_map_qid_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.qid = queue_id;
    ioctl_args.priority = priority;

    ret = ioctl(fd, DLB2_IOC_MAP_QID, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_unlink_qid(
    int fd,
    int port_id,
    int queue_id)
{
    struct dlb2_unmap_qid_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.qid = queue_id;

    ret = ioctl(fd, DLB2_IOC_UNMAP_QID, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_enable_ldb_port(int fd, int port_id)
{
    struct dlb2_enable_ldb_port_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;

    ret = ioctl(fd, DLB2_IOC_ENABLE_LDB_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_enable_dir_port(int fd, int port_id)
{
    struct dlb2_enable_dir_port_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;

    ret = ioctl(fd, DLB2_IOC_ENABLE_DIR_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_disable_ldb_port(int fd, int port_id)
{
    struct dlb2_disable_ldb_port_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;

    ret = ioctl(fd, DLB2_IOC_DISABLE_LDB_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_disable_dir_port(int fd, int port_id)
{
    struct dlb2_disable_dir_port_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;

    ret = ioctl(fd, DLB2_IOC_DISABLE_DIR_PORT, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_block_on_cq_interrupt(int fd,
                                     int port_id,
                                     bool is_ldb,
                                     volatile dlb_dequeue_qe_t *cq_va,
                                     uint8_t cq_gen,
                                     bool arm)
{
    struct dlb2_block_on_cq_interrupt_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.is_ldb = is_ldb;
    ioctl_args.cq_va = (uintptr_t) cq_va;
    ioctl_args.cq_gen = cq_gen;
    ioctl_args.arm = arm;

    ret = ioctl(fd, DLB2_IOC_BLOCK_ON_CQ_INTERRUPT, (unsigned long) &ioctl_args);

    /* Don't print an error if the port was disabled */
    if (ret && errno != EACCES)
        DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    if (ret && errno == EPERM)
        printf("[%s()] Error: no interrupt available for %s port %d\n",
               __func__, is_ldb ? "ldb":"dir", port_id);

    return ret;
}

int dlb2_ioctl_enqueue_domain_alert(int fd, uint64_t aux_alert_data)
{
    struct dlb2_enqueue_domain_alert_args ioctl_args = {0};
    int ret;

    ioctl_args.aux_alert_data = aux_alert_data;

    ret = ioctl(fd, DLB2_IOC_ENQUEUE_DOMAIN_ALERT, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);

    return ret;
}

int dlb2_ioctl_set_sn_allocation(
    int fd,
    unsigned int group,
    unsigned int num)
{
    struct dlb2_set_sn_allocation_args ioctl_args = {0};
    int ret;

    ioctl_args.group = group;
    ioctl_args.num = num;

    ret = ioctl(fd, DLB2_IOC_SET_SN_ALLOCATION, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);

    return ret;
}

int dlb2_ioctl_get_sn_allocation(
    int fd,
    unsigned int group,
    unsigned int *num)
{
    struct dlb2_get_sn_allocation_args ioctl_args = {0};
    int ret;

    ioctl_args.group = group;

    ret = ioctl(fd, DLB2_IOC_GET_SN_ALLOCATION, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);
    else
        *num = ioctl_args.response.id;

    return ret;
}

int dlb2_ioctl_get_sn_occupancy(
    int fd,
    unsigned int group,
    unsigned int *num)
{
    struct dlb2_get_sn_occupancy_args ioctl_args = {0};
    int ret;

    ioctl_args.group = group;

    ret = ioctl(fd, DLB2_IOC_GET_SN_OCCUPANCY, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);
    else
        *num = ioctl_args.response.id;

    return ret;
}

int dlb2_ioctl_query_cq_poll_mode(
    int fd,
    enum dlb2_cq_poll_modes *mode)
{
    struct dlb2_query_cq_poll_mode_args ioctl_args = {0};
    int ret;

    ret = ioctl(fd, DLB2_IOC_QUERY_CQ_POLL_MODE, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);

    *mode = ioctl_args.response.id;

    return ret;
}

static int dlb2_ioctl_get_port_fd(
    int fd,
    int port_id,
    uint32_t ioc)
{
    struct dlb2_get_port_fd_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;

    ret = ioctl(fd, ioc, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_ioctl_get_ldb_port_pp_fd(
    int fd,
    int port_id)
{
    return dlb2_ioctl_get_port_fd(fd, port_id, DLB2_IOC_GET_LDB_PORT_PP_FD);
}

int dlb2_ioctl_get_ldb_port_cq_fd(
    int fd,
    int port_id)
{
    return dlb2_ioctl_get_port_fd(fd, port_id, DLB2_IOC_GET_LDB_PORT_CQ_FD);
}

int dlb2_ioctl_get_dir_port_pp_fd(
    int fd,
    int port_id)
{
    return dlb2_ioctl_get_port_fd(fd, port_id, DLB2_IOC_GET_DIR_PORT_PP_FD);
}

int dlb2_ioctl_get_dir_port_cq_fd(
    int fd,
    int port_id)
{
    return dlb2_ioctl_get_port_fd(fd, port_id, DLB2_IOC_GET_DIR_PORT_CQ_FD);
}

int dlb2_ioctl_enable_cq_weight(
    int fd,
    int port_id,
    int limit)
{
    struct dlb2_enable_cq_weight_args ioctl_args;
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.limit = limit;

    ret = ioctl(fd, DLB2_IOC_ENABLE_CQ_WEIGHT, (unsigned long) &ioctl_args);
    if (ret && ioctl_args.response.status)
        printf("[%s()] Error: %s\n",
               __func__, dlb2_error_strings[ioctl_args.response.status]);

    return ret;
}

int dlb2_ioctl_enable_cq_epoll(
    int fd,
    int port_id,
    bool is_ldb,
    int process_id,
    int event_fd)
{
    struct dlb2_enable_cq_epoll_args ioctl_args = {0};
    int ret;

    ioctl_args.port_id = port_id;
    ioctl_args.is_ldb = (is_ldb) ? 1 : 0;
    ioctl_args.process_id = process_id;
    ioctl_args.event_fd = event_fd;

    ret = ioctl(fd, DLB2_IOC_ENABLE_CQ_EPOLL, (unsigned long) &ioctl_args);

    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    return ret;
}

int dlb2_ioctl_get_xtats(
    int fd,
    uint32_t xstats_type,
    uint32_t xstats_id,
    uint64_t *xstats_val)
{
    struct dlb2_xstats_args ioctl_args = {0};
    int ret;

    ioctl_args.xstats_type = xstats_type;
    ioctl_args.xstats_id = xstats_id;
    ret = ioctl(fd, DLB2_IOC_GET_XSTATS, (unsigned long) &ioctl_args);
    DLB2_LOG_IOCTL_ERROR(ret, ioctl_args.response.status);

    *xstats_val = ioctl_args.xstats_val;

    return ret;
}
