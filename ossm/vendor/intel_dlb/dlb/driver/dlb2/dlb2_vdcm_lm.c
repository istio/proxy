// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2018-2020 Intel Corporation

#include <linux/eventfd.h>
#include <linux/version.h>
#include <linux/vfio.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include "dlb2_vdcm.h"

#ifdef CONFIG_INTEL_DLB2_SIOV
#include "base/dlb2_resource.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_mbox.h"
#include "dlb2_main.h"

#ifdef DLB2_VDCM_MIGRATION_V1
static int dlb2_vdev_migration_stop(struct dlb2_vdev *vdev, u8 *data, u32 data_size)
{
	int ret = 0;
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	if (data) {
		dlb2_lm_pause_device(&dlb2->hw, 1, vdev->id, vdev->mig_state.src_vm_state);

		ret = data_size;
	}

	return ret;
}

static int dlb2_vdev_migration_resume(struct dlb2_vdev *vdev, u8 *data, u32 data_size)
{
	u8 mbox_data[DLB2_VF2PF_REQ_BYTES];
	uint32_t cmd_data_size;
	struct dlb2 *dlb2;
	int i;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	if (data) {
		cmd_data_size = *((uint32_t *)data);
		dev_info(mdev_dev(vdev->mdev), "%s: total data size = %d, cmd_data_size = %d\n",
			 __func__, data_size, cmd_data_size);

		data += DLB2_LM_XMIT_CMD_SIZE_SIZE;
		for (i = 0; i < cmd_data_size / DLB2_LM_CMD_SAVE_DATA_SIZE; i++) {
			u8 *ptr = data + i * DLB2_LM_CMD_SAVE_DATA_SIZE;
			struct dlb2 *dlb2;

			dlb2 = mdev_get_dlb2(vdev->mdev);
			dev_info(mdev_dev(vdev->mdev), "%s: resuming  cmd = %s, i= %d, data_size = %d \n",
				 __func__, dlb2_mbox_cmd_type_strings[DLB2_MBOX_CMD_TYPE(ptr)],
				 i, data_size);
			memcpy(mbox_data, ptr, DLB2_LM_CMD_SAVE_DATA_SIZE);
			dlb2_handle_migration_cmds(dlb2, vdev->id, mbox_data);
		}

		dlb2_lm_restore_device(&dlb2->hw, 1, vdev->id, vdev->mig_state.dst_vm_state);
	}

	return data_size;
}

static int dlb2_vdev_migration_pre_migrate(struct dlb2_vdev *vdev, u8 *data, u32 data_size)
{
	return 0;
}

/**
 * dlb2_vdcm_vdev_mstate_transite() - state machine for migration tgransitions.
 * @vdev: pointer to struct dlb2_vdev.
 * @new_state: new state for the live migration.
 *
 * This function implements the FSM for the live migration. 
 *
 *
 * Return: 0 if successful
 */
static int dlb2_vdcm_vdev_mstate_transite(struct dlb2_vdev *vdev,
					 u32 new_state)
{
	struct vfio_device_migration_info *minfo = vdev->migration.minfo;
	int ret = 0;
	u8 *data;

	dev_info(mdev_dev(vdev->mdev),
		 "%s: state (0x%x -> 0x%x), state size is %llu\n",
		 __func__, minfo->device_state, new_state, minfo->data_size);

	data = (u8 *)minfo + minfo->data_offset;
	switch (new_state) {
	case VFIO_DEVICE_STATE_RUNNING:
		if (minfo->device_state & VFIO_DEVICE_STATE_RESUMING)
			ret = dlb2_vdev_migration_resume(vdev, data, vdev->migration.mdata_size);
		else if (!(minfo->device_state & VFIO_DEVICE_STATE_RUNNING))
			ret = dlb2_vdev_migration_resume(vdev, NULL, 0);
		vdev->migration.mdata_size = 0;
		break;
	case VFIO_DEVICE_STATE_RUNNING | VFIO_DEVICE_STATE_SAVING:
		ret = dlb2_vdev_migration_pre_migrate(vdev, data, minfo->data_size);
		break;
	case VFIO_DEVICE_STATE_SAVING:
		if (minfo->device_state & VFIO_DEVICE_STATE_RUNNING) {
			ret = dlb2_vdev_migration_stop(vdev, NULL, 0);
			break;
		}
		ret = dlb2_vdev_migration_stop(vdev, data, minfo->data_size);
		if (ret > 0) {
			minfo->pending_bytes = ret;
			minfo->data_size = ret;
		}
		break;
	case VFIO_DEVICE_STATE_STOP:
		ret = dlb2_vdev_migration_stop(vdev, NULL, 0);
		break;
	case VFIO_DEVICE_STATE_RESUMING:
		/* wait until all data are received before restoring the DLB state */
		break;
	default:
		dev_warn(mdev_dev(vdev->mdev), "unknown state %u\n",
			 new_state);
		break;
	}

	if (ret >= 0)
		minfo->device_state = new_state;

	return ret;
}

ssize_t
dlb2_vdcm_vdev_dev_region_rw(struct dlb2_vdev *vdev, int reg_idx,
			    u64 pos, char *buf, size_t count,
			    bool is_write)
{
	struct vfio_device_migration_info *minfo = vdev->migration.minfo;

	if (reg_idx != DLB2_VDCM_MIGRATION_REGION) {
		dev_err(mdev_dev(vdev->mdev), "Unsupported dev region%d rw\n",
			reg_idx);
		return -EIO;
	}

	/*
	 *dev_dbg(mdev_dev(vdev->mdev), "%s : %s %lu %s %llu at dev_region %d, pending %llu\n",
	 *	__func__, is_write ? "writing" : "reading",
	 *	count, is_write ? "to" : "from",
	 *	pos, reg_idx, minfo->pending_bytes);
	 */

	if (pos + count < pos ||
	    (pos + count > minfo->data_size + minfo->data_offset)) {
		dev_err(mdev_dev(vdev->mdev), "Access %llu is out of range(%llu)\n",
			pos + count, minfo->data_size + minfo->data_offset);
		return 0;
	}

	if (!is_write) {
		/* read saved data from located memory space */
		memcpy(buf, (u8 *)minfo + pos, count);
		if (pos >= minfo->data_offset)
			minfo->pending_bytes -= count;
	} else {
		/* check to see if it is a state change request*/
		if (pos == offsetof(struct vfio_device_migration_info, device_state)) {
			int ret;
			u32 new_state;

			memcpy(&new_state, buf, count);
			ret = dlb2_vdcm_vdev_mstate_transite(vdev, new_state);
			if (ret < 0)
				return ret;
		} else {
			/* MIGTODO: Ignore the write to minfo RO field.
			 *
			 * Use mdata_size in destination to record the number of bytes of
			 * data received so far.
			 */
			memcpy((u8 *)minfo + pos, buf, count);
			vdev->migration.mdata_size += count;
		}
	}

	return count;
}

static int dlb2_vdcm_populate_mregion_info(struct dlb2_vdev *vdev,
					   struct vfio_region_info *info,
					   struct vfio_info_cap *caps)
{
	struct vfio_region_info_cap_type cap_type = {
		.header.id = VFIO_REGION_INFO_CAP_TYPE,
		.header.version = 1,
		.type = VFIO_REGION_TYPE_MIGRATION,
		.subtype = VFIO_REGION_SUBTYPE_MIGRATION
	};

	info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
	info->size = vdev->migration.size;
	info->flags = VFIO_REGION_INFO_FLAG_READ |
		      VFIO_REGION_INFO_FLAG_WRITE |
		      VFIO_REGION_INFO_FLAG_CAPS;

	return vfio_info_add_capability(caps, &cap_type.header,
					sizeof(cap_type));
}

int dlb2_vdcm_dev_region_info(struct dlb2_vdev *vdev,
				     struct vfio_region_info *info,
				     struct vfio_info_cap *caps,
				     int reg_idx)
{
	if (reg_idx != DLB2_VDCM_MIGRATION_REGION) {
		dev_err(mdev_dev(vdev->mdev), "Unsupported dev region %d\n",
			reg_idx);
		return -EINVAL;
	}

	return dlb2_vdcm_populate_mregion_info(vdev, info, caps);
}

#elif defined DLB2_VDCM_MIGRATION_V2

#if(0)
/* Reuse the structure from vfio.h in kernel 5.15.
 * It is still defined in vfio.h, but will be deprecated.
 * Once it happens, we need to define our own structure.
 */
struct vfio_device_migration_info {
	__u32 device_state;         /* VFIO device state */
	__u32 reserved;
	__u64 pending_bytes;
	__u64 data_offset;
	__u64 data_size;
};
#endif

static int dlb2_vdcm_mig_release(struct inode *inode,
				     struct file *filp)
{
	struct dlb2_vdev *vdev = filp->private_data;
	struct dlb2_vdcm_migration *mig = &vdev->migration;

	mutex_lock(&mig->f_lock);
	mig->f_activated = 0;
	mutex_unlock(&mig->f_lock);
	return 0;
}

static ssize_t dlb2_vdcm_mig_write(struct file *filp,
				   const char __user *buf,
				   size_t len, loff_t *pos)
{
	struct dlb2_vdev *vdev = filp->private_data;
	struct dlb2_vdcm_migration *mig = &vdev->migration;
	u8 *state_data = (u8 *)mig->minfo + mig->minfo->data_offset;
	int state_size = mig->minfo->data_size;
	ssize_t ret = 0;
	loff_t end;

	if (pos)
		return -ESPIPE;
	pos = &filp->f_pos;

	if (*pos < 0 ||
	    check_add_overflow((loff_t)len, *pos, &end) ||
	    end > state_size) {
		dev_err(mdev_dev(vdev->mdev), "%s: write state pos %lld with len %lu out of range %d.\n",
			__func__, *pos, len, state_size);
		return -EINVAL;
	}
	mutex_lock(&mig->f_lock);
	if (!mig->f_activated) {
		dev_err(mdev_dev(vdev->mdev), "%s: mig file is not activated.\n",
			__func__);
		ret = -ENODEV;
		goto out;
	}

	if (copy_from_user(state_data + *pos, buf, len)) {
		ret = -EFAULT;
		goto out;
	}

	*pos += len;
	ret = len;
out:
	mutex_unlock(&mig->f_lock);
	return ret;
}

static ssize_t dlb2_vdcm_mig_read(struct file *filp, char __user *buf,
				   size_t len, loff_t *pos)
{
	struct dlb2_vdev *vdev = filp->private_data;
	struct dlb2_vdcm_migration *mig = &vdev->migration;
	u8 *state_data = (u8 *)mig->minfo + mig->minfo->data_offset;
	int state_size = mig->minfo->data_size;
	ssize_t ret = 0;

	if (pos)
		return -ESPIPE;

	pos = &filp->f_pos;

	mutex_lock(&mig->f_lock);

	if (!mig->f_activated) {
		dev_err(mdev_dev(vdev->mdev), "%s: mig file is not activated.\n",
			__func__);
		ret = -ENODEV;
		goto out;
	}

	if (*pos < 0 || *pos > state_size) {
		dev_err(mdev_dev(vdev->mdev), "%s: read state pos %lld out of range %d.\n",
			__func__, *pos, state_size);
		ret = -EINVAL;
		goto out;
	}

	len = min_t(size_t, state_size - *pos, len);
	if (!len)
		goto out;

	if (copy_to_user(buf, state_data + *pos, len)) {
		ret = -EFAULT;
		goto out;
	}
	*pos += len;
	ret = len;
out:
	mutex_unlock(&mig->f_lock);
	return ret;
}

static struct file *
dlb2_vdcm_get_mig_file(struct dlb2_vdcm_migration *mig,
		       struct dlb2_vdev *vdev,
		       const struct file_operations *fops,
		       int flags)
{
	dev_info(mdev_dev(vdev->mdev), "%s: filp is 0x%llx\n",
		 __func__, (u64)mig->filp);
	mig->filp = anon_inode_getfile("vdev_mig", fops, vdev, flags);
	if (IS_ERR(mig->filp)) {
		dev_err(mdev_dev(vdev->mdev), "%s: failed to getfile\n",
			__func__);
		return NULL;
	}
	get_file(mig->filp);
	stream_open(mig->filp->f_inode, mig->filp);
	mig->f_activated = 1;

	return mig->filp;
}

static int
dlb2_vdcm_put_mig_file(struct dlb2_vdcm_migration *mig,
		       struct dlb2_vdev *vdev)
{
	dev_info(mdev_dev(vdev->mdev), "%s: filp is 0x%llx\n",
		 __func__, (u64)mig->filp);
	if (mig->filp) {
		mig->f_activated = 0;
		fput(mig->filp);
		mig->filp = NULL;
	}
	return 0;
}

static const struct file_operations dlb2_vdcm_save_fops = {
	.owner = THIS_MODULE,
	.read = dlb2_vdcm_mig_read,
	.release = dlb2_vdcm_mig_release,
	.llseek = no_llseek,
};

static const struct file_operations dlb2_vdcm_resume_fops = {
	.owner = THIS_MODULE,
	.write = dlb2_vdcm_mig_write,
	.release = dlb2_vdcm_mig_release,
	.llseek = no_llseek,
};

static int dlb2_vdev_migration_stop(struct dlb2_vdev *vdev, u8 *data, u32 data_size)
{
	int ret = 0;
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	if (data) {
		dlb2_lm_pause_device(&dlb2->hw, 1, vdev->id, vdev->mig_state.src_vm_state);

		ret = data_size;
	}

	return ret;
}

static int dlb2_vdev_migration_resume(struct dlb2_vdev *vdev, u8 *data, u32 data_size)
{
	u8 mbox_data[DLB2_VF2PF_REQ_BYTES];
	uint32_t cmd_data_size;
	struct dlb2 *dlb2;
	int i;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	if (data) {
		cmd_data_size = *((uint32_t *)data);
		dev_info(mdev_dev(vdev->mdev), "%s: total data size = %d, cmd_data_size = %d\n",
			 __func__, data_size, cmd_data_size);

		data += DLB2_LM_XMIT_CMD_SIZE_SIZE;
		for (i = 0; i < cmd_data_size / DLB2_LM_CMD_SAVE_DATA_SIZE; i++) {
			u8 *ptr = data + i * DLB2_LM_CMD_SAVE_DATA_SIZE;
			struct dlb2 *dlb2;

			dlb2 = mdev_get_dlb2(vdev->mdev);
			dev_info(mdev_dev(vdev->mdev), "%s: resuming  cmd = %s, i= %d, data_size = %d \n",
				 __func__, dlb2_mbox_cmd_type_strings[DLB2_MBOX_CMD_TYPE(ptr)],
				 i, data_size);
			memcpy(mbox_data, ptr, DLB2_LM_CMD_SAVE_DATA_SIZE);
			dlb2_handle_migration_cmds(dlb2, vdev->id, mbox_data);
		}

		dlb2_lm_restore_device(&dlb2->hw, 1, vdev->id, vdev->mig_state.dst_vm_state);
	}

	return data_size;
}

static struct file *
_dlb2_vdcm_set_device_state(struct dlb2_vdev *vdev, u32 new)
{
	struct vfio_device_migration_info *minfo = vdev->migration.minfo;
	struct dlb2_vdcm_migration *mig = &vdev->migration;
	u32 cur = mig->minfo->device_state;
	const struct file_operations *fops;
	int ret = 0;
	u8 *data;

	dev_info(mdev_dev(vdev->mdev),
		 "%s: state (0x%x -> 0x%x)\n",
		 __func__, vdev->migration.minfo->device_state, new);

	data = (u8 *)minfo + minfo->data_offset;

	if (cur == VFIO_DEVICE_STATE_STOP) {
		switch (new) {
		case VFIO_DEVICE_STATE_STOP_COPY:
			ret = dlb2_vdev_migration_stop(vdev, data, minfo->data_size);
			if (ret < 0)
				return ERR_PTR(ret);
			fops = &dlb2_vdcm_save_fops;
			return dlb2_vdcm_get_mig_file(mig, vdev,
							  fops, O_RDONLY);
		case VFIO_DEVICE_STATE_RESUMING:
			fops = &dlb2_vdcm_resume_fops;
			return dlb2_vdcm_get_mig_file(mig, vdev,
							  fops, O_WRONLY);
		case VFIO_DEVICE_STATE_RUNNING:
			ret = dlb2_vdev_migration_resume(vdev, data, minfo->data_size);
			if (ret < 0)
				return ERR_PTR(ret);
			return NULL;
		default:
			break;
		}

		return ERR_PTR(-EINVAL);
	}

	if (cur == VFIO_DEVICE_STATE_RUNNING &&
	    new == VFIO_DEVICE_STATE_STOP) {
		//ret = (*vdev->ops->stop)(vqat, NULL, 0);
		if (ret)
			return ERR_PTR(ret);
		return NULL;
	}

	if (cur == VFIO_DEVICE_STATE_STOP_COPY &&
	    new == VFIO_DEVICE_STATE_STOP) {
		ret = dlb2_vdcm_put_mig_file(mig, vdev);
		if (ret < 0)
			return ERR_PTR(ret);
		return NULL;
	}

	if (cur == VFIO_DEVICE_STATE_RESUMING &&
	    new == VFIO_DEVICE_STATE_STOP) {
		//ret = (*vdev->ops->resume)(vqat,
		//			   NULL,
		//			   mig->size);
		if (ret)
			return ERR_PTR(ret);
		ret = dlb2_vdcm_put_mig_file(mig, vdev);
		if (ret < 0)
			return ERR_PTR(ret);
		return NULL;
	}

	dev_warn(mdev_dev(vdev->mdev),
		 "%s: unexpected state transition (0x%x -> 0x%x)\n",
		 __func__, vdev->migration.minfo->device_state, new);

	return ERR_PTR(-EINVAL);
}

static struct file *
dlb2_vdcm_set_device_state(struct vfio_device *vfio_dev,
			       enum vfio_device_mig_state new_state)
{
	struct dlb2_vdev *vdev;
	enum vfio_device_mig_state next_state;
	struct file *res = NULL;
	int ret;

	vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);

	dev_info(mdev_dev(vdev->mdev),
		 "%s: state (0x%x -> 0x%x)\n",
		 __func__, vdev->migration.minfo->device_state, new_state);

	mutex_lock(&vdev->migration.lock);
	while (new_state != vdev->migration.minfo->device_state) {
		ret = vfio_mig_get_next_state(vfio_dev,
					      vdev->migration.minfo->device_state,
					      new_state, &next_state);
		if (ret) {
			res = ERR_PTR(-EINVAL);
			break;
		}

		res = _dlb2_vdcm_set_device_state(vdev, next_state);
		if (IS_ERR(res))
			break;
		vdev->migration.minfo->device_state = next_state;
		if (WARN_ON(res && new_state != vdev->migration.minfo->device_state)) {
			fput(res);
			res = ERR_PTR(-EINVAL);
			break;
		}
	}
	mutex_unlock(&vdev->migration.lock);

	return res;
}

static int
dlb2_vdcm_get_device_state(struct vfio_device *vfio_dev,
			       enum vfio_device_mig_state *curr_state)
{
	struct dlb2_vdev *vdev;

	vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);

	mutex_lock(&vdev->migration.lock);
	*curr_state = vdev->migration.minfo->device_state;
	mutex_unlock(&vdev->migration.lock);

	return 0;
}

#if(1)
static int
dlb2_vdcm_get_data_size(struct vfio_device *vfio_dev,
			    unsigned long *stop_copy_length)
{
	struct dlb2_vdev *vdev;

	vdev = container_of(vfio_dev, struct dlb2_vdev, vfio_dev);

	*stop_copy_length = vdev->migration.minfo->data_size;

	return 0;
}

static struct vfio_migration_ops dlb2_vdcm_migrate_ops = {
	.migration_set_state = dlb2_vdcm_set_device_state,
	.migration_get_state = dlb2_vdcm_get_device_state,
	.migration_get_data_size = dlb2_vdcm_get_data_size,
};

static inline void
dlb2_vdcm_set_mig_ops(struct dlb2_vdev *vdev)
{
	vdev->vfio_dev.mig_ops = &dlb2_vdcm_migrate_ops;
}

#else

static inline void
dlb2_vdcm_set_mig_ops(struct dlb2_vdev *vdev)
{
	struct vfio_device_ops *ops = (struct vfio_device_ops *)vdev->vfio_dev.ops;

	ops->migration_set_state = dlb2_vdcm_set_device_state;
	ops->migration_get_state = dlb2_vdcm_get_device_state;
}
#endif

#endif /* DLB2_VDCM_MIGRATION_V2 */

/**
 * dlb2_vdcm_migration_init() - initialize vdcm live migration data structure
 * @vdev: pointer to struct dlb2_vdev._
 * @cmd_size: size of space needed to store the dlb mbox commands.
 *
 * This function allocates and initializes the memory space needed for the live
 * migration with vdcm. The data structure starts with vfio_device_migration_info
 * structure followed by the space for mbox command storage and dlb2_migration_state
 * structure (for either source or destination VM).
 *
 *    ------------------------------  <------ vdev->migration.minfo
 *    | vfio_device_migration_info |
 *    |        structure           |
 *    ------------------------------
 *    | CMD space used (4 bytes)   |    ^        ^
 *    ------------------------------    |        |
 *    |       CMD 0 (64 bytes)     |    |        |
 *    |       CMD 1 (64 bytes)     |    |        |
 *              ...                              |
 *              ...                  cmd_size    |
 *              ...
 *              ...                     |   total_state_size = minfo->data_size
 *    |         ...                |    |
 *    ------------------------------    V        |
 *    |                            |             |
 *    | dlb2_migration_state struc |             |
 *    |   (for src or dst)         |             |
 *    ------------------------------             V
 *
 * Only first 64 bytes of each mbox cmmand structure (256 bytes) are saved. At
 * the present time, none of the mbox commands are more than 64 bytes.
 *
 * This memory layout is the same as the dev region used for transfering state
 * data during a live migration session, see dlb2_vdcm_vdev_dev_region_rw().
 * total_state_size bytes of data are transfered.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
int dlb2_vdcm_migration_init(struct dlb2_vdev *vdev, int cmd_size)
{
	struct vfio_device_migration_info *minfo;
	struct dlb2_migration_state *m_state;
	u32 total_state_size;
	struct dlb2 *dlb2;

	dlb2 = mdev_get_dlb2(vdev->mdev);

	total_state_size = cmd_size + sizeof(*m_state);

	printk("%s, cmd_size = %d\n", __func__, cmd_size);

	vdev->migration.size = ALIGN(sizeof(vdev->migration.minfo) +
				     total_state_size, PAGE_SIZE);
	//minfo = kzalloc(vdev->migration.size, GFP_KERNEL);
	minfo = vzalloc(vdev->migration.size);

	/* Set DLB migration state space */
	vdev->mig_state.src_vm_state  = (struct dlb2_migration_state *)((u8 *)minfo +
						sizeof(*minfo) + cmd_size);
	vdev->mig_state.dst_vm_state  = (struct dlb2_migration_state *)((u8 *)minfo +
						sizeof(*minfo) + cmd_size);

	if (!minfo) {
		printk("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	minfo->data_offset = sizeof(*minfo);
	minfo->data_size = total_state_size;
	vdev->migration.minfo = minfo;
	vdev->migration.mstate_mgr = NULL;
	vdev->migration.mdata_size = 0;
	vdev->migration.allocated_cmd_size = cmd_size;

#ifdef DLB2_VDCM_MIGRATION_V2
	minfo->device_state = VFIO_DEVICE_STATE_RUNNING;
	vdev->vfio_dev.migration_flags = VFIO_MIGRATION_STOP_COPY;
	dlb2_vdcm_set_mig_ops(vdev);
#endif
	//last_reset = 0;
	return 0;
}

void dlb2_save_cmd_for_migration(struct dlb2 *dlb2, int vdev_id, u8 *data, int data_size)
{
	struct vfio_device_migration_info *minfo;
	struct pci_dev *pdev = dlb2->pdev;
	struct dlb2_vdev *vdev = NULL;
	struct list_head *next;
	uint32_t cmd_offset;

	if (!dlb2->vdcm_initialized)
		return;

	list_for_each(next, &dlb2->vdev_list) {
		vdev = container_of(next, struct dlb2_vdev, next);
		if (vdev->id == vdev_id)
			break;
	}

	if (!vdev || vdev->id != vdev_id) {
		dev_err(&pdev->dev,
			"[%s()] dlb2 vdev not available: %d\n",
			__func__, vdev_id);

		return;
	}

	/* Do not save the status read commands. They are not needed
	 * for restoring the DLB state.
	 */
	switch (DLB2_MBOX_CMD_TYPE(data)) {
		case DLB2_MBOX_CMD_GET_NUM_RESOURCES:
		case DLB2_MBOX_CMD_LDB_PORT_OWNED_BY_DOMAIN:
		case DLB2_MBOX_CMD_DIR_PORT_OWNED_BY_DOMAIN:
		case DLB2_MBOX_CMD_GET_NUM_USED_RESOURCES:
		case DLB2_MBOX_CMD_GET_SN_ALLOCATION:
		case DLB2_MBOX_CMD_GET_LDB_QUEUE_DEPTH:
		case DLB2_MBOX_CMD_GET_DIR_QUEUE_DEPTH:
		case DLB2_MBOX_CMD_GET_COS_BW:
		case DLB2_MBOX_CMD_GET_SN_OCCUPANCY:
		case DLB2_MBOX_CMD_QUERY_CQ_POLL_MODE:
		case DLB2_MBOX_CMD_GET_XSTATS:
			return;
	}

	data_size = DLB2_LM_CMD_SAVE_DATA_SIZE;

	minfo = vdev->migration.minfo;
	cmd_offset =  minfo->data_offset + DLB2_LM_XMIT_CMD_SIZE_SIZE;

	if (vdev->migration.mdata_size + data_size < minfo->data_size) {
		dev_info(mdev_dev(vdev->mdev), "%s: saving cmd = %s %d, %d\n", __func__,
			 dlb2_mbox_cmd_type_strings[DLB2_MBOX_CMD_TYPE(data)], data_size,
			 vdev->migration.mdata_size);
		memcpy((u8 *)minfo + cmd_offset + vdev->migration.mdata_size, data, data_size);
		vdev->migration.mdata_size += data_size;
		/* Record the mdata_size in XMIT data space */
		memcpy((u8 *)minfo + minfo->data_offset, &(vdev->migration.mdata_size),
			DLB2_LM_XMIT_CMD_SIZE_SIZE);
	} else {
		dev_err(&pdev->dev, "%s: No space to save cmd for migration! %d, %d, %lld\n",
			 __func__, vdev->migration.mdata_size, data_size, minfo->data_size);
	}

	if (DLB2_MBOX_CMD_TYPE(data) ==  DLB2_MBOX_CMD_RESET_SCHED_DOMAIN ||
	    DLB2_MBOX_CMD_TYPE(data) ==  DLB2_MBOX_CMD_DEV_RESET) {
		/* reset the cmd buffer, leave CMD_REGISTER in the buffer */
		vdev->migration.mdata_size = DLB2_LM_CMD_SAVE_DATA_SIZE;
		memcpy((u8 *)minfo + minfo->data_offset, &(vdev->migration.mdata_size),
			DLB2_LM_XMIT_CMD_SIZE_SIZE);
	}
}
#endif /* CONFIG_INTEL_DLB2_SIOV */
