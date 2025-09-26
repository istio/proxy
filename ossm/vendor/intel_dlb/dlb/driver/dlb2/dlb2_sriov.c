// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2020 Intel Corporation

#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/pm_runtime.h>

#include "base/dlb2_resource.h"
#include "dlb2_main.h"
#include "dlb2_sriov.h"
#include "dlb2_vdcm.h"

static int dlb2_pci_sriov_enable(struct pci_dev *pdev, int num_vfs)
{
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);
	int ret, i;

	mutex_lock(&dlb2->resource_mutex);

#ifdef CONFIG_INTEL_DLB2_SIOV
	if (dlb2_hw_get_virt_mode(&dlb2->hw) == DLB2_VIRT_SIOV) {
		dev_err(&pdev->dev,
			"dlb2 driver supports either SR-IOV or Scalable IOV, not both.\n");
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	dlb2_vdcm_exit(dlb2->pdev);
#endif

	dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_SRIOV);

	mutex_unlock(&dlb2->resource_mutex);

	/*
	 * Increment the device's usage count and immediately wake it if it was
	 * suspended, assuming pci_enable_sriov() is successful.
	 */
	pm_runtime_get_sync(&pdev->dev);

	ret = pci_enable_sriov(pdev, num_vfs);
	if (ret) {
		pm_runtime_put_sync_suspend(&pdev->dev);
		dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);
		return ret;
	}

	/* Create sysfs files for the newly created VFs */
	for (i = 0; i < num_vfs; i++) {
		ret = sysfs_create_group(&pdev->dev.kobj, dlb2_vf_attrs[i]);
		if (ret) {
			dev_err(&pdev->dev,
				"Internal error: failed to create VF sysfs attr groups.\n");
			pci_disable_sriov(pdev);
			pm_runtime_put_sync_suspend(&pdev->dev);
			dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);
			return ret;
		}
	}

	mutex_lock(&dlb2->resource_mutex);

	dlb2->num_vfs = num_vfs;

	mutex_unlock(&dlb2->resource_mutex);

	return num_vfs;
}

/* Returns the number of host-owned virtual devices in use. */
static int dlb2_host_vdevs_in_use(void)
{
	struct dlb2 *dev;
	int num = 0;

	mutex_lock(&dlb2_driver_mutex);

	list_for_each_entry(dev, &dlb2_dev_list, list) {
		if (DLB2_IS_VF(dev) && dlb2_in_use(dev))
			num++;
	}

	mutex_unlock(&dlb2_driver_mutex);

	return num;
}

static int dlb2_pci_sriov_disable(struct pci_dev *pdev, int num_vfs)
{
	struct dlb2 *dlb2 = pci_get_drvdata(pdev);
	int i;

	mutex_lock(&dlb2->resource_mutex);

	/*
	 * pci_vfs_assigned() checks for VM-owned VFs, but doesn't catch
	 * application-owned VFs in the host -- dlb2_host_vdevs_in_use()
	 * detects that.
	 */
	if (pci_vfs_assigned(pdev) || dlb2_host_vdevs_in_use()) {
		dev_err(&pdev->dev,
			"Unable to disable VFs because one or more are in use.\n");
		mutex_unlock(&dlb2->resource_mutex);
		return -EINVAL;
	}

	for (i = 0; i < pci_num_vf(pdev); i++) {
		/*
		 * If the VF driver didn't exit cleanly, its resources will
		 * still be locked.
		 */
		dlb2_unlock_vdev(&dlb2->hw, i);

		if (dlb2_reset_vdev_resources(&dlb2->hw, i))
			dev_err(&pdev->dev,
				"[%s()] Internal error: failed to reset VF resources\n",
				__func__);

		/* Remove sysfs files for the VFs */
		sysfs_remove_group(&pdev->dev.kobj, dlb2_vf_attrs[i]);
	}

	/*
	 * When a VF is disabled, it will issue an "unregister" mailbox command,
	 * whose ISR requires the PF driver to acquire the resource mutex.
	 *
	 * The PCI layer is holding the device lock during this time, ensuring
	 * that a user cannot invoke dlb2_pci_sriov_configure() again in
	 * parallel.  We must disable SR-IOV before resetting the virt_mode to
	 * ensure that user-space cannot create any Scalable IOV virtual
	 * devices (which requires virt_mode == DLB2_VIRT_NONE) while SR-IOV is
	 * enabled.
	 */
	mutex_unlock(&dlb2->resource_mutex);

	pci_disable_sriov(pdev);

	mutex_lock(&dlb2->resource_mutex);

	dlb2_hw_set_virt_mode(&dlb2->hw, DLB2_VIRT_NONE);

	dlb2->num_vfs = 0;

	mutex_unlock(&dlb2->resource_mutex);

	/*
	 * Decrement the device's usage count and suspend it if the
	 * count reaches zero.
	 */
	pm_runtime_put_sync_suspend(&pdev->dev);

#ifdef CONFIG_INTEL_DLB2_SIOV
	/* Initialize VDCM and MDEV for SIOV */
	if (dlb2_vdcm_init(dlb2))
		dev_err(&pdev->dev, "vdcm init failed.\n");
#endif
	return 0;
}

int dlb2_pci_sriov_configure(struct pci_dev *pdev, int num_vfs)
{
	if (num_vfs)
		return dlb2_pci_sriov_enable(pdev, num_vfs);
	else
		return dlb2_pci_sriov_disable(pdev, num_vfs);
}
