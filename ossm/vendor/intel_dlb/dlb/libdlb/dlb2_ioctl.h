/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef __DLB2_IOCTL_H__
#define __DLB2_IOCTL_H__

#include "dlb2_user.h"
#include "dlb.h"
#include "dlb_priv.h"

void dlb2_ioctl_get_device_version(
    int fd,
    uint8_t *ver,
    uint8_t *rev);

int dlb2_ioctl_create_sched_domain(
    int fd,
    dlb_create_sched_domain_t *args,
    dlb_adv_create_sched_domain_t *adv_args,
    int *domain_fd,
    bool is_2_5);

int dlb2_ioctl_get_num_resources(
    int fd,
    dlb_resources_t *rsrcs,
    bool is_2_5);

int dlb2_ioctl_create_ldb_credit_pool(
    int fd,
    int num_credits);

int dlb2_ioctl_create_dir_credit_pool(
    int fd,
    int num_credits);

int dlb2_ioctl_create_ldb_queue(
    int fd,
    dlb_create_ldb_queue_t *args,
    int depth_threshold);

int dlb2_ioctl_create_dir_queue(
    int fd,
    int port_id,
    int depth_threshold);

int dlb2_ioctl_create_ldb_port(
    int fd,
    dlb_create_port_t *args,
    uint16_t rsvd_tokens);

int dlb2_ioctl_create_dir_port(
    int fd,
    dlb_create_port_t *args,
    int queue_id,
    uint16_t rsvd_tokens);

int dlb2_ioctl_start_domain(
    int fd);

int dlb2_ioctl_link_qid(
    int fd,
    int port_id,
    int queue_id,
    int priority);

int dlb2_ioctl_unlink_qid(
    int fd,
    int port_id,
    int queue_id);

int dlb2_ioctl_enable_ldb_port(int fd, int port_id);
int dlb2_ioctl_enable_dir_port(int fd, int port_id);
int dlb2_ioctl_disable_ldb_port(int fd, int port_id);
int dlb2_ioctl_disable_dir_port(int fd, int port_id);

int dlb2_ioctl_block_on_cq_interrupt(int fd,
                                     int port_id,
                                     bool is_ldb,
                                     volatile dlb_dequeue_qe_t *cq_va,
                                     uint8_t cq_gen,
                                     bool arm);

int dlb2_ioctl_enqueue_domain_alert(int fd, uint64_t aux_alert_data);

int dlb2_ioctl_set_sn_allocation(
    int fd,
    unsigned int group,
    unsigned int num);

int dlb2_ioctl_get_sn_allocation(
    int fd,
    unsigned int group,
    unsigned int *num);

int dlb2_ioctl_get_sn_occupancy(
    int fd,
    unsigned int group,
    unsigned int *num);

int dlb2_ioctl_query_cq_poll_mode(
    int fd,
    enum dlb2_cq_poll_modes *mode);

int dlb2_ioctl_get_ldb_port_pp_fd(
    int fd,
    int port_id);

int dlb2_ioctl_get_ldb_port_cq_fd(
    int fd,
    int port_id);

int dlb2_ioctl_get_dir_port_pp_fd(
    int fd,
    int port_id);

int dlb2_ioctl_get_dir_port_cq_fd(
    int fd,
    int port_id);

int dlb2_ioctl_enable_cq_weight(
    int fd,
    int port_id,
    int limit);

int dlb2_ioctl_enable_cq_epoll(
    int fd,
    int port_id,
    bool is_ldb,
    int process_id,
    int event_fd);

int dlb2_ioctl_get_xtats(
    int fd,
    unsigned int type,
    unsigned int id,
    uint64_t  *val);
#endif /* __DLB2_IOCTL_H__ */
