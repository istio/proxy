/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef __DLB_PRIV_H__
#define __DLB_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <pthread.h>

#include "dlb2_user.h"

#define xstr(s) str(s)
#define str(s) #s

/* DLB related macros */
#define MAX_NUM_SCHED_DOMAINS 32
#define MAX_NUM_LDB_PORTS 64
#define MAX_NUM_DIR_PORTS 128
#define NUM_PORT_TYPES 2
#define MAX_NUM_LDB_QUEUES 128
#define MAX_NUM_DIR_QUEUES 128
#define MAX_NUM_LDB_CREDIT_POOLS 64
#define MAX_NUM_DIR_CREDIT_POOLS 64
#define BYTES_PER_QE 16
#define NUM_QID_INFLIGHTS 2048
/* The DLB has 2K atomic inflights, and we evenly divide them among its
 * load-balanced queues.
 */
#define NUM_V2_ATM_INFLIGHTS_PER_LDB_QUEUE 64
#define DLB_MAX_PATH_LEN (DLB2_MAX_NAME_LEN + 32)

#define NUM_V2_LDB_SN_GROUPS 2
#define NUM_V2_MIN_LDB_SN_ALLOC 64
#define MAX_LDB_SN_ALLOC 1024

/* shm related macros */
/* Each SHM region contains enough memory for dlb_shared_domain_t, every CQ (at
 * most 4KB per CQ), and every PP's popcount (2 cache lines per PP). The SHM
 * region also contains a padding page between dlb_shared_domain_t and the CQ
 * memory, to ensure the first CQ can begin at the start of a page.
 *
 * Note that since port IDs are allocated by the kernel driver, the CQ and PC
 * memory is not laid out in order of port IDs. In other words, port 0 isn't
 * necessarily using the first CQ page or PC cache lines.
 */
#define DLB_SHM_SIZE                           \
    (sizeof(dlb_shared_domain_t) + PAGE_SIZE + \
     (PAGE_SIZE * MAX_NUM_LDB_PORTS) +             \
     (PAGE_SIZE * MAX_NUM_DIR_PORTS) +             \
     ((2 * CACHE_LINE_SIZE) * MAX_NUM_LDB_PORTS) + \
     ((2 * CACHE_LINE_SIZE) * MAX_NUM_DIR_PORTS))

#define ROUND_UP_4KB(addr) \
    (((uintptr_t)addr + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

#define DLB_LDB_CQ_ADDR(base, id)                       \
    (void*)((uintptr_t)base +                           \
            ROUND_UP_4KB(sizeof(dlb_shared_domain_t)) + \
            id * PAGE_SIZE)

#define DLB_DIR_CQ_ADDR(base, id) \
    (void*)(DLB_LDB_CQ_ADDR(base, MAX_NUM_LDB_PORTS) + id * PAGE_SIZE)

#define DLB_LDB_PC_ADDR(base, id) \
    (void*)(DLB_DIR_CQ_ADDR(base, MAX_NUM_DIR_PORTS) + id * 2 * CACHE_LINE_SIZE)

#define DLB_DIR_PC_ADDR(base, id) \
    (void*)(DLB_LDB_PC_ADDR(base, MAX_NUM_LDB_PORTS) + id * 2 * CACHE_LINE_SIZE)

/* Data mover related macros */
#define BYTES_PER_NQ_ENTRY 16
#define HBM_MIN_RING_DEPTH 16384
#define BYTES_PER_RING_ENTRY 8
#define NQ_MIN_DEPTH 4096

/* Software credits related macros */
#define DLB_SW_CREDIT_BATCH_SZ 32

/* Memory system related macros */
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#define CACHE_LINE_SIZE 64
#define CACHE_LINE_MASK (CACHE_LINE_SIZE - 1)

/* Lovingly lifted from DPDK's (also BSD-licensed) EAL library */
#ifndef __OPTIMIZE__
#define BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else
extern int BUG_ON_detected_error;
#define BUG_ON(condition) do {                 \
    ((void)sizeof(char[1 - 2*!!(condition)])); \
    if (condition)                             \
        BUG_ON_detected_error = 1;             \
} while(0)
#endif

/* ASSERT() is always enabled. Used for configuration functions. */
#define ASSERT(cond, err) \
do {                      \
    if (!(cond))          \
    {                     \
        errno = err;      \
        goto cleanup;     \
    }                     \
} while(0)

/* CHECK() can be disabled at compile time. Used for datapath functions. */
#ifndef DISABLE_CHECK
#define CHECK(cond, err) \
do {                     \
    if (!(cond))         \
    {                    \
        errno = err;     \
        goto cleanup;    \
    }                    \
} while(0)
#else
#define CHECK(cond, err)
#endif

/* DEBUG_ONLY() is used for statements that the compiler couldn't otherwise
 * optimize.
 */
#ifndef DISABLE_CHECK
#define DEBUG_ONLY(x) x
#else
#define DEBUG_ONLY(x)
#endif

#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define DLB_MAGIC_NUM  0xBEEFFACE
#define DOMAIN_MAGIC_NUM  0x12344321
#define PORT_MAGIC_NUM 0x43211234

#define VALIDATE_DLB_HANDLE(hdl) \
    ASSERT(hdl != NULL, EINVAL); \
    ASSERT(((dlb_t*) hdl)->magic_num == DLB_MAGIC_NUM, EINVAL)
#define VALIDATE_DOMAIN_HANDLE(hdl) \
    ASSERT(hdl != NULL, EINVAL); \
    ASSERT(((dlb_domain_hdl_internal_t*) hdl)->magic_num == \
            DOMAIN_MAGIC_NUM, EINVAL)
#define VALIDATE_PORT_HANDLE(hdl) \
    ASSERT((dlb_port_hdl_internal_t*) hdl != NULL, EINVAL); \
    ASSERT(((dlb_port_hdl_internal_t*) hdl)->magic_num == PORT_MAGIC_NUM, EINVAL);
#define CHECK_PORT_HANDLE(hdl) \
    CHECK((dlb_port_hdl_internal_t*) hdl != NULL, EINVAL); \
    CHECK(((dlb_port_hdl_internal_t*) hdl)->magic_num == PORT_MAGIC_NUM, EINVAL);

/*** Linked-list helper functions ***/

/* Add 'entry' to list pointed to by 'head' */
#define LIST_ADD(head, entry)     \
    do {                          \
        if (!head) {              \
            head = entry;         \
            entry->next = NULL;   \
        } else {                  \
            entry->next = head;   \
            head = entry;         \
        }                         \
    } while(0)

/* Remove 'entry' from list pointed to by 'head' */
#define LIST_DEL(head, entry, found)                         \
    do {                                                     \
        found = false;                                       \
        if (head == entry) {                                 \
            head = head->next;                               \
            found = true;                                    \
            break;                                           \
        } else {                                             \
            typeof(head) tmp;                                \
            for (tmp = head; tmp != NULL; tmp = tmp->next) { \
                if (tmp->next == entry) {                    \
                    tmp->next = tmp->next->next;             \
                    found = true;                            \
                    break;                                   \
                }                                            \
            }                                                \
        }                                                    \
    } while(0)                                               \

/* Wait profile helper macros */
#define NS_PER_S 1000000000UL
/* Delay between hard poll in nano seconds */
#define POLL_INTERVAL_NS 2000

typedef enum {
    VER_DLB = 1,
    VER_DLB2,
    VER_DLB2_5,
} dlb_device_version;

typedef struct {
    uint8_t ver;
    uint8_t rev;
} dlb_device_ver_t;

#if 0
static inline bool
dlb_is_v1_a_stepping(dlb_device_ver_t ver)
{
	return ver.ver == 1 && ver.rev < DLB_REV_B0;
}
#endif
/* Don't define DLB_SOCKET_PREFIX if it is user-defined */
#ifndef DLB_SOCKET_PREFIX
#define DLB_SOCKET_PREFIX "/tmp/__dlb_domain"
#endif

/*************************/
/** DLB port structures **/
/*************************/


typedef struct dlb_port {
    struct dlb_local_port *local;
    struct dlb_shared_port *shared;
} dlb_port_t;

typedef struct dlb_port_hdl_internal {
    uint32_t magic_num;
    struct dlb_port port;
    dlb_dev_cap_t cap;
    /* Wait profile */
    dlb_wait_profile_t wait_profile;
    int (*wait_func)(struct dlb_port_hdl_internal *hdl,
                     struct dlb_shared_port *port);
    int event_fd;

    uint64_t umwait_ticks;
    /* Cache line's worth of QEs (4) */
    struct dlb_enqueue_qe *qe;
    /* PP-related fields */
    void (*enqueue_four)(struct dlb_enqueue_qe *qe4, uint64_t *pp_addr);
    uint64_t *pp_addr;

    /* Local pointers to shared memory. These copies allow port operations to
     * avoid offset pointer calculations.
     */
    volatile struct dlb_dequeue_qe *cq_base;
    volatile uint16_t *popcount[NUM_PORT_TYPES];

    /* Software credits (v2 only) */
    volatile uint32_t *credit_pool[NUM_PORT_TYPES];

    /* Shared memory base, used for offset pointer calculations */
    void *shared_base;

    dlb_device_ver_t device_version;
    struct dlb_domain_hdl_internal *domain_hdl;
    struct dlb_port_hdl_internal *next;
} dlb_port_hdl_internal_t;

typedef enum {
    LDB,
    DIR,
} dlb_port_type_t;

#define REFRESH_PORT_CREDITS(port_hdl, port, type)            \
    port->credits[type].num = *port_hdl->popcount[type] -     \
                               port->credits[type].pushcount;

typedef struct {
    uint16_t pushcount;
    uint16_t num;
} dlb_port_credits_t;

#define PORT_CQ_IS_EMPTY(hdl, port) \
    (hdl->cq_base[port->cq_idx].cq_gen != port->cq_gen)

typedef struct {
    int64_t count[NUM_DLB_QUEUE_DEPTH_LEVELS];
    int64_t reset[NUM_DLB_QUEUE_DEPTH_LEVELS];
} dlb_queue_level_t;

typedef struct {
    int credit_thres;
    int credit_rem;
    int cnt_thres;
} dlb_credit_return_t;

enum {
    ZERO_DEQ,
    ENQ_FAIL,
    BATCH_SZ_EXCEED,
    BATCH_2SZ_EXCEED,
    RETURN_ALL,
    NUM_CREDIT_RET_TYPES
};

typedef struct dlb_shared_port {
    /* PP-related fields */
    dlb_port_credits_t credits[NUM_PORT_TYPES];
    int ldb_pool_id;
    int dir_pool_id;
    bool ts_enabled;

    /* CQ-related fields */
    int cq_idx;
    int cq_depth;
    uint8_t cq_gen;
    uint8_t qe_stride;
    uint16_t cq_limit;
    uint16_t owed_tokens;
    uint16_t owed_releases;
    uint16_t cq_rsvd_token_deficit;
    bool use_rsvd_token_scheme;
    bool int_armed;

    /* Misc */
    int id;
    dlb_queue_level_t queue_levels[MAX_NUM_LDB_QUEUES];
    dlb_port_type_t type;
    pthread_mutex_t resource_mutex; /* Guards shared and local resources */
    volatile bool enabled;
    bool configured;
    uint16_t credit_return_count[NUM_CREDIT_RET_TYPES]; /* Count for credit return cond true */
} dlb_shared_port_t;

typedef struct dlb_local_port {
    dlb_port_hdl_internal_t *handles;
} dlb_local_port_t;


/***************************/
/** DLB Domain structures **/
/***************************/


typedef struct dlb_domain {
    struct dlb_local_domain *local;
    struct dlb_shared_domain *shared;
    dlb_device_ver_t device_version;
} dlb_domain_t;

typedef struct dlb_domain_hdl_internal {
    uint32_t magic_num;
    int fd;
    struct dlb_domain domain;
    dlb_dev_cap_t cap;

    /* Shared memory base, used for offset pointer calculations */
    void *shared_base;

    struct dlb *dlb;
    struct dlb_domain_hdl_internal *next;
} dlb_domain_hdl_internal_t;

typedef enum {
    QUEUE_TYPE_INVALID = 0,
    QUEUE_TYPE_REGULAR = 1,
} dlb_queue_type_t;

typedef enum {
    DLB_DOMAIN_USER_ALERT_RESET,
} dlb_domain_user_alert_t;

typedef struct {
    domain_alert_callback_t fn;
    void *arg;
} dlb_domain_alert_thread_t;

typedef struct {
    bool configured;
    volatile uint32_t avail_credits;
} dlb_sw_credit_pool_t;

typedef struct {
    uint32_t total_credits[NUM_PORT_TYPES];
    uint32_t avail_credits[NUM_PORT_TYPES];
    dlb_sw_credit_pool_t ldb_pools[MAX_NUM_LDB_CREDIT_POOLS];
    dlb_sw_credit_pool_t dir_pools[MAX_NUM_DIR_CREDIT_POOLS];
} dlb_sw_credits_t;

typedef struct dlb_shared_domain {
    int id;
    dlb_shared_port_t ldb_ports[MAX_NUM_LDB_PORTS];
    dlb_shared_port_t dir_ports[MAX_NUM_DIR_PORTS];
    uint8_t queue_type[NUM_PORT_TYPES][MAX_NUM_LDB_QUEUES];
    uint32_t num_ldb_queues;
    uint32_t num_dir_queues;
    bool use_rsvd_token_scheme;
    bool alert_thread_started;
    int port_index[NUM_PORT_TYPES];
    pthread_mutex_t resource_mutex; /* Guards shared and local resources */
    char name[DLB2_MAX_NAME_LEN];
    dlb_sw_credits_t sw_credits; /* v2 only */
    int refcnt;
    bool configured;
    bool started;
} dlb_shared_domain_t;

typedef struct dlb_local_domain {
    /* Valid only for the domain creator */
    int creator_fd;
    bool creator;
    pthread_t socket_thread;
    int socket_fd;

    dlb_local_port_t ldb_ports[MAX_NUM_LDB_PORTS];
    dlb_local_port_t dir_ports[MAX_NUM_DIR_PORTS];
    dlb_domain_alert_thread_t thread;
    dlb_domain_hdl_internal_t *handles;
    /* Shared memory base, used for offset pointer calculations */
    void *shared_base;
} dlb_local_domain_t;


/********************/
/** DLB structures **/
/********************/


typedef struct dlb {
    uint32_t magic_num;
    int id;
    int fd;
    dlb_device_ver_t device_version;
    dlb_dev_cap_t cap;
    pthread_mutex_t resource_mutex; /* Guards shared and local resources */
    dlb_shared_domain_t *shared_domains[MAX_NUM_SCHED_DOMAINS];
    dlb_local_domain_t local_domains[MAX_NUM_SCHED_DOMAINS];
} dlb_t;

/*******************/
/** QE structures **/
/*******************/

#define DLB2_CMD_ARM 5

#define QE_COMP_SHIFT 1
#define QE_CMD_MASK 0x0F
#define QE_WEIGHT_MASK 0x06

typedef struct {
    uint8_t qe_cmd:4;
    uint8_t int_arm:1;
    uint8_t error:1;
    uint8_t rsvd:2;
} __attribute__((packed)) dlb_enqueue_cmd_info_t;

typedef struct dlb_enqueue_qe {
    uint64_t data;
    uint16_t opaque;
    uint8_t qid;
    uint8_t sched_byte;
    union {
        uint16_t flow_id;
        uint16_t num_tokens_minus_one;
    };
    union {
        struct {
            uint8_t meas_lat:1;
            uint8_t weight:2;
            uint8_t no_dec:1;
            uint8_t cmp_id:4;
        };
        uint8_t misc_byte;
    };
    union {
        dlb_enqueue_cmd_info_t cmd_info;
        uint8_t cmd_byte;
    };
} __attribute__((packed)) __attribute__ ((aligned (sizeof(long long)))) dlb_enqueue_qe_t;

typedef struct dlb_dequeue_qe {
    uint64_t data;
    uint16_t opaque;
    uint8_t qid;
    uint8_t sched_byte;
    uint16_t pp_id:10;
    uint16_t rsvd0:6;
    uint8_t debug;
    uint8_t cq_gen:1;
    uint8_t qid_depth:2;
    uint8_t rsvd1:2;
    uint8_t error:1;
    uint8_t rsvd2:2;
} __attribute__((packed)) dlb_dequeue_qe_t;

#ifdef __cplusplus
}
#endif

#endif /* __DLB_PRIV_H__ */
