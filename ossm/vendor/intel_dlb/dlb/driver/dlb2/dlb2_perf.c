// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 Intel Corporation. All rights rsvd. */

#include <linux/sched/task.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include "dlb2_main.h"
#include "dlb2_perf.h"
#include "base/dlb2_mbox.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_resource.h"

static ssize_t cpumask_show(struct device *dev, struct device_attribute *attr,
			    char *buf);

static cpumask_t dlb2_perf_cpu_mask;
static bool cpuhp_set_up;
static enum cpuhp_state cpuhp_slot;

/*
 * perf userspace reads this attribute to determine which cpus to open
 * counters on.  It's connected to dlb2_perf_cpu_mask, which is
 * maintained by the cpu hotplug handlers.
 */
static DEVICE_ATTR_RO(cpumask);

static struct attribute *dlb2_perf_cpumask_attrs[] = {
        &dev_attr_cpumask.attr,
        NULL,
};

static struct attribute_group cpumask_attr_group = {
        .attrs = dlb2_perf_cpumask_attrs,
};

/*
 * These attributes specify the bits in the config word that the perf
 * syscall uses to pass the event ids and categories to dlb2_perf.
 */
DEFINE_DLB2_PERF_FORMAT_ATTR(event_category, "config:28-31");
DEFINE_DLB2_PERF_FORMAT_ATTR(event, "config:0-27");

static struct attribute *dlb2_perf_format_attrs[] = {
        &format_attr_dlb2_event_category.attr,
        &format_attr_dlb2_event.attr,
        NULL,
};

static struct attribute_group dlb2_perf_format_attr_group = {
        .name = "format",
        .attrs = dlb2_perf_format_attrs,
};

static const struct attribute_group *dlb2_perf_attr_groups[] = {
        &dlb2_perf_format_attr_group,
        &cpumask_attr_group,
        NULL,
};

static ssize_t cpumask_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return cpumap_print_to_pagebuf(true, buf, &dlb2_perf_cpu_mask);
}

static bool is_dlb2_event(struct dlb2_pmu *dlb2_pmu, struct perf_event *event)
{
	return &dlb2_pmu->pmu == event->pmu;
}

static int dlb2_perf_collect_events(struct dlb2_pmu *dlb2_pmu,
				    struct perf_event *leader,
				    bool do_grp)
{
	struct perf_event *event;
	int n, max_count;

	max_count = dlb2_pmu->n_counters;
	n = dlb2_pmu->n_events;

	if (n >= max_count)
		return -EINVAL;

	if (is_dlb2_event(dlb2_pmu, leader)) {
		dlb2_pmu->event_list[n] = leader;
		dlb2_pmu->event_list[n]->hw.idx = n;
		n++;
	}

	if (!do_grp)
		return n;

	for_each_sibling_event(event, leader) {
		if (!is_dlb2_event(dlb2_pmu, event) ||
		    event->state <= PERF_EVENT_STATE_OFF)
			continue;

		if (n >= max_count)
			return -EINVAL;

		dlb2_pmu->event_list[n] = event;
		dlb2_pmu->event_list[n]->hw.idx = n;
		n++;
	}

	return n;
}

static int dlb2_perf_assign_event(struct dlb2_pmu *dlb2_pmu,
				  struct perf_event *event)
{
	int i;

	for (i = 0; i < DLB2_PMU_EVENT_MAX; i++)
		if (!test_and_set_bit(i, dlb2_pmu->used_mask))
			return i;

	return -EINVAL;
}

static int dlb2_perf_pmu_event_init(struct perf_event *event)
{
	struct dlb2 *dlb2;
	int ret = 0;

	dlb2 = event_to_dlb2(event);
	event->hw.idx = -1;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	if (event->cpu < 0)
		return -EINVAL;

	if (event->pmu != &dlb2->dlb2_pmu->pmu)
		return -EINVAL;

	event->cpu = dlb2->dlb2_pmu->cpu;
	event->hw.config = event->attr.config;

	return ret;
}

static inline u64 dlb2_perf_pmu_read_counter(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
        struct dlb2_sched_idle_counts data;
	int cntr = hwc->idx;
	struct dlb2 *dlb2;

	dlb2 = event_to_dlb2(event);

	dlb2_read_sched_idle_counts(&dlb2->hw, &data, cntr);
	return data.ldb_perf_counters[cntr];
}

/* This function is called when userspace issues a read() on an event
 * file descriptor. The difference/delta of the counters is returned.
 */
static void dlb2_perf_pmu_event_update(struct perf_event *event)
{
	u64 prev_raw_count, new_raw_count, delta;
	struct hw_perf_event *hwc = &event->hw;

	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = dlb2_perf_pmu_read_counter(event);

	delta = new_raw_count - prev_raw_count;

	local64_add(delta, &event->count);
}

/* This function is called when the counters are enabled and started. Counters
 * are read at this point for the first time and stored in hwc->prev_count.
 * This is used in dlb2_perf_pmu_event_update() to get the delta of the counters.
 */
static void dlb2_perf_pmu_event_start(struct perf_event *event, int mode)
{
	struct hw_perf_event *hwc = &event->hw;
	struct dlb2_sched_idle_counts data;
	u64 event_enc, event_cat = 0;
	union event_cfg event_cfg;
	struct dlb2 *dlb2;
	int cntr;

	dlb2 = event_to_dlb2(event);

	/* Obtain event category and event value from user space */
	event_cfg.val = event->attr.config;
	event_cat = event_cfg.event_cat;
	event_enc = event_cfg.event_enc;

	hwc->idx = event_cat;
	cntr = hwc->idx;

	event->hw.idx = hwc->idx;

	dlb2_read_sched_idle_counts(&dlb2->hw, &data, cntr);
	local64_set(&event->hw.prev_count, data.ldb_perf_counters[cntr]);
}

static void dlb2_perf_pmu_event_stop(struct perf_event *event, int mode)
{
	struct hw_perf_event *hwc = &event->hw;
	struct dlb2 *dlb2;
	int i, cntr = hwc->idx;

	dlb2 = event_to_dlb2(event);

	/* remove this event from event list */
	for (i = 0; i < dlb2->dlb2_pmu->n_events; i++) {
		if (event != dlb2->dlb2_pmu->event_list[i])
			continue;

		for (++i; i < dlb2->dlb2_pmu->n_events; i++)
			dlb2->dlb2_pmu->event_list[i - 1] = dlb2->dlb2_pmu->event_list[i];
		--dlb2->dlb2_pmu->n_events;
		break;
	}
	if (mode == PERF_EF_UPDATE)
		dlb2_perf_pmu_event_update(event);

	event->hw.idx = -1;
	clear_bit(cntr, dlb2->dlb2_pmu->used_mask);
}

static void dlb2_perf_pmu_event_del(struct perf_event *event, int mode)
{
	dlb2_perf_pmu_event_stop(event, PERF_EF_UPDATE);
}

static int dlb2_perf_pmu_event_add(struct perf_event *event, int flags)
{
	struct dlb2 *dlb2 = event_to_dlb2(event);
	struct hw_perf_event *hwc = &event->hw;
	struct dlb2_pmu *dlb2_pmu;
	int idx, n;

	dlb2_pmu = dlb2->dlb2_pmu;
	n = dlb2_perf_collect_events(dlb2_pmu, event, false);
	if (n < 0)
		return n;

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (!(flags & PERF_EF_START))
		hwc->state |= PERF_HES_ARCH;

	idx = dlb2_perf_assign_event(dlb2_pmu, event);
	if (idx < 0)
		return idx;

	if (flags & PERF_EF_START)
		dlb2_perf_pmu_event_start(event, 0);

	dlb2_pmu->n_events = n;

	return 0;
}

static void enable_dlb2_perf_pmu(struct dlb2 *dlb2)
{
       dlb2_enable_ldb_sched_perf_ctrl(&dlb2->hw);
}

static void dlb2_perf_pmu_enable(struct pmu *pmu)
{
	struct dlb2 *dlb2 = pmu_to_dlb2(pmu);

	enable_dlb2_perf_pmu(dlb2);
}

static void dlb2_perf_pmu_disable(struct pmu *pmu)
{
	/* Intentionally returning without any action */
	return;
}

static void dlb2_pmu_init(struct dlb2_pmu *dlb2_pmu)
{
	dlb2_pmu->pmu.name		= dlb2_pmu->name;
	dlb2_pmu->pmu.attr_groups       = dlb2_perf_attr_groups;
	dlb2_pmu->pmu.task_ctx_nr       = perf_invalid_context;
	dlb2_pmu->pmu.event_init	= dlb2_perf_pmu_event_init;
	dlb2_pmu->pmu.pmu_enable	= dlb2_perf_pmu_enable,
	dlb2_pmu->pmu.pmu_disable	= dlb2_perf_pmu_disable,
	dlb2_pmu->pmu.add		= dlb2_perf_pmu_event_add;
	dlb2_pmu->pmu.del		= dlb2_perf_pmu_event_del;
	dlb2_pmu->pmu.start		= dlb2_perf_pmu_event_start;
	dlb2_pmu->pmu.stop		= dlb2_perf_pmu_event_stop;
	dlb2_pmu->pmu.read		= dlb2_perf_pmu_event_update;
	dlb2_pmu->pmu.capabilities	= PERF_PMU_CAP_NO_EXCLUDE;
	dlb2_pmu->pmu.module		= THIS_MODULE;
}

void dlb2_perf_pmu_remove(struct dlb2 *dlb2)
{
	if (!dlb2->dlb2_pmu)
		return;

	cpuhp_state_remove_instance(cpuhp_slot, &dlb2->dlb2_pmu->cpuhp_node);
	perf_pmu_unregister(&dlb2->dlb2_pmu->pmu);
	kfree(dlb2->dlb2_pmu);
	dlb2->dlb2_pmu = NULL;
}

static int perf_event_cpu_online(unsigned int cpu, struct hlist_node *node)
{
	struct dlb2_pmu *dlb2_pmu;

	dlb2_pmu = hlist_entry_safe(node, typeof(*dlb2_pmu), cpuhp_node);
	if (!dlb2_pmu)
		return -1;
	/* select the first online CPU as the designated reader */
	if (cpumask_empty(&dlb2_perf_cpu_mask)) {
		cpumask_set_cpu(cpu, &dlb2_perf_cpu_mask);
		dlb2_pmu->cpu = cpu;
	}

	return 0;
}

static int perf_event_cpu_offline(unsigned int cpu, struct hlist_node *node)
{
	struct dlb2_pmu *dlb2_pmu;
	unsigned int target;

	dlb2_pmu = hlist_entry_safe(node, typeof(*dlb2_pmu), cpuhp_node);

	if (!cpumask_test_and_clear_cpu(cpu, &dlb2_perf_cpu_mask))
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);

	/* migrate events if there is a valid target */
	if (target < nr_cpu_ids)
		cpumask_set_cpu(target, &dlb2_perf_cpu_mask);
	else
		target = -1;

	perf_pmu_migrate_context(&dlb2_pmu->pmu, cpu, target);

	return 0;
}

int dlb2_perf_pmu_init(struct dlb2 *dlb2)
{
	union dlb2_perfcap perfcap;
	struct dlb2_pmu *dlb2_pmu;
	int rc = -ENODEV;

	/* perf module initialization failed, nothing to do.*/
	if (!cpuhp_set_up)
		return -ENODEV;

        perfcap.num_event_category = 1; 
        perfcap.num_perf_counter = DLB2_PMU_EVENT_MAX;
        perfcap.counter_width = 16;
        perfcap.cap_per_counter = 0;

	dlb2_pmu = kzalloc(sizeof(*dlb2_pmu), GFP_KERNEL);
	if (!dlb2_pmu)
		return -ENOMEM;

	dlb2_pmu->dlb2 = dlb2;
	dlb2->dlb2_pmu = dlb2_pmu;

	rc = sprintf(dlb2_pmu->name, "dlb%d", dlb2->id);
	if (rc < 0)
		goto free;

	dlb2_pmu->n_event_categories = perfcap.num_event_category;
	dlb2_pmu->per_counter_caps_supported = perfcap.cap_per_counter;

	/* Store the total number of counters categories, and counter width */
	dlb2_pmu->n_counters = perfcap.num_perf_counter;
	dlb2_pmu->counter_width = perfcap.counter_width;

	/* Define callback functions for dlb perf pmu */
	dlb2_pmu_init(dlb2_pmu);

	/* Register dlb pmu under the linux perf framework */
	rc = perf_pmu_register(&dlb2_pmu->pmu, dlb2_pmu->name, -1);
	if (rc)
		goto free;

	rc = cpuhp_state_add_instance(cpuhp_slot, &dlb2_pmu->cpuhp_node);
	if (rc) {
		perf_pmu_unregister(&dlb2->dlb2_pmu->pmu);
		goto free;
	}
out:
	return rc;
free:
	kfree(dlb2_pmu);
	dlb2->dlb2_pmu = NULL;

	goto out;
}

/* CPU hotplug providing multi-instance support for multiple dlb devices.
 * First available online cpu is found and made available through cpumask
 * sys file for userspace to read.
 */
void __init dlb2_perf_init(void)
{
	int rc = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
					 "driver/dlb2/perf:online",
					 perf_event_cpu_online,
					 perf_event_cpu_offline);
	if (WARN_ON(rc < 0))
		return;

	cpuhp_slot = rc;
	cpuhp_set_up = true;
}

void __exit dlb2_perf_exit(void)
{
	if (cpuhp_set_up)
		cpuhp_remove_multi_state(cpuhp_slot);
}
