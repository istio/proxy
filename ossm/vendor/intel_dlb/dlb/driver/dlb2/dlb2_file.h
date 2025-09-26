/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef __DLB2_FILE_H
#define __DLB2_FILE_H

#include <linux/file.h>

#include "dlb2_main.h"

void dlb2_release_fs(struct dlb2 *dlb2);

struct file *dlb2_getfile(struct dlb2 *dlb2,
			  int flags,
			  const struct file_operations *fops,
			  const char *name);

#endif /* __DLB2_FILE_H */
