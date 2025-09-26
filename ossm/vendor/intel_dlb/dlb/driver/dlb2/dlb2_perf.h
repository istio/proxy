/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 Intel Corporation. All rights rsvd. */

#ifndef _PERFMON_H_
#define _PERFMON_H_

#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/sbitmap.h>
#include <linux/dmaengine.h>
#include <linux/percpu-rwsem.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/uuid.h>
#include <linux/perf_event.h>
#include "dlb2_main.h"

union dlb2_perfcap {
	struct {
		u64 num_perf_counter:6;
		u64 rsvd1:2;
		u64 counter_width:8;
		u64 num_event_category:4;
		u64 global_event_category:16;
		u64 filter:8;
		u64 rsvd2:8;
		u64 cap_per_counter:1;
		u64 writeable_counter:1;
		u64 counter_freeze:1;
		u64 overflow_interrupt:1;
		u64 rsvd3:8;
	};
	u64 bits;
} __packed;

struct dlb2_event {
	union {
		struct {
			u32 events:28;
			u32 event_category:4;
		};
		u32 val;
	};
} __packed;

union event_cfg {
	struct {
		u64 event_cat:4;
		u64 event_enc:28;
	};
	u64 val;
} __packed;

static inline struct dlb2_pmu *event_to_pmu(struct perf_event *event)
{
	struct dlb2_pmu *dlb2_pmu;
	struct pmu *pmu;

	pmu = event->pmu;
	dlb2_pmu = container_of(pmu, struct dlb2_pmu, pmu);

	return dlb2_pmu;
}

static inline struct dlb2 *event_to_dlb2(struct perf_event *event)
{
	struct dlb2_pmu *dlb2_pmu;
	struct pmu *pmu;

	pmu = event->pmu;
	dlb2_pmu = container_of(pmu, struct dlb2_pmu, pmu);

	return dlb2_pmu->dlb2;
}

static inline struct dlb2 *pmu_to_dlb2(struct pmu *pmu)
{
	struct dlb2_pmu *dlb2_pmu;

	dlb2_pmu = container_of(pmu, struct dlb2_pmu, pmu);

	return dlb2_pmu->dlb2;
}

#define DEFINE_DLB2_PERF_FORMAT_ATTR(_name, _format)			\
static ssize_t __dlb2_perf_##_name##_show(struct kobject *kobj,	\
				struct kobj_attribute *attr,		\
				char *page)				\
{									\
	BUILD_BUG_ON(sizeof(_format) >= PAGE_SIZE);			\
	return sprintf(page, _format "\n");				\
}									\
static struct kobj_attribute format_attr_dlb2_##_name =			\
	__ATTR(_name, 0444, __dlb2_perf_##_name##_show, NULL)

#endif
