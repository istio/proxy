/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2018-2020 Intel Corporation
 */

#ifndef __DLB2_VDCM_H
#define __DLB2_VDCM_H

#include <linux/mdev.h>
#include <linux/vfio.h>
#include <linux/version.h>
#include "base/dlb2_hw_types.h"

#ifdef OPENEULER_VERSION_CODE
#if OPENEULER_VERSION(2203, 1) <= OPENEULER_VERSION_CODE
#define DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN
#endif
#elif KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE
#define DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN
#endif

#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE
#ifndef CONFIG_IOMMUFD
#undef CONFIG_INTEL_DLB2_SIOV
#else
#define DLB2_NEW_MDEV_IOMMUFD
#endif
#endif

#ifdef CONFIG_INTEL_DLB2_SIOV

#include <linux/pci.h>

#ifdef DLB2_NEW_MDEV_IOMMUFD
#include <linux/iommufd.h>
#include <linux/vfio_pci_core.h>
#if KERNEL_VERSION(5, 19, 0) >= LINUX_VERSION_CODE
#include <linux/intel-iommu.h>
#endif
#endif

#define DLB2_SIOV_IMS_WORKAROUND

#ifdef VFIO_REGION_TYPE_MIGRATION
#define DLB2_VDCM_MIGRATION_V1
#else
#if KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE
#define DLB2_VDCM_MIGRATION_V2
#endif
#endif

/************************/
/****** mdev attrs ******/
/************************/

static struct device *dlb2_mdev_parent_dev(struct mdev_device *mdev)
{
#ifndef DLB2_NEW_MDEV_IOMMUFD
        return mdev_parent_dev(mdev);
#else
        return mdev->type->parent->dev;
#endif
}

static inline struct pci_dev *mdev_get_pdev(struct mdev_device *mdev)
{
        struct device *dev = dlb2_mdev_parent_dev(mdev);

        return container_of(dev, struct pci_dev, dev);
}

static inline struct dlb2 *mdev_get_dlb2(struct mdev_device *mdev)
{
        return pci_get_drvdata(mdev_get_pdev(mdev));
}

static inline struct dlb2_vdev *dlb2_dev_get_drvdata(struct device *dev)
{
#ifndef DLB2_NEW_MDEV_IOMMUFD
        return mdev_get_drvdata(mdev_from_dev(dev));
#else
        return dev_get_drvdata(dev);
#endif
}

static inline void dlb2_dev_set_drvdata(struct device *dev, struct dlb2_vdev *vdev)
{
#ifndef DLB2_NEW_MDEV_IOMMUFD
        mdev_set_drvdata(mdev_from_dev(dev), vdev);
#else
        dev_set_drvdata(dev, vdev);
#endif
}

/* Helper macros copied from pci/vfio_pci_private.h */
#ifndef VFIO_PCI_OFFSET_SHIFT
#define VFIO_PCI_OFFSET_SHIFT 40
#define VFIO_PCI_OFFSET_TO_INDEX(off) ((off) >> VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_INDEX_TO_OFFSET(index) ((u64)(index) << VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_OFFSET_MASK (((u64)1 << VFIO_PCI_OFFSET_SHIFT) - 1)
#endif

#define VDCM_MSIX_MSG_CTRL_OFFSET       (0x60 + PCI_MSIX_FLAGS)
#define VDCM_MSIX_MAX_ENTRIES           256
/* Note: VDCM_MSIX_TBL_OFFSET must be in sync with dlb2_pci_config. */
#define VDCM_MSIX_TBL_OFFSET            0x01000000
#define VDCM_MSIX_TBL_ENTRY_SZ          16
/* Note: VDCM_MSIX_TBL_SZ_BYTES must be in sync with dlb2_pci_config. */
#define VDCM_MSIX_TBL_SZ_BYTES          (VDCM_MSIX_TBL_ENTRY_SZ * \
                                         VDCM_MSIX_MAX_ENTRIES)
#define VDCM_MSIX_TBL_END_OFFSET        (VDCM_MSIX_TBL_OFFSET + \
                                         VDCM_MSIX_TBL_SZ_BYTES - 1)
#define VDCM_MSIX_PBA_OFFSET            (VDCM_MSIX_TBL_OFFSET + \
                                         VDCM_MSIX_TBL_SZ_BYTES)
#define VDCM_MSIX_PBA_SZ_QWORD          (VDCM_MSIX_MAX_ENTRIES / 64)
#define VDCM_MSIX_PBA_SZ_BYTES          (VDCM_MSIX_MAX_ENTRIES / 8)
#define VDCM_MSIX_PBA_END_OFFSET        (VDCM_MSIX_PBA_OFFSET + \
                                         VDCM_MSIX_PBA_SZ_BYTES - 1)

#define VDCM_PCIE_DEV_CTRL_OFFSET       (0x6C + PCI_EXP_DEVCTL)

#define VDCM_MBOX_MSIX_VECTOR           0

/* Use DLB 2.5 dir ports for worst-case array sizing */
#define VDCM_MAX_NUM_IMS_ENTRIES        (DLB2_MAX_NUM_LDB_PORTS + \
                                         DLB2_MAX_NUM_DIR_PORTS(DLB2_HW_V2_5))

#define DLB2_LM_XMIT_CMD_SIZE_SIZE      4
#define DLB2_LM_CMD_SAVE_DATA_SIZE      64
#define DLB2_LM_MIGRATION_CMD_SIZE   (4096*8)

#if KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE
#define DLB2_VDCM_MIGRATION_REGION      0
#define DLB2_VDCM_NUM_DEV_REGIONS       1
#else
#define DLB2_VDCM_NUM_DEV_REGIONS       0
#endif

struct dlb2;
int dlb2_vdcm_init(struct dlb2 *dlb2);
void dlb2_vdcm_exit(struct pci_dev *pdev);
void dlb2_save_cmd_for_migration(struct dlb2 *dlb2, int vdev_id, u8 *data, int data_size);
void dlb2_handle_migration_cmds(struct dlb2 *dlb2, int vdev_id, u8 *data);


struct dlb2_vdcm_migration {
	/* Page aligned migration info size */
	int size;
	void *mstate_mgr;
	struct vfio_device_migration_info *minfo;
	int mdata_size;
	int allocated_cmd_size;

#ifdef DLB2_VDCM_MIGRATION_V2
        /* lock for migration state */
        struct mutex lock;
        struct file *filp;
        /* lock for migration data file */
        struct mutex f_lock;
        bool f_activated;
#endif
};

struct dlb2_ims_irq_entry {
	struct dlb2_vdev *vdev;
	unsigned int int_src;
	u32 cq_id;
	bool is_ldb;
	bool reenable;
	bool in_use;
	u32 irq;
};

struct dlb2_vdev {
#ifdef DLB2_NEW_MDEV_IOMMUFD
	struct vfio_device vfio_dev;
	struct iommufd_device *idev;
	int iommufd;
	struct xarray pasid_xa;
#endif

	struct list_head next;
	bool released;
	unsigned int id;
	struct mdev_device *mdev;
	struct notifier_block iommu_notifier;
	struct notifier_block group_notifier;
	struct work_struct release_work;
	struct eventfd_ctx *msix_eventfd[VDCM_MSIX_MAX_ENTRIES];

	/* IOMMU */
	ioasid_t pasid;
#if defined(DLB2_USE_VFIO_GROUP_IOMMU_DOMAIN) && !defined(DLB2_NEW_MDEV_IOMMUFD)
	struct vfio_group *vfio_group;
#endif

	/* DLB resources */
	u32 num_ldb_ports;
	u32 num_dir_ports;

	/* Config region */
	u32 num_regions;
	u8 cfg[PCI_CFG_SPACE_SIZE];

	/* Software mailbox */
	u8 *pf_to_vdev_mbox;
	u8 *vdev_to_pf_mbox;

	/* BAR 0 */
	u64 bar0_addr;
	u8 msix_table[VDCM_MSIX_TBL_SZ_BYTES];
	u64 msix_pba[VDCM_MSIX_PBA_SZ_QWORD];

	/* IMS IRQs */
	int group_id;
	struct dlb2_ims_irq_entry irq_entries[VDCM_MAX_NUM_IMS_ENTRIES];
	u32 ims_idx[VDCM_MAX_NUM_IMS_ENTRIES];

	u32 ldb_ports_mask[DLB2_MAX_NUM_LDB_PORTS / 32];
	u16 ldb_ports_phys_id[DLB2_MAX_NUM_LDB_PORTS];

	u16 dir_ports_phys_id[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	u32 dir_ports_mask[DLB2_MAX_NUM_DIR_PORTS_V2_5 / 32];

	/* VM Live Migration */
	struct dlb2_vdcm_migration migration;
	struct dlb2_migrate_t mig_state;

};

int dlb2_vdcm_migration_init(struct dlb2_vdev *vdev, int state_size);
void dlb2_save_cmd_for_migration(struct dlb2 *dlb2, int vdev_id, u8 *data, int data_size);

#ifdef DLB2_VDCM_MIGRATION_V1
ssize_t
dlb2_vdcm_vdev_dev_region_rw(struct dlb2_vdev *vdev, int reg_idx,
                            u64 pos, char *buf, size_t count,
                            bool is_write);
int dlb2_vdcm_dev_region_info(struct dlb2_vdev *vdev,
                                     struct vfio_region_info *info,
                                     struct vfio_info_cap *caps,
                                     int reg_idx);
#endif

#endif /* CONFIG_INTEL_DLB2_SIOV */
#endif /* __DLB2_VDCM_H */
