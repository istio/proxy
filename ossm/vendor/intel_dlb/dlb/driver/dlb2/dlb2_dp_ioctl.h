/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_IOCTL_DP_H
#define __DLB2_IOCTL_DP_H

#include "dlb2_dp_priv.h"
#include "dlb2_main.h"

int dlb2_ioctl_query_cq_poll_mode(struct dlb2 *dlb2, void *karg);

int dlb2_ioctl_get_num_resources(struct dlb2 *dlb2,
				 void *karg);

int dlb2_ioctl_get_xstats(struct dlb2 *dlb2,
			  void *karg);

int __dlb2_ioctl_create_sched_domain(struct dlb2 *dlb2,
				     void *karg,
				     bool user,
				     struct dlb2_dp *dp);

int dlb2_domain_ioctl_create_ldb_queue(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_create_dir_queue(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_create_ldb_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);

int dlb2_domain_ioctl_create_dir_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);

int dlb2_domain_ioctl_map_qid(struct dlb2 *dlb2,
			      struct dlb2_domain *domain,
			      void *karg);

int dlb2_domain_ioctl_unmap_qid(struct dlb2 *dlb2,
				struct dlb2_domain *domain,
				void *karg);

int dlb2_domain_ioctl_get_ldb_queue_depth(struct dlb2 *dlb2,
					  struct dlb2_domain *domain,
					  void *karg);

int dlb2_domain_ioctl_get_dir_queue_depth(struct dlb2 *dlb2,
					  struct dlb2_domain *domain,
					  void *karg);

int dlb2_domain_ioctl_pending_port_unmaps(struct dlb2 *dlb2,
					  struct dlb2_domain *domain,
					  void *karg);

int dlb2_domain_ioctl_enable_cq_weight(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_enable_ldb_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);

int dlb2_domain_ioctl_enable_dir_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);

int dlb2_domain_ioctl_disable_ldb_port(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_disable_dir_port(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_start_domain(struct dlb2 *dlb2,
				   struct dlb2_domain *domain,
				   void *karg);

int dlb2_domain_ioctl_block_on_cq_interrupt(struct dlb2 *dlb2,
					    struct dlb2_domain *domain,
					    void *karg);

int dlb2_domain_ioctl_enqueue_domain_alert(struct dlb2 *dlb2,
					   struct dlb2_domain *domain,
					   void *karg);

int dlb2_domain_ioctl_stop_domain(struct dlb2 *dlb2,
				  struct dlb2_domain *domain,
				  void *karg);
 
int dlb2_domain_ioctl_cq_inflight_ctrl(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg);

int dlb2_domain_ioctl_enable_cq_epoll(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);
 
#endif /* __DLB2_IOCTL_DP_H */
