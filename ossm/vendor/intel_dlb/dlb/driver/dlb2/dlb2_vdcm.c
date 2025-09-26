// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2018-2020 Intel Corporation

#include <linux/eventfd.h>
#include <linux/pci-ats.h>
#include <linux/pm_runtime.h>
#include <linux/mdev.h>
#include <linux/msi.h>
#include <linux/version.h>
#include <linux/vfio.h>

#include "dlb2_vdcm.h"
#include "base/dlb2_regs.h"
#include "base/dlb2_resource.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_mbox.h"
#include "dlb2_main.h"

#ifdef CONFIG_INTEL_DLB2_SIOV

#ifndef DLB2_SIOV_IMS_WORKAROUND
#include <linux/irqchip/irq-ims-msi.h>
#endif

#ifdef DLB2_NEW_MDEV_IOMMUFD
MODULE_IMPORT_NS(IOMMUFD);
#endif

#ifdef DLB2_NEW_MDEV_IOMMUFD
struct vdcm_hwpt {
	ioasid_t pasid;
	u32	 hwpt_id;
};

static const struct vfio_device_ops dlb2_vdcm_ops;
#endif

static u64 dlb2_pci_config[] = {
	0x0010000027118086ULL, /* 0x00-0x40: PCI config header */
	0x000000000b400000ULL,
	0x000000000000000cULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL,
	0x0000808600000000ULL,
	0x0000006000000000ULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL, /* 0x40-0x60: unused */
	0x0000000000000000ULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL,
	0x0100000000406c11ULL, /* 0x60-0x6C: MSI-X Capability */
	0x0002001001001000ULL, /* 0x6C-0xB0: PCIe Capability */
	0x0000291010008062ULL,
	0x1011000000400c11ULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL,
	0x0000000000700010ULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL,
	0x0000000000000000ULL,
};

#define KB		    1024
#define MB		    (1024 * KB)
#define DLB2_VDEV_BAR0_SIZE (64 * MB)

#ifndef DLB2_NEW_MDEV_IOMMUFD
/**********************************/
/****** Supported type attrs ******/
/**********************************/

static struct attribute *dlb2_mdev_types_attrs[] = {
	NULL,
};

static struct attribute_group dlb2_mdev_type_group = {
	.name = "dlb",
	.attrs = dlb2_mdev_types_attrs,
};

static struct attribute_group *dlb2_mdev_type_groups[] = {
	&dlb2_mdev_type_group,
	NULL,
};
#endif

static ssize_t num_sched_domains_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_sched_domains + used.num_sched_domains;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_sched_domains_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	unsigned long num;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_sched_domains(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_ldb_queues_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_ldb_queues + used.num_ldb_queues;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_ldb_queues_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	unsigned long num;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_ldb_queues(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_ldb_ports_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_ldb_ports + used.num_ldb_ports;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_ldb_ports_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	unsigned long num;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_ldb_ports(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

#define DLB2_COS_LDB_PORTS_SHOW(cos_id)					       \
static ssize_t num_cos##cos_id##_ldb_ports_show(struct device *dev,	       \
						struct device_attribute *attr, \
						char *buf)		       \
{									       \
	struct dlb2_get_num_resources_args avail, used;			       \
	struct dlb2_vdev *vdev;						       \
	struct dlb2_hw *hw;						       \
	struct dlb2 *dlb2;						       \
	int val;							       \
									       \
	vdev = dlb2_dev_get_drvdata(dev);					       \
	dlb2 = mdev_get_dlb2(vdev->mdev);			       \
	hw = &dlb2->hw;							       \
									       \
	mutex_lock(&dlb2->resource_mutex);				       \
									       \
	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);	       \
	if (val) {							       \
		mutex_unlock(&dlb2->resource_mutex);			       \
		return -1;						       \
	}								       \
									       \
	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);       \
	if (val) {							       \
		mutex_unlock(&dlb2->resource_mutex);			       \
		return -1;						       \
	}								       \
									       \
	mutex_unlock(&dlb2->resource_mutex);				       \
									       \
	val = avail.num_cos_ldb_ports[cos_id] + used.num_cos_ldb_ports[cos_id];\
									       \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);			       \
}

DLB2_COS_LDB_PORTS_SHOW(0)
DLB2_COS_LDB_PORTS_SHOW(1)
DLB2_COS_LDB_PORTS_SHOW(2)
DLB2_COS_LDB_PORTS_SHOW(3)

#define DLB2_COS_LDB_PORTS_STORE(cos_id)				 \
static ssize_t num_cos##cos_id##_ldb_ports_store(struct device *dev,	 \
					struct device_attribute *attr,	 \
					const char *buf,		 \
					size_t count)			 \
{									 \
	struct dlb2_vdev *vdev;						 \
	struct dlb2_hw *hw;						 \
	unsigned long num;						 \
	struct dlb2 *dlb2;						 \
	int ret;							 \
									 \
	ret = kstrtoul(buf, 0, &num);					 \
	if (ret)							 \
		return -1;						 \
									 \
	vdev = dlb2_dev_get_drvdata(dev);					 \
	dlb2 = mdev_get_dlb2(vdev->mdev);			 \
	hw = &dlb2->hw;							 \
									 \
	mutex_lock(&dlb2->resource_mutex);				 \
									 \
	ret = dlb2_update_vdev_ldb_cos_ports(hw, vdev->id, cos_id, num); \
									 \
	mutex_unlock(&dlb2->resource_mutex);				 \
									 \
	if (ret == 0)							 \
		ret = count;						 \
									 \
	return ret;							 \
}

DLB2_COS_LDB_PORTS_STORE(0)
DLB2_COS_LDB_PORTS_STORE(1)
DLB2_COS_LDB_PORTS_STORE(2)
DLB2_COS_LDB_PORTS_STORE(3)

static ssize_t num_dir_ports_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2 *dlb2;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_dir_ports + used.num_dir_ports;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_dir_ports_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	unsigned long num;
	struct dlb2 *dlb2;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_dir_ports(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_ldb_credits_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_ldb_credits + used.num_ldb_credits;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_ldb_credits_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	unsigned long num;
	struct dlb2 *dlb2;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_ldb_credits(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_dir_credits_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_dir_credits + used.num_dir_credits;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_dir_credits_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	unsigned long num;
	struct dlb2 *dlb2;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_dir_credits(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_hist_list_entries_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_hist_list_entries + used.num_hist_list_entries;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_hist_list_entries_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	unsigned long num;
	struct dlb2 *dlb2;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_hist_list_entries(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

static ssize_t num_atomic_inflights_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct dlb2_get_num_resources_args avail, used;
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);
	if (val) {
		mutex_unlock(&dlb2->resource_mutex);
		return -1;
	}

	mutex_unlock(&dlb2->resource_mutex);

	val = avail.num_atomic_inflights + used.num_atomic_inflights;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t num_atomic_inflights_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t count)
{
	struct dlb2_vdev *vdev;
	struct dlb2_hw *hw;
	unsigned long num;
	struct dlb2 *dlb2;
	int ret;

	ret = kstrtoul(buf, 0, &num);
	if (ret)
		return -1;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);
	hw = &dlb2->hw;

	mutex_lock(&dlb2->resource_mutex);

	ret = dlb2_update_vdev_atomic_inflights(hw, vdev->id, num);

	mutex_unlock(&dlb2->resource_mutex);

	if (ret == 0)
		ret = count;

	return ret;
}

#define DLB2_SN_SLOTS_SHOW(group_id)					    \
static ssize_t num_sn##group_id##_slots_show(struct device *dev,	    \
					     struct device_attribute *attr, \
					     char *buf)			    \
{									    \
	struct dlb2_get_num_resources_args avail, used;			    \
	struct dlb2_vdev *vdev;						    \
	struct dlb2_hw *hw;						    \
	struct dlb2 *dlb2;						    \
	int val;							    \
									    \
	vdev = dlb2_dev_get_drvdata(dev);			    \
	dlb2 = mdev_get_dlb2(vdev->mdev);			    \
	hw = &dlb2->hw;							    \
									    \
	mutex_lock(&dlb2->resource_mutex);				    \
									    \
	val = dlb2_hw_get_num_resources(hw, &avail, true, vdev->id);	    \
	if (val) {							    \
		mutex_unlock(&dlb2->resource_mutex);			    \
		return -1;						    \
	}								    \
									    \
	val = dlb2_hw_get_num_used_resources(hw, &used, true, vdev->id);    \
	if (val) {							    \
		mutex_unlock(&dlb2->resource_mutex);			    \
		return -1;						    \
	}								    \
									    \
	mutex_unlock(&dlb2->resource_mutex);				    \
									    \
	val = avail.num_sn_slots[group_id] + used.num_sn_slots[group_id];   \
									    \
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);			    \
}									    \

#define DLB2_SN_SLOTS_STORE(group_id)					    \
static ssize_t num_sn##group_id##_slots_store(struct device *dev,	    \
					      struct device_attribute *attr,\
					      const char *buf,		    \
					      size_t count)		    \
{									    \
	struct dlb2_vdev *vdev;						    \
	struct dlb2_hw *hw;						    \
	unsigned long num;						    \
	struct dlb2 *dlb2;						    \
	int ret;							    \
									    \
	ret = kstrtoul(buf, 0, &num);					    \
	if (ret)							    \
		return -1;						    \
									    \
	vdev = dlb2_dev_get_drvdata(dev);			    \
	dlb2 = mdev_get_dlb2(vdev->mdev);			    \
	hw = &dlb2->hw;							    \
									    \
	mutex_lock(&dlb2->resource_mutex);				    \
									    \
	ret = dlb2_update_vdev_sn_slots(hw, vdev->id, group_id, num);	    \
									    \
	mutex_unlock(&dlb2->resource_mutex);				    \
									    \
	if (ret == 0)							    \
		ret = count;						    \
									    \
	return ret;							    \
}									    \

static ssize_t locked_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct dlb2_vdev *vdev;
	struct dlb2 *dlb2;
	int val;

	vdev = dlb2_dev_get_drvdata(dev);
	dlb2 = mdev_get_dlb2(vdev->mdev);

	val = (int)dlb2_vdev_is_locked(&dlb2->hw, vdev->id);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

DLB2_SN_SLOTS_SHOW(0)
DLB2_SN_SLOTS_SHOW(1)
DLB2_SN_SLOTS_STORE(0)
DLB2_SN_SLOTS_STORE(1)

static DEVICE_ATTR_RW(num_sched_domains);
static DEVICE_ATTR_RW(num_ldb_queues);
static DEVICE_ATTR_RW(num_ldb_ports);
static DEVICE_ATTR_RW(num_cos0_ldb_ports);
static DEVICE_ATTR_RW(num_cos1_ldb_ports);
static DEVICE_ATTR_RW(num_cos2_ldb_ports);
static DEVICE_ATTR_RW(num_cos3_ldb_ports);
static DEVICE_ATTR_RW(num_dir_ports);
static DEVICE_ATTR_RW(num_ldb_credits);
static DEVICE_ATTR_RW(num_dir_credits);
static DEVICE_ATTR_RW(num_hist_list_entries);
static DEVICE_ATTR_RW(num_atomic_inflights);
static DEVICE_ATTR_RW(num_sn0_slots);
static DEVICE_ATTR_RW(num_sn1_slots);
static DEVICE_ATTR_RO(locked);

static struct attribute *dlb2_mdev_attrs[] = {
	&dev_attr_num_sched_domains.attr,
	&dev_attr_num_ldb_queues.attr,
	&dev_attr_num_ldb_ports.attr,
	&dev_attr_num_cos0_ldb_ports.attr,
	&dev_attr_num_cos1_ldb_ports.attr,
	&dev_attr_num_cos2_ldb_ports.attr,
	&dev_attr_num_cos3_ldb_ports.attr,
	&dev_attr_num_dir_ports.attr,
	&dev_attr_num_ldb_credits.attr,
	&dev_attr_num_dir_credits.attr,
	&dev_attr_num_hist_list_entries.attr,
	&dev_attr_num_atomic_inflights.attr,
	&dev_attr_num_sn0_slots.attr,
	&dev_attr_num_sn1_slots.attr,
	&dev_attr_locked.attr,
	NULL,
};

static const struct attribute_group dlb2_mdev_attr_group = {
	.name = "dlb2_mdev",
	.attrs = dlb2_mdev_attrs,
};

static const struct attribute_group *dlb2_mdev_attr_groups[] = {
	&dlb2_mdev_attr_group,
	NULL,
};

/****************************/
/****** mdev callbacks ******/
/****************************/

static int dlb2_alloc_vdev_id(struct dlb2 *dlb2)
{
	return ida_alloc_max(&dlb2->vdev_ids,
			     DLB2_MAX_NUM_VDEVS - 1, GFP_KERNEL);
}

static void dlb2_free_vdev_id(struct dlb2 *dlb2, int id)
{
	ida_free(&dlb2->vdev_ids, id);
}

static unsigned int dlb2_vdcm_num_cq_irqs(struct dlb2_vdev *vdev)
{
	/* 1 interrupt per CQ */
	return vdev->num_ldb_ports + vdev->num_dir_ports;
}

static unsigned int dlb2_vdcm_num_irqs(struct dlb2_vdev *vdev)
{
	/* 1 mailbox interrupt in addition to the CQ interrupts */
	return 1 + dlb2_vdcm_num_cq_irqs(vdev);
}

/* Return true if either the vector or the function is masked */
static bool vdcm_msix_is_masked(struct dlb2_vdev *vdev, int vector)
{
	u8 *entry = &vdev->msix_table[vector * VDCM_MSIX_TBL_ENTRY_SZ];
	u16 msg_ctrl = vdev->cfg[VDCM_MSIX_MSG_CTRL_OFFSET];

	if ((entry[PCI_MSIX_ENTRY_VECTOR_CTRL] & PCI_MSIX_ENTRY_CTRL_MASKBIT) ||
	    (msg_ctrl & PCI_MSIX_FLAGS_MASKALL))
		return true;

	return false;
}

static int vdcm_send_interrupt(struct dlb2_vdev *vdev, int vector)
{
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	int ret = -1;

	if (!vdev->msix_eventfd[vector]) {
		dev_err(dev, "[%s()] vector %d's eventfd not found\n",
			__func__, vector);
		return -EINVAL;
	}

	ret = eventfd_signal(vdev->msix_eventfd[vector], 1);

	dev_dbg(dev, "[%s()] vector %d interrupt triggered\n",
		__func__, vector);

	if (ret != 1)
		dev_err(dev, "[%s()] vector %d eventfd signal failed\n",
			__func__, vector);

	return ret;
}

#ifndef DLB2_SIOV_IMS_WORKAROUND
static irqreturn_t dlb2_vdcm_cq_isr(int irq, void *data)
{
	struct dlb2_ims_irq_entry *irq_entry = data;
	struct dlb2_vdev *vdev = irq_entry->vdev;
	int msix_idx = irq_entry->int_src + 1; /* +1 due to mailbox vector */
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	if (vdcm_msix_is_masked(vdev, msix_idx))
		set_bit(msix_idx, (unsigned long *)vdev->msix_pba);
	else
		vdcm_send_interrupt(vdev, msix_idx);

	return IRQ_HANDLED;
}

#else

static irqreturn_t dlb2_vdcm_cq_isr(int irq, void *data)
{
	u32 dir_cq_interrupts_vdev[DLB2_MAX_NUM_DIR_PORTS_V2_5 / 32] = {0};
	u32 dir_cq_interrupts_all[DLB2_MAX_NUM_DIR_PORTS_V2_5 / 32] = {0};
	u32 ldb_cq_interrupts_vdev[DLB2_MAX_NUM_LDB_PORTS / 32] = {0};
	u32 ldb_cq_interrupts_all[DLB2_MAX_NUM_LDB_PORTS / 32] = {0};
	struct dlb2_ims_irq_entry *irq_entry = data;
	struct dlb2_vdev *vdev = irq_entry->vdev;
	u32 idx, port_id, mask;
	struct dlb2 *dlb2;
	int i;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	dlb2_read_compressed_cq_intr_status(&dlb2->hw,
					    ldb_cq_interrupts_all,
					    dir_cq_interrupts_all);

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS / 32; i++)
		ldb_cq_interrupts_vdev[i] = ldb_cq_interrupts_all[i] &
					    vdev->ldb_ports_mask[i];

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS_V2_5 / 32; i++)
		dir_cq_interrupts_vdev[i] = dir_cq_interrupts_all[i] &
					    vdev->dir_ports_mask[i];

	dlb2_ack_compressed_cq_intr(&dlb2->hw,
				    ldb_cq_interrupts_vdev,
				    dir_cq_interrupts_vdev);

	for (i = 0; i < vdev->num_ldb_ports; i++) {
		port_id = vdev->ldb_ports_phys_id[i];

		mask = 1 << (port_id % 32);
		idx = port_id / 32;

		if (!(ldb_cq_interrupts_vdev[idx] & mask))
			continue;

		vdcm_send_interrupt(vdev, i + 1);
	}

	for (i = 0; i < vdev->num_dir_ports; i++) {
		port_id = vdev->dir_ports_phys_id[i];

		mask = 1 << (port_id % 32);
		idx = port_id / 32;

		if (!(dir_cq_interrupts_vdev[idx] & mask))
			continue;

		vdcm_send_interrupt(vdev, i + 1 + vdev->num_ldb_ports);
	}

	return IRQ_HANDLED;
}
#endif

#ifndef DLB2_SIOV_IMS_WORKAROUND
static int dlb2_vdcm_alloc_ims_irq_vectors(struct dlb2_vdev *vdev)
{
	struct dlb2_ims_irq_entry *irq_entry;
	struct device *dev, *parent_dev;
	struct msi_desc *desc;
	unsigned int nvec, i;
	struct dlb2 *dlb2;
	int ret;

	dev = mdev_dev(vdev->mdev);
	parent_dev = dlb2_mdev_parent_dev(vdev->mdev);

	nvec = dlb2_vdcm_num_cq_irqs(vdev);

	dlb2 = mdev_get_dlb2(vdev->mdev);

	/* IMS is not support in DLB 2.0 */
	if (dlb2->hw_ver < DLB2_HW_V2_5)
		return -EINVAL;

	dev_set_msi_domain(dev, dlb2->ims_domain);

	ret = msi_domain_alloc_irqs(dlb2->ims_domain, dev, nvec);
	if (ret < 0) {
		dev_err(parent_dev,
			"[%s()] failed to allocate %d ims irqs (%d)\n",
			__func__, nvec, ret);
		goto alloc_ims_fail;
	}

	for (i = 0; i < nvec; i++) {
		bool is_ldb = i < vdev->num_ldb_ports;

		irq_entry = &vdev->irq_entries[i];

		irq_entry->vdev = vdev;
		irq_entry->int_src = i;
		irq_entry->is_ldb = is_ldb;
		irq_entry->reenable = false;
		irq_entry->in_use = false;
		irq_entry->cq_id = is_ldb ? i : i - vdev->num_ldb_ports;
	}

	i = 0;
#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
	for_each_dev_msi_entry(desc, dev) {
#else
	for_each_msi_entry(desc, dev) {
#endif
		irq_entry = &vdev->irq_entries[i];

		ret = request_threaded_irq(desc->irq,
					   NULL, dlb2_vdcm_cq_isr,
					   IRQF_ONESHOT,
					   "dlb2-ims", irq_entry);
		if (ret)
			goto request_irq_fail;

		irq_entry->in_use = true;
		irq_entry->irq = desc->irq;
		vdev->ims_idx[i] = desc->device_msi.hwirq;

		i++;
	}

	return 0;

request_irq_fail:
	irq_entry = &vdev->irq_entries[0];
	msi_domain_free_irqs(dlb2->ims_domain, dev);
alloc_ims_fail:
	return ret;
}

static void dlb2_vdcm_free_ims_irq_vectors(struct dlb2_vdev *vdev)
{
	struct device *dev = mdev_dev(vdev->mdev);
	struct dlb2_ims_irq_entry *irq_entry;
	struct msi_desc *desc;
	int i;

	i = 0;
#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
	for_each_dev_msi_entry(desc, dev) {
#else
	for_each_msi_entry(desc, dev) {
#endif
		irq_entry = &vdev->irq_entries[i];

		irq_entry->in_use = false;
		free_irq(desc->irq, irq_entry);
		i++;
	}

	msi_domain_free_irqs(dev_get_msi_domain(dev), dev);
}

#else

static int dlb2_vdcm_alloc_ims_irq_vectors(struct dlb2_vdev *vdev)
{
	struct dlb2_ims_irq_entry *irq_entry;
	u32 mask, idx;
	int port_id;
	struct device *dev;
	struct dlb2 *dlb2;
	unsigned int i;
	int ret, irq;

	dev = mdev_dev(vdev->mdev);

	dlb2 = mdev_get_dlb2(vdev->mdev);

	for (i = 0; i < vdev->num_ldb_ports; i++) {
		port_id = dlb2_hw_get_ldb_port_phys_id(&dlb2->hw, i, vdev->id);
		if (port_id < 0)
			return EINVAL;

		mask = 1 << (port_id % 32);
		idx = port_id / 32;
		vdev->ldb_ports_mask[idx] |= mask;
		vdev->ldb_ports_phys_id[i] = port_id;

		irq_entry = &vdev->irq_entries[i];

		irq_entry->vdev = vdev;
		irq_entry->int_src = i;
		irq_entry->is_ldb = true;
		irq_entry->reenable = false;
		irq_entry->in_use = false;
		irq_entry->cq_id = i;
	}

	for (i = 0; i < vdev->num_dir_ports; i++) {
		port_id = dlb2_hw_get_dir_port_phys_id(&dlb2->hw, i, vdev->id);
		if (port_id < 0)
			return EINVAL;

		mask = 1 << (port_id % 32);
		idx = port_id / 32;
		vdev->dir_ports_mask[idx] |= mask;
		vdev->dir_ports_phys_id[i] = port_id;

		irq_entry = &vdev->irq_entries[i + vdev->num_ldb_ports];

		irq_entry->vdev = vdev;
		irq_entry->int_src = i + vdev->num_ldb_ports;
		irq_entry->is_ldb = false;
		irq_entry->reenable = false;
		irq_entry->in_use = false;
		irq_entry->cq_id = i;
	}

	irq_entry = &vdev->irq_entries[0];

	irq = pci_irq_vector(dlb2->pdev, DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID
			     + 1 + vdev->id);

	ret = request_threaded_irq(irq,
				   NULL, dlb2_vdcm_cq_isr,
				   IRQF_ONESHOT,
				   "dlb2-ims", irq_entry);
	if (ret)
		goto request_irq_fail;

	irq_entry->in_use = true;

	return 0;

request_irq_fail:
	irq_entry = &vdev->irq_entries[0];

	return ret;
}

static void dlb2_vdcm_free_ims_irq_vectors(struct dlb2_vdev *vdev)
{
	struct dlb2_ims_irq_entry *irq_entry;
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	irq_entry = &vdev->irq_entries[0];

	free_irq(pci_irq_vector(dlb2->pdev,
				DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID
				+ 1 + vdev->id),
				irq_entry);
	irq_entry->in_use = false;
}
#endif /* !DLB2_SIOV_IMS_WORKAROUND */

static struct dlb2_vdev *dlb2_vdev_create(struct dlb2 *dlb2,
					  struct mdev_device *mdev)
{
	struct dlb2_vdev *vdev;
	int id;

#ifndef DLB2_NEW_MDEV_IOMMUFD
	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
#else
	vdev = vfio_alloc_device(dlb2_vdev, vfio_dev,
				 mdev_dev(mdev), &dlb2_vdcm_ops);
#endif
	if (!vdev)
		return NULL;

	id = dlb2_alloc_vdev_id(dlb2);

	if (id < 0) {
		kfree(vdev);
		return NULL;
	}

	BUILD_BUG_ON(sizeof(vdev->cfg) < sizeof(dlb2_pci_config));

	memcpy(vdev->cfg, dlb2_pci_config, sizeof(dlb2_pci_config));

	/* Set VDEV Device ID (2715) for DLB 2.5 */
	if (dlb2->hw_ver == DLB2_HW_V2_5)
		*((u32 *)vdev->cfg) = 0x27158086UL;

	vdev->id = id;

	return vdev;
}

static void __dlb2_vdcm_release(struct dlb2_vdev *vdev)
{
	struct device *dev;
	struct dlb2 *dlb2;
#ifndef DLB2_NEW_MDEV_IOMMUFD
	int ret;
#endif

	dlb2 = mdev_get_dlb2(vdev->mdev);
	dev = dlb2_mdev_parent_dev(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	if (vdev->released) {
		mutex_unlock(&dlb2->resource_mutex);
		return;
	}

	vdev->released = true;

#ifdef DLB2_NEW_MDEV_IOMMUFD
	vfio_device_set_pasid(&vdev->vfio_dev, IOMMU_PASID_INVALID);
	ioasid_put(NULL, vdev->pasid);
#endif

	/*
	 * There's no guarantee the VM exited cleanly, so reset the VDEV before
	 * releasing it. If the VDEV was reset, this function will detect that
	 * and return early.
	 */
	dlb2_reset_vdev(&dlb2->hw, vdev->id);

	dlb2_vdcm_free_ims_irq_vectors(vdev);

#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN) && !defined(DLB2_NEW_MDEV_IOMMUFD)
	if (vdev->vfio_group) {
		vfio_group_put_external_user(vdev->vfio_group);
		vdev->vfio_group = NULL;
	}
#endif

	dlb2_unlock_vdev(&dlb2->hw, vdev->id);

	dlb2_hw_unregister_sw_mbox(&dlb2->hw, vdev->id);

	free_page((unsigned long)vdev->pf_to_vdev_mbox);
	free_page((unsigned long)vdev->vdev_to_pf_mbox);

#ifndef DLB2_NEW_MDEV_IOMMUFD
	ret = vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_GROUP_NOTIFY,
				       &vdev->group_notifier);
	WARN(ret, "vfio_unregister_notifier group failed: %d\n", ret);

	ret = vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_IOMMU_NOTIFY,
				       &vdev->iommu_notifier);
	WARN(ret, "vfio_unregister_notifier iommu failed: %d\n", ret);
#endif

	/*
	 * Decrement the device's usage count and suspend it if the
	 * count reaches zero.
	 */
	pm_runtime_put_sync_suspend(dev);

	mutex_unlock(&dlb2->resource_mutex);
}

static void dlb2_vdcm_release_work(struct work_struct *work)
{
	struct dlb2_vdev *vdev;

	vdev = container_of(work, struct dlb2_vdev, release_work);

	__dlb2_vdcm_release(vdev);
}

static void dlb2_vdcm_send_unmasked_interrupts(struct dlb2_vdev *vdev)
{
	int i;

	for (i = 0; i < VDCM_MSIX_MAX_ENTRIES; i++) {
		if (!vdcm_msix_is_masked(vdev, i) &&
		    test_and_clear_bit(i, (unsigned long *)vdev->msix_pba))
			vdcm_send_interrupt(vdev, i);
	}
}

static void dlb2_trigger_mbox_interrupt(void *arg)
{
	struct dlb2_vdev *vdev = arg;

	/* Caller is expected to hold the resource_mutex */

	if (vdcm_msix_is_masked(vdev, VDCM_MBOX_MSIX_VECTOR))
		set_bit(VDCM_MBOX_MSIX_VECTOR, (unsigned long *)vdev->msix_pba);
	else
		vdcm_send_interrupt(vdev, VDCM_MBOX_MSIX_VECTOR);
}

/*
 * Temporary workaround. Derived from pci_enable_pasid(), minus the end-to-end
 * TLP prefix capability error checking.
 */
static int dlb2_enable_pasid(struct pci_dev *pdev)
{
	int pos, ret;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_PASID);
	if (!pos)
		return -EINVAL;

	pdev->pasid_features = 0;

	ret = pci_write_config_word(pdev, pos + PCI_PASID_CTRL,
				    PCI_PASID_CTRL_ENABLE);

	if (ret) {
		dev_err(&pdev->dev,
			"[%s()] Failed to write PCI configuration register: %d\n",
			__func__, ret);

		return ret;
	}

	pdev->pasid_enabled = 1;
#if KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE
	pdev->pasid_cap = pos;
#endif

	return 0;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static int dlb2_vdcm_probe(struct mdev_device *mdev)
#elif KERNEL_VERSION(5, 13, 0) <= LINUX_VERSION_CODE
static int dlb2_vdcm_create(struct mdev_device *mdev)
#else
static int dlb2_vdcm_create(struct kobject *kobj, struct mdev_device *mdev)
#endif
{
	struct dlb2_vdev *vdev;
	struct pci_dev *pdev;
	struct device *dev;
	struct dlb2 *dlb2;
	int ret;

	dev = dlb2_mdev_parent_dev(mdev);
	dlb2 = mdev_get_dlb2(mdev);
	pdev = mdev_get_pdev(mdev);

	mutex_lock(&dlb2->resource_mutex);

	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SRIOV) {
		ret = -EINVAL;
		dev_err(dev, "SRIOV mode in VDCM!\n");
		goto virt_mode_err;
	}

	if (dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_SIOV)) {
		ret = -EINVAL;
		dev_err(dev, "Failed to set Scalable IOV mode\n");
		goto virt_mode_err;
	}

	/*
	 * Indicate to the mdev layer that this device uses IOMMU-provided
	 * isolation and protection through the DLB PCI device. Each DLB mdev
	 * receives its own IOMMU domain, identified by a unique PASID.
	 */
#ifndef DLB2_NEW_MDEV_IOMMUFD
#if KERNEL_VERSION(5, 13, 0) <= LINUX_VERSION_CODE
	mdev_set_iommu_device(mdev, dlb2_mdev_parent_dev(mdev));
#else
	mdev_set_iommu_device(mdev_dev(mdev), dlb2_mdev_parent_dev(mdev));
#endif
#endif

	if (list_empty(&dlb2->vdev_list) && !pdev->pasid_enabled) {
		ret = pci_enable_pasid(pdev, 0);
		if (ret) {
			/*
			 * TEMPORARY: DLB 2.0 uses the PASID enabled status to
			 * enable certain producer port functionality in
			 * scalable-IOV mode. On development platforms that
			 * lack end-to-end TLP prefix support, force PASID
			 * enable.
			 */
			ret = dlb2_enable_pasid(pdev);
			if (ret) {
				dev_err(&pdev->dev,
					"[%s()] Failed to enable PASID: %d\n",
					__func__, ret);
				goto enable_pasid_fail;
			}
		}
	}
#if (0)
	{
		struct device_domain_info *info = dev_iommu_priv_get(&pdev->dev);
		struct intel_iommu *iommu = info->iommu;
		int ret;

		ret = intel_iommu_enable_pasid(iommu, &pdev->dev);
		if (ret)
			return -EINVAL;
	}
#endif

	vdev = dlb2_vdev_create(dlb2, mdev);
	if (IS_ERR_OR_NULL(vdev)) {
		ret = (!vdev) ? -EFAULT : PTR_ERR(vdev);
		dev_err(dev,
			"[%s()] Failed to create dlb2 vdev: %d\n",
			__func__, ret);
		goto create_fail;
	}

	INIT_WORK(&vdev->release_work, dlb2_vdcm_release_work);

	vdev->mdev = mdev;

	dlb2_dev_set_drvdata(mdev_dev(mdev), vdev);

#ifdef DLB2_NEW_MDEV_IOMMUFD
	ret =  vfio_register_emulated_iommu_dev(&vdev->vfio_dev);

	if (ret) {
		dev_err(dev,
			"[%s()] Failed to  register vfio group: %d\n",
			__func__, ret);
		goto vfio_register_fail;
	}
#endif

#if defined(DLB2_VDCM_MIGRATION_V1) || defined(DLB2_VDCM_MIGRATION_V2)
	dlb2_vdcm_migration_init(vdev, DLB2_LM_MIGRATION_CMD_SIZE);
#endif

	list_add(&vdev->next, &dlb2->vdev_list);

	mutex_unlock(&dlb2->resource_mutex);

	return 0;

#ifdef DLB2_NEW_MDEV_IOMMUFD
vfio_register_fail:
	kfree(vdev);
#endif
create_fail:
	if (list_empty(&dlb2->vdev_list))
		pci_disable_pasid(pdev);
enable_pasid_fail:
	if (list_empty(&dlb2->vdev_list))
		dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);
virt_mode_err:
	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static void dlb2_vdcm_remove(struct mdev_device *mdev)
#else
static int dlb2_vdcm_remove(struct mdev_device *mdev)
#endif
{
	struct dlb2_vdev *vdev;
	struct dlb2 *dlb2;

	vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
	dlb2 = mdev_get_dlb2(mdev);

#ifndef DLB2_NEW_MDEV_IOMMUFD
	/*
	 * Ensure this dlb2_vdev's release operation completes before acquiring the
	 * resource_mutex.
	 * flush_schedule_work() was depreciated in kernel 6.6. It is not needed
	 * anyway as we do not use/schedule release_work with new iommufd.
	 * Todo: clean up release_work related code.
	 */
	flush_scheduled_work();
#endif
	mutex_lock(&dlb2->resource_mutex);

	list_del(&vdev->next);

	if (list_empty(&dlb2->vdev_list)) {
		pci_disable_pasid(mdev_get_pdev(mdev));
		dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);
	}

	dlb2_reset_vdev_resources(&dlb2->hw, vdev->id);

	dlb2_free_vdev_id(dlb2, vdev->id);

#ifdef DLB2_NEW_MDEV_IOMMUFD
	vfio_unregister_group_dev(&vdev->vfio_dev);
	vfio_put_device(&vdev->vfio_dev);
#else
	kfree(vdev);
#endif
	mutex_unlock(&dlb2->resource_mutex);

#ifndef DLB2_NEW_MDEV_IOMMUFD
	return 0;
#endif
}

#ifndef DLB2_NEW_MDEV_IOMMUFD
static int dlb2_vdcm_iommu_notifier(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	struct dlb2_vdev *vdev;
	struct dlb2 *dlb2;

	vdev = container_of(nb, struct dlb2_vdev, iommu_notifier);

	dlb2 = mdev_get_dlb2(vdev->mdev);

	/*
	 * The user is unmapping the IOMMU space before releasing the dlb2_vdev, so
	 * we must reset the VDEV now (while its IOVAs are still valid).
	 */
	if (action == VFIO_IOMMU_NOTIFY_DMA_UNMAP) {
		mutex_lock(&dlb2->resource_mutex);

		dlb2_reset_vdev(&dlb2->hw, vdev->id);

		mutex_unlock(&dlb2->resource_mutex);
	}

	return NOTIFY_OK;
}

static int dlb2_vdcm_group_notifier(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	struct dlb2_vdev *vdev;

	vdev = container_of(nb, struct dlb2_vdev, group_notifier);

	if (action == VFIO_GROUP_NOTIFY_SET_KVM) {
		/*
		 * If the VFIO group is being deleted, schedule the release
		 * workqueue. (Run in a separate context because a notifier
		 * callout routine "must not try to register or unregister
		 * entries on its own chain.")
		 */
		if (!data)
			schedule_work(&vdev->release_work);
	}

	return NOTIFY_OK;
}
#endif

static int dlb2_get_mdev_pasid(struct mdev_device *mdev)
{
#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN)
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif
	struct device *dev = mdev_dev(mdev);
#ifdef DLB2_NEW_MDEV_IOMMUFD
	struct device *pasid_dev;
	struct dlb2 *dlb2;
	ioasid_t pasid;
#else
	struct iommu_domain *domain;
#endif

	/*
	 * PASID override is intended for development systems without PASID
	 * support.
	 */
	if (dlb2_pasid_override)
		return 0;

#ifndef DLB2_NEW_MDEV_IOMMUFD
#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN)
	if (!vdev->vfio_group) {
		dev_warn(dev, "Missing vfio_group.\n");
		return -EINVAL;
	}

	domain = vfio_group_iommu_domain(vdev->vfio_group);
#else
	domain = mdev_get_iommu_domain(dev);
#endif
	if (IS_ERR_OR_NULL(domain))
		return -EINVAL;

	return iommu_aux_get_pasid(domain, dev->parent);
#else

	dlb2 = mdev_get_dlb2(mdev);
	pasid_dev = &dlb2->pdev->dev;  /* this is same as dlb2_mdev_parent_dev(mdev) */

	pasid = ioasid_alloc(NULL, 1, pasid_dev->iommu->max_pasids, vdev, 0);
	if (pasid == INVALID_IOASID) {
		dev_err(dev, "Unable to allocate pasid\n");
		return -ENODEV;
	}

	vfio_device_set_pasid(&vdev->vfio_dev, pasid);
	return pasid;
#endif
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static int dlb2_vdcm_open(struct vfio_device *vfio_dev)
#else
static int dlb2_vdcm_open(struct mdev_device *mdev)
#endif
{
#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN) && !defined(DLB2_NEW_MDEV_IOMMUFD)
	struct device *dev = mdev_dev(mdev);
	struct vfio_group *vfio_group;
#endif

	struct dlb2_get_num_resources_args rsrcs;
	struct device *parent_dev;
	struct dlb2_vdev *vdev;
	struct dlb2 *dlb2;
	int ret;
#ifdef DLB2_NEW_MDEV_IOMMUFD
	struct mdev_device *mdev;

	vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	mdev = vdev->mdev;
#else
	unsigned long events;

	vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif

	dlb2 = mdev_get_dlb2(mdev);
	parent_dev = dlb2_mdev_parent_dev(mdev);

	mutex_lock(&dlb2->resource_mutex);

	/*
	 * Increment the device's usage count and immediately wake it if it was
	 * suspended.
	 */
	pm_runtime_get_sync(parent_dev);

#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN) && !defined(DLB2_NEW_MDEV_IOMMUFD)
	vfio_group = vfio_group_get_external_user_from_dev(dev);
	if (IS_ERR_OR_NULL(vfio_group)) {
		ret = -EFAULT;
		goto vfio_group_fail;
	}
	vdev->vfio_group = vfio_group;
#endif

#if KERNEL_VERSION(6, 6, 0) > LINUX_VERSION_CODE
	/* moved this block to dlb2_vdcm_bind_iommufd()
	 * in kernel 6.6
	 */
	ret = dlb2_get_mdev_pasid(mdev);
	if (ret < 0) {
		dev_err(dlb2->dev,
			"[%s()] PASID get failed with error %d\n",
			__func__, ret);
		goto pasid_fail;
	}

	vdev->pasid = ret;

	ret = dlb2_hw_register_pasid(&dlb2->hw, vdev->id, vdev->pasid);
	if (ret)
		goto pasid_fail;
#endif

#ifndef DLB2_NEW_MDEV_IOMMUFD
	/* vfio_register_notifier() causes crash in 5.19, and is removed in
	 * kernel 6.0 anyway
	 */
	/*
	 * Register a notifier for when VFIO is about to unmap IOVAs, in order
	 * to reset the mdev if it is active, to prevent the device from
	 * attempting to write to invalid an IOVA.
	 */
	vdev->iommu_notifier.notifier_call = dlb2_vdcm_iommu_notifier;
	events = VFIO_IOMMU_NOTIFY_DMA_UNMAP;

	ret = vfio_register_notifier(mdev_dev(mdev), VFIO_IOMMU_NOTIFY, &events,
				     &vdev->iommu_notifier);
	if (ret != 0) {
		dev_err(dlb2->dev,
			"[%s()] Failed to register iommu notifier: %d\n",
			__func__, ret);
		goto iommu_notif_register_fail;
	}

	/*
	 * Register a KVM notifier for when a VFIO group is registered or
	 * unregistered with KVM.
	 */
	vdev->group_notifier.notifier_call = dlb2_vdcm_group_notifier;
	events = VFIO_GROUP_NOTIFY_SET_KVM;

	ret = vfio_register_notifier(mdev_dev(mdev), VFIO_GROUP_NOTIFY, &events,
				     &vdev->group_notifier);
	if (ret != 0) {
		dev_err(dlb2->dev,
			"[%s()] Failed to register group notifier: %d\n",
			__func__, ret);
		goto group_notif_register_fail;
	}
#endif

	/* Mailbox mapping is at a page granularity, so round up size to 4KB */
	vdev->pf_to_vdev_mbox = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!vdev->pf_to_vdev_mbox) {
		dev_err(dlb2->dev,
			"[%s()] Failed to alloc PF2VF mailbox\n", __func__);
		goto pf_to_vdev_alloc_fail;
	}

	vdev->vdev_to_pf_mbox = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!vdev->vdev_to_pf_mbox) {
		dev_err(dlb2->dev,
			"[%s()] Failed to alloc VF2PF mailbox\n", __func__);
		goto vdev_to_pf_alloc_fail;
	}

	dlb2_hw_register_sw_mbox(&dlb2->hw,
				 vdev->id,
				 (u32 *)vdev->vdev_to_pf_mbox,
				 (u32 *)vdev->pf_to_vdev_mbox,
				 dlb2_trigger_mbox_interrupt,
				 vdev);

	/* Cache the assigned number of ldb and dir ports, used for IMS */
	ret = dlb2_hw_get_num_resources(&dlb2->hw, &rsrcs,
					true, vdev->id);
	if (ret)
		goto get_num_resources_fail;

	vdev->num_ldb_ports = rsrcs.num_ldb_ports;
	vdev->num_dir_ports = rsrcs.num_dir_ports;

	/* Set MSI-X table size using N-1 encoding */
	vdev->cfg[VDCM_MSIX_MSG_CTRL_OFFSET] = dlb2_vdcm_num_irqs(vdev) - 1;

	dlb2_lock_vdev(&dlb2->hw, vdev->id);
	/*
	 * IMS configuration must be done after locking the dlb2_vdev, which sets
	 * its virtual->physical port ID mapping.
	 */
	ret = dlb2_vdcm_alloc_ims_irq_vectors(vdev);
	if (ret != 0) {
		dev_err(dlb2->dev,
			"[%s()] failed to allocate ims irq vectors: %d\n",
			__func__, ret);
		goto ims_irq_vector_alloc_fail;
	}

#ifndef DLB2_SIOV_IMS_WORKAROUND
	dlb2_vdev_set_ims_idx(&dlb2->hw, vdev->id, vdev->ims_idx);
#endif
	vdev->released = false;

	mutex_unlock(&dlb2->resource_mutex);

	return 0;

ims_irq_vector_alloc_fail:
	dlb2_unlock_vdev(&dlb2->hw, vdev->id);
get_num_resources_fail:
	dlb2_hw_unregister_sw_mbox(&dlb2->hw, vdev->id);
vdev_to_pf_alloc_fail:
	free_page((unsigned long)vdev->pf_to_vdev_mbox);
pf_to_vdev_alloc_fail:
#ifndef DLB2_NEW_MDEV_IOMMUFD
	vfio_unregister_notifier(mdev_dev(mdev), VFIO_GROUP_NOTIFY,
				 &vdev->group_notifier);
group_notif_register_fail:
	vfio_unregister_notifier(mdev_dev(mdev), VFIO_IOMMU_NOTIFY,
				 &vdev->iommu_notifier);
iommu_notif_register_fail:
#endif
#if KERNEL_VERSION(6, 6, 0) > LINUX_VERSION_CODE
pasid_fail:
#endif
#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN) && !defined(DLB2_NEW_MDEV_IOMMUFD)
	vfio_group_put_external_user(vdev->vfio_group);
	vdev->vfio_group = NULL;
vfio_group_fail:
#endif
	pm_runtime_put_sync_suspend(parent_dev);

	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static void dlb2_vdcm_release(struct vfio_device *vfio_dev)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#else
static void dlb2_vdcm_release(struct mdev_device *mdev)
{
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif

	__dlb2_vdcm_release(vdev);
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static int dlb2_vdcm_bind_iommufd(struct vfio_device *vfio_dev,
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
				  struct vfio_device_bind_iommufd *bind)
#else
				  struct iommufd_ctx *ictx, u32 *out_device_id)
#endif
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	struct iommufd_device *idev;
	int rc = 0;
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	u32 id;
#endif

	/* Should we have a lock per dlb2_vdev? */
	mutex_lock(&dlb2->resource_mutex);

	/* Allow only one iommufd per vfio_device */
	if (vdev->idev) {
		rc = -EBUSY;
		goto out;
	}
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	rc = dlb2_get_mdev_pasid(vdev->mdev);
	if (rc < 0) {
		dev_err(dlb2->dev,
			"[%s()] PASID get failed with error %d\n",
			__func__, rc);
		goto out;
	}

	vdev->pasid = rc;

	rc = dlb2_hw_register_pasid(&dlb2->hw, vdev->id, vdev->pasid);
	if (rc)
		goto out;
#endif

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	idev = iommufd_bind_device(bind->iommufd, &dlb2->pdev->dev,
				IOMMUFD_BIND_FLAGS_BYPASS_DMA_OWNERSHIP, &id);
#elif KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
	idev = iommufd_device_bind(ictx, &dlb2->pdev->dev, out_device_id,
					IOMMUFD_BIND_FLAGS_BYPASS_DMA_OWNERSHIP);
#else
	idev = iommufd_device_bind_pasid(ictx, &dlb2->pdev->dev, vdev->pasid, out_device_id);
#endif

	if (IS_ERR(idev)) {
		rc = PTR_ERR(idev);
		goto out;
	}

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	vdev->iommufd = bind->iommufd;
#endif
	vdev->idev = idev;
	xa_init_flags(&vdev->pasid_xa, XA_FLAGS_ALLOC);

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	bind->out_devid = id;
#else
	vfio_dev->iommufd_device = idev;
#endif

out:
	mutex_unlock(&dlb2->resource_mutex);
	return rc;
}

static ioasid_t dlb2_vdcm_get_pasid(struct dlb2_vdev *vdev,
				    ioasid_t pasid)
{
	ioasid_t vdev_pasid = INVALID_IOASID;

	if (pasid_valid(pasid))
		return pasid;

	vdev_pasid = vfio_device_get_pasid(&vdev->vfio_dev);
	return vdev_pasid;
}

static void dlb2_vdcm_unbind_iommufd(struct vfio_device *vfio_dev)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	if (vdev->idev) {
#if KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
		struct vfio_pci_hwpt *hwpt;
#else
		struct vdcm_hwpt *hwpt;
#endif
		unsigned long index;

		xa_for_each(&vdev->pasid_xa, index, hwpt) {
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
			iommufd_device_pasid_detach(vdev->idev, hwpt->pasid);
#elif KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
			iommufd_device_detach(vdev->idev, hwpt->pasid);
#else
			if (!pasid_valid(hwpt->pasid)) {
				continue;
			}
			iommufd_device_pasid_detach(vdev->idev, hwpt->pasid);
#endif
			kfree(hwpt);
		}
		ioasid_put(NULL, vdev->pasid);
		xa_destroy(&vdev->pasid_xa);
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
		iommufd_unbind_device(vdev->idev);
#else
                iommufd_device_unbind(vdev->idev);
#endif
		vdev->idev = NULL;
	}
	mutex_unlock(&dlb2->resource_mutex);
}

static int dlb2_vdcm_pasid_attach(struct dlb2_vdev *vdev, ioasid_t pasid, u32 *pt_id)
{
	struct vdcm_hwpt *hwpt, *tmp;
	int ret;

	/* userspace needs to detach a hwpt before attaching a new */
	hwpt = xa_load(&vdev->pasid_xa, pasid);
	if (hwpt) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		ret = iommufd_device_pasid_replace(vdev->idev, pasid, *pt_id);
#endif
		return ret;
	}

	hwpt = kzalloc(sizeof(*hwpt), GFP_KERNEL);
	if (!hwpt)
		return -ENOMEM;

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	ret = iommufd_device_pasid_attach(vdev->idev, pt_id, pasid,
					IOMMUFD_ATTACH_FLAGS_ALLOW_UNSAFE_INTERRUPT);
#elif KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
	ret = iommufd_device_attach(vdev->idev, pt_id, pasid);
#else
	ret = iommufd_device_pasid_attach(vdev->idev, pasid, *pt_id);
#endif

	if (ret) {
		struct device *dev;

		dev = mdev_dev(vdev->mdev);
		dev_dbg(dev, "[%s()] iommufd_device_pasid_attach failed: %d\n",
			__func__, ret);

		goto out_free;
	}

	hwpt->hwpt_id = *pt_id;
	hwpt->pasid = pasid;
	tmp = xa_store(&vdev->pasid_xa, hwpt->pasid, hwpt, GFP_KERNEL);
	if (IS_ERR(tmp)) {
		ret = PTR_ERR(tmp);
		goto out_detach;
	}
	return 0;
out_detach:
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	iommufd_device_pasid_detach(vdev->idev, pasid);
#elif KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
	iommufd_device_detach(vdev->idev, pasid);
#else
	iommufd_device_pasid_detach(vdev->idev, pasid);
#endif

out_free:
	kfree(hwpt);
	return ret;
}

static int dlb2_vdcm_attach_ioas(struct vfio_device *vfio_dev,
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
				 struct vfio_device_attach_ioas *attach)
#else
				 u32 *pt_id)
#endif
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	u32 pt_id = attach->ioas_id;
#endif
	u32 pasid;
	int rc = 0;

	mutex_lock(&dlb2->resource_mutex);

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	if (!vdev->idev || vdev->iommufd != attach->iommufd) {
#else
	if (!vdev->idev) {
#endif
		rc = -EINVAL;
		goto out_unlock;
	}

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	/* Only allows one IOAS attach */
	if (!xa_empty(&vdev->pasid_xa)) {
		rc = -EBUSY;
		goto out_unlock;
	}
#endif

	pasid = vfio_device_get_pasid(vfio_dev);
	if (!pasid_valid(pasid)) {
		rc = -ENODEV;
		goto out_unlock;
	}

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	rc = dlb2_vdcm_pasid_attach(vdev, pasid, &pt_id);
#else
#if KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
	if (!pt_id) {
		struct vfio_pci_hwpt *hwpt;

		hwpt = xa_load(&vdev->pasid_xa, pasid);
		if (!hwpt) {
			goto out_unlock;
		}
		xa_erase(&vdev->pasid_xa, hwpt->pasid);
		kfree(hwpt);
		iommufd_device_detach(vdev->idev, pasid);
		goto out_unlock;
        }
#endif

	rc = dlb2_vdcm_pasid_attach(vdev, pasid, pt_id);
#endif
	if (rc)
		goto out_unlock;

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	WARN_ON(attach->ioas_id == pt_id);
	attach->out_hwpt_id = pt_id;
#endif
out_unlock:
	mutex_unlock(&dlb2->resource_mutex);
	return rc;
}

#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
static void dlb2_vdcm_detach_ioas(struct vfio_device *vfio_dev)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	struct vdcm_hwpt *hwpt;
	u32 pasid;

	mutex_lock(&dlb2->resource_mutex);

	if (!vdev->idev) {
		goto out_unlock;
	}

	pasid = vfio_device_get_pasid(vfio_dev);
	if (!pasid_valid(pasid)) {
		goto out_unlock;
	}

	hwpt = xa_load(&vdev->pasid_xa, pasid);
	if (!hwpt) {
		goto out_unlock;
	}

	xa_erase(&vdev->pasid_xa, pasid);
	kfree(hwpt);
	iommufd_device_pasid_detach(vdev->idev, pasid);

out_unlock:
	mutex_unlock(&dlb2->resource_mutex);
	return;
}

static int dlb2_vdcm_pasid_attach_ioas(struct vfio_device *vfio_dev,
			  u32 pasid, u32 pt_id)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	int ret;

	mutex_lock(&dlb2->resource_mutex);

	if (!vdev->idev) {
		ret = -EINVAL;
		goto out_unlock;
	}

	pasid = dlb2_vdcm_get_pasid(vdev, pasid);
	if (!pasid_valid(pasid)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = dlb2_vdcm_pasid_attach(vdev, pasid, &pt_id);
	if (ret)
		goto out_unlock;

out_unlock:
	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

static void dlb2_vdcm_pasid_detach_ioas(struct vfio_device *vfio_dev,
			  u32 pasid)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	struct vfio_pci_hwpt *hwpt;

	mutex_lock(&dlb2->resource_mutex);

	if (!vdev->idev)
		goto out_unlock;

	pasid = dlb2_vdcm_get_pasid(vdev, pasid);
	if (!pasid_valid(pasid))
		goto out_unlock;

	hwpt = xa_load(&vdev->pasid_xa, pasid);
	if (!hwpt)
		goto out_unlock;

	xa_erase(&vdev->pasid_xa, pasid);
	kfree(hwpt);
	iommufd_device_pasid_detach(vdev->idev, pasid);
out_unlock:
	mutex_unlock(&dlb2->resource_mutex);
}

#else

static int dlb2_vdcm_attach_hwpt(struct vfio_device *vfio_dev,
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
			  struct vfio_device_attach_hwpt *attach)
#else
			  u32 *pt_id, ioasid_t pasid)
#endif
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	ioasid_t pasid = attach->flags & VFIO_DEVICE_ATTACH_FLAG_PASID ?
			 attach->pasid : INVALID_IOASID;
	u32 pt_id = attach->hwpt_id;
#endif
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	int ret;

	mutex_lock(&dlb2->resource_mutex);

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	if (!vdev->idev || vdev->iommufd != attach->iommufd) {
#else
	if (!vdev->idev) {
#endif
		ret = -EINVAL;
		goto out_unlock;
	}

	pasid = dlb2_vdcm_get_pasid(vdev, pasid);
	if (!pasid_valid(pasid)) {
		ret = -EINVAL;
		goto out_unlock;
	}

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	ret = dlb2_vdcm_pasid_attach(vdev, pasid, &pt_id);
#else
	ret = dlb2_vdcm_pasid_attach(vdev, pasid, pt_id);
#endif
	if (ret)
		goto out_unlock;

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	WARN_ON(attach->hwpt_id != pt_id);
#endif
out_unlock:
	mutex_unlock(&dlb2->resource_mutex);

	return ret;
}

#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
static void dlb2_vdcm_detach_hwpt(struct vfio_device *vfio_dev,
			   struct vfio_device_detach_hwpt *detach)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
	ioasid_t pasid = detach->flags & VFIO_DEVICE_DETACH_FLAG_PASID ?
			 detach->pasid : INVALID_IOASID;
	struct dlb2 *dlb2 = mdev_get_dlb2(vdev->mdev);
	struct vfio_pci_hwpt *hwpt;

	mutex_lock(&dlb2->resource_mutex);

	if (!vdev->idev || vdev->iommufd != detach->iommufd)
		goto out_unlock;

	pasid = dlb2_vdcm_get_pasid(vdev, pasid);
	if (!pasid_valid(pasid))
		goto out_unlock;
	hwpt = xa_load(&vdev->pasid_xa, pasid);
	if (!hwpt)
		goto out_unlock;

	xa_erase(&vdev->pasid_xa, pasid);
	iommufd_device_pasid_detach(vdev->idev, pasid);
	kfree(hwpt);
out_unlock:
	mutex_unlock(&dlb2->resource_mutex);
}
#endif
#endif
#endif

static u64 get_reg_val(void *buf, int size)
{
	u64 val = 0;

	if (size == 8)
		val = *(u64 *)buf;
	else if (size == 4)
		val = *(u32 *)buf;
	else if (size == 2)
		val = *(u16 *)buf;
	else if (size == 1)
		val = *(u8 *)buf;

	return val;
}

static int dlb2_vdcm_cfg_read(struct dlb2_vdev *vdev, unsigned int pos,
			      unsigned char *buf, unsigned int count)
{
	unsigned int offset = pos & (PCI_CFG_SPACE_SIZE - 1);
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	memcpy(buf, &vdev->cfg[offset], count);

	mutex_unlock(&dlb2->resource_mutex);

	dev_dbg(dev, "[%s()] config[%d:%d] = 0x%llx\n",
		__func__, offset, offset + count, get_reg_val(buf, count));

	return 0;
}

static int dlb2_vdcm_cfg_write(struct dlb2_vdev *vdev, unsigned int pos,
			       unsigned char *buf, unsigned int count)
{
	unsigned int offset = pos & (PCI_CFG_SPACE_SIZE - 1);
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	u8 *cfg = vdev->cfg;
	struct dlb2 *dlb2;
	u64 val, bar;

	/*
	 * Only look for writable config registers. Ignore BARs 2-5;
	 * unimplemented BARs are hard-wired to zero.
	 */

	dev_dbg(dev, "[%s()] config[%d:%d] = 0x%llx\n",
		__func__, offset, offset + count, get_reg_val(buf, count));

	dlb2 = mdev_get_dlb2(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	switch (offset) {
	case PCI_COMMAND:
		memcpy(&cfg[offset], buf, count);
		if (count < 4)
			break;
		offset += 2;
		buf = buf + 2;
		count -= 2;
		/* Falls through */
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		fallthrough;
#endif
	case PCI_STATUS:
	{
		/* Bits 8 and 11-15 are WOCLR, the rest are RO */
		u16 mask;

		mask = get_reg_val(buf, count) << (offset & 1) * 8;
		mask &= 0xf900;

		*(u16 *)&cfg[offset] = *((u16 *)&cfg[offset]) & ~mask;
		break;
	}

	case PCI_CACHE_LINE_SIZE:
	case PCI_INTERRUPT_LINE:
		memcpy(&cfg[offset], buf, count);
		break;

	case PCI_BASE_ADDRESS_0:
	case PCI_BASE_ADDRESS_1:
		/*
		 * Allow software to write all 1s to query the BAR size. Save
		 * the overwritten BAR address in case it is needed before the
		 * BAR is restored.
		 */
		val = get_reg_val(buf, count);
		bar = *(u64 *)&cfg[PCI_BASE_ADDRESS_0];

		/* Copy data either BAR0 or BAR1, depending on offset */
		memcpy((u8 *)&bar + (offset & 0x7), buf, count);

		/* Unused address bits are hardwired to zero. */
		bar &= ~(DLB2_VDEV_BAR0_SIZE - 1);

		*(u64 *)&cfg[PCI_BASE_ADDRESS_0] = bar |
			PCI_BASE_ADDRESS_MEM_TYPE_64 |
			PCI_BASE_ADDRESS_MEM_PREFETCH;

		/* Don't overwrite BAR addr if the user is querying the size */
		if (val == -1U || val == -1ULL)
			break;

		vdev->bar0_addr = bar;

		break;

	case VDCM_PCIE_DEV_CTRL_OFFSET:
		val = get_reg_val(buf, count);

		if (val & PCI_EXP_DEVCTL_BCR_FLR)
			dlb2_reset_vdev(&dlb2->hw, vdev->id);

		/*
		 * Per spec, software always reads 0 for the initiate FLR bit,
		 * and for the dlb2_vdev Aux PME is hard-wired to 0.
		 */
		val &= ~(PCI_EXP_DEVCTL_BCR_FLR | PCI_EXP_DEVCTL_AUX_PME);

		memcpy(&cfg[offset], &val, count);

		break;

	case VDCM_MSIX_MSG_CTRL_OFFSET:
		/*
		 * Allow software to write all 1s to query the BAR size. Save
		 * the overwritten BAR address in case it is needed before the
		 * BAR is restored.
		 */
		val = get_reg_val(buf, count);

		/* Bits [15:14] are writeable, the rest are RO */
		val &= 0xC000;

		memcpy(&cfg[offset], &val, count);

		/*
		 * If the function is unmasked and any pending bits are set,
		 * fire the interrupt(s) and clear the pending bit.
		 */
		dlb2_vdcm_send_unmasked_interrupts(vdev);

		break;

	default:
		break;
	}

	mutex_unlock(&dlb2->resource_mutex);

	return 0;
}

static int dlb2_vdcm_mmio_read(struct dlb2_vdev *vdev, u64 pos, void *buf,
			       unsigned int size)
{
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	u32 offs = pos & (DLB2_VDEV_BAR0_SIZE - 1);
	struct dlb2 *dlb2;
	u8 *addr;

	/*
	 * The function expects reads of either 8, 4, or 2 bytes, and the
	 * location to be aligned to the read size.
	 */
	if (((size & (size - 1)) != 0) || size > 8 ||
	    ((offs & (size - 1)) != 0))
		return -EINVAL;

	dev_dbg(dev, "[%s()] mmio[%d:%d] = 0x%llx\n",
		__func__, offs, offs + size, get_reg_val(buf, size));

	dlb2 = mdev_get_dlb2(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	switch (offs) {
	case VDCM_MSIX_TBL_OFFSET ... VDCM_MSIX_TBL_END_OFFSET:
		addr = &vdev->msix_table[offs - VDCM_MSIX_TBL_OFFSET];
		break;

	case VDCM_MSIX_PBA_OFFSET ... VDCM_MSIX_PBA_END_OFFSET:
		addr = (u8 *)vdev->msix_pba;
		addr = &addr[offs - VDCM_MSIX_PBA_OFFSET];
		break;

	default:
		addr = NULL;
		break;
	}

	if (addr)
		memcpy(buf, addr, size);
	else
		memset(buf, 0, size);

	mutex_unlock(&dlb2->resource_mutex);

	return 0;
}

static int dlb2_vdcm_mmio_write(struct dlb2_vdev *vdev, u64 pos, void *buf,
				unsigned int size)
{
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	u32 offs = pos & (DLB2_VDEV_BAR0_SIZE - 1);
	struct dlb2 *dlb2;
	u8 *entry, *reg;
	int idx;

	/*
	 * The function expects writes of either 8, 4, or 2 bytes, and the
	 * location to be aligned to the write size.
	 */
	if (((size & (size - 1)) != 0) || size > 8 ||
	    ((offs & (size - 1)) != 0))
		return -EINVAL;

	dev_dbg(dev, "[%s()] mmio[%d:%d] = 0x%llx\n",
		__func__, offs, offs + size, get_reg_val(buf, size));

	dlb2 = mdev_get_dlb2(vdev->mdev);

	switch (offs) {
	case VDCM_MSIX_TBL_OFFSET ... VDCM_MSIX_TBL_END_OFFSET:
		mutex_lock(&dlb2->resource_mutex);

		/* Calculate the MSI-X vector */
		idx = (offs - VDCM_MSIX_TBL_OFFSET) / VDCM_MSIX_TBL_ENTRY_SZ;

		/* Find the corresponding table entry */
		entry = &vdev->msix_table[idx * VDCM_MSIX_TBL_ENTRY_SZ];
		entry += offs & (VDCM_MSIX_TBL_ENTRY_SZ - 1);

		memcpy(entry, buf, size);

		/*
		 * If the vector is unmasked and its pending bit is
		 * set, fire the interrupt and clear the pending bit.
		 */
		if (!vdcm_msix_is_masked(vdev, idx) &&
		    test_and_clear_bit(idx, (unsigned long *)vdev->msix_pba))
			vdcm_send_interrupt(vdev, idx);

		mutex_unlock(&dlb2->resource_mutex);

		break;

	case VF_SIOV_MBOX_ISR_TRIGGER:
		/*
		 * Set the vdev->PF ISR in progress bit. The PF driver clears
		 * this when it's done processing the mailbox request, while
		 * the dlb2_vdev driver polls it.
		 */
		reg = vdev->vdev_to_pf_mbox;
		reg += (VF_VF2PF_MAILBOX_ISR % 0x1000);

		mutex_lock(&dlb2->resource_mutex);

		if (dlb2->mbox[vdev->id].enabled && *(u32 *)reg == 0) {
			*(u32 *)reg = 1;

			dlb2_handle_mbox_interrupt(dlb2, vdev->id);
		}

		mutex_unlock(&dlb2->resource_mutex);

		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static ssize_t dlb2_vdcm_rw(struct dlb2_vdev *vdev, char *buf,
			    size_t count, const loff_t *ppos, bool is_write)
{
	unsigned int index = VFIO_PCI_OFFSET_TO_INDEX(*ppos);
	u64 pos = *ppos & VFIO_PCI_OFFSET_MASK;
	int ret = -EINVAL;

	if (index >= VFIO_PCI_NUM_REGIONS + DLB2_VDCM_NUM_DEV_REGIONS)
		return -EINVAL;

	switch (index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		if (is_write)
			ret = dlb2_vdcm_cfg_write(vdev, pos, buf, count);
		else
			ret = dlb2_vdcm_cfg_read(vdev, pos, buf, count);
		break;
	case VFIO_PCI_BAR0_REGION_INDEX:
		if (is_write)
			ret = dlb2_vdcm_mmio_write(vdev, pos, buf, count);
		else
			ret = dlb2_vdcm_mmio_read(vdev, pos, buf, count);
		break;
#ifdef DLB2_VDCM_MIGRATION_V1
#if KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE
	/* We only support DEV_REGIONS when migration is supported */
	case VFIO_PCI_NUM_REGIONS ...
		VFIO_PCI_NUM_REGIONS + DLB2_VDCM_NUM_DEV_REGIONS:
		index -= VFIO_PCI_NUM_REGIONS;
		ret = dlb2_vdcm_vdev_dev_region_rw(vdev, index, pos,
						  buf, count,
						  is_write);
		break;
#endif
#endif
	default:
		break;
	}

	if (ret == 0)
		ret = count;

	return ret;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static ssize_t dlb2_vdcm_read(struct vfio_device *vfio_dev, char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#else
static ssize_t dlb2_vdcm_read(struct mdev_device *mdev, char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif

	unsigned int done = 0;
	int ret;

	while (count) {
		size_t filled;

		if (count >= 8 && !(*ppos % 8)) {
			u64 val;

			ret = dlb2_vdcm_rw(vdev, (char *)&val, sizeof(val),
					   ppos, false);
			if (ret <= 0)
				goto read_err;

			if (copy_to_user(buf, &val, sizeof(val)))
				goto read_err;

			filled = 8;
		} else if (count >= 4 && !(*ppos % 4)) {
			u32 val;

			ret = dlb2_vdcm_rw(vdev, (char *)&val, sizeof(val),
					   ppos, false);
			if (ret <= 0)
				goto read_err;

			if (copy_to_user(buf, &val, sizeof(val)))
				goto read_err;

			filled = 4;
		} else if (count >= 2 && !(*ppos % 2)) {
			u16 val;

			ret = dlb2_vdcm_rw(vdev, (char *)&val, sizeof(val),
					   ppos, false);
			if (ret <= 0)
				goto read_err;

			if (copy_to_user(buf, &val, sizeof(val)))
				goto read_err;

			filled = 2;
		} else {
			u8 val;

			ret = dlb2_vdcm_rw(vdev, &val, sizeof(val), ppos,
					   false);
			if (ret <= 0)
				goto read_err;

			if (copy_to_user(buf, &val, sizeof(val)))
				goto read_err;

			filled = 1;
		}

		count -= filled;
		done += filled;
		*ppos += filled;
		buf += filled;
	}

	return done;

read_err:
	return -EFAULT;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static ssize_t dlb2_vdcm_write(struct vfio_device *vfio_dev,
			       const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#else
static ssize_t dlb2_vdcm_write(struct mdev_device *mdev,
			       const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif
	unsigned int done = 0;
	int ret;

	while (count) {
		size_t filled;

		if (count >= 8 && !(*ppos % 8)) {
			u64 val;

			if (copy_from_user(&val, buf, sizeof(val)))
				goto write_err;

			ret = dlb2_vdcm_rw(vdev, (char *)&val, sizeof(val),
					   ppos, true);
			if (ret <= 0)
				goto write_err;

			filled = 8;
		} else if (count >= 4 && !(*ppos % 4)) {
			u32 val;

			if (copy_from_user(&val, buf, sizeof(val)))
				goto write_err;

			ret = dlb2_vdcm_rw(vdev, (char *)&val, sizeof(val),
					   ppos, true);
			if (ret <= 0)
				goto write_err;

			filled = 4;
		} else if (count >= 2 && !(*ppos % 2)) {
			u16 val;

			if (copy_from_user(&val, buf, sizeof(val)))
				goto write_err;

			ret = dlb2_vdcm_rw(vdev, (char *)&val,
					   sizeof(val), ppos, true);
			if (ret <= 0)
				goto write_err;

			filled = 2;
		} else {
			u8 val;

			if (copy_from_user(&val, buf, sizeof(val)))
				goto write_err;

			ret = dlb2_vdcm_rw(vdev, &val, sizeof(val),
					   ppos, true);
			if (ret <= 0)
				goto write_err;

			filled = 1;
		}

		count -= filled;
		done += filled;
		*ppos += filled;
		buf += filled;
	}

	return done;
write_err:
	return -EFAULT;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static int dlb2_vdcm_mmap(struct vfio_device *vfio_dev, struct vm_area_struct *vma)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#else
static int dlb2_vdcm_mmap(struct mdev_device *mdev, struct vm_area_struct *vma)
{
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif
	unsigned long virt_port_id, bar_pgoff, offset, index;
	struct dlb2 *dlb2;
	pgprot_t pgprot;
	s32 port_id;

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;
	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;

	index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

	if (index != VFIO_PCI_BAR0_REGION_INDEX)
		return -EINVAL;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	offset = (vma->vm_pgoff << PAGE_SHIFT) & VFIO_PCI_OFFSET_MASK;

	switch (offset) {
	case DLB2_LDB_PP_BASE ... DLB2_LDB_PP_BOUND - 1:
		bar_pgoff = dlb2->hw.func_phys_addr >> PAGE_SHIFT;

		/*
		 * The VDEV has a 0-based port ID space, but those ports can
		 * map to any physical port. Convert the virt ID to a physical
		 * ID, and in doing so check if the virt ID is valid.
		 */
		virt_port_id = (offset - DLB2_LDB_PP_BASE) / PAGE_SIZE;

		port_id = dlb2_hw_get_ldb_port_phys_id(&dlb2->hw,
						       virt_port_id,
						       vdev->id);
		if (port_id == -1)
			return -EINVAL;

		offset = DLB2_LDB_PP_BASE + port_id * DLB2_LDB_PP_STRIDE;
		offset >>= PAGE_SHIFT;
		offset += bar_pgoff;

		pgprot = pgprot_noncached(vma->vm_page_prot);

		return io_remap_pfn_range(vma,
					  vma->vm_start,
					  offset,
					  vma->vm_end - vma->vm_start,
					  pgprot);
	case DLB2_DIR_PP_BASE ... DLB2_DIR_PP_BOUND - 1:
		bar_pgoff = dlb2->hw.func_phys_addr >> PAGE_SHIFT;

		virt_port_id = (offset - DLB2_DIR_PP_BASE) / PAGE_SIZE;

		/*
		 * The VDEV has a 0-based port ID space, but those ports can
		 * map to any physical port. Convert the virt ID to a physical
		 * ID, and in doing so check if the virt ID is valid.
		 */
		port_id = dlb2_hw_get_dir_port_phys_id(&dlb2->hw,
						       virt_port_id,
						       vdev->id);
		if (port_id == -1)
			return -EINVAL;

		offset = DLB2_DIR_PP_BASE + port_id * DLB2_DIR_PP_STRIDE;
		offset >>= PAGE_SHIFT;
		offset += bar_pgoff;

		pgprot = pgprot_noncached(vma->vm_page_prot);

		return io_remap_pfn_range(vma,
					  vma->vm_start,
					  offset,
					  vma->vm_end - vma->vm_start,
					  pgprot);
	case VF_PF2VF_MAILBOX(0):
		return vm_insert_page(vma,
				      vma->vm_start,
				      virt_to_page(vdev->pf_to_vdev_mbox));
	case VF_VF2PF_MAILBOX(0):
		return vm_insert_page(vma,
				      vma->vm_start,
				      virt_to_page(vdev->vdev_to_pf_mbox));
	default:
		break;
	}

	return -EINVAL;
}

static long dlb2_vfio_device_get_info(struct dlb2_vdev *vdev,
				      unsigned long arg)
{
	struct vfio_device_info info;
	unsigned long minsz;

	minsz = offsetofend(struct vfio_device_info, num_irqs);

	if (copy_from_user(&info, (void __user *)arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz)
		return -EINVAL;

	vdev->num_regions = DLB2_VDCM_NUM_DEV_REGIONS;

	info.flags = VFIO_DEVICE_FLAGS_PCI | VFIO_DEVICE_FLAGS_RESET;
	info.num_regions = VFIO_PCI_NUM_REGIONS + vdev->num_regions;
	info.num_irqs = VFIO_PCI_NUM_IRQS;

	return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

static long dlb2_vfio_device_get_region_info(struct dlb2_vdev *vdev,
					     unsigned long arg)
{
	struct vfio_region_info_cap_sparse_mmap *sparse = NULL;
	struct vfio_region_info info;
	struct vfio_info_cap caps;
	unsigned long minsz;
	int num_areas, ret;
	unsigned int i;
	size_t sz;

	minsz = offsetofend(struct vfio_region_info, offset);

	if (copy_from_user(&info, (void __user *)arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz)
		return -EINVAL;

	info.cap_offset = 0;

	switch (info.index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = PCI_CFG_SPACE_SIZE;
		info.flags = VFIO_REGION_INFO_FLAG_READ |
			     VFIO_REGION_INFO_FLAG_WRITE;
		break;
	case VFIO_PCI_BAR0_REGION_INDEX:
		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = DLB2_VDEV_BAR0_SIZE;
		info.flags = VFIO_REGION_INFO_FLAG_CAPS |
			     VFIO_REGION_INFO_FLAG_MMAP |
			     VFIO_REGION_INFO_FLAG_READ |
			     VFIO_REGION_INFO_FLAG_WRITE;

		/*
		 * 1 mmap'able region per LDB and DIR PP, plus one each for
		 * PF->VF and VF->PF mailbox memory.
		 */
		num_areas = vdev->num_ldb_ports + vdev->num_dir_ports + 2;
		sz = sizeof(*sparse) + num_areas * sizeof(*sparse->areas);

		sparse = kzalloc(sz, GFP_KERNEL);
		if (!sparse)
			return -ENOMEM;

		sparse->header.id = VFIO_REGION_INFO_CAP_SPARSE_MMAP;
		sparse->header.version = 1;
		sparse->nr_areas = num_areas;

		/* LDB PP mapping info */
		for (i = 0; i < vdev->num_ldb_ports; i++) {
			sparse->areas[i].offset = DLB2_LDB_PP_BASE;
			sparse->areas[i].offset += i * PAGE_SIZE;
			sparse->areas[i].size = PAGE_SIZE;
		}

		/* DIR PP mapping info */
		for (; i < vdev->num_ldb_ports + vdev->num_dir_ports; i++) {
			u32 idx = i - vdev->num_ldb_ports;

			sparse->areas[i].offset = DLB2_DIR_PP_BASE;
			sparse->areas[i].offset += idx * PAGE_SIZE;
			sparse->areas[i].size = PAGE_SIZE;
		}

		/* PF->VF mbox memory */
		sparse->areas[i].offset = VF_PF2VF_MAILBOX(0);
		sparse->areas[i].size = PAGE_SIZE;

		/* VF->PF mbox memory */
		sparse->areas[++i].offset = VF_VF2PF_MAILBOX(0);
		sparse->areas[i].size = PAGE_SIZE;

		caps.buf = NULL;
		caps.size = 0;

		/*
		 * Create a capability header chain and copy the sparse mmap
		 * info into it. This is later copied into the user buffer.
		 */
		ret = vfio_info_add_capability(&caps, &sparse->header, sz);
		if (ret) {
			kfree(sparse);
			return ret;
		}

		/*
		 * Getting this region's info is a two step operation:
		 * 1. User calls with argsz == sizeof(info), and the driver
		 *    notifies the user of the buffer size required to store
		 *    the additional sparse mmap info.
		 * 2. User retries with a sufficiently large buffer and the
		 *    driver copies the region and sparse mmap info into it.
		 */
		info.flags |= VFIO_REGION_INFO_FLAG_CAPS;
		if (info.argsz < sizeof(info) + caps.size) {
			info.argsz = sizeof(info) + caps.size;
			info.cap_offset = 0;
		} else {
			vfio_info_cap_shift(&caps, sizeof(info));
			if (copy_to_user((void __user *)(arg + sizeof(info)),
					 caps.buf,
					 caps.size)) {
				kfree(caps.buf);
				kfree(sparse);
				return -EFAULT;
			}
			info.cap_offset = sizeof(info);
		}

		kfree(caps.buf);
		kfree(sparse);

		break;
	case VFIO_PCI_BAR1_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
	case VFIO_PCI_ROM_REGION_INDEX:
	case VFIO_PCI_VGA_REGION_INDEX:
		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = 0;
		info.flags = 0;
		break;
#ifdef DLB2_VDCM_MIGRATION_V1
	/* We only support DEV_REGIONS when migration is supported */
	case VFIO_PCI_NUM_REGIONS ...
		VFIO_PCI_NUM_REGIONS + DLB2_VDCM_NUM_DEV_REGIONS:
		caps.buf = NULL;
		caps.size = 0;
		dlb2_vdcm_dev_region_info(vdev,
					  &info,
					  &caps,
					  info.index -
					  VFIO_PCI_NUM_REGIONS);

		if (info.argsz < sizeof(info) + caps.size) {
			info.argsz = sizeof(info) + caps.size;
			info.cap_offset = 0;
		} else {
			vfio_info_cap_shift(&caps, sizeof(info));
			if (copy_to_user((void __user *)(arg + sizeof(info)),
					 caps.buf,
					 caps.size)) {
				kfree(caps.buf);
				return -EFAULT;
			}
			info.cap_offset = sizeof(info);
		}

		break;
#endif

	default:
		return -EINVAL;
	}

	return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

static long dlb2_vfio_device_get_irq_info(struct dlb2_vdev *vdev,
					  unsigned long arg)
{
	struct vfio_irq_info info;
	unsigned long minsz;

	minsz = offsetofend(struct vfio_irq_info, count);

	if (copy_from_user(&info, (void __user *)arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz || info.index >= VFIO_PCI_NUM_IRQS)
		return -EINVAL;

	/* Only (virtual) MSI-X interrupts are supported */
	if (info.index != VFIO_PCI_MSIX_IRQ_INDEX)
		return -EINVAL;

	info.flags = VFIO_IRQ_INFO_EVENTFD;

	info.count = dlb2_vdcm_num_irqs(vdev);

	info.flags |= VFIO_IRQ_INFO_NORESIZE;

	return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

static void dlb2_vdcm_disable_msix_entry(struct dlb2_vdev *vdev, int i)
{
	if (!vdev->msix_eventfd[i])
		return;

	eventfd_ctx_put(vdev->msix_eventfd[i]);

	vdev->msix_eventfd[i] = NULL;
}

static int dlb2_vdcm_disable_msix_entries(struct dlb2_vdev *vdev)
{
	int i;

	for (i = 0; i < VDCM_MSIX_MAX_ENTRIES; i++)
		dlb2_vdcm_disable_msix_entry(vdev, i);

	return 0;
}

static int dlb2_vdcm_set_eventfd(struct dlb2_vdev *vdev,
				 struct vfio_irq_set *hdr,
				 u32 *fds)
{
	struct device *dev = dlb2_mdev_parent_dev(vdev->mdev);
	struct eventfd_ctx *ctx;
	unsigned int i;

	for (i = hdr->start; i < hdr->start + hdr->count; i++) {
		int fd = fds[i - hdr->start];

		/* fd == -1: deassign the interrupt if cfg'ed or skip it */
		if (fd < 0) {
			if (vdev->msix_eventfd[i])
				dlb2_vdcm_disable_msix_entry(vdev, i);
			continue;
		}

		ctx = eventfd_ctx_fdget(fd);
		if (IS_ERR(ctx)) {
			dev_err(dev, "[%s()] eventfd_ctx_fdget failed\n",
				__func__);

			return PTR_ERR(ctx);
		}

		vdev->msix_eventfd[i] = ctx;
	}

	return 0;
}

static int dlb2_vdcm_trigger_interrupt(struct dlb2_vdev *vdev,
				       struct vfio_irq_set *hdr,
				       bool *trigger)
{
	bool data_none = hdr->flags & VFIO_IRQ_SET_DATA_NONE;
	unsigned int i;

	for (i = hdr->start; i < hdr->start + hdr->count; i++) {
		if (!vdev->msix_eventfd[i])
			continue;

		if (data_none || trigger[i - hdr->start])
			eventfd_signal(vdev->msix_eventfd[i], 1);
	}

	return 0;
}

static int dlb2_vdcm_set_msix_trigger(struct dlb2_vdev *vdev,
				      struct vfio_irq_set *hdr,
				      void *data)
{
	if (hdr->count == 0 && (hdr->flags & VFIO_IRQ_SET_DATA_NONE))
		return dlb2_vdcm_disable_msix_entries(vdev);

	if (hdr->flags & VFIO_IRQ_SET_DATA_EVENTFD)
		return dlb2_vdcm_set_eventfd(vdev, hdr, data);

	if (hdr->flags & (VFIO_IRQ_SET_DATA_BOOL | VFIO_IRQ_SET_DATA_NONE))
		return dlb2_vdcm_trigger_interrupt(vdev, hdr, data);

	return 0;
}

static long dlb2_vfio_device_set_irqs(struct dlb2_vdev *vdev,
				      unsigned long arg)
{
	struct vfio_irq_set hdr;
	size_t data_size = 0;
	unsigned long minsz;
	struct dlb2 *dlb2;
	u8 *data = NULL;
	int ret;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	minsz = offsetofend(struct vfio_irq_set, count);

	if (copy_from_user(&hdr, (void __user *)arg, minsz))
		return -EFAULT;

	if (hdr.argsz < minsz || hdr.index != VFIO_PCI_MSIX_IRQ_INDEX)
		return -EINVAL;

	ret = vfio_set_irqs_validate_and_prepare(&hdr,
						 dlb2_vdcm_num_irqs(vdev),
						 VFIO_PCI_NUM_IRQS,
						 &data_size);
	if (ret)
		return -EINVAL;

	/* hdr contains data, so copy it */
	if (data_size) {
		data = memdup_user((void __user *)(arg + minsz), data_size);
		if (IS_ERR(data))
			return PTR_ERR(data);
	}

	mutex_lock(&dlb2->resource_mutex);

	switch (hdr.flags & VFIO_IRQ_SET_ACTION_TYPE_MASK) {
	case VFIO_IRQ_SET_ACTION_MASK:
	case VFIO_IRQ_SET_ACTION_UNMASK:
		ret = -ENOTTY;
		break;
	case VFIO_IRQ_SET_ACTION_TRIGGER:
		ret = dlb2_vdcm_set_msix_trigger(vdev, &hdr, data);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&dlb2->resource_mutex);

	kfree(data);

	return ret;
}

static long dlb2_vfio_device_reset(struct dlb2_vdev *vdev)
{
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	mutex_lock(&dlb2->resource_mutex);

	dlb2_reset_vdev(&dlb2->hw, vdev->id);

	mutex_unlock(&dlb2->resource_mutex);

	return 0;
}

#ifdef DLB2_NEW_MDEV_IOMMUFD
static long dlb2_vdcm_ioctl(struct vfio_device *vfio_dev, unsigned int cmd,
			    unsigned long arg)
{
	struct dlb2_vdev *vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);
#else
static long dlb2_vdcm_ioctl(struct mdev_device *mdev, unsigned int cmd,
			    unsigned long arg)
{
	struct dlb2_vdev *vdev = dlb2_dev_get_drvdata(mdev_dev(mdev));
#endif
	switch (cmd) {
	case VFIO_DEVICE_GET_INFO:
		return dlb2_vfio_device_get_info(vdev, arg);
	case VFIO_DEVICE_GET_REGION_INFO:
		return dlb2_vfio_device_get_region_info(vdev, arg);
	case VFIO_DEVICE_GET_IRQ_INFO:
		return dlb2_vfio_device_get_irq_info(vdev, arg);
	case VFIO_DEVICE_SET_IRQS:
		return dlb2_vfio_device_set_irqs(vdev, arg);
	case VFIO_DEVICE_RESET:
		return dlb2_vfio_device_reset(vdev);
	default:
		break;
	}

	return -ENOTSUPP;
}

#ifndef DLB2_NEW_MDEV_IOMMUFD
static struct mdev_parent_ops dlb2_vdcm_ops = {
	.mdev_attr_groups       = dlb2_mdev_attr_groups,
	.supported_type_groups  = dlb2_mdev_type_groups,
	.create			= dlb2_vdcm_create,
	.remove			= dlb2_vdcm_remove,

#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
	.open			= dlb2_vdcm_open,
	.release		= dlb2_vdcm_release,
#else
	.open_device		= dlb2_vdcm_open,
	.close_device		= dlb2_vdcm_release,
#endif

	.read			= dlb2_vdcm_read,
	.write			= dlb2_vdcm_write,
	.mmap			= dlb2_vdcm_mmap,
	.ioctl			= dlb2_vdcm_ioctl,
};

#else

static unsigned int dlb2_vdcm_get_available(struct mdev_type *mtype)
{
	return 16;
}

static const struct vfio_device_ops dlb2_vdcm_ops = {
	.name = "vfio-dlb2",
	.open_device		= dlb2_vdcm_open,
	.close_device		= dlb2_vdcm_release,
	.bind_iommufd		= dlb2_vdcm_bind_iommufd,
	.unbind_iommufd		= dlb2_vdcm_unbind_iommufd,
	.attach_ioas		= dlb2_vdcm_attach_ioas,
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
	.attach_hwpt            = dlb2_vdcm_attach_hwpt,
	.detach_hwpt            = dlb2_vdcm_detach_hwpt,
#elif KERNEL_VERSION(6, 4, 0) >= LINUX_VERSION_CODE
	.attach_hwpt            = dlb2_vdcm_attach_hwpt,
#else
	.detach_ioas		= dlb2_vdcm_detach_ioas,
	.pasid_attach_ioas	= dlb2_vdcm_pasid_attach_ioas,
	.pasid_detach_ioas	= dlb2_vdcm_pasid_detach_ioas,
#endif
	.read			= dlb2_vdcm_read,
	.write			= dlb2_vdcm_write,
	.mmap			= dlb2_vdcm_mmap,
	.ioctl			= dlb2_vdcm_ioctl,
};

static struct mdev_driver dlb2_vdcm_driver = {
	.device_api = VFIO_DEVICE_API_PCI_STRING,
	.driver = {
		.name = "mdev-dlb2",
		.owner = THIS_MODULE,
		.mod_name = KBUILD_MODNAME,
		.dev_groups = dlb2_mdev_attr_groups,
	},
	.probe = dlb2_vdcm_probe,
	.remove = dlb2_vdcm_remove,
	.get_available = dlb2_vdcm_get_available,
};
#endif

#if KERNEL_VERSION(5, 11, 0) > LINUX_VERSION_CODE
#define PCI_EXT_CAP_ID_DVSEC	0x23	/* Designated Vendor-Specific */
/* Designated Vendor-Specific (DVSEC, PCI_EXT_CAP_ID_DVSEC) */
#define PCI_DVSEC_HEADER1		0x4 /* Designated Vendor-Specific Header1 */
#define PCI_DVSEC_HEADER2		0x8 /* Designated Vendor-Specific Header2 */
#endif

/*
 * Copy pci_siov_supported() and pci_find_dvsec() from intel-next here. Use them
 * to detect if SIOV is supported. In a guest VM, dlb2_pci_siov_supported()
 * should return -ENOTSUPP.
 *
 * These functions and #define PCI_DVSEC_ID_INTEL_SIOV may be removed once
 * they are upstreamed to the linux kernel.
 */

/**
 * pci_find_dvsec - return position of DVSEC with provided vendor and dvsec id
 * @dev: the PCI device
 * @vendor: Vendor for the DVSEC
 * @id: the DVSEC cap id
 *
 * Return the offset of DVSEC on success or -ENOTSUPP if not found
 */
static int dlb2_pci_find_dvsec(struct pci_dev *dev, u16 vendor, u16 id)
{
	u16 dev_vendor, dev_id;
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DVSEC);
	if (!pos)
		return -ENOTSUPP;

	while (pos) {
		pci_read_config_word(dev, pos + PCI_DVSEC_HEADER1, &dev_vendor);
		pci_read_config_word(dev, pos + PCI_DVSEC_HEADER2, &dev_id);
		if (dev_vendor == vendor && dev_id == id)
			return pos;

		pos = pci_find_next_ext_capability(dev, pos, PCI_EXT_CAP_ID_DVSEC);
	}

	return -ENOTSUPP;
}

#define PCI_DVSEC_ID_INTEL_SIOV	5

/*
 * A PCI express designated vendor specific extended capability is defined
 * in the section 3.7 of Intel scalable I/O virtualization technical spec
 * for system software and tools to detect endpoint devices supporting the
 * Intel scalable IO virtualization without host driver dependency.
 */

/**
 * pci_siov_supported - check if the device can use SIOV
 * @dev: the PCI device
 *
 * Returns true if the device supports SIOV,  false otherwise.
 */
static bool dlb2_pci_siov_supported(struct pci_dev *dev)
{
	return dlb2_pci_find_dvsec(dev, PCI_VENDOR_ID_INTEL,
				   PCI_DVSEC_ID_INTEL_SIOV) < 0 ? false : true;
}

int dlb2_mdev_driver_registered = 0;

int dlb2_vdcm_init(struct dlb2 *dlb2)
{
	struct pci_dev *pdev = dlb2->pdev;
#ifndef DLB2_SIOV_IMS_WORKAROUND
	struct ims_array_info ims_info;
#endif
	int ret;

	dlb2->vdcm_initialized = 0;

	if (!dlb2_pci_siov_supported(pdev)) {
		dev_info(&pdev->dev,
			 "[%s()]: SIOV not supported\n", __func__);
		return 0;
	}

#ifndef DLB2_NEW_MDEV_IOMMUFD
	ret = iommu_dev_enable_feature(&pdev->dev, IOMMU_DEV_FEAT_AUX);
	if (ret) {
		dev_info(&pdev->dev,
			 "[%s()] Failed to enable aux domains; no SIOV support\n",
			 __func__);
		goto aux_enable_fail;
	}
#endif

#ifndef DLB2_SIOV_IMS_WORKAROUND
	if (dlb2->hw_ver >= DLB2_HW_V2_5) {
		/*
		 * Initialize IMS for SIOV.
		 * CONFIG_IMS_MSI_ARRAY support is needed
		 */
		dlb2->ims_base = DLB2_CSR_REG_ADDR(&dlb2->hw, SYS_AI_ADDR_L(0));
		ims_info.max_slots = DLB2_MAX_NUM_LDB_PORTS +
				     DLB2_MAX_NUM_DIR_PORTS(dlb2->hw_ver);
		ims_info.slots = dlb2->ims_base;
		dlb2->ims_domain = pci_ims_array_create_msi_irq_domain(pdev,
								&ims_info);
		if (!dlb2->ims_domain) {
			dev_warn(&pdev->dev, "Fail to acquire IMS domain\n");
			iommu_dev_disable_feature(&pdev->dev, IOMMU_DEV_FEAT_AUX);
			return -ENODEV;
		}

		dev_info(&pdev->dev,
			 "[%s()]: IMS for SIOV initialized\n", __func__);
	}
#endif
	ida_init(&dlb2->vdev_ids);

#ifdef DLB2_NEW_MDEV_IOMMUFD
	if (!dlb2_mdev_driver_registered) {
		ret = mdev_register_driver(&dlb2_vdcm_driver);
		if (ret) {
			dev_info(&pdev->dev,
				 "[%s()]: mdev_reister_driver() failed\n", __func__);
			goto register_fail;
		}
	}

	dlb2->vdcm_mdev_types[0] = kzalloc(sizeof(struct mdev_type), GFP_KERNEL);
	dlb2->vdcm_mdev_types[0]->sysfs_name = "dlb";
	ret = mdev_register_parent(&dlb2->parent, &pdev->dev,
				   &dlb2_vdcm_driver, dlb2->vdcm_mdev_types,
				   1);
#else
	ret = mdev_register_device(&pdev->dev, &dlb2_vdcm_ops);
#endif
	if (ret) {
		dev_info(&pdev->dev,
			 "[%s()]: mdev_reister_device() failed\n", __func__);
		goto register_device_fail;
	}

	INIT_LIST_HEAD(&dlb2->vdev_list);
	dlb2->vdcm_initialized = 1;
	dlb2_mdev_driver_registered++;

	return 0;

register_device_fail:
#ifdef DLB2_NEW_MDEV_IOMMUFD
	mdev_unregister_driver(&dlb2_vdcm_driver);
register_fail:
#endif
	ida_destroy(&dlb2->vdev_ids);
#ifndef DLB2_SIOV_IMS_WORKAROUND
	irq_domain_remove(dlb2->ims_domain);
#endif
#ifndef DLB2_NEW_MDEV_IOMMUFD
	iommu_dev_disable_feature(&pdev->dev, IOMMU_DEV_FEAT_AUX);
aux_enable_fail:
#endif
	if (pdev->pasid_enabled)
		pci_disable_pasid(pdev);

	return ret;
}

void dlb2_vdcm_exit(struct pci_dev *pdev)
{
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);

	if (!dlb2->vdcm_initialized)
		return;

	dlb2_mdev_driver_registered--;
#ifdef DLB2_NEW_MDEV_IOMMUFD
	mdev_unregister_parent(&dlb2->parent);
	if (!dlb2_mdev_driver_registered)
		mdev_unregister_driver(&dlb2_vdcm_driver);
#else
	mdev_unregister_device(&pdev->dev);
#endif

	ida_destroy(&dlb2->vdev_ids);

#ifndef DLB2_SIOV_IMS_WORKAROUND
	irq_domain_remove(dlb2->ims_domain);
#endif

#ifndef DLB2_NEW_MDEV_IOMMUFD
	iommu_dev_disable_feature(&pdev->dev, IOMMU_DEV_FEAT_AUX);
#endif

	/* DLB does not support SIOV and SRIOV simultaneously. Many registers are shared
	 * between two modes. Disable pasid when exiting VDCM.
	 */
	if (pdev->pasid_enabled)
		pci_disable_pasid(pdev);
}

#endif /* CONFIG_INTEL_DLB2_SIOV */
