/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_MAIN_H
#define __DLB2_MAIN_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/perf_event.h>
#include <linux/mdev.h>
#include <linux/version.h>

#include "uapi/linux/dlb2_user.h"

#include "base/dlb2_hw_types.h"

static const char dlb2_driver_name[] = KBUILD_MODNAME;

#ifdef RHEL_RELEASE_CODE
#if RHEL_RELEASE_VERSION(9, 4) <= RHEL_RELEASE_CODE
#define DLB2_RHEL_GE_9_4
#endif
#endif

/*
 * The dlb2 driver uses a different minor number for each device file, of which
 * there are:
 * - 33 per device (PF or VF/VDEV): 1 for the device, 32 for scheduling domains
 * - Up to 17 devices per PF: 1 PF and up to 16 VFs/VDEVs
 * - Up to 128 PFs per system
 */
#define DLB2_MAX_NUM_PFS	  128
#define DLB2_NUM_FUNCS_PER_DEVICE (1 + DLB2_MAX_NUM_VDEVS)
#define DLB2_MAX_NUM_DEVICES	  (DLB2_MAX_NUM_PFS * DLB2_NUM_FUNCS_PER_DEVICE)

#define DLB2_DEFAULT_RESET_TIMEOUT_S	 5
#define DLB2_VF_FLR_DONE_POLL_TIMEOUT_MS 1000
#define DLB2_VF_FLR_DONE_SLEEP_PERIOD_MS 1

#define DLB2_NAME_SIZE		128
#define DLB2_PMU_EVENT_MAX	9

#define DLB2_DEFAULT_PROBE_CORE 1
#define DLB2_PROD_PROBE_CORE 0
#define DLB2_NUM_PROBE_ENQS 1000
#define DLB2_HCW_MEM_SIZE 8
#define DLB2_HCW_64B_OFF 4
#define DLB2_HCW_ALIGN_MASK 0x3F

#define DLB2_VDCM_MDEV_TYPES_NUM	1

#if KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE
#define DLB2_EVENTFD_SIGNAL(ctx)	eventfd_signal(ctx)
#else
#define DLB2_EVENTFD_SIGNAL(ctx)	eventfd_signal(ctx, 1)
#endif


extern struct list_head dlb2_dev_list;
extern struct mutex dlb2_driver_mutex;
extern unsigned int dlb2_qe_sa_pct;
extern unsigned int dlb2_qid_sa_pct;
extern unsigned int dlb2_qidx_wrr_weight;

enum dlb2_device_type {
	DLB2_PF,
	DLB2_VF,
	DLB2_5_PF,
	DLB2_5_VF,
};

#define DLB2_IS_PF(dev) (dev->type == DLB2_PF || dev->type == DLB2_5_PF)
#define DLB2_IS_VF(dev) (dev->type == DLB2_VF || dev->type == DLB2_5_VF)

struct dlb2;

struct dlb2_device_ops {
	int (*map_pci_bar_space)(struct dlb2 *dlb2, struct pci_dev *pdev);
	void (*unmap_pci_bar_space)(struct dlb2 *dlb2, struct pci_dev *pdev);
	int (*init_driver_state)(struct dlb2 *dlb2);
	void (*free_driver_state)(struct dlb2 *dlb2);
	int (*sysfs_create)(struct dlb2 *dlb2);
	void (*sysfs_reapply)(struct dlb2 *dlb2);
	int (*init_interrupts)(struct dlb2 *dlb2, struct pci_dev *pdev);
	int (*enable_ldb_cq_interrupts)(struct dlb2 *dlb2,
					int domain_id,
					int port_id,
					u16 thresh);
	int (*enable_dir_cq_interrupts)(struct dlb2 *dlb2,
					int domain_id,
					int port_id,
					u16 thresh);
	int (*arm_cq_interrupt)(struct dlb2 *dlb2,
				int domain_id,
				int port_id,
				bool is_ldb);
	void (*reinit_interrupts)(struct dlb2 *dlb2);
	void (*free_interrupts)(struct dlb2 *dlb2, struct pci_dev *pdev);
	void (*enable_pm)(struct dlb2 *dlb2);
	int (*wait_for_device_ready)(struct dlb2 *dlb2, struct pci_dev *pdev);
	int (*register_driver)(struct dlb2 *dlb2);
	void (*unregister_driver)(struct dlb2 *dlb2);
	int (*create_sched_domain)(struct dlb2_hw *hw,
				   struct dlb2_create_sched_domain_args *args,
				   struct dlb2_cmd_response *resp);
	int (*create_ldb_queue)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_create_ldb_queue_args *args,
				struct dlb2_cmd_response *resp);
	int (*create_dir_queue)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_create_dir_queue_args *args,
				struct dlb2_cmd_response *resp);
	int (*create_ldb_port)(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_ldb_port_args *args,
			       uintptr_t cq_dma_base,
			       struct dlb2_cmd_response *resp);
	int (*create_dir_port)(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_dir_port_args *args,
			       uintptr_t cq_dma_base,
			       struct dlb2_cmd_response *resp);
	int (*start_domain)(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_start_domain_args *args,
			    struct dlb2_cmd_response *resp);
	int (*stop_domain)(struct dlb2_hw *hw,
			   u32 domain_id,
			   struct dlb2_stop_domain_args *args,
			   struct dlb2_cmd_response *resp);
	int (*map_qid)(struct dlb2_hw *hw,
		       u32 domain_id,
		       struct dlb2_map_qid_args *args,
		       struct dlb2_cmd_response *resp);
	int (*unmap_qid)(struct dlb2_hw *hw,
			 u32 domain_id,
			 struct dlb2_unmap_qid_args *args,
			 struct dlb2_cmd_response *resp);
	int (*pending_port_unmaps)(struct dlb2_hw *hw,
				   u32 domain_id,
				   struct dlb2_pending_port_unmaps_args *args,
				   struct dlb2_cmd_response *resp);
	int (*enable_ldb_port)(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_enable_ldb_port_args *args,
			       struct dlb2_cmd_response *resp);
	int (*disable_ldb_port)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_disable_ldb_port_args *args,
				struct dlb2_cmd_response *resp);
	int (*enable_dir_port)(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_enable_dir_port_args *args,
			       struct dlb2_cmd_response *resp);
	int (*disable_dir_port)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_disable_dir_port_args *args,
				struct dlb2_cmd_response *resp);
	int (*get_num_resources)(struct dlb2_hw *hw,
				 struct dlb2_get_num_resources_args *args);
	int (*reset_domain)(struct dlb2_hw *hw, u32 domain_id);
	int (*ldb_port_owned_by_domain)(struct dlb2_hw *hw,
					u32 domain_id,
					u32 port_id);
	int (*dir_port_owned_by_domain)(struct dlb2_hw *hw,
					u32 domain_id,
					u32 port_id);
	int (*get_sn_allocation)(struct dlb2_hw *hw, u32 group_id);
	int (*set_sn_allocation)(struct dlb2_hw *hw, u32 group_id, u32 num);
	int (*get_sn_occupancy)(struct dlb2_hw *hw, u32 group_id);
	int (*get_ldb_queue_depth)(struct dlb2_hw *hw,
				   u32 domain_id,
				   struct dlb2_get_ldb_queue_depth_args *args,
				   struct dlb2_cmd_response *resp);
	int (*get_dir_queue_depth)(struct dlb2_hw *hw,
				   u32 domain_id,
				   struct dlb2_get_dir_queue_depth_args *args,
				   struct dlb2_cmd_response *resp);
	int (*set_cos_bw)(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth);
	int (*get_cos_bw)(struct dlb2_hw *hw, u32 cos_id);
	void (*init_hardware)(struct dlb2 *dlb2);
	int (*query_cq_poll_mode)(struct dlb2 *dlb2,
				  struct dlb2_cmd_response *user_resp);
	int (*mbox_dev_reset)(struct dlb2 *dlb2);
	int (*enable_cq_weight)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_enable_cq_weight_args *args,
				struct dlb2_cmd_response *resp);
	int (*cq_inflight_ctrl)(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_cq_inflight_ctrl_args *args,
				struct dlb2_cmd_response *resp);
	int (*get_xstats)(struct dlb2_hw *hw,
			  struct dlb2_xstats_args *args);
};

extern const struct attribute_group *dlb2_vf_attrs[];
extern struct dlb2_device_ops dlb2_pf_ops;
extern struct dlb2_device_ops dlb2_vf_ops;
extern const struct file_operations dlb2_domain_fops;
extern const struct file_operations dlb2_pp_fops;
extern const struct file_operations dlb2_cq_fops;

struct dlb2_port {
	void *cq_base;
	dma_addr_t cq_dma_base;
	struct dlb2_domain *domain;
	struct eventfd_ctx *efd_ctx;
	int id;
	u8 is_ldb;
	u8 valid;
};

#define DLB2_DOMAIN_ALERT_RING_SIZE 256

struct dlb2_domain {
#ifdef CONFIG_INTEL_DLB2_DATAPATH
	struct dlb2_dp_domain *dp;
#endif
	struct dlb2 *dlb2;
	struct dlb2_domain_alert alerts[DLB2_DOMAIN_ALERT_RING_SIZE];
	wait_queue_head_t wq_head;
	/* The alert lock protects access to the alert ring. */
	spinlock_t alert_lock;
	struct kref refcnt;
	u8 alert_rd_idx;
	u8 alert_wr_idx;
	u8 id;
	u8 user_mode;
	u8 valid;
};

struct dlb2_cq_intr {
	wait_queue_head_t wq_head;
	/*
	 * The CQ interrupt mutex guarantees one thread is blocking on a CQ's
	 * interrupt at a time.
	 */
	struct mutex mutex;
	u8 wake;
	u8 configured;
	u8 domain_id;
	/*
	 * disabled is true if the port is disabled. In that
	 * case, the driver doesn't allow applications to block on the
	 * port's interrupt.
	 */
	u8 disabled;
} ____cacheline_aligned;

struct dlb2_vdev_cq_int_info {
	char name[32];
	s16 port_id;
	u8 is_ldb;
};

struct dlb2_intr {
	struct dlb2_cq_intr ldb_cq_intr[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2_cq_intr dir_cq_intr[DLB2_MAX_NUM_DIR_PORTS_V2_5];

	/* *_owner and cq_int_info arrays only used by the virtual device */
	struct dlb2_vdev_cq_int_info
		msi_map[DLB2_VDEV_MAX_NUM_INTERRUPT_VECTORS_V2_5];
	struct dlb2 *ldb_cq_intr_owner[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2 *dir_cq_intr_owner[DLB2_MAX_NUM_DIR_PORTS_V2_5];

	/*
	 * The Scalable IOV VDEV has more possible interrupt vectors than the
	 * PF or VF, so we simply over-allocate in those cases
	 */
	u8 isr_registered[DLB2_VDEV_MAX_NUM_INTERRUPT_VECTORS_V2_5];
	int num_ldb_ports;
	int num_dir_ports;
	int num_vectors;
	int base_vector;
	int mode;
	u8 num_cq_intrs; /* (VF only) */
};

struct dlb2_datapath {
	/* Linked list of datapath handles */
	struct list_head hdl_list;
};

struct vf_id_state {
	/*
	 * If this is an auxiliary VF, primary_vf points to the dlb2 structure
	 * of its primary sibling.
	 */
	struct dlb2 *primary_vf;
	/*
	 * pf_id and vf_id contain unique identifiers given by the PF at driver
	 * register time. These IDs can be used to identify VFs from the same
	 * physical device.
	 */
	u8 pf_id;
	u8 vf_id;
	/*
	 * An auxiliary VF has no resources of its own. It exists to provide
	 * its primary VF sibling with MSI vectors, so a VF user can exceed the
	 * 31 vector per VF limit.
	 */
	u8 is_auxiliary_vf;
	/*
	 * If this is an auxiliary VF, primary_vf_id is the 'vf_id' of its
	 * primary sibling.
	 */
	u8 primary_vf_id;
	/*
	 * MBOX interface Version supported by PF. This allows VF to check if a
	 * certain mbox command is supported by PF driver.
	 */
	u32 pf_interface_version;
};

struct dlb2_vf_perf_metric_data {
	u64 ldb_cq[DLB2_MAX_NUM_LDB_PORTS];
	u64 dir_cq[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	u32 elapsed;
	u8 valid;
};

/*
 * ISR overload is defined as more than DLB2_ISR_OVERLOAD_THRESH interrupts
 * (of a particular type) occurring in a 1s period. If overload is detected,
 * the driver blocks that interrupt (exact mechanism depending on the
 * interrupt) from overloading the PF driver.
 */
#define DLB2_ISR_OVERLOAD_THRESH   1000
#define DLB2_ISR_OVERLOAD_PERIOD_S 1

struct dlb2_alarm {
	ktime_t ts;
	unsigned int enabled;
	u32 count;
};

struct dlb2 {
	struct pci_dev *pdev;
	struct dlb2_hw hw;
	struct cdev cdev;
	struct vf_id_state vf_id_state; /* (VF only) */
	struct vf_id_state child_id_state[DLB2_MAX_NUM_VDEVS]; /* (PF only) */
	struct dlb2_device_ops *ops;
	struct list_head list;
	struct device *dev;
	struct dlb2_domain *sched_domains[DLB2_MAX_NUM_DOMAINS];
	struct dlb2_port ldb_port[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2_port dir_port[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	/*
	 * Anonymous inode used to share an address_space for all domain
	 * device file mappings.
	 */
	struct inode *inode;
#ifdef CONFIG_INTEL_DLB2_DATAPATH
	struct dlb2_datapath dp;
#endif
	struct dlb2_intr intr;
	/*
	 * The resource mutex serializes access to driver data structures and
	 * hardware registers.
	 */
	struct mutex resource_mutex;
	/*
	 * There are two entry points to the service ISR: an interrupt, and a
	 * Scalable IOV software mailbox request. This mutex ensures only one
	 * thread executes the ISR at any time.
	 */
	struct mutex svc_isr_mutex;
	/*
	 * This workqueue thread is responsible for processing all CQ->QID unmap
	 * requests.
	 */
	struct work_struct work;
	struct dlb2_alarm ingress_err;
	struct dlb2_alarm mbox[DLB2_MAX_NUM_VDEVS];
	struct list_head vdev_list;
	/*
	 * The enqueue_four function enqueues four HCWs (one cache-line worth)
	 * to the DLB, using whichever mechanism is supported by the platform
	 * on which this driver is running.
	 */
	void (*enqueue_four)(void *qe4, void __iomem *pp_addr);
	enum dlb2_device_type type;
	enum dlb2_hw_ver hw_ver;
	int id;
	u32 inode_cnt;
	dev_t dev_number;
	u8 domain_reset_failed;
	u8 reset_active;
	u8 worker_launched;
	u8 num_vfs;
	u8 needs_mbox_reset;
	u8 vf_registered[DLB2_MAX_NUM_VDEVS];
	struct ida vdev_ids;
	u8 vdcm_initialized;
	struct irq_domain *ims_domain;
	struct ims_slot *ims_base;

	u32 dlb2_perf_offset;
	struct dlb2_pmu *dlb2_pmu;

#ifdef DLB2_NEW_MDEV_IOMMUFD
	struct mdev_parent parent;
	struct mdev_type *vdcm_mdev_types[DLB2_VDCM_MDEV_TYPES_NUM];
#endif
};

struct dlb2_pmu {
        struct dlb2 *dlb2;

        struct perf_event *event_list[DLB2_PMU_EVENT_MAX];
        int n_events;

        DECLARE_BITMAP(used_mask, DLB2_PMU_EVENT_MAX);

        struct pmu pmu;
        char name[DLB2_NAME_SIZE];
        int cpu;

        int n_counters;
        int counter_width;
        int n_event_categories;
        bool per_counter_caps_supported;

        struct hlist_node cpuhp_node;
};

struct dlb2_pp_thread_data {
	int pp;
	int cycles;
};

enum {
	DLB2_NO_PROBE,
	DLB2_PROBE_SLOW,
	DLB2_PROBE_FAST,
};

int dlb2_init_domain(struct dlb2 *dlb2, u32 domain_id);
#ifdef CONFIG_INTEL_DLB2_DATAPATH
int __dlb2_free_domain(struct dlb2_domain *domain, bool skip_reset);
#endif
void dlb2_free_domain(struct kref *kref);
int dlb2_read_domain_alert(struct dlb2 *dlb2,
			   struct dlb2_domain *domain,
			   struct dlb2_domain_alert *alert,
			   bool nonblock);
int dlb2_port_probe(struct dlb2 *dlb2);

struct dlb2_dp;
void dlb2_register_dp_handle(struct dlb2_dp *dp);
void dlb2_unregister_dp_handle(struct dlb2_dp *dp);

extern bool dlb2_pasid_override;
extern bool dlb2_wdto_disable;

/* Return the number of registered VFs */
static inline bool dlb2_is_registered_vf(struct dlb2 *dlb2, int vf_id)
{
	return dlb2->vf_registered[vf_id];
}

/* Return the number of registered VFs */
static inline int dlb2_num_vfs_registered(struct dlb2 *dlb2)
{
	int i, cnt = 0;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++)
		cnt += !!dlb2_is_registered_vf(dlb2, i);

	return cnt;
}

int dlb2_write_domain_alert(struct dlb2_domain *domain,
			    u64 alert_id,
			    u64 aux_alert_data);

bool dlb2_in_use(struct dlb2 *dlb2);
void dlb2_stop_users(struct dlb2 *dlb2);
void dlb2_unmap_all_mappings(struct dlb2 *dlb2);
void dlb2_release_device_memory(struct dlb2 *dlb2);

void dlb2_handle_mbox_interrupt(struct dlb2 *dlb2, int id);

int dlb2_perf_pmu_init(struct dlb2 *dlb2);
void dlb2_perf_pmu_remove(struct dlb2 *dlb2);
void dlb2_perf_counter_overflow(struct dlb2 *dlb2);
void dlb2_perf_init(void);
void dlb2_perf_exit(void);

#endif /* __DLB2_MAIN_H */
