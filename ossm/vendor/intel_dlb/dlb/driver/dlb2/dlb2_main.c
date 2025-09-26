// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2018-2020 Intel Corporation

#include <linux/aer.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>

#include "base/dlb2_mbox.h"
#include "base/dlb2_resource.h"
#include "dlb2_file.h"
#include "dlb2_intr.h"
#include "dlb2_ioctl.h"
#include "dlb2_main.h"
#include "dlb2_sriov.h"
#include "dlb2_perf.h"

#ifdef CONFIG_INTEL_DLB2_DATAPATH
#include "dlb2_dp_priv.h"
#endif

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Copyright(c) 2018-2020 Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Dynamic Load Balancer 2.x Driver");
MODULE_VERSION(DLB2_DRIVER_VERSION);

static unsigned int dlb2_reset_timeout_s = DLB2_DEFAULT_RESET_TIMEOUT_S;
module_param_named(reset_timeout_s, dlb2_reset_timeout_s, uint, 0644);
MODULE_PARM_DESC(reset_timeout_s,
		 "Wait time (in seconds) after reset is requested given for app shutdown until driver zaps VMAs");
bool dlb2_pasid_override;
module_param_named(pasid_override, dlb2_pasid_override, bool, 0444);
MODULE_PARM_DESC(pasid_override, "Override allocated PASID with 0");
bool dlb2_wdto_disable;
module_param_named(wdto_disable, dlb2_wdto_disable, bool, 0444);
MODULE_PARM_DESC(wdto_disable, "Disable per-CQ watchdog timers");

unsigned int dlb2_qe_sa_pct = 1;
module_param_named(qe_sa_pct, dlb2_qe_sa_pct, uint, 0444);
MODULE_PARM_DESC(qe_sa_pct,
		 "Percentage of QE selections that use starvation avoidance (SA) instead of strict priority. SA boosts one priority level for that selection; if there are no schedulable QEs of the boosted priority, the device selects according to normal priorities. Priorities 1-7 have an equal chance of being boosted when SA is used for QE selection. If SA is 0%, the device will use strict priority whenever possible. (Valid range: 0-100, default: 1)");
unsigned int dlb2_qid_sa_pct;
module_param_named(qid_sa_pct, dlb2_qid_sa_pct, uint, 0444);
MODULE_PARM_DESC(qid_sa_pct,
		 "Percentage of QID selections that use starvation avoidance (SA) instead of strict priority. SA boosts one priority level for that selection; if there are no schedulable QIDs of the boosted priority, the device selects according to normal priorities. Priorities 1-7 have an equal chance of being boosted when SA is used for QID selection. If SA is 0%, the device will use strict priority whenever possible. (Valid range: 0-100, default: 0)");


unsigned int dlb2_qidx_wrr_weight = DLB2_DEFAULT_QIDX_WRR_SCHEDULER_WEIGHT;
module_param_named(qidx_wrr_weight, dlb2_qidx_wrr_weight, uint, 0444);
MODULE_PARM_DESC(qidx_wrr_weight,
		 "All QIDIX share a common 3 bit weight register. A weight of 0 implements a standard RR, a weight of 1 means the same QEs for the CQ may be scheduled 2 times before rotating. Default value is 0");


/* The driver mutex protects data structures that used by multiple devices. */
DEFINE_MUTEX(dlb2_driver_mutex);
struct list_head dlb2_dev_list = LIST_HEAD_INIT(dlb2_dev_list);

static struct class *dlb2_class;
static dev_t dlb2_devt;
static DEFINE_IDA(dlb2_ids);

static int port_probe = DLB2_PROBE_FAST;

static int dlb2_param_set(const char *val, const struct kernel_param *kp)
{
	uint32_t v;
	int ret;

	ret = kstrtouint(val, 0, &v);
	if (ret != 0 || v > DLB2_PROBE_FAST)
		return -EINVAL;

	return param_set_int(val, kp);
}

static const struct kernel_param_ops param_ops = {
	.set	= dlb2_param_set,
	.get	= param_get_int,
};

module_param_cb(port_probe, &param_ops, &port_probe, 0444);
MODULE_PARM_DESC(port_probe, "Probe DLB2 ports for best port selection (0=disable, 1=slow (most reliable), 2=fast (default, mostly reliable))");

int dlb2_port_probe(struct dlb2 *dlb2)
{
	if (DLB2_IS_VF(dlb2))
		return DLB2_NO_PROBE;

	return port_probe;
}

static int dlb2_reset_device(struct pci_dev *pdev)
{
	int ret;

	ret = pci_save_state(pdev);
	if (ret)
		return ret;

	ret = __pci_reset_function_locked(pdev);
	if (ret)
		return ret;

	pci_restore_state(pdev);

	return 0;
}

static void dlb2_assign_ops(struct dlb2 *dlb2,
			    const struct pci_device_id *pdev_id)
{
	dlb2->type = pdev_id->driver_data;

	switch (pdev_id->driver_data) {
	case DLB2_PF:
	case DLB2_5_PF:
		dlb2->ops = &dlb2_pf_ops;
		break;
	case DLB2_VF:
	case DLB2_5_VF:
		dlb2->ops = &dlb2_vf_ops;
		break;
	}

	if (dlb2->type == DLB2_PF || dlb2->type == DLB2_VF)
		dlb2->hw_ver = DLB2_HW_V2;
	else
		dlb2->hw_ver = DLB2_HW_V2_5;
}

static int dlb2_cdev_add(struct dlb2 *dlb2,
			 const struct file_operations *fops)
{
	int ret;

	dlb2->dev_number = MKDEV(MAJOR(dlb2_devt), MINOR(dlb2_devt) + dlb2->id);

	cdev_init(&dlb2->cdev, fops);

	dlb2->cdev.dev   = dlb2->dev_number;
	dlb2->cdev.owner = THIS_MODULE;

	ret = cdev_add(&dlb2->cdev, dlb2->cdev.dev, 1);
	if (ret < 0)
		dev_err(dlb2->dev,
			"%s: cdev_add() returned %d\n",
			dlb2_driver_name, ret);

	return ret;
}

static int dlb2_device_create(struct dlb2 *dlb2, struct pci_dev *pdev)
{
	/*
	 * Create a new device in order to create a /dev/dlb node. This device
	 * is a child of the DLB PCI device.
	 */
	dlb2->dev = device_create(dlb2_class,
				  &pdev->dev,
				  dlb2->dev_number,
				  dlb2,
				  "dlb%d",
				  dlb2->id);
	if (IS_ERR(dlb2->dev)) {
		dev_err(dlb2->dev,
			"%s: device_create() returned %ld\n",
			dlb2_driver_name, PTR_ERR(dlb2->dev));

		return PTR_ERR(dlb2->dev);
	}

	return 0;
}

/********************************/
/****** Char dev callbacks ******/
/********************************/

static int dlb2_open(struct inode *i, struct file *f)
{
	struct dlb2 *dlb2 = container_of(f->f_inode->i_cdev, struct dlb2, cdev);

	/* See dlb2_reset_prepare() for more details */
	if (dlb2->reset_active)
		return -EINVAL;

	f->private_data = dlb2;

	/*
	 * Increment the device's usage count and immediately wake it
	 * if it was suspended.
	 */
	pm_runtime_get_sync(&dlb2->pdev->dev);

	return 0;
}

static int dlb2_close(struct inode *i, struct file *f)
{
	struct dlb2 *dlb2 = container_of(f->f_inode->i_cdev, struct dlb2, cdev);

	/*
	 * Decrement the device's usage count and suspend it when
	 * the application stops using it.
	 */
	pm_runtime_put_sync_suspend(&dlb2->pdev->dev);

	return 0;
}

static const struct file_operations dlb2_fops = {
	.owner   = THIS_MODULE,
	.open    = dlb2_open,
	.release = dlb2_close,
#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
	.unlocked_ioctl = dlb2_ioctl,
#else
	.ioctl   = dlb2_ioctl,
#endif
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
	.compat_ioctl = compat_ptr_ioctl,
#else
	.compat_ioctl = dlb2_ioctl,
#endif
};

int dlb2_init_domain(struct dlb2 *dlb2, u32 domain_id)
{
	struct dlb2_domain *domain;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	domain->id = domain_id;

	domain->valid = true;
	kref_init(&domain->refcnt);
	domain->dlb2 = dlb2;

	spin_lock_init(&domain->alert_lock);
	init_waitqueue_head(&domain->wq_head);

	dlb2->sched_domains[domain_id] = domain;

	/*
	 * The matching put is in dlb2_free_domain, executed when the domain's
	 * refcnt reaches zero.
	 */
	pm_runtime_get_sync(&dlb2->pdev->dev);

	return 0;
}

static void dlb2_release_port_memory(struct dlb2 *dlb2,
				     struct dlb2_port *port,
				     bool check_domain,
				     u32 domain_id)
{
	if (port->valid &&
	    (!check_domain || port->domain->id == domain_id)) {
		dma_free_attrs(&dlb2->pdev->dev,
			       DLB2_CQ_SIZE,
			       port->cq_base,
			       port->cq_dma_base,
			       DMA_ATTR_FORCE_CONTIGUOUS);

		port->valid = false;
	}
}

static void dlb2_release_domain_memory(struct dlb2 *dlb2,
				       bool check_domain,
				       u32 domain_id)
{
	struct dlb2_port *port;
	int i;

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		port = &dlb2->ldb_port[i];

		dlb2_release_port_memory(dlb2, port, check_domain, domain_id);
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++) {
		port = &dlb2->dir_port[i];

		dlb2_release_port_memory(dlb2, port, check_domain, domain_id);
	}
}

void dlb2_release_device_memory(struct dlb2 *dlb2)
{
	dlb2_release_domain_memory(dlb2, false, 0);
}

#ifndef CONFIG_INTEL_DLB2_DATAPATH
static
#endif
int __dlb2_free_domain(struct dlb2_domain *domain,
		       bool skip_reset)
{
	struct dlb2 *dlb2 = domain->dlb2;
	int i, ret = 0;

	/*
	 * Check if the domain was reset and its memory released during FLR
	 * handling.
	 */
	if (!domain->valid) {
		/*
		 * Before clearing the sched_domains[] pointer, confirm the
		 * slot isn't in use by a newer (valid) domain.
		 */
		if (dlb2->sched_domains[domain->id] == domain)
			dlb2->sched_domains[domain->id] = NULL;

		kfree(domain);
		return 0;
	}

	if (!skip_reset)
		ret = dlb2->ops->reset_domain(&dlb2->hw, domain->id);

	/* Unpin all memory pages associated with the domain */
	dlb2_release_domain_memory(dlb2, true, domain->id);

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++)
		if (dlb2->intr.ldb_cq_intr[i].domain_id == domain->id)
			dlb2->intr.ldb_cq_intr[i].configured = false;
	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++)
		if (dlb2->intr.dir_cq_intr[i].domain_id == domain->id)
			dlb2->intr.dir_cq_intr[i].configured = false;

	if (ret) {
		dlb2->domain_reset_failed = true;
		dev_err(dlb2->dev,
			"Internal error: Domain reset failed. To recover, reset the device.\n");
	}

	dlb2->sched_domains[domain->id] = NULL;

	kfree(domain);

	/*
	 * Decrement the device's usage count and suspend it when
	 * the last application stops using it. The matching get is in
	 * dlb2_init_domain.
	 */
	pm_runtime_put_sync_suspend(&dlb2->pdev->dev);

	return ret;
}

void dlb2_free_domain(struct kref *kref)
{
	struct dlb2_domain *domain;
	struct dlb2 *dlb2;

	domain = container_of(kref, struct dlb2_domain, refcnt);

	dlb2 = domain->dlb2;

	__dlb2_free_domain(domain, false);
}

static int dlb2_domain_close(struct inode *i, struct file *f)
{
	struct dlb2_domain *domain = f->private_data;
	struct dlb2 *dlb2 = domain->dlb2;

	mutex_lock(&dlb2->resource_mutex);

	dev_dbg(dlb2->dev,
		"Closing domain %d's device file\n", domain->id);

	kref_put(&domain->refcnt, dlb2_free_domain);

	mutex_unlock(&dlb2->resource_mutex);

	return 0;
}

int dlb2_write_domain_alert(struct dlb2_domain *domain,
			    u64 alert_id,
			    u64 aux_alert_data)
{
	struct dlb2_domain_alert alert;
	int idx;

	if (!domain || !domain->valid)
		return -EINVAL;

	/* Grab the alert mutex to access the read and write indexes */
	spin_lock(&domain->alert_lock);

	/* If there's no space for this notification, return */
	if ((domain->alert_wr_idx - domain->alert_rd_idx) ==
	    (DLB2_DOMAIN_ALERT_RING_SIZE - 1)) {
		spin_unlock(&domain->alert_lock);
		return 0;
	}

	alert.alert_id = alert_id;
	alert.aux_alert_data = aux_alert_data;

	idx = domain->alert_wr_idx % DLB2_DOMAIN_ALERT_RING_SIZE;

	domain->alerts[idx] = alert;

	domain->alert_wr_idx++;

	spin_unlock(&domain->alert_lock);

	/* Wake any blocked readers */
	wake_up_interruptible(&domain->wq_head);

	return 0;
}

static bool dlb2_alerts_avail(struct dlb2_domain *domain)
{
	bool ret;

	spin_lock(&domain->alert_lock);

	ret = domain->alert_rd_idx != domain->alert_wr_idx;

	spin_unlock(&domain->alert_lock);

	return ret;
}

int dlb2_read_domain_alert(struct dlb2 *dlb2,
			   struct dlb2_domain *domain,
			   struct dlb2_domain_alert *alert,
			   bool nonblock)
{
	u8 idx;

	/* Grab the alert lock to access the read and write indexes */
	spin_lock(&domain->alert_lock);

	while (domain->alert_rd_idx == domain->alert_wr_idx) {
		/*
		 * Release the alert lock before putting the thread on the wait
		 * queue.
		 */
		spin_unlock(&domain->alert_lock);

		if (nonblock)
			return -EWOULDBLOCK;

		dev_dbg(dlb2->dev,
			"Thread %d is blocking waiting for an alert in domain %d\n",
			current->pid, domain->id);

		if (wait_event_interruptible(domain->wq_head,
					     dlb2_alerts_avail(domain) ||
					     !READ_ONCE(domain->valid)))
			return -ERESTARTSYS;

		/* See dlb2_reset_prepare() for more details */
		if (!READ_ONCE(domain->valid)) {
			alert->alert_id = DLB2_DOMAIN_ALERT_DEVICE_RESET;
			return 0;
		}

		spin_lock(&domain->alert_lock);
	}

	/* The alert indexes are not equal, so there is an alert available. */
	idx = domain->alert_rd_idx % DLB2_DOMAIN_ALERT_RING_SIZE;

	memcpy(alert, &domain->alerts[idx], sizeof(*alert));

	domain->alert_rd_idx++;

	spin_unlock(&domain->alert_lock);

	return 0;
}

static ssize_t dlb2_domain_read(struct file *f,
				char __user *buf,
				size_t len,
				loff_t *offset)
{
	struct dlb2_domain *domain = f->private_data;
	struct dlb2 *dlb2 = domain->dlb2;
	struct dlb2_domain_alert alert;
	int ret;

	if (len != sizeof(alert))
		return -EINVAL;

	if (!domain->valid) {
		alert.alert_id = DLB2_DOMAIN_ALERT_DEVICE_RESET;
		goto copy;
	}

	/* See dlb2_user.h for details on domain alert notifications */

	ret = dlb2_read_domain_alert(dlb2,
				     domain,
				     &alert,
				     f->f_flags & O_NONBLOCK);
	if (ret)
		return ret;

copy:
	if (copy_to_user(buf, &alert, sizeof(alert)))
		return -EFAULT;

	dev_dbg(dlb2->dev,
		"Thread %d received alert 0x%llx, with aux data 0x%llx\n",
		current->pid, ((u64 *)&alert)[0], ((u64 *)&alert)[1]);

	return sizeof(alert);
}

const struct file_operations dlb2_domain_fops = {
	.owner   = THIS_MODULE,
	.release = dlb2_domain_close,
	.read    = dlb2_domain_read,
#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
	.unlocked_ioctl = dlb2_domain_ioctl,
#else
	.ioctl   = dlb2_domain_ioctl,
#endif
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
	.compat_ioctl = compat_ptr_ioctl,
#else
	.compat_ioctl = dlb2_domain_ioctl,
#endif
};

static int dlb2_pp_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct dlb2_port *port = f->private_data;
	struct dlb2_domain *domain = port->domain;
	struct dlb2 *dlb2 = domain->dlb2;
	unsigned long pgoff;
	pgprot_t pgprot;
	int ret;

	dev_dbg(dlb2->dev, "[%s()] %s port %d\n",
		__func__, port->is_ldb ? "LDB" : "DIR", port->id);

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		ret = -EINVAL;
		goto end;
	}

	if ((vma->vm_end - vma->vm_start) != DLB2_PP_SIZE) {
		ret = -EINVAL;
		goto end;
	}

	pgprot = pgprot_noncached(vma->vm_page_prot);

	pgoff = dlb2->hw.func_phys_addr;

	/* Use non-maskable address windows for PF and maskable address
	 * windows for VF/VDEV.
	 *
	 * The non-maskable PP address does not work in DLB 2.5 simics model.
	 *
	 */
	if (dlb2->type == DLB2_PF || dlb2->type == DLB2_5_PF) {
		if (port->is_ldb)
			pgoff += DLB2_DRV_LDB_PP_OFFS(port->id);
		else
			pgoff += DLB2_DRV_DIR_PP_OFFS(port->id);
	} else {
		if (port->is_ldb)
			pgoff += DLB2_LDB_PP_OFFS(port->id);
		else
			pgoff += DLB2_DIR_PP_OFFS(port->id);
	}

	ret = io_remap_pfn_range(vma,
				 vma->vm_start,
				 pgoff >> PAGE_SHIFT,
				 vma->vm_end - vma->vm_start,
				 pgprot);

end:
	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

static int dlb2_cq_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct dlb2_port *port = f->private_data;
	struct dlb2_domain *domain = port->domain;
	struct dlb2 *dlb2 = domain->dlb2;
	struct page *page;
	int ret;

	dev_dbg(dlb2->dev, "[%s()] %s port %d\n",
		__func__, port->is_ldb ? "LDB" : "DIR", port->id);

	mutex_lock(&dlb2->resource_mutex);

	if (!domain->valid) {
		ret = -EINVAL;
		goto end;
	}

	if ((vma->vm_end - vma->vm_start) != DLB2_CQ_SIZE) {
		ret = -EINVAL;
		goto end;
	}

	page = virt_to_page(port->cq_base);

	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      page_to_pfn(page),
			      vma->vm_end - vma->vm_start,
			      vma->vm_page_prot);

end:
	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

static int dlb2_port_close(struct inode *i, struct file *f)
{
	struct dlb2_port *port = f->private_data;
	struct dlb2_domain *domain = port->domain;
	struct dlb2 *dlb2 = domain->dlb2;

	mutex_lock(&dlb2->resource_mutex);

	dev_dbg(dlb2->dev,
		"Closing domain %d's port file\n", domain->id);

	kref_put(&domain->refcnt, dlb2_free_domain);

	/* Decrement the refcnt of the pseudo-FS used to allocate the inode */
	dlb2_release_fs(dlb2);

	mutex_unlock(&dlb2->resource_mutex);

	return 0;
}

const struct file_operations dlb2_pp_fops = {
	.owner   = THIS_MODULE,
	.release = dlb2_port_close,
	.mmap    = dlb2_pp_mmap,
};

const struct file_operations dlb2_cq_fops = {
	.owner   = THIS_MODULE,
	.release = dlb2_port_close,
	.mmap    = dlb2_cq_mmap,
};

/**********************************/
/****** PCI driver callbacks ******/
/**********************************/

static int dlb2_probe(struct pci_dev *pdev,
		      const struct pci_device_id *pdev_id)
{
	struct dlb2 *dlb2;
	int ret;

	dlb2 = devm_kzalloc(&pdev->dev, sizeof(*dlb2), GFP_KERNEL);
	if (!dlb2)
		return -ENOMEM;

	dlb2_assign_ops(dlb2, pdev_id);

	pci_set_drvdata(pdev, dlb2);

	dlb2->pdev = pdev;

	dlb2->id = ida_alloc_max(&dlb2_ids,
				 DLB2_MAX_NUM_DEVICES - 1,
				 GFP_KERNEL);
	if (dlb2->id < 0) {
		dev_err(&pdev->dev, "probe: device ID allocation failed\n");

		ret = dlb2->id;
		goto alloc_id_fail;
	}

	ret = pci_enable_device(pdev);
	if (ret != 0) {
		dev_err(&pdev->dev, "pci_enable_device() returned %d\n", ret);

		goto pci_enable_device_fail;
	}

	ret = pci_request_regions(pdev, dlb2_driver_name);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"pci_request_regions(): returned %d\n", ret);

		goto pci_request_regions_fail;
	}

	pci_set_master(pdev);

#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE && !defined(DLB2_RHEL_GE_9_4))
	if (pci_enable_pcie_error_reporting(pdev))
		dev_info(&pdev->dev, "[%s()] AER is not supported\n", __func__);
#endif

#ifdef CONFIG_INTEL_DLB2_SIOV
	/*
	 * Don't call pci_disable_pasid() if it is already disabled to avoid
	 * the WARN_ON() print
	 */
	if (pdev->pasid_enabled)
		pci_disable_pasid(pdev);
#endif

	ret = dlb2->ops->map_pci_bar_space(dlb2, pdev);
	if (ret)
		goto map_pci_bar_fail;

	/* (VF only) Register the driver with the PF driver */
	ret = dlb2->ops->register_driver(dlb2);
	if (ret)
		goto driver_registration_fail;

	/*
	 * If this is an auxiliary VF, it can skip the rest of the probe
	 * function. This VF is only used for its MSI interrupt vectors, and
	 * the VF's register_driver callback will initialize them.
	 */
	if (DLB2_IS_VF(dlb2) && dlb2->vf_id_state.is_auxiliary_vf)
		goto aux_vf_probe;

	ret = dlb2_cdev_add(dlb2, &dlb2_fops);
	if (ret)
		goto cdev_add_fail;

	ret = dlb2_device_create(dlb2, pdev);
	if (ret)
		goto device_add_fail;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		goto dma_set_mask_fail;

	ret = dlb2->ops->sysfs_create(dlb2);
	if (ret)
		goto sysfs_create_fail;

	/*
	 * PM enable must be done before any other MMIO accesses, and this
	 * setting is persistent across device reset.
	 */
	dlb2->ops->enable_pm(dlb2);

	ret = dlb2->ops->wait_for_device_ready(dlb2, pdev);
	if (ret)
		goto wait_for_device_ready_fail;

	ret = dlb2_resource_probe(&dlb2->hw, NULL);
	if (ret)
		goto dlb2_resource_probe_fail;

	ret = dlb2_reset_device(pdev);
	if (ret && DLB2_IS_PF(dlb2))
		goto dlb2_reset_fail;

	ret = dlb2->ops->init_interrupts(dlb2, pdev);
	if (ret)
		goto init_interrupts_fail;

	ret = dlb2_resource_init(&dlb2->hw, dlb2->hw_ver);
	if (ret)
		goto resource_init_fail;

	ret = dlb2->ops->init_driver_state(dlb2);
	if (ret)
		goto init_driver_state_fail;

#ifdef CONFIG_INTEL_DLB2_DATAPATH
	dlb2_datapath_init(dlb2, dlb2->id);
#endif

	dlb2->ops->init_hardware(dlb2);

	/*
	 * Undo the 'get' operation by the PCI layer during probe and
	 * (if PF) immediately suspend the device. Since the device is only
	 * enabled when an application requests it, an autosuspend delay is
	 * likely not beneficial.
	 */
	pm_runtime_put_sync_suspend(&pdev->dev);

	/* Initialize dlb performance monitoring */
	if (dlb2->type != DLB2_PF)
		dev_info(&pdev->dev, "perf pmu not supported. Skipping perf init\n");
	else {
		ret = dlb2_perf_pmu_init(dlb2);
		if (ret < 0)
			dev_info(&pdev->dev, "[%s()] Failed to initialize dlb2_perf. \
					      No PMU support: %d\n", __func__, ret);
	}

aux_vf_probe:
	mutex_lock(&dlb2_driver_mutex);
	list_add(&dlb2->list, &dlb2_dev_list);
	mutex_unlock(&dlb2_driver_mutex);

	return 0;

init_driver_state_fail:
	dlb2_resource_free(&dlb2->hw);
resource_init_fail:
	dlb2->ops->free_interrupts(dlb2, pdev);
dlb2_resource_probe_fail:
init_interrupts_fail:
dlb2_reset_fail:
wait_for_device_ready_fail:
sysfs_create_fail:
dma_set_mask_fail:
	device_destroy(dlb2_class, dlb2->dev_number);
device_add_fail:
	cdev_del(&dlb2->cdev);
cdev_add_fail:
	dlb2->ops->unregister_driver(dlb2);
driver_registration_fail:
	dlb2->ops->unmap_pci_bar_space(dlb2, pdev);
map_pci_bar_fail:
#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE && !defined(DLB2_RHEL_GE_9_4))
	pci_disable_pcie_error_reporting(pdev);
#endif
	pci_release_regions(pdev);
pci_request_regions_fail:
	pci_disable_device(pdev);
pci_enable_device_fail:
	ida_free(&dlb2_ids, dlb2->id);
alloc_id_fail:
	return ret;
}

static void dlb2_remove(struct pci_dev *pdev)
{
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);

	mutex_lock(&dlb2_driver_mutex);
	list_del(&dlb2->list);
	mutex_unlock(&dlb2_driver_mutex);

	/* If this is an auxiliary VF, it skipped past most of the probe code */
	if (DLB2_IS_VF(dlb2) && dlb2->vf_id_state.is_auxiliary_vf)
		goto aux_vf_remove;

	/*
	 * Attempt to remove VFs before taking down the PF, since VFs cannot
	 * operate without a PF driver (in part because hardware doesn't
	 * support (CMD.MEM == 0 && IOV_CTRL.MSE == 1)).
	 */
	if (!pdev->is_virtfn && pci_num_vf(pdev) &&
	    dlb2_pci_sriov_configure(pdev, 0))
		dev_err(&pdev->dev,
			"Warning: DLB VFs will become unusable when the PF driver is removed\n");

	if (dlb2->type == DLB2_PF)
		dlb2_perf_pmu_remove(dlb2);

	/* Undo the PM operation in dlb2_probe(). */
	pm_runtime_get_noresume(&pdev->dev);

#ifdef CONFIG_INTEL_DLB2_DATAPATH
	dlb2_datapath_free(dlb2->id);
#endif

	dlb2->ops->free_driver_state(dlb2);

	dlb2_resource_free(&dlb2->hw);

	dlb2->ops->free_interrupts(dlb2, pdev);

	dlb2_release_device_memory(dlb2);

	device_destroy(dlb2_class, dlb2->dev_number);

	cdev_del(&dlb2->cdev);

aux_vf_remove:
	dlb2->ops->unregister_driver(dlb2);

	dlb2->ops->unmap_pci_bar_space(dlb2, pdev);

#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE && !defined(DLB2_RHEL_GE_9_4))
	pci_disable_pcie_error_reporting(pdev);
#endif

	pci_release_regions(pdev);

	pci_disable_device(pdev);

	ida_free(&dlb2_ids, dlb2->id);
}

static void dlb2_reset_hardware_state(struct dlb2 *dlb2, bool issue_flr)
{
	if (issue_flr)
		dlb2_reset_device(dlb2->pdev);

	/* Reinitialize interrupt configuration */
	dlb2->ops->reinit_interrupts(dlb2);

	/* Reset configuration done through the sysfs */
	dlb2->ops->sysfs_reapply(dlb2);

	/* Reinitialize any other hardware state */
	dlb2->ops->init_hardware(dlb2);
}

#ifdef CONFIG_PM
static int dlb2_runtime_suspend(struct device *dev)
{
	/* Return and let the PCI subsystem put the device in D3hot. */

	return 0;
}

static int dlb2_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);
	int ret;

	/*
	 * The PCI subsystem put the device in D0, but the device may not have
	 * completed powering up. Wait until the device is ready before
	 * proceeding.
	 */
	ret = dlb2->ops->wait_for_device_ready(dlb2, pdev);
	if (ret)
		return ret;

	/* Now reinitialize the device state. */
	dlb2_reset_hardware_state(dlb2, true);

	return 0;
}
#endif

static struct pci_device_id dlb2_id_table[] = {
	{ PCI_DEVICE_DATA(INTEL, DLB2_PF, DLB2_PF) },
	{ PCI_DEVICE_DATA(INTEL, DLB2_VF, DLB2_VF) },
	{ PCI_DEVICE_DATA(INTEL, DLB2_5_PF, DLB2_5_PF) },
	{ PCI_DEVICE_DATA(INTEL, DLB2_5_VF, DLB2_5_VF) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, dlb2_id_table);

#ifdef CONFIG_INTEL_DLB2_DATAPATH
void dlb2_register_dp_handle(struct dlb2_dp *dp)
{
	struct dlb2 *dlb2 = dp->dlb2;

	mutex_lock(&dlb2->resource_mutex);

	list_add(&dp->next, &dlb2->dp.hdl_list);

#ifdef CONFIG_PM
	pm_runtime_get_sync(&dlb2->pdev->dev);

	dp->pm_refcount = 1;
#endif

	mutex_unlock(&dlb2->resource_mutex);
}

static void dlb2_dec_dp_refcount(struct dlb2_dp *dp, struct dlb2 *dlb2)
{
#ifdef CONFIG_PM
	if (dp->pm_refcount) {
		/*
		 * Decrement the device's usage count and suspend it when
		 * the application stops using it.
		 */
		pm_runtime_put_sync_suspend(&dlb2->pdev->dev);

		dp->pm_refcount = 0;
	}
#endif
}

void dlb2_unregister_dp_handle(struct dlb2_dp *dp)
{
	struct dlb2 *dlb2 = dp->dlb2;

	mutex_lock(&dlb2->resource_mutex);

	list_del(&dp->next);

	dlb2_dec_dp_refcount(dp, dlb2);

	mutex_unlock(&dlb2->resource_mutex);
}

static void dlb2_disable_kernel_threads(struct dlb2 *dlb2)
{
	struct dlb2_dp *dp;
	int i;

	/*
	 * Kernel threads using DLB aren't killed, but are prevented from
	 * continuing to use their scheduling domain.
	 */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		if (!dlb2->sched_domains[i])
			continue;

		if (!dlb2->sched_domains[i]->user_mode &&
		    dlb2->sched_domains[i]->dp)
			dlb2->sched_domains[i]->dp->shutdown = true;
	}

	/*
	 * When the kernel thread calls dlb2_close(), it will unregister its
	 * handle and decrement the PM refcount. If even one of these kernel
	 * threads don't follow the correct shutdown procedure, though,
	 * the device's PM reference counting will be incorrect. So, we
	 * proactively decrement every datapath handle's refcount here.
	 */
	list_for_each_entry(dp, &dlb2->dp.hdl_list, next)
		dlb2_dec_dp_refcount(dp, dlb2);
}
#endif

static int dlb2_vdevs_in_use(struct dlb2 *dlb2)
{
	int i;

	/*
	 * For each VF with 1+ domains configured, query whether it is still in
	 * use, where "in use" is determined by the VF calling dlb2_in_use().
	 */

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		struct dlb2_get_num_resources_args used_rsrcs;

		dlb2_hw_get_num_used_resources(&dlb2->hw, &used_rsrcs, true, i);

		if (!used_rsrcs.num_sched_domains)
			continue;

		if (dlb2_vdev_in_use(&dlb2->hw, i))
			return true;
	}

	return false;
}

/* This function must be called with the resource_mutex held. */
static unsigned int dlb2_total_device_file_refcnt(struct dlb2 *dlb2)
{
	unsigned int cnt = 0;
	int i;

	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++)
		if (dlb2->sched_domains[i])
			cnt += kref_read(&dlb2->sched_domains[i]->refcnt);

	return cnt;
}

/* This function must be called with the resource_mutex held. */
bool dlb2_in_use(struct dlb2 *dlb2)
{
#ifdef CONFIG_INTEL_DLB2_DATAPATH
	return ((DLB2_IS_PF(dlb2) && dlb2_vdevs_in_use(dlb2)) ||
		(dlb2_total_device_file_refcnt(dlb2) != 0 ||
		!list_empty(&dlb2->dp.hdl_list)));
#else
	return ((DLB2_IS_PF(dlb2) && dlb2_vdevs_in_use(dlb2)) ||
		(dlb2_total_device_file_refcnt(dlb2) != 0));
#endif
}

/* This function must be called with the resource_mutex held. */
static void dlb2_wait_to_quiesce(struct dlb2 *dlb2)
{
	unsigned int i;

	for (i = 0; i < dlb2_reset_timeout_s * 10; i++) {
		/*
		 * Check for any application threads in the driver, extant
		 * mmaps, or open scheduling domain files.
		 */
		if (!dlb2_in_use(dlb2))
			return;

		mutex_unlock(&dlb2->resource_mutex);

		cond_resched();
		msleep(100);

		mutex_lock(&dlb2->resource_mutex);
	}

	dev_err(dlb2->dev,
		"PF driver timed out waiting for applications to stop\n");
}

void dlb2_unmap_all_mappings(struct dlb2 *dlb2)
{
	if (dlb2->inode)
		unmap_mapping_range(dlb2->inode->i_mapping, 0, 0, 1);
}

static void dlb2_disable_domain_files(struct dlb2 *dlb2)
{
	int i;

	/*
	 * Set all domain->valid flags to false to prevent existing device
	 * files from being used to enter the device driver.
	 */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		if (dlb2->sched_domains[i])
			dlb2->sched_domains[i]->valid = false;
	}
}

static void dlb2_wake_threads(struct dlb2 *dlb2)
{
	int i;

	/*
	 * Wake any blocked device file readers. These threads will return the
	 * DLB2_DOMAIN_ALERT_DEVICE_RESET alert, and well-behaved applications
	 * will close their fds and unmap DLB memory as a result.
	 */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		if (!dlb2->sched_domains[i])
			continue;

		wake_up_interruptible(&dlb2->sched_domains[i]->wq_head);
	}

	/* Wake threads blocked on a CQ interrupt */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++)
		dlb2_wake_thread(&dlb2->intr.ldb_cq_intr[i], WAKE_DEV_RESET);

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++)
		dlb2_wake_thread(&dlb2->intr.dir_cq_intr[i], WAKE_DEV_RESET);
}

void dlb2_stop_users(struct dlb2 *dlb2)
{
#ifdef CONFIG_INTEL_DLB2_DATAPATH
	/*
	 * Kernel datapath users are not force killed. Instead their domain's
	 * shutdown flag is set, which prevents them from continuing to use
	 * their scheduling domain. These kernel threads must clean up their
	 * current handles and create a new domain in order to keep using the
	 * DLB.
	 */
	dlb2_disable_kernel_threads(dlb2);
#endif

	/*
	 * Disable existing domain files to prevent applications from enter the
	 * device driver through file operations. (New files can't be opened
	 * while the resource mutex is held.)
	 */
	dlb2_disable_domain_files(dlb2);

	/* Wake any threads blocked in the kernel */
	dlb2_wake_threads(dlb2);
}

static void dlb2_reset_prepare(struct pci_dev *pdev)
{
	/*
	 * Unexpected FLR. Applications may be actively using the device at
	 * the same time, which poses two problems:
	 * - If applications continue to enqueue to the hardware they will
	 *   cause hardware errors, because the FLR will have reset the
	 *   scheduling domains, ports, and queues.
	 * - When the applications end, they must not trigger the driver's
	 *   domain reset code. The domain reset procedure would fail because
	 *   the device's registers will have been reset by the FLR.
	 *
	 * To avoid these problems, the driver handles unexpected resets as
	 * follows:
	 * 1. Set the reset_active flag. This flag blocks new device files
	 *    from being opened and is used as a wakeup condition in the
	 *    driver's wait queues.
	 * 2. If this is a PF FLR and there are active VFs, send them a
	 *    pre-reset notification, so they can stop any VF applications.
	 * 3. Disable all device files (set the per-file valid flag to false,
	 *    which prevents the file from being used after FLR completes) and
	 *    wake any threads on a wait queue.
	 * 4. If the DLB is not in use -- i.e. no open device files or memory
	 *    mappings, and no VFs in use (PF FLR only) -- the FLR can begin.
	 * 5. Else, the driver waits (up to a user-specified timeout, default
	 *    5s) for software to stop using the driver and the device. If the
	 *    timeout elapses, the driver zaps any remaining MMIO mappings.
	 *
	 * After the FLR:
	 * 1. Clear the per-domain pointers (the memory is freed in either
	 *    dlb2_close or dlb2_stop_users).
	 * 2. Release any remaining allocated port or CQ memory, now that it's
	 *    guaranteed the device is unconfigured and won't write to memory.
	 * 3. Reset software and hardware state
	 * 4. Set reset_active to false.
	 */

	struct dlb2 *dlb2 = pci_get_drvdata(pdev);

	mutex_lock(&dlb2->resource_mutex);

	/* Block any new device files from being opened */
	dlb2->reset_active = true;

	/*
	 * If the device has 1+ VFs, even if they're not in use, it will not be
	 * suspended. To avoid having to handle two cases (reset while device
	 * suspended and reset while device active), increment the device's PM
	 * refcnt here, to guarantee that the device is in D0 for the duration
	 * of the reset.
	 */
	pm_runtime_get_sync(&pdev->dev);

	/*
	 * Notify all registered VF drivers so they stop their applications
	 * from attempting to use the VF while the PF FLR is in progress.
	 */
	if (DLB2_IS_PF(dlb2)) {
		enum dlb2_mbox_vf_notification_type notif;
		int i;

		notif = DLB2_MBOX_VF_NOTIFICATION_PRE_RESET;

		for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
			if (dlb2_is_registered_vf(dlb2, i))
				dlb2_notify_vf(&dlb2->hw, i, notif);
		}
	}

	/*
	 * Stop existing applications from continuing to use the device by
	 * blocking kernel driver interfaces and waking any threads on wait
	 * queues, but don't zap VMA entries yet.
	 */
	dlb2_stop_users(dlb2);

	/* If no software is using the device, there's nothing to clean up. */
	if (!dlb2_in_use(dlb2))
		goto done;

	/*
	 * Wait until applications stop using the device or
	 * dlb2_reset_timeout_s seconds elapse. If the timeout occurs, zap any
	 * remaining VMA entries to guarantee applications can't reach the
	 * device.
	 */
	dlb2_wait_to_quiesce(dlb2);

	if (!dlb2_in_use(dlb2))
		goto done;

	dlb2_unmap_all_mappings(dlb2);

done:
	/*
	 * If the hypervisor traps VF PCI config space accesses such that the
	 * guest OS cannot trigger the VF FLR interrupt in the PF driver, the
	 * VF driver will request an FLR over the mailbox instead.
	 */
	if (dlb2->needs_mbox_reset && dlb2->ops->mbox_dev_reset(dlb2))
		dev_err(dlb2->dev,
			"Reset failed, and the device may be unusable. Reload the dlb2 driver to recover.\n");

	/*
	 * Don't release resource_mutex until after the FLR occurs. This
	 * prevents applications from accessing the device during reset.
	 */
}

static void dlb2_reset_done(struct pci_dev *pdev)
{
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);
	int i;

	/*
	 * Clear all domain pointers, to be filled in by post-FLR applications
	 * using the device driver.
	 *
	 * Note that domain memory isn't leaked -- it is either freed during
	 * dlb2_stop_users() or in the file close callback.
	 */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++)
		dlb2->sched_domains[i] = NULL;

	/*
	 * Free allocated CQ memory. These are no longer accessible to
	 * user-space: either the applications closed, or their mappings were
	 * zapped in dlb2_reset_prepare().
	 */
	dlb2_release_device_memory(dlb2);

	/* Reset interrupt state */
	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++)
		dlb2->intr.ldb_cq_intr[i].configured = false;
	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver); i++)
		dlb2->intr.dir_cq_intr[i].configured = false;

	/* Reset resource allocation state */
	dlb2_resource_reset(&dlb2->hw);

	/* Reset the hardware state, but don't issue an additional FLR */
	dlb2_reset_hardware_state(dlb2, false);

	/*
	 * VF reset is a software procedure that can take > 100ms (on
	 * emulation). The PCIe spec mandates that a VF FLR will not take more
	 * than 100ms, so Linux simply sleeps for that long. If this function
	 * releases the resource mutex and allows another mailbox request to
	 * occur while the VF is still being reset, undefined behavior can
	 * result. Hence, this function waits until the PF indicates that the
	 * VF reset is done.
	 */
	if (DLB2_IS_VF(dlb2)) {
		int retry_cnt;

		/*
		 * Timeout after DLB2_VF_FLR_DONE_POLL_TIMEOUT_MS of
		 * inactivity, sleep-polling every
		 * DLB2_VF_FLR_DONE_SLEEP_PERIOD_MS.
		 */
		retry_cnt = 0;
		while (!dlb2_vf_flr_complete(&dlb2->hw)) {
			unsigned long sleep_us;

			sleep_us = DLB2_VF_FLR_DONE_SLEEP_PERIOD_MS * 1000;

			usleep_range(sleep_us, sleep_us + 1);

			if (++retry_cnt >= DLB2_VF_FLR_DONE_POLL_TIMEOUT_MS) {
				dev_err(dlb2->dev,
					"VF driver timed out waiting for FLR response\n");
				break;
			}
		}
	}

	dlb2->domain_reset_failed = false;

	dlb2->reset_active = false;

	/* Undo the PM refcnt increment in dlb2_reset_prepare(). */
	pm_runtime_put_sync_suspend(&pdev->dev);

	mutex_unlock(&dlb2->resource_mutex);
}

#if KERNEL_VERSION(4, 13, 0) >= LINUX_VERSION_CODE
static void dlb2_reset_notify(struct pci_dev *pdev, bool prepare)
{
	if (prepare)
		dlb2_reset_prepare(pdev);
	else
		dlb2_reset_done(pdev);
}
#endif

static const struct pci_error_handlers dlb2_err_handler = {
#if KERNEL_VERSION(4, 13, 0) >= LINUX_VERSION_CODE
	.reset_notify  = dlb2_reset_notify,
#else
	.reset_prepare = dlb2_reset_prepare,
	.reset_done    = dlb2_reset_done,
#endif
};

#ifdef CONFIG_PM
static const struct dev_pm_ops dlb2_pm_ops = {
	SET_RUNTIME_PM_OPS(dlb2_runtime_suspend, dlb2_runtime_resume, NULL)
};
#endif

static struct pci_driver dlb2_pci_driver = {
	.name		 = dlb2_driver_name,
	.id_table	 = dlb2_id_table,
	.probe		 = dlb2_probe,
	.remove		 = dlb2_remove,
#ifdef CONFIG_PM
	.driver.pm	 = &dlb2_pm_ops,
#endif
	.sriov_configure = dlb2_pci_sriov_configure,
	.err_handler     = &dlb2_err_handler,
};

static int __init dlb2_init_module(void)
{
	int err;

#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE && !defined(DLB2_RHEL_GE_9_4))
	dlb2_class = class_create(THIS_MODULE, dlb2_driver_name);
#else
	dlb2_class = class_create(dlb2_driver_name);
#endif

	if (IS_ERR(dlb2_class)) {
		pr_err("%s: class_create() returned %ld\n",
		       dlb2_driver_name, PTR_ERR(dlb2_class));

		return PTR_ERR(dlb2_class);
	}

	err = alloc_chrdev_region(&dlb2_devt,
				  0,
				  DLB2_MAX_NUM_DEVICES,
				  dlb2_driver_name);

	if (err < 0) {
		pr_err("%s: alloc_chrdev_region() returned %d\n",
		       dlb2_driver_name, err);

		goto alloc_chrdev_fail;
	}

	/* Setup dlb2_perf by enabling cpu hotplug support. This
	 * allows choosing the first available cpu to read perf counters.
	 */
	dlb2_perf_init();

	err = pci_register_driver(&dlb2_pci_driver);
	if (err < 0) {
		pr_err("%s: pci_register_driver() returned %d\n",
		       dlb2_driver_name, err);

		goto pci_register_fail;
	}

	return 0;

pci_register_fail:
	unregister_chrdev_region(dlb2_devt, DLB2_MAX_NUM_DEVICES);
alloc_chrdev_fail:
	class_destroy(dlb2_class);

	return err;
}

static void __exit dlb2_exit_module(void)
{
	pci_unregister_driver(&dlb2_pci_driver);

	dlb2_perf_exit();

	unregister_chrdev_region(dlb2_devt, DLB2_MAX_NUM_DEVICES);

	class_destroy(dlb2_class);
}

module_init(dlb2_init_module);
module_exit(dlb2_exit_module);
