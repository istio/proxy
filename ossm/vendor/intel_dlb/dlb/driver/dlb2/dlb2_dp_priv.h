/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_PRIV_DP_H
#define __DLB2_PRIV_DP_H

#include <linux/list.h>

#include "base/dlb2_hw_types.h"

/* DLB related macros */
#define NUM_PORT_TYPES 2
#define BYTES_PER_CQ_ENTRY 16
#define PP_BASE(type) ((type == LDB) ? DLB2_LDB_PP_BASE : DLB2_DIR_PP_BASE)
/*
 * There are 32 LDB queues and 2K atomic inflights, and we evenly divide them
 * among the queues (64 per queue).
 */
#define NUM_ATM_INFLIGHTS_PER_LDB_QUEUE 64
#define NUM_LDB_CREDIT_POOLS 64
#define NUM_DIR_CREDIT_POOLS 64

#define DLB2_SW_CREDIT_BATCH_SZ 32

/* Memory system related macros */
#define CACHE_LINE_SIZE 64
#define CACHE_LINE_MASK (CACHE_LINE_SIZE - 1)

/*
 * DEBUG_ONLY() is used for statements that the compiler couldn't otherwise
 * optimize.
 */
#ifndef DISABLE_CHECK
#define DEBUG_ONLY(x) x
#else
#define DEBUG_ONLY(x)
#endif

#define DLB2_MAGIC_NUM  0xBEEFFACE
#define DOMAIN_MAGIC_NUM  0x12344321
#define PORT_MAGIC_NUM 0x43211234

/*************************/
/** DLB port structures **/
/*************************/

struct dlb2_port_hdl {
	struct list_head list;
	u32 magic_num;
	struct dlb2_dp_port *port;
	/* Cache line's worth of QEs (4) */
	struct dlb2_enqueue_qe *qe;
};

enum dlb2_port_type {
	LDB,
	DIR,
};

struct dlb2_dp_port {
	/* PP-related fields */
	void __iomem *pp_addr;
	atomic_t *credit_pool[NUM_PORT_TYPES];
	u16 num_credits[NUM_PORT_TYPES];

	void (*enqueue_four)(void *qe4, void __iomem *pp_addr);

	/* CQ-related fields */
	int cq_idx;
	int cq_depth;
	u8 cq_gen;
	uint8_t qe_stride;
	uint16_t cq_limit;
	struct dlb2_dequeue_qe *cq_base;
	u16 owed_tokens;
	u16 owed_releases;
	u8 int_armed;

	/* Misc */
	int id;
	struct dlb2_dp_domain *domain;
	enum dlb2_port_type type;
	struct list_head hdl_list_head;
	/* resource_mutex protects port data during configuration operations */
	struct mutex resource_mutex;
	u8 enabled;
	u8 configured;
};

/***************************/
/** DLB Domain structures **/
/***************************/

struct dlb2_domain_hdl {
	struct list_head list;
	u32 magic_num;
	struct dlb2_dp_domain *domain;
};

enum dlb2_domain_user_alert {
	DLB2_DOMAIN_USER_ALERT_RESET,
};

struct dlb2_domain_alert_thread {
	void (*fn)(void *alert, int domain_id, void *arg);
	void *arg;
	u8 started;
};

struct dlb2_sw_credit_pool {
	u8 configured;
	atomic_t avail_credits;
};

struct dlb2_sw_credits {
	u32 avail_credits[NUM_PORT_TYPES];
	struct dlb2_sw_credit_pool ldb_pools[NUM_LDB_CREDIT_POOLS];
	struct dlb2_sw_credit_pool dir_pools[NUM_DIR_CREDIT_POOLS];
};

struct dlb2_dp_domain {
	int id;
	struct dlb2 *dlb2;
	struct dlb2_domain *domain_dev;
	u8 shutdown;
	struct dlb2_dp_port ldb_ports[DLB2_MAX_NUM_LDB_PORTS];
	struct dlb2_dp_port dir_ports[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	u8 queue_valid[NUM_PORT_TYPES][DLB2_MAX_NUM_LDB_QUEUES];
	struct dlb2_sw_credits sw_credits;
	u8 reads_allowed;
	unsigned int num_readers;
	struct dlb2_domain_alert_thread thread;
	struct dlb2_dp *dlb2_dp;
	/* resource_mutex protects domain data during configuration ops */
	struct mutex resource_mutex;
	u8 configured;
	u8 started;
	struct list_head hdl_list_head;
};

#define DEV_FROM_DLB2_DP_DOMAIN(dom) (&(dom)->dlb2->pdev->dev)

/********************/
/** DLB structures **/
/********************/

struct dlb2_dp {
	struct list_head next;
	u32 magic_num;
	int id;
#ifdef CONFIG_PM
	int pm_refcount;
#endif
	struct dlb2 *dlb2;
	/* resource_mutex protects device data during configuration ops */
	struct mutex resource_mutex;
	struct dlb2_dp_domain domains[DLB2_MAX_NUM_DOMAINS];
};

/***************************/
/** "Advanced" structures **/
/***************************/

/*
 * Possible future work: Expose advanced port creation functions to allow
 * expert users to provide their own memory space for CQ and PC and their
 * own credit configurations.
 */
struct dlb2_create_port_adv {
	/* CQ base address */
	uintptr_t cq_base;
	/* History list size */
	u16 cq_history_list_size;
	/* Load-balanced credit low watermark */
	u16 ldb_credit_low_watermark;
	/* Load-balanced credit quantum */
	u16 ldb_credit_quantum;
	/* Directed credit low watermark */
	u16 dir_credit_low_watermark;
	/* Directed credit quantum */
	u16 dir_credit_quantum;
};

/*******************/
/** QE structures **/
/*******************/

#define CMD_ARM 5

struct dlb2_enqueue_cmd_info {
	u8 qe_cmd:4;
	u8 int_arm:1;
	u8 error:1;
	u8 rsvd:2;
} __packed;

struct dlb2_enqueue_qe {
	u64 data;
	u16 opaque;
	u8 qid;
	u8 sched_byte;
	u16 flow_id;
	u8 meas_lat:1;
	u8 rsvd1:2;
	u8 no_dec:1;
	u8 cmp_id:4;
	union {
		struct dlb2_enqueue_cmd_info cmd_info;
		u8 cmd_byte;
	};
} __packed;

#define DLB2_QE_STATUS_CQ_GEN_MASK 0x1

struct dlb2_dequeue_qe {
	u64 data;
	u16 opaque;
	u8 qid;
	u8 sched_byte;
	u16 pp_id:10;
	u16 rsvd0:6;
	u8 debug;
	u8 status;
} __packed;

void dlb2_datapath_init(struct dlb2 *dlb2, int id);
void dlb2_datapath_free(int id);

#endif /* __DLB2_PRIV_DP_H */
