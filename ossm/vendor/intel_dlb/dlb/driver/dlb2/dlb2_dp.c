// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/bug.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "base/dlb2_osdep.h"
#include "dlb2_dp.h"
#include "dlb2_dp_ioctl.h"
#include "dlb2_dp_ops.h"
#include "dlb2_dp_priv.h"
#include "dlb2_main.h"

static inline void dlb2_log_ioctl_error(struct device *dev,
					int ret,
					int status)
{
	if (ret && status) {
		dev_err(dev, "[%s()] Error: %s\n", __func__,
			dlb2_error_strings[status]);
	} else if (ret) {
		dev_err(dev, "%s: ioctl failed before handler, ret = %d\n",
			__func__, ret);
	}
}

/*****************/
/* DLB Functions */
/*****************/

/*
 * Pointers to DLB devices. These are set in dlb2_datapath_init() and cleared
 * in dlb2_datapath_free().
 */
static struct dlb2 *dlb2_devices[DLB2_MAX_NUM_DEVICES];

void dlb2_datapath_init(struct dlb2 *dev, int id)
{
	dlb2_devices[id] = dev;

	INIT_LIST_HEAD(&dev->dp.hdl_list);
}

static void dlb2_domain_free(struct dlb2_dp_domain *domain)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		struct dlb2_port_hdl *port_hdl, *next;
		struct dlb2_dp_port *port;
		struct list_head *head;

		port = &domain->ldb_ports[i];

		if (!port->configured)
			continue;

		head = &port->hdl_list_head;

		list_for_each_entry_safe(port_hdl, next, head, list)
			dlb2_detach_port(port_hdl);
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(domain->dlb2->hw_ver); i++) {
		struct dlb2_port_hdl *port_hdl, *next;
		struct dlb2_dp_port *port;
		struct list_head *head;

		port = &domain->dir_ports[i];

		if (!port->configured)
			continue;

		head = &port->hdl_list_head;

		list_for_each_entry_safe(port_hdl, next, head, list)
			dlb2_detach_port(port_hdl);
	}
}

/*
 * dlb2_datapath_free - Clean up all datapath-related state
 *
 * This function is called as part of the driver's remove callback, thus no
 * other kernel modules are actively using the datapath. This function follows
 * the standard clean-up procedure (detach handles, reset domains, close DLB
 * handle) for any resources that other kernel software neglected to clean up.
 */
void dlb2_datapath_free(int id)
{
	struct dlb2_dp *dp, *next;
	struct dlb2 *dlb2;

	dlb2 = dlb2_devices[id];

	if (!dlb2)
		return;

	list_for_each_entry_safe(dp, next, &dlb2->dp.hdl_list, next) {
		int i;

		for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
			struct dlb2_domain_hdl *domain_hdl, *next;
			struct dlb2_dp_domain *domain;
			struct list_head *head;

			domain = &dp->domains[i];

			if (!domain->configured)
				continue;

			dlb2_domain_free(domain);

			head = &domain->hdl_list_head;

			list_for_each_entry_safe(domain_hdl, next, head, list)
				dlb2_detach_sched_domain(domain_hdl);

			dlb2_reset_sched_domain(dp, i);
		}

		dlb2_close(dp);
	}

	dlb2_devices[id] = NULL;
}

int dlb2_open(int device_id,
	      struct dlb2_dp **hdl)
{
	int ret = -1;
	struct dlb2 *dlb2;
	struct dlb2_dp *dlb2_dp = NULL;

	BUILD_BUG_ON(sizeof(struct dlb2_enqueue_qe) != 16);
	BUILD_BUG_ON(sizeof(struct dlb2_dequeue_qe) != 16);
	BUILD_BUG_ON(sizeof(struct dlb2_enqueue_qe) !=
		     sizeof(struct dlb2_send));
	BUILD_BUG_ON(sizeof(struct dlb2_enqueue_qe) !=
		     sizeof(struct dlb2_adv_send));
	BUILD_BUG_ON(sizeof(struct dlb2_dequeue_qe) !=
		     sizeof(struct dlb2_recv));

	if (!(device_id >= 0 && device_id < DLB2_MAX_NUM_DEVICES)) {
		ret = -EINVAL;
		goto cleanup;
	}

	dlb2 = dlb2_devices[device_id];

	if (!dlb2) {
		ret = -EINVAL;
		goto cleanup;
	}

	dlb2_dp = devm_kzalloc(&dlb2->pdev->dev, sizeof(*dlb2_dp), GFP_KERNEL);
	if (!dlb2_dp) {
		ret = -ENOMEM;
		goto cleanup;
	}

	dlb2_dp->dlb2 = dlb2;
	dlb2_dp->magic_num = DLB2_MAGIC_NUM;
	dlb2_dp->id = device_id;

	mutex_init(&dlb2_dp->resource_mutex);

	dlb2_register_dp_handle(dlb2_dp);

	*hdl = dlb2_dp;

	ret = 0;

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_open);

int dlb2_close(struct dlb2_dp *dlb2_dp)
{
	int i, ret = -1;

/*
 * DISABLE_CHECK wraps checks that are helpful to catch errors during
 * development, but not strictly required. Typically used for datapath
 * functions to improve performance.
 */
#ifndef DISABLE_CHECK
	if (dlb2_dp->magic_num != DLB2_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	/* Check if there are any remaining attached domain handles */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++)
		if (dlb2_dp->domains[i].configured &&
		    !list_empty(&dlb2_dp->domains[i].hdl_list_head)) {
			ret = -EEXIST;
			goto cleanup;
		}

	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++)
		if (dlb2_dp->domains[i].configured)
			dlb2_reset_sched_domain(dlb2_dp, i);

	dlb2_unregister_dp_handle(dlb2_dp);

	devm_kfree(&dlb2_dp->dlb2->pdev->dev, dlb2_dp);

	ret = 0;

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_close);

static int dlb2_dp_ioctl_get_num_resources(struct dlb2_dp *dlb2_dp,
					   struct dlb2_resources *rsrcs)
{
	struct dlb2_get_num_resources_args ioctl_args = {0};
	int ret;

	ret = dlb2_ioctl_get_num_resources(dlb2_dp->dlb2,
					   (void *)&ioctl_args);

	rsrcs->num_sched_domains = ioctl_args.num_sched_domains;
	rsrcs->num_ldb_queues = ioctl_args.num_ldb_queues;
	rsrcs->num_ldb_ports = ioctl_args.num_ldb_ports;
	rsrcs->num_dir_ports = ioctl_args.num_dir_ports;
	rsrcs->num_ldb_event_state_entries =
		ioctl_args.num_hist_list_entries;
	rsrcs->max_contiguous_ldb_event_state_entries =
		ioctl_args.max_contiguous_hist_list_entries;
	rsrcs->num_ldb_credits =
		ioctl_args.num_ldb_credits;
	rsrcs->num_dir_credits =
		ioctl_args.num_dir_credits;
	rsrcs->num_ldb_credit_pools = NUM_LDB_CREDIT_POOLS;
	rsrcs->num_dir_credit_pools = NUM_DIR_CREDIT_POOLS;
	rsrcs->num_sn_slots[0] = ioctl_args.num_sn_slots[0];
	rsrcs->num_sn_slots[1] = ioctl_args.num_sn_slots[1];

	return ret;
}

int dlb2_get_num_resources(struct dlb2_dp *dlb2_dp,
			   struct dlb2_resources *rsrcs)
{
	int ret = -1;

#ifndef DISABLE_CHECK
	if (dlb2_dp->magic_num != DLB2_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	ret = dlb2_dp_ioctl_get_num_resources(dlb2_dp, rsrcs);

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_get_num_resources);

/*********************************************/
/* Scheduling domain configuration Functions */
/*********************************************/

static int dlb2_dp_ioctl_create_sch_dom(struct dlb2_dp *dlb2_dp,
					struct dlb2_create_sched_domain *args)
{
	struct dlb2_create_sched_domain_args ioctl_args = {0};
	int ret;

	ioctl_args.num_ldb_queues = args->num_ldb_queues;
	ioctl_args.num_ldb_ports = args->num_ldb_ports;
	ioctl_args.num_dir_ports = args->num_dir_ports;
	ioctl_args.num_atomic_inflights =
		args->num_ldb_queues * NUM_ATM_INFLIGHTS_PER_LDB_QUEUE;
	ioctl_args.num_hist_list_entries = args->num_ldb_event_state_entries;
	ioctl_args.num_ldb_credits = args->num_ldb_credits;
	ioctl_args.num_dir_credits = args->num_dir_credits;
	ioctl_args.num_sn_slots[0] = args->num_sn_slots[0];
	ioctl_args.num_sn_slots[1] = args->num_sn_slots[1];

	//ioctl_args.num_ldb_credit_pools = args->num_ldb_credit_pools;
	//ioctl_args.num_dir_credit_pools = args->num_dir_credit_pools;

	ret = __dlb2_ioctl_create_sched_domain(dlb2_dp->dlb2,
					       (void *)&ioctl_args,
					       false,
					       dlb2_dp);

	dlb2_log_ioctl_error(dlb2_dp->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_create_sched_domain(struct dlb2_dp *dlb2_dp,
			     struct dlb2_create_sched_domain *args)
{
	struct dlb2_dp_domain *domain;
	int id, ret, i;

	ret = -1;

#ifndef DISABLE_CHECK
	if (dlb2_dp->magic_num != DLB2_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	mutex_lock(&dlb2_dp->resource_mutex);

	if (!(args->num_ldb_credit_pools <= NUM_LDB_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!(args->num_dir_credit_pools <= NUM_DIR_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	id = dlb2_dp_ioctl_create_sch_dom(dlb2_dp, args);
	if (id < 0) {
		ret = id;
		mutex_unlock(&dlb2_dp->resource_mutex);
		goto cleanup;
	}

	domain = &dlb2_dp->domains[id];

	memset(domain, 0, sizeof(*domain));

	domain->id = id;
	domain->dlb2 = dlb2_dp->dlb2;
	domain->dlb2_dp = dlb2_dp;

	INIT_LIST_HEAD(&domain->hdl_list_head);

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++)
		INIT_LIST_HEAD(&domain->ldb_ports[i].hdl_list_head);
	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(domain->dlb2->hw_ver); i++)
		INIT_LIST_HEAD(&domain->dir_ports[i].hdl_list_head);

	mutex_init(&domain->resource_mutex);

	domain->domain_dev = domain->dlb2->sched_domains[id];

	domain->sw_credits.avail_credits[LDB] = args->num_ldb_credits;
	domain->sw_credits.avail_credits[DIR] = args->num_dir_credits;

	domain->reads_allowed = true;
	domain->num_readers = 0;
	domain->configured = true;

	ret = id;

	mutex_unlock(&dlb2_dp->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_sched_domain);

struct dlb2_domain_hdl *
dlb2_attach_sched_domain(struct dlb2_dp *dlb2_dp,
			 int domain_id)
{
	struct dlb2_domain_hdl *domain_hdl = NULL;
	struct dlb2_dp_domain *domain;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (dlb2_dp->magic_num != DLB2_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	if (!(domain_id >= 0 && domain_id < DLB2_MAX_NUM_DOMAINS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!dlb2_dp->domains[domain_id].configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	domain = &dlb2_dp->domains[domain_id];

	mutex_lock(&domain->resource_mutex);

	domain_hdl = devm_kcalloc(DEV_FROM_DLB2_DP_DOMAIN(domain),
				  1,
				  sizeof(*domain_hdl),
				  GFP_KERNEL);
	if (!domain_hdl) {
		ret = -ENOMEM;
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	domain_hdl->magic_num = DOMAIN_MAGIC_NUM;
	domain_hdl->domain = domain;
	kref_get(&domain->domain_dev->refcnt);

	/* Add the new handle to the domain's linked list of handles */
	list_add(&domain_hdl->list, &domain->hdl_list_head);

	ret = 0;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return (ret == 0) ? domain_hdl : NULL;
}
EXPORT_SYMBOL(dlb2_attach_sched_domain);

int dlb2_detach_sched_domain(struct dlb2_domain_hdl *hdl)
{
	struct dlb2_dp_domain *domain;
	int i, ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	mutex_lock(&domain->resource_mutex);

	/* All port handles must be detached before the domain handle */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++)
		if (!list_empty(&domain->ldb_ports[i].hdl_list_head)) {
			ret = -EINVAL;
			mutex_unlock(&domain->resource_mutex);
			goto cleanup;
		}
	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(domain->dlb2->hw_ver); i++)
		if (!list_empty(&domain->dir_ports[i].hdl_list_head)) {
			ret = -EINVAL;
			mutex_unlock(&domain->resource_mutex);
			goto cleanup;
		}

	/* Remove the handle from the domain's handles list */
	list_del(&hdl->list);

	kref_put(&domain->domain_dev->refcnt, dlb2_free_domain);

	memset(hdl, 0, sizeof(*hdl));
	devm_kfree(DEV_FROM_DLB2_DP_DOMAIN(domain), hdl);

	ret = 0;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_detach_sched_domain);

int dlb2_create_ldb_credit_pool(struct dlb2_domain_hdl *hdl,
				int num_credits)
{
	struct dlb2_dp_domain *domain;
	int i, ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	if (!(num_credits <= domain->sw_credits.avail_credits[LDB])) {
		ret = -EINVAL;
		goto cleanup;
	}

	mutex_lock(&domain->resource_mutex);

	for (i = 0; i < NUM_LDB_CREDIT_POOLS; i++) {
		if (!domain->sw_credits.ldb_pools[i].configured)
			break;
	}

	if (!(i < NUM_LDB_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	atomic_set(&domain->sw_credits.ldb_pools[i].avail_credits, num_credits);
	domain->sw_credits.ldb_pools[i].configured = true;

	domain->sw_credits.avail_credits[LDB] -= num_credits;

	ret = i;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_ldb_credit_pool);

int dlb2_create_dir_credit_pool(struct dlb2_domain_hdl *hdl,
				int num_credits)
{
	struct dlb2_dp_domain *domain;
	int i, ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	if (!(num_credits <= domain->sw_credits.avail_credits[DIR])) {
		ret = -EINVAL;
		goto cleanup;
	}

	mutex_lock(&domain->resource_mutex);

	for (i = 0; i < NUM_DIR_CREDIT_POOLS; i++) {
		if (!domain->sw_credits.dir_pools[i].configured)
			break;
	}

	if (!(i < NUM_DIR_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	atomic_set(&domain->sw_credits.dir_pools[i].avail_credits, num_credits);
	domain->sw_credits.dir_pools[i].configured = true;

	domain->sw_credits.avail_credits[DIR] -= num_credits;

	ret = i;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_dir_credit_pool);

static int dlb2_dp_ioctl_create_ldb_queue(struct dlb2_dp_domain *domain,
					  struct dlb2_create_ldb_queue *args)
{
	struct dlb2_create_ldb_queue_args ioctl_args = {0};
	int ret;

	ioctl_args.num_sequence_numbers = args->num_sequence_numbers;
	ioctl_args.num_atomic_inflights = NUM_ATM_INFLIGHTS_PER_LDB_QUEUE;
	ioctl_args.lock_id_comp_level = args->lock_id_comp_level;
	if (args->num_sequence_numbers > 0)
		ioctl_args.num_qid_inflights = args->num_sequence_numbers;
	else
		/*
		 * Give each queue half of the QID inflights. Intent is to
		 * support high fan-out queues without allowing one or two
		 * queues to use all the inflights.
		 */
		ioctl_args.num_qid_inflights = DLB2_MAX_NUM_QID_INFLIGHTS / 4;

	ret = dlb2_domain_ioctl_create_ldb_queue(domain->dlb2,
						 domain->domain_dev,
						 (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_create_ldb_queue(struct dlb2_domain_hdl *hdl,
			  struct dlb2_create_ldb_queue *args)
{
	struct dlb2_dp_domain *domain;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!args || !hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&domain->resource_mutex);

	ret = dlb2_dp_ioctl_create_ldb_queue(domain, args);

	if (ret >= 0)
		domain->queue_valid[LDB][ret] = true;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_ldb_queue);

static int dlb2_dp_ioctl_create_dir_queue(struct dlb2_dp_domain *domain,
					  int port_id)
{
	struct dlb2_create_dir_queue_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;

	ret = dlb2_domain_ioctl_create_dir_queue(domain->dlb2,
						 domain->domain_dev,
						 (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_create_dir_queue(struct dlb2_domain_hdl *hdl,
			  int port_id)
{
	struct dlb2_dp_domain *domain;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&domain->resource_mutex);

	ret = dlb2_dp_ioctl_create_dir_queue(domain, port_id);

	if (ret >= 0)
		domain->queue_valid[DIR][ret] = true;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_dir_queue);

static int dlb2_create_ldb_port_adv(struct dlb2_domain_hdl *hdl,
				    struct dlb2_create_port *args,
				    struct dlb2_create_port_adv *adv_args);

int dlb2_create_ldb_port(struct dlb2_domain_hdl *hdl,
			 struct dlb2_create_port *args)
{
	struct dlb2_create_port_adv adv_args = {0};
	struct dlb2_create_port __args;
	struct dlb2_sw_credit_pool *pool;
	struct dlb2_dp_domain *domain;
	int ret = -1;

	/* Create a local copy to allow for modifications */
	__args = *args;

#ifndef DISABLE_CHECK
	if (!args || !hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (!(args->ldb_credit_pool_id <= NUM_LDB_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	pool = &domain->sw_credits.ldb_pools[args->ldb_credit_pool_id];

	if (!pool->configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!(args->dir_credit_pool_id <= NUM_DIR_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	pool = &domain->sw_credits.dir_pools[args->dir_credit_pool_id];

	if (!pool->configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	adv_args.cq_history_list_size = __args.num_ldb_event_state_entries;

	/*
	 * Set the low watermark to 1/2 of the credit allocation, and the
	 * quantum to 1/4.
	 */
	adv_args.ldb_credit_low_watermark = __args.num_ldb_credits >> 1;
	adv_args.dir_credit_low_watermark = __args.num_dir_credits >> 1;
	adv_args.ldb_credit_quantum = __args.num_ldb_credits >> 2;
	adv_args.dir_credit_quantum = __args.num_dir_credits >> 2;

	/* Create the load-balanced port */
	ret = dlb2_create_ldb_port_adv(hdl, &__args, &adv_args);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_create_ldb_port);

static int dlb2_create_dir_port_adv(struct dlb2_domain_hdl *hdl,
				    struct dlb2_create_port *args,
				    struct dlb2_create_port_adv *adv_args,
				    int queue_id);

int dlb2_create_dir_port(struct dlb2_domain_hdl *hdl,
			 struct dlb2_create_port *args,
			 int queue_id)
{
	struct dlb2_create_port_adv adv_args = {0};
	struct dlb2_create_port __args;
	struct dlb2_sw_credit_pool *pool;
	struct dlb2_dp_domain *domain;
	int ret = -1;

	/* Create a local copy to allow for modifications */
	__args = *args;

#ifndef DISABLE_CHECK
	if (!args || !hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (!(args->ldb_credit_pool_id <= NUM_LDB_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	pool = &domain->sw_credits.ldb_pools[args->ldb_credit_pool_id];

	if (!pool->configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!(args->dir_credit_pool_id <= NUM_DIR_CREDIT_POOLS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	pool = &domain->sw_credits.dir_pools[args->dir_credit_pool_id];

	if (!pool->configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	/*
	 * Set the low watermark to 1/2 of the credit allocation, and the
	 * quantum to 1/4.
	 */
	adv_args.ldb_credit_low_watermark = __args.num_ldb_credits >> 1;
	adv_args.dir_credit_low_watermark = __args.num_dir_credits >> 1;
	adv_args.ldb_credit_quantum = __args.num_ldb_credits >> 2;
	adv_args.dir_credit_quantum = __args.num_dir_credits >> 2;

	/* Create the directed port */
	ret = dlb2_create_dir_port_adv(hdl, &__args, &adv_args, queue_id);

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_create_dir_port);

struct dlb2_port_hdl *
dlb2_attach_ldb_port(struct dlb2_domain_hdl *hdl,
		     int port_id)
{
	struct dlb2_port_hdl *port_hdl = NULL;
	struct dlb2_dp_domain *domain;
	struct dlb2_dp_port *port;
	struct device *device;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;
	device = DEV_FROM_DLB2_DP_DOMAIN(domain);

	if (!(port_id >= 0 && port_id < DLB2_MAX_NUM_LDB_PORTS)) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!domain->ldb_ports[port_id].configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	port = &domain->ldb_ports[port_id];

	mutex_lock(&port->resource_mutex);

	port_hdl = devm_kzalloc(device, sizeof(*port_hdl), GFP_KERNEL);
	if (!port_hdl) {
		ret = -ENOMEM;
		mutex_unlock(&port->resource_mutex);
		goto cleanup;
	}

	/* Allocate cache-line-aligned memory for sending QEs */
	port_hdl->qe = (void *)devm_get_free_pages(device,
						   GFP_KERNEL,
						   0);
	if (!port_hdl->qe) {
		ret = -ENOMEM;
		mutex_unlock(&port->resource_mutex);
		goto cleanup;
	}

	port_hdl->magic_num = PORT_MAGIC_NUM;
	port_hdl->port = port;

	/* Add the newly created handle to the port's linked list of handles */
	list_add(&port_hdl->list, &port->hdl_list_head);

	ret = 0;

	mutex_unlock(&port->resource_mutex);

cleanup:

	if (ret) {
		if (port_hdl && port_hdl->qe)
			devm_free_pages(DEV_FROM_DLB2_DP_DOMAIN(domain),
					(unsigned long)port_hdl->qe);
		if (port_hdl)
			devm_kfree(DEV_FROM_DLB2_DP_DOMAIN(domain), port_hdl);
		port_hdl = NULL;
	}

	return port_hdl;
}
EXPORT_SYMBOL(dlb2_attach_ldb_port);

struct dlb2_port_hdl *
dlb2_attach_dir_port(struct dlb2_domain_hdl *hdl,
		     int port_id)
{
	struct dlb2_port_hdl *port_hdl = NULL;
	struct dlb2_dp_domain *domain;
	struct dlb2_dp_port *port;
	struct device *device;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;
	device = DEV_FROM_DLB2_DP_DOMAIN(domain);

	if (!(port_id >= 0 &&
	      port_id < DLB2_MAX_NUM_DIR_PORTS(domain->dlb2->hw_ver))) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (!domain->dir_ports[port_id].configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	port = &domain->dir_ports[port_id];

	mutex_lock(&port->resource_mutex);

	port_hdl = devm_kzalloc(device, sizeof(*port_hdl), GFP_KERNEL);
	if (!port_hdl) {
		ret = -ENOMEM;
		mutex_unlock(&port->resource_mutex);
		goto cleanup;
	}

	/* Allocate cache-line-aligned memory for sending QEs */
	port_hdl->qe = (void *)devm_get_free_pages(device,
						   GFP_KERNEL,
						   0);
	if (!port_hdl->qe) {
		ret = -ENOMEM;
		mutex_unlock(&port->resource_mutex);
		goto cleanup;
	}

	port_hdl->magic_num = PORT_MAGIC_NUM;
	port_hdl->port = port;

	/* Add the new handle to the port's linked list of handles */
	list_add(&port_hdl->list, &port->hdl_list_head);

	ret = 0;

	mutex_unlock(&port->resource_mutex);

cleanup:

	if (ret) {
		if (port_hdl && port_hdl->qe)
			devm_free_pages(DEV_FROM_DLB2_DP_DOMAIN(domain),
					(unsigned long)port_hdl->qe);
		if (port_hdl)
			devm_kfree(DEV_FROM_DLB2_DP_DOMAIN(domain), port_hdl);
		port_hdl = NULL;
	}

	return port_hdl;
}
EXPORT_SYMBOL(dlb2_attach_dir_port);

int dlb2_detach_port(struct dlb2_port_hdl *hdl)
{
	struct dlb2_dp_port *port;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	mutex_lock(&port->resource_mutex);

	/* Remove the handle from the port's handles list */
	list_del(&hdl->list);

	memset(hdl, 0, sizeof(*hdl));
	devm_kfree(DEV_FROM_DLB2_DP_DOMAIN(port->domain), hdl);

	ret = 0;

	mutex_unlock(&port->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_detach_port);

static int dlb2_dp_ioctl_link_qid(struct dlb2_dp_domain *domain,
				  int port_id,
				  int queue_id,
				  int priority)
{
	struct dlb2_map_qid_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;
	ioctl_args.qid = queue_id;
	ioctl_args.priority = priority;

	ret = dlb2_domain_ioctl_map_qid(domain->dlb2,
					domain->domain_dev,
					(void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

int dlb2_link_queue(struct dlb2_port_hdl *hdl,
		    int queue_id,
		    int priority)
{
	struct dlb2_dp_port *port;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!(priority >= 0 && priority <= 7)) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

#ifndef DISABLE_CHECK
	if (!hdl || !hdl->port || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&port->resource_mutex);

	ret = dlb2_dp_ioctl_link_qid(port->domain,
				     port->id,
				     queue_id,
				     priority);

	mutex_unlock(&port->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_link_queue);

static int dlb2_dp_ioctl_unlink_qid(struct dlb2_dp_domain *domain,
				    int port_id,
				    int queue_id)
{
	struct dlb2_unmap_qid_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;
	ioctl_args.qid = queue_id;

	ret = dlb2_domain_ioctl_unmap_qid(domain->dlb2,
					  domain->domain_dev,
					  (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? 0 : ret;
}

int dlb2_unlink_queue(struct dlb2_port_hdl *hdl,
		      int queue_id)
{
	struct dlb2_dp_port *port;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || !hdl->port || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&port->resource_mutex);

	ret = dlb2_dp_ioctl_unlink_qid(port->domain,
				       port->id,
				       queue_id);

	mutex_unlock(&port->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_unlink_queue);

static int dlb2_dp_ioctl_enable_ldb_port(struct dlb2_dp_domain *domain,
					 int port_id)
{
	struct dlb2_enable_ldb_port_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;

	ret = dlb2_domain_ioctl_enable_ldb_port(domain->dlb2,
						domain->domain_dev,
						(void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

static int dlb2_dp_ioctl_enable_dir_port(struct dlb2_dp_domain *domain,
					 int port_id)
{
	struct dlb2_enable_dir_port_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;

	ret = dlb2_domain_ioctl_enable_dir_port(domain->dlb2,
						domain->domain_dev,
						(void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

int dlb2_enable_port(struct dlb2_port_hdl *hdl)
{
	struct dlb2_dp_port *port;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || !hdl->port || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&port->resource_mutex);

	if (port->type == LDB)
		ret = dlb2_dp_ioctl_enable_ldb_port(port->domain, port->id);
	else
		ret = dlb2_dp_ioctl_enable_dir_port(port->domain, port->id);

	if (!ret)
		port->enabled = true;

	mutex_unlock(&port->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_enable_port);

static int dlb2_dp_ioctl_disable_ldb_port(struct dlb2_dp_domain *domain,
					  int port_id)
{
	struct dlb2_disable_ldb_port_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;

	ret = dlb2_domain_ioctl_disable_ldb_port(domain->dlb2,
						 domain->domain_dev,
						 (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

static int dlb2_dp_ioctl_disable_dir_port(struct dlb2_dp_domain *domain,
					  int port_id)
{
	struct dlb2_disable_dir_port_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;

	ret = dlb2_domain_ioctl_disable_dir_port(domain->dlb2,
						 domain->domain_dev,
						 (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

int dlb2_disable_port(struct dlb2_port_hdl *hdl)
{
	struct dlb2_dp_port *port;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || !hdl->port || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&port->resource_mutex);

	if (port->type == LDB)
		ret = dlb2_dp_ioctl_disable_ldb_port(port->domain, port->id);
	else
		ret = dlb2_dp_ioctl_disable_dir_port(port->domain, port->id);

	if (!ret)
		port->enabled = false;

	mutex_unlock(&port->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_disable_port);

static int dlb2_dp_ioctl_start_domain(struct dlb2_dp_domain *domain)
{
	struct dlb2_start_domain_args ioctl_args = {0};
	int ret;

	ret = dlb2_domain_ioctl_start_domain(domain->dlb2,
					     domain->domain_dev,
					     (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

int dlb2_start_sched_domain(struct dlb2_domain_hdl *hdl)
{
	struct dlb2_dp_domain *domain;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || !hdl->domain || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	mutex_lock(&domain->resource_mutex);

	if (!domain->thread.started) {
		ret = -ESRCH;
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	ret = dlb2_dp_ioctl_start_domain(domain);
	if (ret) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	domain->started = true;

	ret = 0;

	mutex_unlock(&domain->resource_mutex);

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_start_sched_domain);

static int dlb2_dp_ioctl_enqueue_domain_alert(struct dlb2_dp_domain *domain,
					      u64 aux_alert_data)
{
	struct dlb2_enqueue_domain_alert_args ioctl_args = {0};
	int ret;

	ioctl_args.aux_alert_data = aux_alert_data;

	ret = dlb2_domain_ioctl_enqueue_domain_alert(domain->dlb2,
						     domain->domain_dev,
						     (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

int dlb2_reset_sched_domain(struct dlb2_dp *dlb2_dp,
			    int domain_id)
{
	struct dlb2_dp_domain *domain;
	struct device *device;
	int i, ret = -1;

#ifndef DISABLE_CHECK
	if (dlb2_dp->magic_num != DLB2_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = &dlb2_dp->domains[domain_id];
	device = DEV_FROM_DLB2_DP_DOMAIN(domain);

	if (!domain->configured) {
		ret = -EINVAL;
		goto cleanup;
	}

	/*
	 * A domain handle can't be detached if there are any remaining port
	 * handles, so if there are no domain handles then there are no port
	 * handles.
	 */
	if (!list_empty(&domain->hdl_list_head)) {
		ret = -EINVAL;
		goto cleanup;
	}

	/* Free and iounmap memory associated with the reset ports */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		struct dlb2_dp_port *port = &domain->ldb_ports[i];

		if (port->configured)
			devm_iounmap(domain->dlb2->dev,
				     port->pp_addr);
		memset(port, 0, sizeof(*port));
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(domain->dlb2->hw_ver); i++) {
		struct dlb2_dp_port *port = &domain->dir_ports[i];

		if (port->configured)
			devm_iounmap(domain->dlb2->dev, port->pp_addr);

		memset(port, 0, sizeof(*port));
	}

	/*
	 * Wake this domain's alert thread and prevent further reads. The thread
	 * may have already exited if the device is unexpectedly reset, so check
	 * the started flag first.
	 */
	mutex_lock(&domain->resource_mutex);

	if (domain->thread.started) {
		u64 data = DLB2_DOMAIN_USER_ALERT_RESET;

		dlb2_dp_ioctl_enqueue_domain_alert(domain, data);
	}

	mutex_unlock(&domain->resource_mutex);

	while (1) {
		bool started;

		mutex_lock(&domain->resource_mutex);

		started = domain->thread.started;

		mutex_unlock(&domain->resource_mutex);

		if (!started)
			break;

		schedule();
	}

	/*
	 * The domain device file is opened in
	 * dlb2_ioctl_create_sched_domain(), so close it here. This also
	 * resets the domain.
	 */
	mutex_lock(&dlb2_dp->dlb2->resource_mutex);

	ret = __dlb2_free_domain(domain->domain_dev, domain->shutdown);

	mutex_unlock(&dlb2_dp->dlb2->resource_mutex);

	if (ret)
		goto cleanup;

	memset(domain, 0, sizeof(*domain));

	ret = 0;

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_reset_sched_domain);

static int dlb2_read_domain_device_file(struct dlb2_dp_domain *domain,
					struct dlb2_dp_domain_alert *alert)
{
	struct dlb2_domain_alert kernel_alert;
	u64 alert_id;
	int ret = -1;

	ret = dlb2_read_domain_alert(domain->dlb2,
				     domain->domain_dev,
				     &kernel_alert,
				     false);
	if (ret)
		goto cleanup;

	alert->data = kernel_alert.aux_alert_data;
	alert_id = kernel_alert.alert_id;

	switch (alert_id) {
	case DLB2_DOMAIN_ALERT_DEVICE_RESET:
		alert->id = DLB2_ALERT_DEVICE_RESET;
		break;

	case DLB2_DOMAIN_ALERT_USER:
		if (alert->data == DLB2_DOMAIN_USER_ALERT_RESET)
			alert->id = DLB2_ALERT_DOMAIN_RESET;
		break;

	default:
		if (alert_id < NUM_DLB2_DOMAIN_ALERTS)
			dev_err(domain->dlb2->dev,
				"[%s()] Internal error: received kernel alert %s\n",
				__func__,
				dlb2_domain_alert_strings[alert_id]);
		else
			dev_err(domain->dlb2->dev,
				"[%s()] Internal error: received invalid alert id %llu\n",
				__func__, alert_id);
		ret = -EINVAL;
		break;
	}

cleanup:
	return ret;
}

static int __alert_fn(void *__args)
{
	struct dlb2_dp_domain *domain = __args;

	while (1) {
		struct dlb2_dp_domain_alert alert;

		if (dlb2_read_domain_device_file(domain, &alert))
			break;

		if (domain->thread.fn)
			domain->thread.fn(&alert,
					  domain->id,
					  domain->thread.arg);

		if (alert.id == DLB2_ALERT_DOMAIN_RESET ||
		    alert.id == DLB2_ALERT_DEVICE_RESET)
			break;
	}

	mutex_lock(&domain->resource_mutex);

	domain->thread.started = false;

	mutex_unlock(&domain->resource_mutex);

#if KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE
	kthread_complete_and_exit(NULL, 0);
#elif defined(RHEL_RELEASE_CODE)
#if (RHEL_RELEASE_VERSION(9, 2) <= RHEL_RELEASE_CODE)
	kthread_complete_and_exit(NULL, 0);
#else
	do_exit(0);
#endif
#else
	do_exit(0);
#endif

	return 0;
}

int dlb2_launch_domain_alert_thread(struct dlb2_domain_hdl *hdl,
				    void (*cb)(struct dlb2_dp_domain_alert *,
					       int, void *),
				    void *cb_arg)
{
	struct task_struct *alert_thread;
	struct dlb2_dp_domain *domain;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	mutex_lock(&domain->resource_mutex);

	/* Only one thread per domain allowed */
	if (domain->thread.started) {
		ret = -EEXIST;
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	domain->thread.fn = (void (*)(void *, int, void *)) cb;
	domain->thread.arg = cb_arg;

	alert_thread = kthread_create(__alert_fn,
				      (void *)domain,
				      "domain %d alert thread",
				      domain->id);

	if (IS_ERR(alert_thread)) {
		ret = PTR_ERR(alert_thread);
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	wake_up_process(alert_thread);

	domain->thread.started = true;

	ret = 0;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	return ret;
}
EXPORT_SYMBOL(dlb2_launch_domain_alert_thread);

/****************************************/
/* Scheduling domain datapath functions */
/****************************************/

static const bool credits_required[NUM_QE_CMD_TYPES] = {
	false, /* NOOP */
	false, /* BAT_T */
	false, /* REL */
	false, /* REL_t */
	false, /* (unused) */
	false, /* (unused) */
	false, /* (unused) */
	false, /* (unused) */
	true,  /* NEW */
	true,  /* NEW_T */
	true,  /* FWD */
	true,  /* FWD_T */
};

static inline int num_token_pops(struct dlb2_enqueue_qe *enqueue_qe)
{
	enum dlb2_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;
	int num = 0;

	/* All token return commands set bit 0. BAT_T is a special case. */
	if (cmd & 0x1) {
		num = 1;
		if (cmd == BAT_T)
			num += enqueue_qe->flow_id;
	}

	return num;
}

static inline bool is_release(struct dlb2_enqueue_qe *enqueue_qe)
{
	enum dlb2_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

	return (cmd == REL || cmd == REL_T);
}

static inline bool is_enq_hcw(struct dlb2_enqueue_qe *enqueue_qe)
{
	enum dlb2_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

	return cmd == NEW || cmd == NEW_T || cmd == FWD || cmd == FWD_T;
}

static inline void __attribute__((always_inline))
copy_send_qe(struct dlb2_enqueue_qe *dest,
	     struct dlb2_adv_send *src)
{
	((u64 *)dest)[0] = ((u64 *)src)[0];
	((u64 *)dest)[1] = ((u64 *)src)[1];
}

static bool cmd_releases_hist_list_entry(enum dlb2_event_cmd_t cmd)
{
	return (cmd == REL || cmd == REL_T || cmd == FWD || cmd == FWD_T);
}

static inline void dec_port_owed_releases(struct dlb2_dp_port *port,
					  struct dlb2_enqueue_qe *enqueue_qe)
{
	enum dlb2_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

	port->owed_releases -= cmd_releases_hist_list_entry(cmd);
}

static inline void inc_port_owed_releases(struct dlb2_dp_port *port,
					  int cnt)
{
	port->owed_releases += cnt;
}

static inline void dec_port_owed_tokens(struct dlb2_dp_port *port,
					struct dlb2_enqueue_qe *enqueue_qe)
{
	enum dlb2_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

	/* All token return commands set bit 0. BAT_T is a special case. */
	if (cmd & 0x1) {
		port->owed_tokens--;
		if (cmd == BAT_T)
			port->owed_tokens -= enqueue_qe->flow_id;
	}
}

static inline void inc_port_owed_tokens(struct dlb2_dp_port *port, int cnt)
{
	port->owed_tokens += cnt;
}

static inline void release_port_credits(struct dlb2_dp_port *port)
{
	/*
	 * When a port's local credit cache reaches a threshold, release them
	 * back to the domain's pool.
	 */

	if (port->num_credits[LDB] >= 2 * DLB2_SW_CREDIT_BATCH_SZ) {
		atomic_add(DLB2_SW_CREDIT_BATCH_SZ, port->credit_pool[LDB]);
		port->num_credits[LDB] -= DLB2_SW_CREDIT_BATCH_SZ;
	}

	if (port->num_credits[DIR] >= 2 * DLB2_SW_CREDIT_BATCH_SZ) {
		atomic_add(DLB2_SW_CREDIT_BATCH_SZ, port->credit_pool[DIR]);
		port->num_credits[DIR] -= DLB2_SW_CREDIT_BATCH_SZ;
	}
}

static inline void refresh_port_credits(struct dlb2_dp_port *port,
					enum dlb2_port_type type)
{
	u32 credits = atomic_read(port->credit_pool[type]);
	u32 batch_size = DLB2_SW_CREDIT_BATCH_SZ;
	u32 new;

	if (!credits)
		return;

	batch_size = (credits < batch_size) ? credits : batch_size;

	new = credits - batch_size;

	if (atomic_cmpxchg(port->credit_pool[type], credits, new) == credits)
		port->num_credits[type] += batch_size;
}

static inline void inc_port_credits(struct dlb2_dp_port *port, int num)
{
	port->num_credits[port->type] += num;
}

static inline int __attribute__((always_inline))
__dlb2_adv_send_no_credits(struct dlb2_port_hdl *hdl,
			   u32 num,
			   union dlb2_event *evts,
			   bool issue_store_fence,
			   int *error)
{
	struct dlb2_enqueue_qe *enqueue_qe;
	struct dlb2_dp_port *port = NULL;
	int i, j, ret, count;

	count = 0;
	ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		pr_info("!hdl || hdl->magic_num != PORT_MAGIC_NUM in %s\n",
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	enqueue_qe = hdl->qe;
	port = hdl->port;

#ifndef DISABLE_CHECK
	if (!port->domain->started) {
		pr_info("!port->domain->started in %s\n", __func__);
		ret = -EPERM;
		goto cleanup;
	}
#endif

	/*
	 * Process the send events. DLB accepts 4 QEs (one cache line's worth)
	 * at a time, so process in chunks of four.
	 */
	for (i = 0; i < num; i += 4) {
		if (issue_store_fence)
			/*
			 * Use a store fence to ensure that only one
			 * write-combining operation is present from this core
			 * on the system bus at a time.
			 */
			wmb();

		/*
		 * Initialize the four commands to NOOP and zero int_arm and
		 * rsvd
		 */
		enqueue_qe[0].cmd_byte = NOOP;
		enqueue_qe[1].cmd_byte = NOOP;
		enqueue_qe[2].cmd_byte = NOOP;
		enqueue_qe[3].cmd_byte = NOOP;

		for (j = 0; j < 4 && (i + j) < num; j++, count++) {
			struct dlb2_adv_send *adv_send;

			adv_send = &evts[i + j].adv_send;

			/* Copy the 16B QE */
			copy_send_qe(&enqueue_qe[j], adv_send);

			/*
			 * Zero out meas_lat, no_dec, cmp_id, int_arm, error,
			 * and rsvd.
			 */
			((struct dlb2_adv_send *)&enqueue_qe[j])->rsvd1 = 0;
			((struct dlb2_adv_send *)&enqueue_qe[j])->rsvd2 = 0;

			dec_port_owed_tokens(port, &enqueue_qe[j]);
			dec_port_owed_releases(port, &enqueue_qe[j]);
		}

		if (j != 0)
			port->enqueue_four(enqueue_qe, port->pp_addr);

		if (j != 4)
			break;
	}

	ret = 0;

cleanup:

	if (port)
		release_port_credits(port);

	if (error)
		*error = ret;

	return count;
}

static inline int __attribute__((always_inline))
__dlb2_adv_send(struct dlb2_port_hdl *hdl,
		u32 num,
		union dlb2_event *evts,
		int *error,
		bool issue_store_fence,
		bool credits_required_for_all_cmds)
{
	int used_credits[NUM_PORT_TYPES];
	struct dlb2_enqueue_qe *enqueue_qe;
	struct dlb2_dp_port *port = NULL;
	struct dlb2_dp_domain *domain;
	int i, j, ret, count;

	count = 0;
	ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		pr_info("%s: !hdl || hdl->magic_num != PORT_MAGIC_NUM\n",
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	enqueue_qe = hdl->qe;
	port = hdl->port;
	domain = port->domain;

#ifndef DISABLE_CHECK
	if (!domain->started) {
		pr_info("%s: !domain->started\n", __func__);
		ret = -EPERM;
		goto cleanup;
	}
#endif

	for (i = 0; i < num; i++) {
		enum dlb2_port_type sched_type;
		bool queue_valid;
		int queue_id;

		sched_type = (evts[i].adv_send.sched_type == SCHED_DIRECTED);
		queue_id = evts[i].adv_send.queue_id;
		queue_valid = domain->queue_valid[sched_type][queue_id];

		if (!is_enq_hcw((struct dlb2_enqueue_qe *)&evts[i]))
			continue;

#ifndef DISABLE_CHECK
		if (!queue_valid) {
			pr_info("%s: !queue_valid\n", __func__);
			ret = -EINVAL;
			goto cleanup;
		}
#endif
	}

	used_credits[DIR] = 0;
	used_credits[LDB] = 0;

	/*
	 * Process the send events. DLB accepts 4 QEs (one cache line's worth)
	 * at a time, so process in chunks of four.
	 */
	for (i = 0; i < num; i += 4) {
		if (issue_store_fence)
			/*
			 * Use a store fence to ensure that writes to the
			 * pointed-to data have completed before enqueueing the
			 * HCW, and that only one HCW from this core is on the
			 * system bus at a time.
			 */
			wmb();

		/*
		 * Initialize the four commands to NOOP and zero int_arm and
		 * rsvd.
		 */
		enqueue_qe[0].cmd_byte = NOOP;
		enqueue_qe[1].cmd_byte = NOOP;
		enqueue_qe[2].cmd_byte = NOOP;
		enqueue_qe[3].cmd_byte = NOOP;

		for (j = 0; j < 4 && (i + j) < num; j++, count++) {
			struct dlb2_adv_send *adv_send;
			enum dlb2_port_type type;

			adv_send = &evts[i + j].adv_send;

			type = (adv_send->sched_type == SCHED_DIRECTED);

			/* Copy the 16B QE */
			copy_send_qe(&enqueue_qe[j], adv_send);

			/*
			 * Zero out meas_lat, no_dec, cmp_id, int_arm, error,
			 * and rsvd.
			 */
			((struct dlb2_adv_send *)&enqueue_qe[j])->rsvd1 = 0;
			((struct dlb2_adv_send *)&enqueue_qe[j])->rsvd2 = 0;

			dec_port_owed_tokens(port, &enqueue_qe[j]);
			dec_port_owed_releases(port, &enqueue_qe[j]);

			if (!credits_required_for_all_cmds &&
			    !credits_required[adv_send->cmd])
				continue;

			/* Check credit availability */
			if (port->num_credits[type] == used_credits[type]) {
				/*
				 * Check if the device has replenished this
				 * port's credits.
				 */
				refresh_port_credits(port, type);

				if (port->num_credits[type] ==
				    used_credits[type]) {
					/*
					 * Undo the 16B QE copy by setting cmd
					 * to NOOP.
					 */
					enqueue_qe[j].cmd_byte = 0;
					break;
				}
			}

			used_credits[type]++;
		}

		if (j != 0)
			port->enqueue_four(enqueue_qe, port->pp_addr);

		if (j != 4)
			break;
	}

	port->num_credits[LDB] -= used_credits[LDB];
	port->num_credits[DIR] -= used_credits[DIR];

	ret = 0;

cleanup:

	if (port)
		release_port_credits(port);

	if (error)
		*error = ret;

	return count;
}

static inline int dlb2_adv_send_wrapper(struct dlb2_port_hdl *hdl,
					u32 num,
					union dlb2_event *send,
					int *err,
					enum dlb2_event_cmd_t cmd)
{
	struct dlb2_dp_port *port;
	int i, ret = -1;
	bool is_bat;

#ifndef DISABLE_CHECK
	if (!send || !hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		if (err) {
			pr_info("!send || !hdl || hdl->magic_num != PORT_MAGIC_NUM\n");
			*err = -EINVAL;
		}
		ret = 0;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		if (err) {
			pr_info("port->domain->shutdown error\n");
			*err = -EINTR;
		}
		ret = 0;
		goto cleanup;
	}
#ifndef DISABLE_CHECK
	if (!port->domain->started) {
		if (err) {
			pr_info("!port->domain->started\n");
			*err = -EPERM;
		}
		ret = 0;
		goto cleanup;
	}
#endif

	for (i = 0; i < num; i++)
		send[i].adv_send.cmd = cmd;

	is_bat = (cmd == BAT_T);

	/*
	 * Since we're sending the same command for all events, we can use
	 * specialized send functions according to whether or not credits
	 * are required.
	 *
	 * A store fence isn't required if this is a BAT_T command, which is
	 * safe to reorder and doesn't point to any data.
	 */
	if (credits_required[cmd])
		ret = __dlb2_adv_send(hdl, num, send, err, true, true);
	else
		ret = __dlb2_adv_send_no_credits(hdl, num, send, !is_bat, err);

cleanup:
	return ret;
}

int dlb2_send(struct dlb2_port_hdl *hdl,
	      u32 num,
	      union dlb2_event *event,
	      int *error)
{
	return dlb2_adv_send_wrapper(hdl, num, event, error, NEW);
}
EXPORT_SYMBOL(dlb2_send);

int dlb2_release(struct dlb2_port_hdl *hdl,
		 u32 num,
		 int *error)
{
#define REL_BATCH_SZ 4
	/* This variable intentionally left blank */
	union dlb2_event send[REL_BATCH_SZ];
	struct dlb2_dp_port *port;
	int i, ret = -1;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		if (error)
			*error = -EINVAL;
		ret = 0;
		goto cleanup;
	}
#endif

	port = hdl->port;

#ifndef DISABLE_CHECK
	if (port->type != LDB) {
		if (error)
			*error = -EINVAL;
		ret = 0;
		goto cleanup;
	}
#endif

	/* Prevent the user from releasing more events than are owed. */
	num = (num < port->owed_releases) ? num : port->owed_releases;

	ret = 0;
	for (i = 0; i < num; i += REL_BATCH_SZ) {
		int n, num_to_send = min_t(u32, REL_BATCH_SZ, num);

		n = dlb2_adv_send_wrapper(hdl, num_to_send, send, error, REL);

		ret += n;

		if (n != num_to_send)
			break;
	}

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_release);

int dlb2_forward(struct dlb2_port_hdl *hdl,
		 u32 num,
		 union dlb2_event *event,
		 int *error)
{
	return dlb2_adv_send_wrapper(hdl, num, event, error, FWD);
}
EXPORT_SYMBOL(dlb2_forward);

int dlb2_pop_cq(struct dlb2_port_hdl *hdl,
		u32 num,
		int *error)
{
	/*
	 * Self-initialize send so that GCC doesn't issue a "may be
	 * uninitialized" warning when the udata64 field (which is
	 * intentionally uninitialized) is dereferenced in copy_send_qe().
	 */
	struct dlb2_adv_send send = send;

	struct dlb2_dp_port *port;
	int ret;

#ifndef DISABLE_CHECK
	if (!hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		if (error)
			*error = -EINVAL;
		ret = 0;
		goto cleanup;
	}
#endif

	port = hdl->port;

	/*
	 * Prevent the user from popping more tokens than are owed. This is
	 * required when using dlb2_recv_no_pop() and CQ interrupts (see
	 * __dlb2_block_on_cq_interrupt() for more details), and prevents user
	 * errors when using dlb2_recv().
	 */
	send.num_tokens_minus_one = (num < port->owed_tokens) ?
				     num : port->owed_tokens;
	if (send.num_tokens_minus_one == 0)
		return 0;

	/* The BAT_T count is zero-based so decrement num_tokens_minus_one */
	send.num_tokens_minus_one--;

	ret = dlb2_adv_send_wrapper(hdl,
				    1,
				    (union dlb2_event *)&send,
				    error,
				    BAT_T);

cleanup:
	return ret;
}
EXPORT_SYMBOL(dlb2_pop_cq);

static inline void __attribute__((always_inline))
copy_recv_qe(struct dlb2_recv *dest,
	     struct dlb2_dequeue_qe *src)
{
	((u64 *)dest)[0] = ((u64 *)src)[0];
	((u64 *)dest)[1] = ((u64 *)src)[1];
}

static inline void __dlb2_issue_int_arm_hcw(struct dlb2_port_hdl *hdl,
					    struct dlb2_dp_port *port)
{
	struct dlb2_enqueue_qe *enqueue_qe = hdl->qe;

	memset(enqueue_qe, 0, sizeof(*enqueue_qe) * 4);

	enqueue_qe[0].cmd_byte = CMD_ARM;
	/* Initialize the other commands to NOOP and zero int_arm and rsvd */
	enqueue_qe[1].cmd_byte = NOOP;
	enqueue_qe[2].cmd_byte = NOOP;
	enqueue_qe[3].cmd_byte = NOOP;

	port->enqueue_four(enqueue_qe, port->pp_addr);
}

static int dlb2_dp_ioctl_block_on_cq_interrupt(struct dlb2_dp_domain *domain,
					       int port_id,
					       bool is_ldb,
					       struct dlb2_dequeue_qe *cq_va,
					       u8 cq_gen,
					       bool arm)
{
	struct dlb2_block_on_cq_interrupt_args ioctl_args = {0};
	int ret;

	ioctl_args.port_id = port_id;
	ioctl_args.is_ldb = is_ldb;
	ioctl_args.cq_va = (uintptr_t)cq_va;
	ioctl_args.cq_gen = cq_gen;
	ioctl_args.arm = arm;

	ret = dlb2_domain_ioctl_block_on_cq_interrupt(domain->dlb2,
						      domain->domain_dev,
						      (void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return ret;
}

static inline int __dlb2_block_on_cq_interrupt(struct dlb2_port_hdl *hdl,
					       struct dlb2_dp_port *port)
{
	int ret;

	/*
	 * If the interrupt is not armed, either sleep-poll (see comment below)
	 * or arm the interrupt.
	 */
	if (!port->int_armed)
		__dlb2_issue_int_arm_hcw(hdl, port);

	ret = dlb2_dp_ioctl_block_on_cq_interrupt(port->domain,
						  port->id,
						  port->type == LDB,
						  &port->cq_base[port->cq_idx],
						  port->cq_gen,
						  false);

	/* If the CQ int ioctl was unsuccessful, the interrupt remains armed */
	port->int_armed = (ret != 0);

	return ret;
}

static inline bool port_cq_is_empty(struct dlb2_dp_port *port)
{
	u8 status = READ_ONCE(port->cq_base[port->cq_idx].status);

	return (status & DLB2_QE_STATUS_CQ_GEN_MASK) != port->cq_gen;
}

static inline int __dlb2_recv(struct dlb2_port_hdl *hdl,
			      u32 max,
			      bool wait,
			      bool pop,
			      struct dlb2_recv *event,
			      int *err)
{
	struct dlb2_dp_port *port;
	int ret = -1;
	int cnt = 0;

#ifndef DISABLE_CHECK
	if (!event || !hdl || hdl->magic_num != PORT_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	port = hdl->port;

	if (port->domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}
#ifndef DISABLE_CHECK
	if (!port->domain->started) {
		ret = -EPERM;
		goto cleanup;
	}
#endif

	/* If the port is disabled and its CQ is empty, notify the user */
	if (!port->enabled && port_cq_is_empty(port)) {
		ret = -EACCES;
		goto cleanup;
	}

	/* Wait until at least one QE is available if wait == true */
	/* Future work: wait profile */
	while (wait && port_cq_is_empty(port)) {
		if (__dlb2_block_on_cq_interrupt(hdl, port)) {
			ret = -EINTR;
			goto cleanup;
		}
		if (READ_ONCE(port->domain->shutdown)) {
			ret = -EINTR;
			goto cleanup;
		}
		/* Return if the port is disabled and its CQ is empty */
		if (!port->enabled && port_cq_is_empty(port)) {
			ret = -EACCES;
			goto cleanup;
		}
	}

	ret = 0;

	for (cnt = 0; cnt < max; cnt++) {
		/* TODO: optimize cq_base and other port-> structures */
		if (port_cq_is_empty(port))
			break;

		/* Copy the 16B QE into the user's event structure */
		copy_recv_qe(&event[cnt], &port->cq_base[port->cq_idx]);

		port->cq_idx += port->qe_stride;

		if (unlikely(port->cq_idx == port->cq_limit)) {
			port->cq_gen ^= 1;
			port->cq_idx = 0;
		}
	}

	inc_port_owed_tokens(port, cnt);
	inc_port_owed_releases(port, cnt);

	inc_port_credits(port, cnt);

	if (pop && cnt > 0)
		dlb2_pop_cq(hdl, cnt, NULL);

cleanup:

	if (err)
		*err = ret;

	return cnt;
}

int dlb2_recv(struct dlb2_port_hdl *hdl,
	      u32 max,
	      bool wait,
	      union dlb2_event *event,
	      int *err)
{
	return __dlb2_recv(hdl, max, wait, true, &event->recv, err);
}
EXPORT_SYMBOL(dlb2_recv);

int dlb2_recv_no_pop(struct dlb2_port_hdl *hdl,
		     u32 max,
		     bool wait,
		     union dlb2_event *event,
		     int *err)
{
	return __dlb2_recv(hdl, max, wait, false, &event->recv, err);
}
EXPORT_SYMBOL(dlb2_recv_no_pop);

/************************************/
/* Advanced Configuration Functions */
/************************************/

static int map_consumer_queue(struct dlb2 *dlb2, struct dlb2_dp_port *port)
{
	if (port->type == LDB)
		port->cq_base = dlb2->ldb_port[port->id].cq_base;
	else
		port->cq_base = dlb2->dir_port[port->id].cq_base;

	return (!port->cq_base) ? -1 : 0;
}

static int map_producer_port(struct dlb2 *dlb2, struct dlb2_dp_port *port)
{
	port->pp_addr = devm_ioremap_wc(dlb2->dev,
					dlb2->hw.func_phys_addr +
						PP_BASE(port->type) +
						port->id * PAGE_SIZE,
					PAGE_SIZE);

	return (!port->pp_addr) ? -1 : 0;
}

static int dlb2_dp_ioctl_create_ldb_port(struct dlb2_dp_domain *domain,
					 struct dlb2_create_port *args,
					 struct dlb2_create_port_adv *adv_args)
{
	struct dlb2_create_ldb_port_args ioctl_args = {0};
	int ret;

	ioctl_args.cq_depth = args->cq_depth;
	ioctl_args.cq_depth_threshold = 1;
	ioctl_args.cq_history_list_size = adv_args->cq_history_list_size;

	ret = dlb2_domain_ioctl_create_ldb_port(domain->dlb2,
						domain->domain_dev,
						(void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

static int dlb2_create_ldb_port_adv(struct dlb2_domain_hdl *hdl,
				    struct dlb2_create_port *args,
				    struct dlb2_create_port_adv *adv_args)
{
	struct dlb2_query_cq_poll_mode_args arg = {0};
	struct dlb2_sw_credit_pool *ldb_pool;
	struct dlb2_sw_credit_pool *dir_pool;
	struct dlb2_dp_port *port = NULL;
	struct dlb2_dp_domain *domain;
	enum dlb2_cq_poll_modes mode;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!args || !adv_args || !hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif


	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	dlb2_ioctl_query_cq_poll_mode(domain->dlb2, &arg);
	mode = arg.response.id;

	mutex_lock(&domain->resource_mutex);

	ret = dlb2_dp_ioctl_create_ldb_port(domain, args, adv_args);
	if (ret < 0) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	port = &domain->ldb_ports[ret];

	port->id = ret;
	port->domain = domain;
	port->type = LDB;
	mutex_init(&port->resource_mutex);

	port->pp_addr = NULL;
	port->cq_base = NULL;

	ret = map_producer_port(domain->dlb2, port);
	if (ret) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	ret = map_consumer_queue(domain->dlb2, port);
	if (ret) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	if (movdir64b_supported())
		port->enqueue_four = dlb2_movdir64b;
	else
		port->enqueue_four = dlb2_movntdq;

	ldb_pool = &domain->sw_credits.ldb_pools[args->ldb_credit_pool_id];
	dir_pool = &domain->sw_credits.dir_pools[args->dir_credit_pool_id];

	port->credit_pool[LDB] = &ldb_pool->avail_credits;
	port->credit_pool[DIR] = &dir_pool->avail_credits;
	port->num_credits[LDB] = 0;
	port->num_credits[DIR] = 0;

	/* CQ depths less than 8 use an 8-entry queue but withhold credits */
	port->cq_depth = args->cq_depth <= 8 ? 8 : args->cq_depth;
	port->cq_idx = 0;
	port->cq_gen = 1;

	/* In sparse CQ mode, DLB writes one QE per cache line. */
	if (mode == DLB2_CQ_POLL_MODE_STD)
	    port->qe_stride = 1;
	else
	    port->qe_stride = 4;

	port->cq_limit = port->cq_depth * port->qe_stride;


	port->int_armed = false;

	WRITE_ONCE(port->enabled, true);
	port->configured = true;

	ret = port->id;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	if (ret < 0) {
		if (port && port->pp_addr)
			devm_iounmap(domain->dlb2->dev,
				     port->pp_addr);
	}

	return ret;
}

static int dlb2_dp_ioctl_create_dir_port(struct dlb2_dp_domain *domain,
					 struct dlb2_create_port *args,
					 struct dlb2_create_port_adv *adv_args,
					 int queue_id)
{
	struct dlb2_create_dir_port_args ioctl_args = {0};
	int ret;

	ioctl_args.cq_depth = args->cq_depth;
	ioctl_args.cq_depth_threshold = 1;

	ioctl_args.queue_id = queue_id;

	ret = dlb2_domain_ioctl_create_dir_port(domain->dlb2,
						domain->domain_dev,
						(void *)&ioctl_args);

	dlb2_log_ioctl_error(domain->dlb2->dev,
			     ret,
			     ioctl_args.response.status);

	return (ret == 0) ? ioctl_args.response.id : ret;
}

static int dlb2_create_dir_port_adv(struct dlb2_domain_hdl *hdl,
				    struct dlb2_create_port *args,
				    struct dlb2_create_port_adv *adv_args,
				    int queue_id)
{
	struct dlb2_query_cq_poll_mode_args arg = {0};
	struct dlb2_sw_credit_pool *ldb_pool;
	struct dlb2_sw_credit_pool *dir_pool;
	struct dlb2_dp_port *port = NULL;
	struct dlb2_dp_domain *domain;
	enum dlb2_cq_poll_modes mode;
	int ret = -1;

#ifndef DISABLE_CHECK
	if (!args || !adv_args || !hdl || hdl->magic_num != DOMAIN_MAGIC_NUM) {
		ret = -EINVAL;
		goto cleanup;
	}
#endif

	domain = hdl->domain;

	if (domain->shutdown) {
		ret = -EINTR;
		goto cleanup;
	}

	dlb2_ioctl_query_cq_poll_mode(domain->dlb2, &arg);
	mode = arg.response.id;

	mutex_lock(&domain->resource_mutex);

	ret = dlb2_dp_ioctl_create_dir_port(domain, args, adv_args, queue_id);
	if (ret < 0) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	port = &domain->dir_ports[ret];

	port->id = ret;
	port->domain = domain;
	port->type = DIR;
	mutex_init(&port->resource_mutex);

	port->pp_addr = NULL;
	port->cq_base = NULL;

	ret = map_producer_port(domain->dlb2, port);
	if (ret) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	ret = map_consumer_queue(domain->dlb2, port);
	if (ret) {
		mutex_unlock(&domain->resource_mutex);
		goto cleanup;
	}

	ldb_pool = &domain->sw_credits.ldb_pools[args->ldb_credit_pool_id];
	dir_pool = &domain->sw_credits.dir_pools[args->dir_credit_pool_id];

	port->credit_pool[LDB] = &ldb_pool->avail_credits;
	port->credit_pool[DIR] = &dir_pool->avail_credits;
	port->num_credits[LDB] = 0;
	port->num_credits[DIR] = 0;

	/* CQ depths less than 8 use an 8-entry queue but withhold credits */
	port->cq_depth = args->cq_depth <= 8 ? 8 : args->cq_depth;
	port->cq_idx = 0;
	port->cq_gen = 1;

	/* In sparse CQ mode, DLB writes one QE per cache line. */
	if (mode == DLB2_CQ_POLL_MODE_STD)
	    port->qe_stride = 1;
	else
	    port->qe_stride = 4;

	port->cq_limit = port->cq_depth * port->qe_stride;

	port->int_armed = false;

	if (movdir64b_supported())
		port->enqueue_four = dlb2_movdir64b;
	else
		port->enqueue_four = dlb2_movntdq;

	port->enabled = true;
	port->configured = true;

	ret = port->id;

	mutex_unlock(&domain->resource_mutex);

cleanup:

	if (ret < 0) {
		if (port && port->pp_addr)
			devm_iounmap(domain->dlb2->dev, port->pp_addr);
	}

	return ret;
}
