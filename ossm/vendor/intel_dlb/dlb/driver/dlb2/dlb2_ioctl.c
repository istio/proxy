// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/anon_inodes.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/pid.h>
#include <linux/fdtable.h>
#include <linux/eventfd.h>

#include "dlb2_file.h"
#include "dlb2_intr.h"
#include "dlb2_ioctl.h"
#include "dlb2_dp_ioctl.h"
#include "dlb2_main.h"

#ifdef CONFIG_INTEL_DLB2_DATAPATH
#include "dlb2_dp_priv.h"
#endif

/*
 * The DLB domain ioctl callback template minimizes replication of boilerplate
 * code to copy arguments, acquire and release the resource lock, and execute
 * the command.  The arguments and response structure name should have the
 * format dlb2_<lower_name>_args.
 */
#define DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(lower_name)			   \
int dlb2_domain_ioctl_##lower_name(struct dlb2 *dlb2,			   \
				   struct dlb2_domain *domain,		   \
				   void *karg)				   \
{									   \
	struct dlb2_cmd_response response = {0};			   \
	struct dlb2_##lower_name##_args *arg = karg;			   \
	int ret;							   \
									   \
	mutex_lock(&dlb2->resource_mutex);				   \
									   \
	if (!domain->valid) {						   \
		mutex_unlock(&dlb2->resource_mutex);			   \
		return -EINVAL;						   \
	}								   \
									   \
	ret = dlb2->ops->lower_name(&dlb2->hw,				   \
				    domain->id,				   \
				    arg,				   \
				    &response);				   \
									   \
	mutex_unlock(&dlb2->resource_mutex);				   \
									   \
	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);		   \
									   \
	memcpy(karg, &response, sizeof(response));			   \
									   \
	return ret;							   \
}

DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(create_ldb_queue)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(create_dir_queue)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(start_domain)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(stop_domain)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(map_qid)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(unmap_qid)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(get_ldb_queue_depth)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(get_dir_queue_depth)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(pending_port_unmaps)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(enable_cq_weight)
DLB2_DOMAIN_IOCTL_CALLBACK_TEMPLATE(cq_inflight_ctrl)

/*
 * Port enable/disable ioctls don't use the callback template macro because
 * they have additional CQ interrupt management logic.
 */
int dlb2_domain_ioctl_enable_ldb_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_enable_ldb_port_args *arg = karg;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->enable_ldb_port(&dlb2->hw,
					 domain->id,
					 arg,
					 &response);

	/* Allow threads to block on this port's CQ interrupt */
	if (!ret)
		WRITE_ONCE(dlb2->intr.ldb_cq_intr[arg->port_id].disabled, false);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_enable_dir_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_enable_dir_port_args *arg = karg;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->enable_dir_port(&dlb2->hw,
					 domain->id,
					 arg,
					 &response);

	/* Allow threads to block on this port's CQ interrupt */
	if (!ret)
		WRITE_ONCE(dlb2->intr.dir_cq_intr[arg->port_id].disabled, false);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_disable_ldb_port(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_disable_ldb_port_args *arg = karg;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->disable_ldb_port(&dlb2->hw,
					  domain->id,
					  arg,
					  &response);

	/*
	 * Wake threads blocked on this port's CQ interrupt, and prevent
	 * subsequent attempts to block on it.
	 */
	if (!ret)
		dlb2_wake_thread(&dlb2->intr.ldb_cq_intr[arg->port_id],
				 WAKE_PORT_DISABLED);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_disable_dir_port(struct dlb2 *dlb2,
				       struct dlb2_domain *domain,
				       void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_disable_dir_port_args *arg = karg;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->disable_dir_port(&dlb2->hw,
					  domain->id,
					  arg,
					  &response);

	/*
	 * Wake threads blocked on this port's CQ interrupt, and prevent
	 * subsequent attempts to block on it.
	 */
	if (!ret)
		dlb2_wake_thread(&dlb2->intr.dir_cq_intr[arg->port_id],
				 WAKE_PORT_DISABLED);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

/*
 * Port creation ioctls don't use the callback template macro because they have
 * a number of OS-dependent memory operations.
 */
int dlb2_domain_ioctl_create_ldb_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_create_ldb_port_args *arg = karg;
	dma_addr_t cq_dma_base = 0;
	void *cq_base;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	cq_base = dma_alloc_attrs(&dlb2->pdev->dev,
				  DLB2_CQ_SIZE,
				  &cq_dma_base,
				  GFP_KERNEL,
				  DMA_ATTR_FORCE_CONTIGUOUS);
	if (!cq_base) {
		response.status = DLB2_ST_NO_MEMORY;
		ret = -ENOMEM;
		goto unlock;
	}

	ret = dlb2->ops->create_ldb_port(&dlb2->hw,
					 domain->id,
					 arg,
					 (uintptr_t)cq_dma_base,
					 &response);
	if (ret)
		goto unlock;

	ret = dlb2->ops->enable_ldb_cq_interrupts(dlb2,
						  domain->id,
						  response.id,
						  arg->cq_depth_threshold);
	if (ret)
		goto unlock; /* Internal error, don't unwind port creation */

	/* Fill out the per-port data structure */
	dlb2->ldb_port[response.id].id = response.id;
	dlb2->ldb_port[response.id].is_ldb = true;
	dlb2->ldb_port[response.id].domain = domain;
	dlb2->ldb_port[response.id].cq_base = cq_base;
	dlb2->ldb_port[response.id].cq_dma_base = cq_dma_base;
	dlb2->ldb_port[response.id].efd_ctx = NULL;
	dlb2->ldb_port[response.id].valid = true;

unlock:
	if (ret && cq_dma_base)
		dma_free_attrs(&dlb2->pdev->dev,
			       DLB2_CQ_SIZE,
			       cq_base,
			       cq_dma_base,
			       DMA_ATTR_FORCE_CONTIGUOUS);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_create_dir_port(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_create_dir_port_args *arg = karg;
	dma_addr_t cq_dma_base = 0;
	void *cq_base;
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	cq_base = dma_alloc_attrs(&dlb2->pdev->dev,
				  DLB2_CQ_SIZE,
				  &cq_dma_base,
				  GFP_KERNEL,
				  DMA_ATTR_FORCE_CONTIGUOUS);
	if (!cq_base) {
		response.status = DLB2_ST_NO_MEMORY;
		ret = -ENOMEM;
		goto unlock;
	}

	ret = dlb2->ops->create_dir_port(&dlb2->hw,
					 domain->id,
					 arg,
					 (uintptr_t)cq_dma_base,
					 &response);
	if (ret)
		goto unlock;

	ret = dlb2->ops->enable_dir_cq_interrupts(dlb2,
						  domain->id,
						  response.id,
						  arg->cq_depth_threshold);
	if (ret)
		goto unlock; /* Internal error, don't unwind port creation */

	/* Fill out the per-port data structure */
	dlb2->dir_port[response.id].id = response.id;
	dlb2->dir_port[response.id].is_ldb = false;
	dlb2->dir_port[response.id].domain = domain;
	dlb2->dir_port[response.id].cq_base = cq_base;
	dlb2->dir_port[response.id].cq_dma_base = cq_dma_base;
	dlb2->dir_port[response.id].efd_ctx = NULL;
	dlb2->dir_port[response.id].valid = true;

unlock:
	if (ret && cq_dma_base)
		dma_free_attrs(&dlb2->pdev->dev,
			       DLB2_CQ_SIZE,
			       cq_base,
			       cq_dma_base,
			       DMA_ATTR_FORCE_CONTIGUOUS);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_block_on_cq_interrupt(struct dlb2 *dlb2,
					    struct dlb2_domain *domain,
					    void *karg)
{
	struct dlb2_block_on_cq_interrupt_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	/*
	 * Note: dlb2_block_on_cq_interrupt() checks domain->valid again when
	 * it puts the thread on the waitqueue
	 */
	if (!domain->valid)
		return -EINVAL;

	ret = dlb2_block_on_cq_interrupt(dlb2,
					 domain,
					 arg->port_id,
					 arg->is_ldb,
					 arg->cq_va,
					 arg->cq_gen,
					 arg->arm);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_domain_ioctl_enable_cq_epoll(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_enable_cq_epoll_args *arg = karg;
	struct dlb2_port *port;
	struct eventfd_ctx *efd_ctx;
	struct task_struct *tasks;
	struct file *efd_file;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}
	tasks = pid_task(find_vpid(arg->process_id), PIDTYPE_PID);

#if KERNEL_VERSION(5, 11, 0) > LINUX_VERSION_CODE
	efd_file = fcheck_files(tasks->files, arg->event_fd);
#else
	efd_file = files_lookup_fd_raw(tasks->files, arg->event_fd);
#endif
	efd_ctx = eventfd_ctx_fileget(efd_file);

	port = (arg->is_ldb == 1) ? &dlb2->ldb_port[arg->port_id] :
                                    &dlb2->dir_port[arg->port_id];

	port->efd_ctx = efd_ctx;

	response.status = 0;
        response.id = arg->port_id;

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(&(arg->response), &response, sizeof(response));

	return 0;
}

int dlb2_domain_ioctl_enqueue_domain_alert(struct dlb2 *dlb2 __attribute__((unused)),
					   struct dlb2_domain *domain,
					   void *karg)
{
	struct dlb2_enqueue_domain_alert_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	ret = dlb2_write_domain_alert(domain,
				      DLB2_DOMAIN_ALERT_USER,
				      arg->aux_alert_data);

	memcpy(karg, &response, sizeof(response));
	return ret;
}

static int dlb2_create_port_fd(struct dlb2 *dlb2,
			       const char *prefix,
			       u32 id,
			       const struct file_operations *fops,
			       int *fd,
			       struct file **f)
{
	char *name;
	int ret;

	ret = get_unused_fd_flags(O_RDWR);
	if (ret < 0)
		return ret;

	*fd = ret;

	name = kasprintf(GFP_KERNEL, "%s:%d", prefix, id);
	if (!name) {
		put_unused_fd(*fd);
		return -ENOMEM;
	}

	*f = dlb2_getfile(dlb2, O_RDWR | O_CLOEXEC, fops, name);

	kfree(name);

	if (IS_ERR(*f)) {
		put_unused_fd(*fd);
		return PTR_ERR(*f);
	}

	return 0;
}

static int dlb2_domain_get_port_fd(struct dlb2 *dlb2,
				   struct dlb2_domain *domain,
				   void *karg,
				   const char *name,
				   const struct file_operations *fops,
				   bool is_ldb)
{
	struct dlb2_cmd_response response = {0};
	struct dlb2_get_port_fd_args *arg = karg;
	struct dlb2_port *port;
	struct file *file;
	int ret, fd;

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	if ((is_ldb &&
	     dlb2->ops->ldb_port_owned_by_domain(&dlb2->hw,
						 domain->id,
						 arg->port_id) != 1)) {
		response.status = DLB2_ST_INVALID_PORT_ID;
		ret = -EINVAL;
		goto end;
	}

	if (!is_ldb &&
	    dlb2->ops->dir_port_owned_by_domain(&dlb2->hw,
						domain->id,
						arg->port_id) != 1) {
		response.status = DLB2_ST_INVALID_PORT_ID;
		ret = -EINVAL;
		goto end;
	}

	port = (is_ldb) ? &dlb2->ldb_port[arg->port_id] :
			  &dlb2->dir_port[arg->port_id];

	if (!port->valid) {
		response.status = DLB2_ST_INVALID_PORT_ID;
		ret = -EINVAL;
		goto end;
	}

	ret = dlb2_create_port_fd(dlb2, name, arg->port_id, fops, &fd, &file);
	if (ret < 0)
		goto end;

	file->private_data = port;

	response.id = fd;

end:
	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);

	memcpy(karg, &response, sizeof(response));

	/*
	 * Save fd_install() until after the last point of failure. The domain
	 * refcnt is decremented in the close callback.
	 */
	if (ret == 0) {
		kref_get(&domain->refcnt);

		fd_install(fd, file);
	}

	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

static int dlb2_domain_ioctl_get_ldb_port_pp_fd(struct dlb2 *dlb2,
						struct dlb2_domain *domain,
						void *karg)
{
	return dlb2_domain_get_port_fd(dlb2, domain, karg,
				       "dlb2_ldb_pp:", &dlb2_pp_fops, true);
}

static int dlb2_domain_ioctl_get_ldb_port_cq_fd(struct dlb2 *dlb2,
						struct dlb2_domain *domain,
						void *karg)
{
	return dlb2_domain_get_port_fd(dlb2, domain, karg,
				       "dlb2_ldb_cq:", &dlb2_cq_fops, true);
}

static int dlb2_domain_ioctl_get_dir_port_pp_fd(struct dlb2 *dlb2,
						struct dlb2_domain *domain,
						void *karg)
{
	return dlb2_domain_get_port_fd(dlb2, domain, karg,
				       "dlb2_dir_pp:", &dlb2_pp_fops, false);
}

static int dlb2_domain_ioctl_get_dir_port_cq_fd(struct dlb2 *dlb2,
						struct dlb2_domain *domain,
						void *karg)
{
	return dlb2_domain_get_port_fd(dlb2, domain, karg,
				       "dlb2_dir_cq:", &dlb2_cq_fops, false);
}

typedef int (*dlb2_domain_ioctl_fn_t)(struct dlb2 *dlb2,
				      struct dlb2_domain *domain,
				      void *karg);

static dlb2_domain_ioctl_fn_t dlb2_domain_ioctl_fns[NUM_DLB2_DOMAIN_CMD] = {
	dlb2_domain_ioctl_create_ldb_queue,
	dlb2_domain_ioctl_create_dir_queue,
	dlb2_domain_ioctl_create_ldb_port,
	dlb2_domain_ioctl_create_dir_port,
	dlb2_domain_ioctl_start_domain,
	dlb2_domain_ioctl_map_qid,
	dlb2_domain_ioctl_unmap_qid,
	dlb2_domain_ioctl_enable_ldb_port,
	dlb2_domain_ioctl_enable_dir_port,
	dlb2_domain_ioctl_disable_ldb_port,
	dlb2_domain_ioctl_disable_dir_port,
	dlb2_domain_ioctl_block_on_cq_interrupt,
	dlb2_domain_ioctl_enqueue_domain_alert,
	dlb2_domain_ioctl_get_ldb_queue_depth,
	dlb2_domain_ioctl_get_dir_queue_depth,
	dlb2_domain_ioctl_pending_port_unmaps,
	dlb2_domain_ioctl_get_ldb_port_pp_fd,
	dlb2_domain_ioctl_get_ldb_port_cq_fd,
	dlb2_domain_ioctl_get_dir_port_pp_fd,
	dlb2_domain_ioctl_get_dir_port_cq_fd,
	dlb2_domain_ioctl_enable_cq_weight,
	dlb2_domain_ioctl_enable_cq_epoll,
	dlb2_domain_ioctl_cq_inflight_ctrl,
	dlb2_domain_ioctl_stop_domain
};

static int dlb2_domain_ioctl_arg_size[NUM_DLB2_DOMAIN_CMD] = {
	sizeof(struct dlb2_create_ldb_queue_args),
	sizeof(struct dlb2_create_dir_queue_args),
	sizeof(struct dlb2_create_ldb_port_args),
	sizeof(struct dlb2_create_dir_port_args),
	sizeof(struct dlb2_start_domain_args),
	sizeof(struct dlb2_map_qid_args),
	sizeof(struct dlb2_unmap_qid_args),
	sizeof(struct dlb2_enable_ldb_port_args),
	sizeof(struct dlb2_enable_dir_port_args),
	sizeof(struct dlb2_disable_ldb_port_args),
	sizeof(struct dlb2_disable_dir_port_args),
	sizeof(struct dlb2_block_on_cq_interrupt_args),
	sizeof(struct dlb2_enqueue_domain_alert_args),
	sizeof(struct dlb2_get_ldb_queue_depth_args),
	sizeof(struct dlb2_get_dir_queue_depth_args),
	sizeof(struct dlb2_pending_port_unmaps_args),
	sizeof(struct dlb2_get_port_fd_args),
	sizeof(struct dlb2_get_port_fd_args),
	sizeof(struct dlb2_get_port_fd_args),
	sizeof(struct dlb2_get_port_fd_args),
	sizeof(struct dlb2_enable_cq_weight_args),
	sizeof(struct dlb2_enable_cq_epoll_args),
	sizeof(struct dlb2_cq_inflight_ctrl_args),
	sizeof(struct dlb2_stop_domain_args)
};

#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
long
dlb2_domain_ioctl(struct file *f,
		  unsigned int cmd,
		  unsigned long user_arg)
#else
int
dlb2_domain_ioctl(struct inode *i,
		  struct file *f,
		  unsigned int cmd,
		  unsigned long user_arg)
#endif
{
	struct dlb2_domain *dom = f->private_data;
	struct dlb2 *dlb2 = dom->dlb2;
	dlb2_domain_ioctl_fn_t fn;
	void *karg;
	int size;
	int ret;

	if (_IOC_NR(cmd) >= NUM_DLB2_DOMAIN_CMD) {
		dev_err(dlb2->dev, "[%s()] Unexpected DLB2 command %d\n",
			__func__, _IOC_NR(cmd));
		return -ENOTTY;
	}

	size = dlb2_domain_ioctl_arg_size[_IOC_NR(cmd)];
	fn = dlb2_domain_ioctl_fns[_IOC_NR(cmd)];

	karg = kzalloc(size, GFP_KERNEL);
	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, (void __user *)user_arg, size)) {
		ret = -EFAULT;
		goto end;
	}

	ret = fn(dlb2, dom, karg);

	if (copy_to_user((void __user *)user_arg, karg, size))
		ret = -EFAULT;

end:
	kfree(karg);
	return ret;
}

/* [7:0]: device revision, [15:8]: device version */
#define DLB2_SET_DEVICE_VERSION(ver, rev) (((ver) << 8) | (rev))

static int
dlb2_ioctl_get_device_version(struct dlb2 *dlb2 __attribute__((unused)),
			      void *karg)
{
	struct dlb2_get_device_version_args *arg = karg;
	u8 ver;

	ver = (dlb2->hw_ver == DLB2_HW_V2) ? 2 : 3;

	arg->response.status = 0;
	arg->response.id = DLB2_SET_DEVICE_VERSION(ver, DLB2_REV_A0);

	return 0;
}

int __dlb2_ioctl_create_sched_domain(struct dlb2 *dlb2,
				     void *karg,
				     bool user,
				     struct dlb2_dp *dlb2_dp)
{
	struct dlb2_create_sched_domain_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	struct dlb2_domain *domain;
	size_t offset;
	int ret, fd;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	if (dlb2->domain_reset_failed) {
		response.status = DLB2_ST_DOMAIN_RESET_FAILED;
		ret = -EINVAL;
		goto unlock;
	}

	ret = dlb2->ops->create_sched_domain(&dlb2->hw, arg, &response);
	if (ret)
		goto unlock;

	ret = dlb2_init_domain(dlb2, response.id);
	if (ret) {
		dlb2->ops->reset_domain(&dlb2->hw, response.id);
		goto unlock;
	}

	domain = dlb2->sched_domains[response.id];

	domain->user_mode = user;
#ifdef CONFIG_INTEL_DLB2_DATAPATH
	if (!user) {
		/*
		 * The dp pointer is used to set the structure's 'shutdown'
		 * field in case of an unexpected FLR.
		 */
		domain->dp = &dlb2_dp->domains[response.id];
		goto unlock;
	}
#endif

	fd = anon_inode_getfd("[dlb2domain]", &dlb2_domain_fops,
			      domain, O_RDWR);

	if (fd < 0) {
		dev_err(dlb2->dev,
			"[%s()] Failed to get anon fd.\n", __func__);
		kref_put(&domain->refcnt, dlb2_free_domain);
		ret = fd;
		goto unlock;
	}

	offset = offsetof(struct dlb2_create_sched_domain_args, domain_fd);

	/* There's no reason this should fail, since the copy was validated by
	 * dlb2_copy_from_user() earlier in the function. Regardless, check for
	 * an error (but skip the unwind code).
	 */
	memcpy((void *)(karg + offset), &fd, sizeof(fd));

unlock:
	mutex_unlock(&dlb2->resource_mutex);

	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_create_sched_domain(struct dlb2 *dlb2,
					  void *karg)
{
	return __dlb2_ioctl_create_sched_domain(dlb2, karg, true, NULL);
}

int dlb2_ioctl_get_num_resources(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_get_num_resources_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->get_num_resources(&dlb2->hw, arg);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_set_sn_allocation(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_set_sn_allocation_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->set_sn_allocation(&dlb2->hw, arg->group, arg->num);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_get_sn_allocation(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_get_sn_allocation_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->get_sn_allocation(&dlb2->hw, arg->group);

	response.id = ret;

	if (ret > 0)
		ret = 0;

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_set_cos_bw(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_set_cos_bw_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->set_cos_bw(&dlb2->hw, arg->cos_id, arg->bandwidth);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_get_cos_bw(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_get_cos_bw_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->get_cos_bw(&dlb2->hw, arg->cos_id);

	mutex_unlock(&dlb2->resource_mutex);

	response.id = ret;

	if (ret > 0)
		ret = 0;

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

static int dlb2_ioctl_get_sn_occupancy(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_get_sn_occupancy_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->get_sn_occupancy(&dlb2->hw, arg->group);

	response.id = ret;

	if (ret > 0)
		ret = 0;

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_ioctl_query_cq_poll_mode(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_query_cq_poll_mode_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->query_cq_poll_mode(dlb2, &response);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

int dlb2_ioctl_get_xstats(struct dlb2 *dlb2, void *karg)
{
	struct dlb2_xstats_args *arg = karg;
	struct dlb2_cmd_response response = {0};
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2->reset_active) {
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	ret = dlb2->ops->get_xstats(&dlb2->hw, arg);

	mutex_unlock(&dlb2->resource_mutex);

	BUILD_BUG_ON(offsetof(typeof(*arg), response) != 0);
	memcpy(karg, &response, sizeof(response));

	return ret;
}

typedef int (*dlb2_ioctl_fn_t)(struct dlb2 *dlb2,
			       void *karg);

static dlb2_ioctl_fn_t dlb2_ioctl_fns[NUM_DLB2_CMD] = {
	dlb2_ioctl_get_device_version,
	dlb2_ioctl_create_sched_domain,
	dlb2_ioctl_get_num_resources,
	NULL,
	NULL,
	dlb2_ioctl_set_sn_allocation,
	dlb2_ioctl_get_sn_allocation,
	dlb2_ioctl_set_cos_bw,
	dlb2_ioctl_get_cos_bw,
	dlb2_ioctl_get_sn_occupancy,
	dlb2_ioctl_query_cq_poll_mode,
	dlb2_ioctl_get_xstats,
};

static int dlb2_ioctl_arg_size[NUM_DLB2_CMD] = {
	sizeof(struct dlb2_get_device_version_args),
	sizeof(struct dlb2_create_sched_domain_args),
	sizeof(struct dlb2_get_num_resources_args),
	0,
	0,
	sizeof(struct dlb2_set_sn_allocation_args),
	sizeof(struct dlb2_get_sn_allocation_args),
	sizeof(struct dlb2_set_cos_bw_args),
	sizeof(struct dlb2_get_cos_bw_args),
	sizeof(struct dlb2_get_sn_occupancy_args),
	sizeof(struct dlb2_query_cq_poll_mode_args),
	sizeof(struct dlb2_xstats_args),
};

#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
long
dlb2_ioctl(struct file *f,
	   unsigned int cmd,
	   unsigned long user_arg)
#else
int
dlb2_ioctl(struct inode *i,
	   struct file *f,
	   unsigned int cmd,
	   unsigned long user_arg)
#endif
{
	struct dlb2 *dlb2 = container_of(f->f_inode->i_cdev, struct dlb2, cdev);
	dlb2_ioctl_fn_t fn;
	void *karg;
	int size;
	int ret;

	if (_IOC_NR(cmd) >= NUM_DLB2_CMD ||
	    _IOC_NR(cmd) == DLB2_CMD_RESERVED1 ||
	    _IOC_NR(cmd) == DLB2_CMD_RESERVED2) {
		dev_err(dlb2->dev, "[%s()] Unexpected DLB2 command %d\n",
			__func__, _IOC_NR(cmd));
		return -ENOTTY;
	}

	size = dlb2_ioctl_arg_size[_IOC_NR(cmd)];
	fn = dlb2_ioctl_fns[_IOC_NR(cmd)];

	karg = kzalloc(size, GFP_KERNEL);
	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, (void __user *)user_arg, size)) {
		ret = -EFAULT;
		goto end;
	}

	ret = fn(dlb2, karg);

	if (copy_to_user((void __user *)user_arg, karg, size))
		ret = -EFAULT;

end:
	kfree(karg);
	return ret;
}
