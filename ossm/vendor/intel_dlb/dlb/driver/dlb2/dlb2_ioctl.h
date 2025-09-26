/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_IOCTL_H
#define __DLB2_IOCTL_H

#include <linux/version.h>

#include "dlb2_main.h"

#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
long dlb2_ioctl(struct file *f,
		unsigned int cmd,
		unsigned long arg);
#else
int dlb2_ioctl(struct inode *i,
	       struct file *f,
	       unsigned int cmd,
	       unsigned long arg);
#endif

#if KERNEL_VERSION(2, 6, 35) <= LINUX_VERSION_CODE
long dlb2_domain_ioctl(struct file *f,
		       unsigned int cmd,
		       unsigned long arg);
#else
int dlb2_domain_ioctl(struct inode *i,
		      struct file *f,
		      unsigned int cmd,
		      unsigned long arg);
#endif

#endif /* __DLB2_IOCTL_H */
