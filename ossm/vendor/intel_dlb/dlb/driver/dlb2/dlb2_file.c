// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2020 Intel Corporation

#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE
#include <linux/pseudo_fs.h>
#else
#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_VERSION(8, 4) <= RHEL_RELEASE_CODE
#include <linux/pseudo_fs.h>
#endif
#endif
#endif

#include "dlb2_file.h"

/*
 * dlb2 tracks its memory mappings so it can revoke them when an FLR is
 * requested and user-space cannot be allowed to access the device. To achieve
 * that, the driver creates a single inode through which all driver-created
 * files can share a struct address_space, and unmaps the inode's address space
 * during the reset preparation phase. Since the anon inode layer shares its
 * inode with multiple kernel components, we cannot use that here.
 *
 * Doing so requires a custom pseudo-filesystem to allocate the inode. The FS
 * and the inode are allocated on demand when a file is created, and both are
 * freed when the last such file is closed.
 *
 * This is inspired by other drivers (cxl, dax, mem) and the anon inode layer.
 */
static int dlb2_fs_cnt;
static struct vfsmount *dlb2_vfs_mount;

#define DLB2FS_MAGIC 0x444C4232 /* ASCII for DLB2 */

#ifndef RHEL_RELEASE_CODE
#if KERNEL_VERSION(5, 3, 0) > LINUX_VERSION_CODE
static struct dentry *dlb2_fs_mount(struct file_system_type *fs_type, int flags,
				    const char *dev_name, void *data)
{
	return mount_pseudo(fs_type, "dlb:", NULL, NULL, DLB2FS_MAGIC);
}
#else
static int dlb2_init_fs_context(struct fs_context *fc)
{
	return init_pseudo(fc, DLB2FS_MAGIC) ? 0 : -ENOMEM;
}
#endif
#else
#if RHEL_RELEASE_VERSION(8, 4) > RHEL_RELEASE_CODE
static struct dentry *dlb2_fs_mount(struct file_system_type *fs_type, int flags,
				    const char *dev_name, void *data)
{
	return mount_pseudo(fs_type, "dlb:", NULL, NULL, DLB2FS_MAGIC);
}
#else
static int dlb2_init_fs_context(struct fs_context *fc)
{
	return init_pseudo(fc, DLB2FS_MAGIC) ? 0 : -ENOMEM;
}
#endif
#endif

static struct file_system_type dlb2_fs_type = {
	.name	 = "dlb2",
	.owner	 = THIS_MODULE,
#ifndef RHEL_RELEASE_CODE
#if KERNEL_VERSION(5, 3, 0) > LINUX_VERSION_CODE
	.mount	 = dlb2_fs_mount,
#else
	.init_fs_context = dlb2_init_fs_context,
#endif
#else
#if RHEL_RELEASE_VERSION(8, 4) > RHEL_RELEASE_CODE
	.mount	 = dlb2_fs_mount,
#else
	.init_fs_context = dlb2_init_fs_context,
#endif
#endif
	.kill_sb = kill_anon_super,
};

/* Allocate an anonymous inode. Must hold the resource mutex while calling. */
static struct inode *dlb2_alloc_inode(struct dlb2 *dlb2)
{
	struct inode *inode;
	int ret;

	/* Increment the pseudo-FS's refcnt and (if not already) mount it. */
	ret = simple_pin_fs(&dlb2_fs_type, &dlb2_vfs_mount, &dlb2_fs_cnt);
	if (ret < 0) {
		dev_err(dlb2->dev,
			"[%s()] Cannot mount pseudo filesystem: %d\n",
			__func__, ret);
		return ERR_PTR(ret);
	}

	dlb2->inode_cnt++;

	if (dlb2->inode_cnt > 1) {
		ihold(dlb2->inode);
		return dlb2->inode;
	}

	inode = alloc_anon_inode(dlb2_vfs_mount->mnt_sb);
	if (IS_ERR(inode)) {
		dev_err(dlb2->dev,
			"[%s()] Cannot allocate inode: %d\n",
			__func__, ret);
		dlb2->inode_cnt = 0;
		simple_release_fs(&dlb2_vfs_mount, &dlb2_fs_cnt);
	}

	dlb2->inode = inode;

	return inode;
}

/*
 * Decrement the inode reference count and release the FS. Intended for
 * unwinding dlb2_alloc_inode(). Must hold the resource mutex while calling.
 */
static void dlb2_free_inode(struct inode *inode)
{
	iput(inode);
	simple_release_fs(&dlb2_vfs_mount, &dlb2_fs_cnt);
}

/*
 * Release the FS. Intended for use in a file_operations release callback,
 * which decrements the inode reference count separately. Must hold the
 * resource mutex while calling.
 */
void dlb2_release_fs(struct dlb2 *dlb2)
{
	mutex_lock(&dlb2_driver_mutex);

	simple_release_fs(&dlb2_vfs_mount, &dlb2_fs_cnt);

	dlb2->inode_cnt--;

	if (dlb2->inode_cnt == 0)
		dlb2->inode = NULL;

	mutex_unlock(&dlb2_driver_mutex);
}

/*
 * Allocate a file with the requested flags, file operations, and name that
 * uses the device's shared inode. Must hold the resource mutex while calling.
 *
 * Caller must separately allocate an fd and install the file in that fd.
 */
struct file *dlb2_getfile(struct dlb2 *dlb2,
			  int flags,
			  const struct file_operations *fops,
			  const char *name)
{
	struct inode *inode;
	struct file *f;

	if (!try_module_get(THIS_MODULE))
		return ERR_PTR(-ENOENT);

	mutex_lock(&dlb2_driver_mutex);

	inode = dlb2_alloc_inode(dlb2);
	if (IS_ERR(inode)) {
		mutex_unlock(&dlb2_driver_mutex);
		module_put(THIS_MODULE);
		return ERR_CAST(inode);
	}

	f = alloc_file_pseudo(inode, dlb2_vfs_mount, name, flags, fops);
	if (IS_ERR(f)) {
		dlb2_free_inode(inode);
		mutex_unlock(&dlb2_driver_mutex);
		module_put(THIS_MODULE);
		return f;
	}

	mutex_unlock(&dlb2_driver_mutex);

	return f;
}
