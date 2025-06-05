/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_HW_TYPES_H
#define __DLB2_HW_TYPES_H

#include "uapi/linux/dlb2_user.h"

#include <linux/bitfield.h>
#include "dlb2_osdep_list.h"
#include "dlb2_osdep_types.h"
#include "dlb2_regs.h"

#define DLB2_CQ_ISR_MODE_MSIX_FOR_SIOV	(DLB2_CQ_ISR_MODE_MSIX + 10)

#define BITS_SET(x, val, mask)			\
		(x = ((x) & ~(mask)) | (((val) << __bf_shf(mask)) & (mask)))

#define BITS_CLR(x, mask)	(x &= ~(mask))
#define BIT_SET(x, mask)	((x) |= (mask))
#define BITS_GET(x, mask)	(((x) & (mask)) >> __bf_shf(mask))

#define SYND2(y)        BITS_GET(synd2, SYS_ALARM_PF_SYND2_##y)
#define SYND1(y)        BITS_GET(synd1, SYS_ALARM_PF_SYND1_##y)
#define SYND0(y)        BITS_GET(synd0, SYS_ALARM_PF_SYND0_##y)
#define SYND(y)         BITS_GET(synd, SYS_ALARM_HW_SYND_##y)

#define DLB2_MAX_NUM_VDEVS			16
#define DLB2_MAX_NUM_DOMAINS			32
#define DLB2_MAX_NUM_LDB_QUEUES			32 /* LDB == load-balanced */
#define DLB2_MAX_NUM_DIR_QUEUES_V2		64 /* DIR == directed */
#define DLB2_MAX_NUM_DIR_QUEUES_V2_5		96
/* When needed for array sizing, the DLB 2.5 macro is used */
#define DLB2_MAX_NUM_DIR_QUEUES(ver)		(ver == DLB2_HW_V2 ? \
						 DLB2_MAX_NUM_DIR_QUEUES_V2 : \
						 DLB2_MAX_NUM_DIR_QUEUES_V2_5)
#define DLB2_MAX_NUM_LDB_PORTS			64
#define DLB2_MAX_NUM_DIR_PORTS_V2		DLB2_MAX_NUM_DIR_QUEUES_V2
#define DLB2_MAX_NUM_DIR_PORTS_V2_5		DLB2_MAX_NUM_DIR_QUEUES_V2_5
#define DLB2_MAX_NUM_DIR_PORTS(ver)		(ver == DLB2_HW_V2 ? \
						 DLB2_MAX_NUM_DIR_PORTS_V2 : \
						 DLB2_MAX_NUM_DIR_PORTS_V2_5)
#define DLB2_MAX_NUM_LDB_CREDITS(ver)		(ver == DLB2_HW_V2 ? \
						 8192 : 16384)
#define DLB2_MAX_NUM_DIR_CREDITS(ver)		(ver == DLB2_HW_V2 ? 4096 : 0)
#define DLB2_HIST_LIST_ENTRIES_USED_BY_LM	8
#define DLB2_MAX_NUM_HIST_LIST_ENTRIES		2048
#define DLB2_MAX_NUM_AQED_ENTRIES		2048
#define DLB2_MAX_NUM_QIDS_PER_LDB_CQ		8
#define DLB2_MAX_NUM_SEQUENCE_NUMBERS		1024
#define DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS	2
#define DLB2_MAX_NUM_SEQUENCE_NUMBER_MODES	5
#define DLB2_QID_PRIORITIES			8
#define DLB2_NUM_ARB_WEIGHTS			8
#define DLB2_MAX_WEIGHT				255
#define DLB2_NUM_COS_DOMAINS			4
#define DLB2_COS_DEFAULT			255
#define DLB2_MAX_NUM_LDB_PORTS_PER_COS		(DLB2_MAX_NUM_LDB_PORTS/DLB2_NUM_COS_DOMAINS)
#define DLB2_MAX_CQ_COMP_CHECK_LOOPS		409600
#define DLB2_MAX_QID_EMPTY_CHECK_LOOPS(ver)	(4 * DLB2_MAX_NUM_LDB_CREDITS(ver))
#define DLB2_MAX_NUM_QID_INFLIGHTS		2048
#ifdef FPGA
#define DLB2_HZ					2000000
#else
#define DLB2_HZ					800000000
#endif
#define DLB2_FUNC_BAR				0
#define DLB2_CSR_BAR				2

#define PCI_DEVICE_ID_INTEL_DLB2_PF 0x2710
#define PCI_DEVICE_ID_INTEL_DLB2_VF 0x2711
#define PCI_DEVICE_ID_INTEL_DLB2_5_PF 0x2714
#define PCI_DEVICE_ID_INTEL_DLB2_5_VF 0x2715

/* Interrupt related macros */
#define DLB2_PF_NUM_NON_CQ_INTERRUPT_VECTORS 1
#define DLB2_PF_NUM_CQ_INTERRUPT_VECTORS     64
#define DLB2_PF_TOTAL_NUM_INTERRUPT_VECTORS \
	(DLB2_PF_NUM_NON_CQ_INTERRUPT_VECTORS + \
	 DLB2_PF_NUM_CQ_INTERRUPT_VECTORS)
#define DLB2_PF_NUM_COMPRESSED_MODE_VECTORS \
	(DLB2_PF_NUM_NON_CQ_INTERRUPT_VECTORS + 1)
#define DLB2_PF_NUM_PACKED_MODE_VECTORS \
	DLB2_PF_TOTAL_NUM_INTERRUPT_VECTORS
#define DLB2_PF_COMPRESSED_MODE_CQ_VECTOR_ID \
	DLB2_PF_NUM_NON_CQ_INTERRUPT_VECTORS

/* DLB non-CQ interrupts (alarm, mailbox, WDT) */
#define DLB2_INT_NON_CQ 0

#define DLB2_WB_CNTL_RATE_LIMIT			3
#define DLB2_DEFAULT_QIDX_WRR_SCHEDULER_WEIGHT	0
#define DLB2_MAX_QIDX_WRR_SCHEDULER_WEIGHT	7

#define DLB2_ALARM_HW_SOURCE_SYS 0
#define DLB2_ALARM_HW_SOURCE_DLB 1

#define DLB2_ALARM_HW_UNIT_CHP 4

#define DLB2_ALARM_SYS_AID_ILLEGAL_QID		3
#define DLB2_ALARM_SYS_AID_DISABLED_QID		4
#define DLB2_ALARM_SYS_AID_ILLEGAL_HCW		5
#define DLB2_ALARM_HW_CHP_AID_ILLEGAL_ENQ	1
#define DLB2_ALARM_HW_CHP_AID_EXCESS_TOKEN_POPS 2

#define DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS 1
#define DLB2_VF_NUM_CQ_INTERRUPT_VECTORS     31
#define DLB2_VF_BASE_CQ_VECTOR_ID	     0
#define DLB2_VF_LAST_CQ_VECTOR_ID	     30
#define DLB2_VF_MBOX_VECTOR_ID		     31
#define DLB2_VF_TOTAL_NUM_INTERRUPT_VECTORS \
	(DLB2_VF_NUM_NON_CQ_INTERRUPT_VECTORS + \
	 DLB2_VF_NUM_CQ_INTERRUPT_VECTORS)

#define DLB2_VDEV_MAX_NUM_INTERRUPT_VECTORS_V2 \
	(DLB2_MAX_NUM_LDB_PORTS + DLB2_MAX_NUM_DIR_PORTS_V2 + 1)
#define DLB2_VDEV_MAX_NUM_INTERRUPT_VECTORS_V2_5 \
	(DLB2_MAX_NUM_LDB_PORTS + DLB2_MAX_NUM_DIR_PORTS_V2_5 + 1)

/*
 * Hardware-defined base addresses. Those prefixed 'DLB2_DRV' are only used by
 * the PF driver.
 */
#define DLB2_DRV_LDB_PP_BASE   0x2300000
#define DLB2_DRV_LDB_PP_STRIDE 0x1000
#define DLB2_DRV_LDB_PP_BOUND  (DLB2_DRV_LDB_PP_BASE + \
				DLB2_DRV_LDB_PP_STRIDE * DLB2_MAX_NUM_LDB_PORTS)
#define DLB2_DRV_LDB_PP_OFFS(id)   (DLB2_DRV_LDB_PP_BASE + (id) * DLB2_PP_SIZE)
#define DLB2_DRV_DIR_PP_BASE   0x2200000
#define DLB2_DRV_DIR_PP_STRIDE 0x1000
#define DLB2_DRV_DIR_PP_BOUND  (DLB2_DRV_DIR_PP_BASE + \
				DLB2_DRV_DIR_PP_STRIDE * DLB2_MAX_NUM_DIR_PORTS)
#define DLB2_DRV_DIR_PP_OFFS(id)   (DLB2_DRV_DIR_PP_BASE + (id) * DLB2_PP_SIZE)
#define DLB2_LDB_PP_BASE       0x2100000
#define DLB2_LDB_PP_STRIDE     0x1000
#define DLB2_LDB_PP_BOUND      (DLB2_LDB_PP_BASE + \
				DLB2_LDB_PP_STRIDE * DLB2_MAX_NUM_LDB_PORTS)
#define DLB2_LDB_PP_OFFS(id)   (DLB2_LDB_PP_BASE + (id) * DLB2_PP_SIZE)
#define DLB2_DIR_PP_BASE       0x2000000
#define DLB2_DIR_PP_STRIDE     0x1000
#define DLB2_DIR_PP_BOUND      (DLB2_DIR_PP_BASE + \
				DLB2_DIR_PP_STRIDE * \
				DLB2_MAX_NUM_DIR_PORTS_V2_5)
#define DLB2_DIR_PP_OFFS(id)   (DLB2_DIR_PP_BASE + (id) * DLB2_PP_SIZE)

struct dlb2_resource_id {
	u32 phys_id;
	u32 virt_id;
	u8 vdev_owned;
	u8 vdev_id;
	u32 ims_idx;
};

struct dlb2_freelist {
	u32 base;
	u32 bound;
	u32 offset;
};

static inline u32 dlb2_freelist_count(struct dlb2_freelist *list)
{
	return list->bound - list->base - list->offset;
}

struct dlb2_hcw {
	u64 data;
	/* Word 3 */
	u16 opaque;
	u8 qid;
	u8 sched_type:2;
	u8 priority:3;
	u8 msg_type:3;
	/* Word 4 */
	u16 lock_id;
	u8 ts_flag:1;
	u8 rsvd1:2;
	u8 no_dec:1;
	u8 cmp_id:4;
	u8 cq_token:1;
	u8 qe_comp:1;
	u8 qe_frag:1;
	u8 qe_valid:1;
	u8 int_arm:1;
	u8 error:1;
	u8 rsvd:2;
};

struct dlb2_ldb_queue {
	struct dlb2_list_entry domain_list;
	struct dlb2_list_entry func_list;
	struct dlb2_resource_id id;
	struct dlb2_resource_id domain_id;
	u32 num_qid_inflights;
	u32 aqed_limit;
	u32 sn_group; /* sn == sequence number */
	u32 sn_slot;
	u32 num_mappings;
	u8 sn_cfg_valid;
	u8 num_pending_additions;
	u8 owned;
	u8 configured;
};

/*
 * Directed ports and queues are paired by nature, so the driver tracks them
 * with a single data structure.
 */
struct dlb2_dir_pq_pair {
	struct dlb2_list_entry domain_list;
	struct dlb2_list_entry func_list;
	struct dlb2_resource_id id;
	struct dlb2_resource_id domain_id;
	u32 ref_cnt;
	u8 init_tkn_cnt;
	u8 queue_configured;
	u8 port_configured;
	u8 owned;
	u8 enabled;
};

enum dlb2_qid_map_state {
	/* The slot doesn't contain a valid queue mapping */
	DLB2_QUEUE_UNMAPPED,
	/* The slot contains a valid queue mapping */
	DLB2_QUEUE_MAPPED,
	/* The driver is mapping a queue into this slot */
	DLB2_QUEUE_MAP_IN_PROG,
	/* The driver is unmapping a queue from this slot */
	DLB2_QUEUE_UNMAP_IN_PROG,
	/*
	 * The driver is unmapping a queue from this slot, and once complete
	 * will replace it with another mapping.
	 */
	DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP,
};

struct dlb2_ldb_port_qid_map {
	enum dlb2_qid_map_state state;
	u16 qid;
	u16 pending_qid;
	u8 priority;
	u8 pending_priority;
};

struct dlb2_ldb_port {
	struct dlb2_list_entry domain_list;
	struct dlb2_list_entry func_list;
	struct dlb2_resource_id id;
	struct dlb2_resource_id domain_id;
	/* The qid_map represents the hardware QID mapping state. */
	struct dlb2_ldb_port_qid_map qid_map[DLB2_MAX_NUM_QIDS_PER_LDB_CQ];
	u32 hist_list_entry_base;
	u32 hist_list_entry_limit;
	u32 ref_cnt;
	u8 cq_depth;
	u8 init_tkn_cnt;
	u8 num_pending_removals;
	u8 num_mappings;
	u8 owned;
	u8 enabled;
	u8 configured;
};

struct dlb2_sn_group {
	u32 mode;
	u32 sequence_numbers_per_queue;
	u32 slot_use_bitmap;
	u32 id;
};

static inline bool dlb2_sn_group_full(struct dlb2_sn_group *group)
{
	const u32 mask[] = {
		0x0000ffff,  /* 64 SNs per queue */
		0x000000ff,  /* 128 SNs per queue */
		0x0000000f,  /* 256 SNs per queue */
		0x00000003,  /* 512 SNs per queue */
		0x00000001}; /* 1024 SNs per queue */

	return group->slot_use_bitmap == mask[group->mode];
}

static inline int dlb2_sn_group_alloc_slot(struct dlb2_sn_group *group)
{
	const u32 bound[] = {16, 8, 4, 2, 1};
	u32 i;

	for (i = 0; i < bound[group->mode]; i++) {
		if (!(group->slot_use_bitmap & (1 << i))) {
			group->slot_use_bitmap |= 1 << i;
			return i;
		}
	}

	return -1;
}

static inline void
dlb2_sn_group_free_slot(struct dlb2_sn_group *group, int slot)
{
	group->slot_use_bitmap &= ~(1 << slot);
}

static inline int dlb2_sn_group_used_slots(struct dlb2_sn_group *group)
{
	int i, cnt = 0;

	for (i = 0; i < 32; i++)
		cnt += !!(group->slot_use_bitmap & (1 << i));

	return cnt;
}

struct dlb2_hw_domain {
	struct dlb2_function_resources *parent_func;
	struct dlb2_list_entry func_list;
	struct dlb2_list_head used_ldb_queues;
	struct dlb2_list_head used_ldb_ports[DLB2_NUM_COS_DOMAINS];
	struct dlb2_list_head used_dir_pq_pairs;
	struct dlb2_list_head avail_ldb_queues;
	struct dlb2_list_head avail_ldb_ports[DLB2_NUM_COS_DOMAINS];
	struct dlb2_list_head avail_dir_pq_pairs;
	struct dlb2_list_head rsvd_dir_pq_pairs;
	u32 total_hist_list_entries;
	u32 avail_hist_list_entries;
	u32 hist_list_entry_base;
	u32 hist_list_entry_offset;
	u32 num_ldb_credits;
	u32 num_dir_credits;
	u32 num_avail_aqed_entries;
	u32 num_used_aqed_entries;
	u32 num_avail_sn_slots[DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS];
	u32 num_used_sn_slots[DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS];
	struct dlb2_resource_id id;
	int num_pending_removals;
	int num_pending_additions;
	int probe_core;
	u8 configured;
	u8 started;
};

struct dlb2_bitmap;

struct dlb2_function_resources {
	struct dlb2_list_head avail_domains;
	struct dlb2_list_head used_domains;
	struct dlb2_list_head avail_ldb_queues;
	struct dlb2_list_head avail_ldb_ports[DLB2_NUM_COS_DOMAINS];
	struct dlb2_list_head avail_dir_pq_pairs;
	struct dlb2_bitmap *avail_hist_list_entries;
	u32 num_avail_domains;
	u32 num_avail_ldb_queues;
	u32 num_avail_ldb_ports[DLB2_NUM_COS_DOMAINS];
	u32 num_avail_dir_pq_pairs;
	u32 num_avail_qed_entries;
	u32 num_avail_dqed_entries;
	u32 num_avail_aqed_entries;
	u32 num_avail_sn_slots[DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS];
	u8 locked; /* (VDEV only) */
};

/*
 * After initialization, each resource in dlb2_hw_resources is located in one
 * of the following lists:
 * -- The PF's available resources list. These are unconfigured resources owned
 *	by the PF and not allocated to a dlb2 scheduling domain.
 * -- A VDEV's available resources list. These are VDEV-owned unconfigured
 *	resources not allocated to a dlb2 scheduling domain.
 * -- A domain's available resources list. These are domain-owned unconfigured
 *	resources.
 * -- A domain's used resources list. These are are domain-owned configured
 *	resources.
 *
 * A resource moves to a new list when a VDEV or domain is created or destroyed,
 * or when the resource is configured.
 */
struct dlb2_hw_resources {
	struct dlb2_ldb_queue ldb_queues[DLB2_MAX_NUM_LDB_QUEUES];
	struct dlb2_ldb_port ldb_ports[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2_dir_pq_pair dir_pq_pairs[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	struct dlb2_sn_group sn_groups[DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS];
};

struct dlb2_mbox {
	u32 *mbox;
	u32 *isr_in_progress;
};

struct dlb2_sw_mbox {
	struct dlb2_mbox vdev_to_pf;
	struct dlb2_mbox pf_to_vdev;
	void (*pf_to_vdev_inject)(void *arg);
	void *pf_to_vdev_inject_arg;
};


/* Live Migration */

typedef struct {
  bool hl_valid;
  bool rob_valid;
  //struct dlb2_ldb_port *hl_port;
  uint16_t hl_port_id;
  uint16_t hl_idx;
} sn_state_t;

typedef struct {
  uint32_t pop_ptr_val;
  uint16_t pop_ptr;
  uint32_t push_ptr_val;
  uint16_t push_ptr;
  bool pop_ptr_gen;
  bool push_ptr_gen;
  uint16_t inflights;
  uint16_t inflights_limit;
  uint16_t tkn_cnt;
  uint16_t hist_list_entry_base;
  uint16_t hist_list_entry_limit;
} mig_cq_state_t;

typedef struct {
  sn_state_t sn_state[DLB2_MAX_NUM_SEQUENCE_NUMBERS];
  uint16_t num_sn_in_hl;
  uint16_t num_sn_in_rob;
  uint16_t sn_list[DLB2_MAX_NUM_SEQUENCE_NUMBERS];
  struct dlb2_hcw drain_hcw[8192];
  struct dlb2_hcw drain_rob_hcw[DLB2_MAX_NUM_SEQUENCE_NUMBERS];
  uint16_t num_drain_hcws;
  uint16_t num_drain_rob_hcws;
  uint8_t renq_qid;
  bool renq_type;
} mig_ldb_qid_state_t;

typedef struct {
  struct dlb2_hcw drain_hcw[4096];
  uint16_t num_drain_hcws;
} mig_dir_qid_state_t;


struct hl_t {
  unsigned sn_fid:12;
  unsigned slot:5;
  unsigned mode:3;
  unsigned qidix:3;
  unsigned qid:7;

  unsigned qidprio:3; /* broken up across 2 entries */

  /* byte 4 */
  unsigned qtype:2;
  unsigned meas:1;
  unsigned cmp_id:4;

  /* byte 5,6 */
  unsigned hid:16;

  /* byte 7 */
  unsigned ecc:7;
  unsigned tlow:1;
};

struct cq_addr_t {
	uint32_t up;
	uint32_t low;
};

struct dlb2_migration_state {
	/* To be maintained locally */
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *dummy_ldb_port;
	struct dlb2_dir_pq_pair *dummy_dir_port;
	struct dlb2_ldb_port *ldb_port[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2_dir_pq_pair *dir_port[DLB2_MAX_NUM_DIR_PORTS_V2];
	struct dlb2_ldb_queue *ldb_queue[DLB2_MAX_NUM_LDB_QUEUES];

	/* To be transferred for the destination */
	uint8_t num_dir_ports;
	uint8_t num_ldb_ports;
	uint8_t num_ldb_queues;
	uint32_t ldb_cq_wptr[DLB2_MAX_NUM_LDB_PORTS];
	struct cq_addr_t ldb_cq_addr[DLB2_MAX_NUM_LDB_PORTS];
	mig_cq_state_t ldb_cq_state[DLB2_MAX_NUM_LDB_PORTS];
	mig_ldb_qid_state_t ldb_qid_state[DLB2_MAX_NUM_LDB_QUEUES];
	struct hl_t hlist_state[DLB2_MAX_NUM_HIST_LIST_ENTRIES];
	uint32_t hl_ptr[DLB2_MAX_NUM_HIST_LIST_ENTRIES];
	
	uint32_t dir_cq_wptr[DLB2_MAX_NUM_DIR_PORTS_V2];
	struct cq_addr_t dir_cq_addr[DLB2_MAX_NUM_DIR_PORTS_V2];
	mig_cq_state_t dir_cq_state[DLB2_MAX_NUM_DIR_PORTS_V2];
	mig_dir_qid_state_t dir_qid_state[DLB2_MAX_NUM_DIR_PORTS_V2];

	int dummy_cq_hist_list_base;
	int dummy_cq_hist_list_limit;
};

struct dlb2_migrate_t {
        struct dlb2_migration_state *src_vm_state;
        struct dlb2_migration_state *dst_vm_state;
};

/*!
 *  @brief Event Scheduling Types
 */
enum dlb2_sched_t {
    ATOMIC = 0,
    UNORDERED,
    ORDERED,
    DIRECTED
};

struct dlb2_hw {
	enum dlb2_hw_ver ver;

	/* BAR 2 address */
	void __iomem *csr_kva;
	unsigned long csr_phys_addr;
	/* BAR 0 address */
	void __iomem *func_kva;
	unsigned long func_phys_addr;

	/* Resource tracking */
	struct dlb2_hw_resources rsrcs;
	struct dlb2_function_resources pf;
	struct dlb2_function_resources vdev[DLB2_MAX_NUM_VDEVS];
	struct dlb2_hw_domain domains[DLB2_MAX_NUM_DOMAINS];
	u8 cos_reservation[DLB2_NUM_COS_DOMAINS];
	/* dir port order per core */
	int **dir_pp_allocations;
	/* ldb port order + cos order per core */
	int **ldb_pp_allocations;
	int num_phys_cpus;
	bool probe_done;

	/* Virtualization */
	int virt_mode;
	struct dlb2_sw_mbox mbox[DLB2_MAX_NUM_VDEVS];
	unsigned int pasid[DLB2_MAX_NUM_VDEVS];

	/* VM Live Migration */
	struct dlb2_migrate_t mig_state;
};

#endif /* __DLB2_HW_TYPES_H */
