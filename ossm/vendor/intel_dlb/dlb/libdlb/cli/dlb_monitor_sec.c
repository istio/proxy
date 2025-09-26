/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

/* This tool displays the monitoring data for libdlb applications.
 *  It obtains data from the dlb device file periodically.
 *  -i can be used to pass the device_id
 *  -z can be used to skip zero values
 *  -w can be used to display the data continuously.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include <pthread.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include "dlb.h"
#include "dlb_priv.h"
#include "dump_dlb_regs.h"
#include "dump_dlb2_5_regs.h"

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))

#define CSR_BAR_SIZE  (4ULL * 1024ULL * 1024ULL * 1024ULL)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define US_PER_S 1000000
#define RTE_DIM(a) (sizeof(a) / sizeof((a)[0]))
#define NUM_EVENTS_PER_LOOP 32
#define RETRY_LIMIT 10000000
#define NUM_LDB_PORTS 20
#define BUF_LEN 256
#define DLB_SYS_PATH_BASE "/sys/class/dlb2/dlb"
#define DLB_RSCRS_PATH "/device/total_resources"
#define DLB_RESOURCE2_PATH "/device/resource2"
#define DLB_DEV_XSTATS_NAME_SIZE 64
#define MAX_PORTS_QUEUES (32 + 64 + 96 + 96) 
#define CQ_DEPTH 8
#define MAX_DEPTH_STRING_LEN 62
#define MAX_CSV_FILE_NAME_LEN 32

static dlb_dev_cap_t cap;
static int dev_id;
static uint64_t num_events;
static unsigned int sns_per_queue;
static int num_credit_combined;
static int num_credit_ldb;
static int num_credit_dir;
static int num_workers = 1;
static bool use_max_credit_combined = true;
static bool use_max_credit_ldb = true;
static bool use_max_credit_dir = true;
static int num_dlb_regs;
static dlb_device_version ver;

static dlb_resources_t rsrcs;
static dlb_hdl_t dlb;

/* Default is false for all flags. Enable them from command line */
static bool do_reset = false;
static bool do_watch = false;  /* Watch mode */
static bool skip_zero = false; /* Skip zero values */
static bool prt_ldb = false;   /* Print LDB queues */
static bool prt_dir = false;   /* Print DIR queues */
static bool prt_cq = false;    /* Print CQ statstics */
static bool prt_glb = false;   /* Print Global Statistics */
static bool out_file = false;  /* Write to CSV file */

static void get_device_xstats(void);
static void collect_config(void);
static void display_config(void);
static void collect_stats(void);
static void display_stats(void);
static void display_queue_stats(void);
static void display_device_config(void);

static char filename[MAX_CSV_FILE_NAME_LEN];
static char header[]="interval,hcw_atm_enq,hcw_atm_deq,hcw_dir_enq,hcw_dir_deq,hcw_nalb_enq,hcw_nalb_deq,inf_evt,evt_limit,nldb_rate,aldb_rate,dir_rate\n";
static FILE *fp=NULL; /* Stats CSV file */
static FILE *res_fp=NULL; /* Resource CSV file */
static uint8_t *base;

struct dlb_dev_xstats_name {
	char name[DLB_DEV_XSTATS_NAME_SIZE];
};

/* Rate calculations */
static uint32_t measure_time_us = 1 * US_PER_S;
static double time_elapsed;
static struct timespec start_time, stop_time;
static uint64_t hcw_ldb_prev;
static uint64_t hcw_atm_prev;
static uint64_t hcw_dir_prev;

/* Following are the dev ice resources read from sysfs*/
static uint32_t num_cos0_ldb_ports;
static uint32_t num_cos1_ldb_ports;
static uint32_t num_cos2_ldb_ports;
static uint32_t num_cos3_ldb_ports;
static uint32_t num_dir_ports;
static uint32_t num_atomic_inflights;
static uint32_t num_dir_credits;
static uint32_t num_hist_list_entries;
static uint32_t num_ldb_credits;
static uint32_t num_ldb_ports;
static uint32_t num_ldb_queues;
static uint32_t num_sched_domains;
static uint32_t num_sn0_slots;
static uint32_t num_sn1_slots;

#define READ_SYS_PRT(val)                                                                                   \
	{                                                                                                   \
		snprintf(path, sizeof(path), "%s%d%s/%s", DLB_SYS_PATH_BASE, dev_id, DLB_RSCRS_PATH, #val); \
		fd = open(path, O_RDONLY);                                                                  \
		if (fd >= 0) {                                                                              \
			if (read(fd, buf, sizeof(buf) - 1) < 0) {                                           \
				printf("Error in reading %s\n", #val);                                      \
				exit(0);                                                                    \
			}                                                                                   \
			val = atoi(buf);                                                                    \
			printf("\t%s: %d\n", #val, val);                                                    \
			close(fd);                                                                          \
		} else {                                                                                    \
			printf("Error in opening file%s\n", path);                                          \
			exit(0);                                                                            \
		}                                                                                           \
	}

static const char * const dev_xstat_strs[] = {
	"cfg_cq_ldb_tot_inflight_count",
	"cfg_cq_ldb_tot_inflight_limit",
	"cfg_fid_inflight_count",
	"cfg_cmp_pp_nq_hptr_ldb_credit",
	"dlb_dm.cfg_cmp_pp_nq_hptr_dir_credit",
	"dev_pool_size",
	"cfg_counter_dequeue_hcw_atm",
	"cfg_counter_enqueue_hcw_atm",
	"cfg_counter_dequeue_hcw_dir",
	"cfg_counter_enqueue_hcw_dir",
	"cfg_counter_dequeue_hcw_nalb",
	"cfg_counter_enqueue_hcw_nalb",
	"cfg_aqed_tot_enqueue_count",
	"cfg_aqed_tot_enqueue_limit",
	"cfg_counter_enqueue_hcw_dir_h",
	"cfg_counter_enqueue_hcw_dir_l",
	"cfg_counter_enqueue_hcw_ldb_h",
	"cfg_counter_enqueue_hcw_ldb_l",
	"cfg_counter_atm_qe_sch_l",
	"cfg_counter_atm_qe_sch_h",
};

enum dlb_dev_xstats {
	DEV_INFL_EVENTS,
	DEV_NB_EVENTS_LIMIT,
	CFG_FID_INF_CNT,
	DEV_LDB_POOL_SIZE,
	DEV_DIR_POOL_SIZE,
	DEV_POOL_SIZE,
	CFG_COUNTER_DEQUEUE_HCW_ATM,
	CFG_COUNTER_ENQUEUE_HCW_ATM,
	CFG_COUNTER_DEQUEUE_HCW_DIR,
	CFG_COUNTER_ENQUEUE_HCW_DIR,
	CFG_COUNTER_DEQUEUE_HCW_NALB,
	CFG_COUNTER_ENQUEUE_HCW_NALB,
	DEV_AQED_ENQ_CNT,
	DEV_AQED_ENQ_LIMIT,
	CFG_CHP_CNT_DIR_HCW_ENQ_H,
	CFG_CHP_CNT_DIR_HCW_ENQ_L,
	CFG_CHP_CNT_LDB_HCW_ENQ_H,
	CFG_CHP_CNT_LDB_HCW_ENQ_L,
	CFG_CHP_CNT_ATM_QE_SCH_L,
	CFG_CHP_CNT_ATM_QE_SCH_H,
};

static const char * const port_xstat_strs[] = {
	"tx_ok",
	"tx_new",
	"tx_fwd",
	"tx_rel",
	"tx_sched_ordered",
	"tx_sched_unordered",
	"tx_sched_atomic",
	"tx_sched_directed",
	"tx_invalid",
	"tx_nospc_ldb_hw_credits",
	"tx_nospc_dir_hw_credits",
	"tx_nospc_hw_credits",
	"tx_nospc_inflight_max",
	"tx_nospc_new_event_limit",
	"tx_nospc_inflight_credits",
	"outstanding_releases",
	"max_outstanding_releases",
	"total_polls",
	"zero_polls",
	"rx_ok",
	"rx_sched_ordered",
	"rx_sched_unordered",
	"rx_sched_atomic",
	"rx_sched_directed",
	"rx_sched_invalid",
	"is_configured",
	"is_load_balanced",
};

enum dlb_port_xstats {
	TX_OK,
	TX_NEW,
	TX_FWD,
	TX_REL,
	TX_SCHED_ORDERED,
	TX_SCHED_UNORDERED,
	TX_SCHED_ATOMIC,
	TX_SCHED_DIRECTED,
	TX_SCHED_INVALID,
	TX_NOSPC_LDB_HW_CREDITS,
	TX_NOSPC_DIR_HW_CREDITS,
	TX_NOSPC_HW_CREDITS,
	TX_NOSPC_INFL_MAX,
	TX_NOSPC_NEW_EVENT_LIM,
	TX_NOSPC_INFL_CREDITS,
	OUTSTANDING_RELEASES,
	MAX_OUTSTANDING_RELEASES,
	TOTAL_POLLS,
	ZERO_POLLS,
	RX_OK,
	RX_SCHED_ORDERED,
	RX_SCHED_UNORDERED,
	RX_SCHED_ATOMIC,
	RX_SCHED_DIRECTED,
	RX_SCHED_INVALID,
	IS_CONFIGURED,
	PORT_IS_LOAD_BALANCED,
};

static const char * const queue_xstat_strs[] = {
	"current_depth",
	"is_load_balanced",
	"cfg_qid_ldb_inflight_count",
	"cfg_qid_ldb_inflight_limit",
	"cfg_qid_aqed_active_count",
	"cfg_atm_qid_dpth_thrsh",
	"cfg_nalb_qid_dpth_thrsh",
	"cfg_ldb_cq_depth",
	"cfg_cq_ldb_token_count",
	"cfg_cq_ldb_token_depth_select",
	"cfg_cq_dir_token_depth_select",
	"cfg_cq_ldb_inflight_count",
	"cfg_dir_cq_depth",
	"cfg_dir_qid_dpth_thrsh",
	"cfg_qid_atq_enqueue_count",
	"cfg_qid_ldb_enqueue_count",
	"cfg_qid_dir_enqueue_count",
};

enum dlb_queue_xstats {
	CURRENT_DEPTH=0,
	QUEUE_IS_LOAD_BALANCED, 
	CFG_QID_LDB_INFLIGHT_COUNT,
	CFG_QID_LDB_INFLIGHT_LIMIT,
	CFG_QID_ATM_ACTIVE,
	CFG_QID_ATM_DEPTH_THRSH,
	CFG_QID_NALB_DEPTH_THRSH,
	CFG_CQ_LDB_DEPTH,
	CFG_CQ_LDB_TOKEN_COUNT,
	CFG_CQ_LDB_TOKEN_DEPTH_SELECT,
	CFG_CQ_DIR_TOKEN_DEPTH_SELECT,
	CFG_CQ_LDB_INFLIGHT_COUNT,
	CFG_CQ_DIR_DEPTH,
	CFG_QID_DIR_DEPTH_THRSH,
	CFG_QID_ATQ_ENQ_CNT,
	CFG_QID_LDB_ENQ_CNT,
	CFG_QID_DIR_ENQ_CNT,
};

unsigned int dev_xstat_ids[RTE_DIM(dev_xstat_strs)];
unsigned int queue_xstat_ids[MAX_PORTS_QUEUES][RTE_DIM(queue_xstat_strs)];
uint64_t dev_xstat_vals[RTE_DIM(dev_xstat_strs)];
uint64_t queue_xstat_vals[MAX_PORTS_QUEUES][RTE_DIM(queue_xstat_strs)] = {0};
static dlb_reg_t *dlb_regs;

/* Signal Handler for Ctrl-C */
void sigHandler(int sigNum) {
	printf("Caught Signal %d - Turning off watch-mode\n", sigNum);
	do_watch = false;
}

static void dump_regs(uint8_t *base)
{
	int i;

	if (!base)
	    return;

	for (i = 0; i < num_dlb_regs; i++) {
		printf("%s 0x%08x 0x%08x\n",
		       dlb_regs[i].name,
		       dlb_regs[i].offset,
		       *(uint32_t *)(base + dlb_regs[i].offset));
	}
}

static void get_xstats(uint8_t *base, uint64_t *val, const char *name)
{
	int i;

	if (!base)
	    return;

	for (i = 0; i < num_dlb_regs; i++) {
		if (strstr(dlb_regs[i].name, name))
		    *val = *(uint32_t *)(base + dlb_regs[i].offset);

	}
}

static int print_resources(dlb_resources_t *rsrcs)
{
	char res_header[]="Device,LDB pool size,DIR pool size,COMB pool size,Domains,LDB queues,LDB ports,DIR ports,ES entries,Contiguous ES entries,LDB credits,Contiguous LDB cred,DIR credits,Contiguous DIR cred,LDB credit pools,DIR credit pools\n";
	char res_header2[]="Device,LDB pool size,DIR pool size,COMB pool size,Domains,LDB queues,LDB ports,DIR ports,ES entries,Contiguous ES entries,Credits,Credit pools\n";
	char res_filename[MAX_CSV_FILE_NAME_LEN];

	printf("\n------------------------------------\n\tDLB's available resources:\n");
	printf("\tDomains:           %d\n", rsrcs->num_sched_domains);
	printf("\tLDB queues:        %d\n", rsrcs->num_ldb_queues);
	printf("\tLDB ports:         %d\n", rsrcs->num_ldb_ports);
	printf("\tDIR ports:         %d\n", rsrcs->num_dir_ports);
	printf("\tES entries:        %d\n", rsrcs->num_ldb_event_state_entries);
	printf("\tContig ES entries: %d\n",
			rsrcs->max_contiguous_ldb_event_state_entries);
	if (!cap.combined_credits) {
		printf("\tLDB credits:       %d\n", rsrcs->num_ldb_credits);
		printf("\tContig LDB cred:   %d\n", rsrcs->max_contiguous_ldb_credits);
		printf("\tDIR credits:       %d\n", rsrcs->num_dir_credits);
		printf("\tContig DIR cred:   %d\n", rsrcs->max_contiguous_dir_credits);
		printf("\tLDB credit pls:    %d\n", rsrcs->num_ldb_credit_pools);
		printf("\tDIR credit pls:    %d\n", rsrcs->num_dir_credit_pools);
	} else {
		printf("\tCredits:           %d\n", rsrcs->num_credits);
		printf("\tCredit pools:      %d\n", rsrcs->num_credit_pools);
	}

	printf("-----------------------------------------");

	if (out_file) {
		/* write resources to file */
		if(res_fp == NULL)
		{
			sprintf(res_filename,"dlb%u_%s",dev_id,"header.csv");
			res_fp = fopen(res_filename, "w+");
			if (res_fp == NULL) {
				perror("open");
				exit(-1);
			}
		}

		if (!cap.combined_credits) {
			fprintf(res_fp, "%s", res_header);
			fprintf(res_fp, "dlb%u,%lu,%lu,%ld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				dev_id,
				dev_xstat_vals[DEV_LDB_POOL_SIZE],
				dev_xstat_vals[DEV_DIR_POOL_SIZE],
				dev_xstat_vals[DEV_POOL_SIZE],
				rsrcs->num_sched_domains,
				rsrcs->num_ldb_queues,
				rsrcs->num_ldb_ports,
				rsrcs->num_dir_ports,
				rsrcs->num_ldb_event_state_entries,
				rsrcs->max_contiguous_ldb_event_state_entries,
				rsrcs->num_ldb_credits,
				rsrcs->max_contiguous_ldb_credits,
				rsrcs->num_dir_credits,
				rsrcs->max_contiguous_dir_credits,
				rsrcs->num_ldb_credit_pools,
				rsrcs->num_dir_credit_pools
			);
		  fflush(res_fp);
		} else {
			fprintf(res_fp,"%s", res_header2);
			fprintf(res_fp, "%2u,%lu,%lu,%ld,%d,%d,%d,%d,%d,%d,%d,%d\n",
				dev_id,
				dev_xstat_vals[DEV_LDB_POOL_SIZE],
				dev_xstat_vals[DEV_DIR_POOL_SIZE],
				dev_xstat_vals[DEV_POOL_SIZE],
				rsrcs->num_sched_domains,
				rsrcs->num_ldb_queues,
				rsrcs->num_ldb_ports,
				rsrcs->num_dir_ports,
				rsrcs->num_ldb_event_state_entries,
				rsrcs->max_contiguous_ldb_event_state_entries,
				rsrcs->num_credits,
				rsrcs->num_credit_pools
			);
		  fflush(res_fp);
		}
	}
	return 0;
}

/* Prints Usage*/
static void
usage(void)
{
	const char *usage_str =
		"Usage: dlb_monitor_sec [options]\n"
		"Options:\n"
		" -i <dev_id>   DLB Device id (default: 0)\n"
		" -r            Reset stats after displaying them\n"
		" -t <duration> Measurement duration (seconds) (min: 1s, default: 1s)\n"
		" -w            Repeatedly print stats\n"
		" -z            Don't print ports or queues with 0 enqueue/dequeue/depth stats\n"
		" -l            Print LDB queue statstics\n"
		" -d            Print DIR queue statstics\n"
		" -c            Print CQ  queue statstics\n"
		" -a            Equivalent to setting 'ldcg' flags\n"
		" -o            Generate CSV output file, (generates header.csv,output_raw.csv prefixed with dlb<devid>)\n"
		"\n";

	printf("%s\n", usage_str);
	printf("Acronyms\n");
	printf("\t ldb_infl: Per-QID count of the number of load balanced QEs {ATM, UNO, ORD} waiting for a completion.\n");
	printf("\t inf_limit: Per-QID maximum number of {ATM, UNO, ORD} QE permitted to wait for a completion.\n");
	printf("\t atm_active: Atomic QID Active Count\n");
	printf("\t atm_th: Atomic QID Depth Threshold\n");
	printf("\t naldb_th: Nonatomic Load Balanced QID Depth Threshold\n");
	printf("\t depth_th: DIR QID Depth Threshold\n");
	printf("\t ldb_cq_depth: Per LDB CQ count of the number of tokens owned by the consumer port.\n");
	printf("\t dir_cq_depth: Per DIR CQ Depth. Number of tokens held by the consumer port.\n");
	printf("\t cq_ldb_token: Count of the number of tokens owned by the LDB CQ.\n");
	printf("\n");
	exit(1);
}

/* Parses the input args*/
static int
parse_app_args(int argc, char **argv)
{
	int option_index, c;

	opterr = 0;

	for (;;) {
		c = getopt_long(argc, argv, "i:rt:wzldcgao", NULL, &option_index); /* :=has value */
		if (c == -1)
			break;

		switch (c) {
		case 'i':
			dev_id = atoi(optarg);
			break;
		case 'r':
			do_reset = true;
			break;
		case 't':
			if (atoi(optarg) < 1)
				usage();
			measure_time_us = atoi(optarg) * US_PER_S;
			break;
		case 'w':
			do_watch = true;
			break;
		case 'z':
			skip_zero = true;
			break;
		case 'l':
			prt_ldb = true;
			break;
		case 'd':
			prt_dir = true;
			break;
		case 'c':
			prt_cq = true;
			break;
		case 'g':
			prt_glb = true;
			out_file = true;
			break;
		case 'a': /* set ldcg */
			prt_glb = true;
			prt_ldb = true;
			prt_dir = true;
			prt_cq = true;
			out_file = true;
			break;
		case 'o': /* write to csv file */
			out_file = true;
			break;
		default:
			usage();
			break;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct sigaction sigHandle;
	dlb_resources_t rsrcs;
	int total = 0, cnt;
	char path[BUF_LEN];
	int fd;

	if (parse_app_args(argc, argv))
		return -1;

	/* Catch Ctrl-C signal */

	sigHandle.sa_handler = sigHandler;
	sigemptyset(&sigHandle.sa_mask);
	sigHandle.sa_flags = 0;
	sigaction(SIGINT, &sigHandle, NULL);

	if (dlb_open(dev_id, &dlb) == -1)
		error(1, errno, "dlb_open");

	printf("\n==================================\n\tLIBDLB Monitor\n");

	ver = ((dlb_t*)dlb)->device_version.ver;
	if (ver == VER_DLB2) {
		dlb_regs = dlb2_regs;
		num_dlb_regs = ARRAY_SIZE(dlb2_regs);
		printf("DLB device ID:%d version: 2.0\n", dev_id);
	} else if (ver == VER_DLB2_5) {
		dlb_regs = dlb2_5_regs;
		num_dlb_regs = ARRAY_SIZE(dlb2_5_regs);
		printf("DLB device ID:%d version: 2.5\n", dev_id);
	} else {
		printf("Unsupported HW device..!\n");
		exit(-1);
	}
	printf("===================================\n");

	if (dlb_get_dev_capabilities(dlb, &cap)) {
		error(1, errno, "dlb_get_dev_capabilities");
		exit(-1);
	}
	if (dlb_get_num_resources(dlb, &rsrcs)) {
		error(1, errno, "dlb_get_num_resources");
		exit(-1);
	}

	sprintf(path, "%s%d%s", DLB_SYS_PATH_BASE, dev_id, DLB_RESOURCE2_PATH);

	fd = open(path, O_RDWR | O_SYNC);
	if (fd >= 0) {
		/* mmap only if resource file exists - most likely case for host
		 * Otherwise or for VM, use ioctls to read from driver.
		 */
		base = mmap(0, CSR_BAR_SIZE, PROT_READ, MAP_SHARED, fd, 0);
		if (base == (void *)-1) {
			perror("mmap");
			exit(-1);
		}

		close(fd);
	}

	/* Get and output any stats requested on the command line */
	collect_config();

	if (print_resources(&rsrcs)) {
		error(1, errno, "print_resources");
		exit(-1);
	}

	printf("\n");

	display_config();
	cnt = 0;
	if (out_file) {
		if(fp == NULL) {
			sprintf(filename,"dlb%u_%s",dev_id,"output_raw.csv");
			fp = fopen(filename, "w+");
			if (fp == NULL) {
				perror("open");
				exit(-1);
			}
		}
		fprintf(fp,"%s", header);
		fflush(fp);
	}
	do {
		collect_stats();
		if (do_watch)
			printf("Sample #%d\n", cnt++);

		if (skip_zero)
			printf("Skipping ports and queues with zero stats\n");

		display_stats();
	} while (do_watch);

	if (dlb_close(dlb) == -1)
		error(1, errno, "dlb_close");

	if(out_file) 
	{
  	fclose(fp);
	fclose(res_fp);
  }
	return 0;
}

/* Display device configuration params like dev id, pool size etc*/
static void
display_device_config(void)
{
	printf("\n");
	printf("          Device Configuration\n");
	printf("-----------------------------------------------------------\n");
	printf("      |  LDB pool size |  DIR pool size |  COMB pool size |\n");
	printf("Device|    (DLB 2.0)   |    (DLB 2.0)   |     (DLB 2.5)   |\n");
	printf("------|----------------|----------------|-----------------|\n");

	printf("  %2u  |     %5"PRIu64"      |      %4"PRIu64"      |",
			dev_id,
			dev_xstat_vals[DEV_LDB_POOL_SIZE],
			dev_xstat_vals[DEV_DIR_POOL_SIZE]);
	printf("      %5"PRIu64"      |\n",
			dev_xstat_vals[DEV_POOL_SIZE]);
	printf("-----------------------------------------------------------\n");
	printf("\n");
}

static void
display_config(void)
{
	display_device_config();

	/* TODO */
	/*display_port_config();*/

	/* TODO */
	/*display_queue_config();*/
}

static void
collect_config(void)
{
	char path[BUF_LEN], buf[BUF_LEN];
	uint32_t attr_id;
	unsigned int i;
	int fd;

	/* Read and display from resources from sysfs */
	printf("--------------------------------------\n\tDLB Total resources:\n");
	READ_SYS_PRT(num_cos0_ldb_ports);
	READ_SYS_PRT(num_cos1_ldb_ports);
	READ_SYS_PRT(num_cos2_ldb_ports);
	READ_SYS_PRT(num_cos3_ldb_ports);
	READ_SYS_PRT(num_dir_ports);
	READ_SYS_PRT(num_atomic_inflights);
	READ_SYS_PRT(num_dir_credits);
	READ_SYS_PRT(num_hist_list_entries);
	READ_SYS_PRT(num_ldb_credits);
	READ_SYS_PRT(num_ldb_ports);
	READ_SYS_PRT(num_ldb_queues);
	READ_SYS_PRT(num_sched_domains);
	READ_SYS_PRT(num_sn0_slots);
	READ_SYS_PRT(num_sn1_slots);
	printf("-------------------------------------\n");
	get_device_xstats();
}

/* get Configuration from dlb device registers*/
static void
get_device_xstats()
{
	if (ver == VER_DLB2) {
		dev_xstat_vals[DEV_LDB_POOL_SIZE] = num_ldb_credits;
		dev_xstat_vals[DEV_DIR_POOL_SIZE] = num_dir_credits;
	} else {
		dev_xstat_vals[DEV_POOL_SIZE] = num_ldb_credits;
	}
	dev_xstat_vals[DEV_NB_EVENTS_LIMIT] = num_atomic_inflights;

	get_xstats(base, &dev_xstat_vals[DEV_AQED_ENQ_CNT], dev_xstat_strs[DEV_AQED_ENQ_CNT]);
	get_xstats(base, &dev_xstat_vals[DEV_AQED_ENQ_LIMIT], dev_xstat_strs[DEV_AQED_ENQ_LIMIT]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_LDB_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_LDB_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_L], dev_xstat_strs[CFG_CHP_CNT_ATM_QE_SCH_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_H], dev_xstat_strs[CFG_CHP_CNT_ATM_QE_SCH_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_FID_INF_CNT], dev_xstat_strs[CFG_FID_INF_CNT]);
}

/* display device stats*/
static void
display_device_stats(void)
{
	uint64_t events_inflight, nb_events_limit, aqed_enq_cnt, aqed_enq_limit;
	double aldb_rate, ldb_rate, dir_rate;
	float ldb_sched_throughput, dir_sched_throughput;
	uint64_t total = 0, tot_ldb_enq, tot_dir_enq;
	uint64_t hcw_atm_sch;
	unsigned int i;
	uint32_t interval = (measure_time_us/US_PER_S);
	static uint64_t timestamp = (1);
	if (timestamp == 1) {
		timestamp = timestamp * interval;
	}

	events_inflight = dev_xstat_vals[DEV_INFL_EVENTS];
	nb_events_limit = dev_xstat_vals[DEV_NB_EVENTS_LIMIT];
	aqed_enq_cnt = dev_xstat_vals[DEV_AQED_ENQ_CNT];
	aqed_enq_limit = dev_xstat_vals[DEV_AQED_ENQ_LIMIT];

	tot_dir_enq = (dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H] << 32) + dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L];
	tot_ldb_enq = (dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_H] << 32) + dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_L];
	hcw_atm_sch = (dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_H] << 32) + dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_L];

	ldb_sched_throughput = 0.0f;
	dir_sched_throughput = 0.0f;

	/* Throughput is displayed in millions of events per second, so no need
	 * to convert microseconds to seconds.
	 */
	ldb_sched_throughput = ldb_sched_throughput / measure_time_us;
	dir_sched_throughput = dir_sched_throughput / measure_time_us;
	ldb_sched_throughput = ldb_sched_throughput / measure_time_us;
	dir_sched_throughput = dir_sched_throughput / measure_time_us;

	printf("                        Device stats\n");
	printf("-----------------------------------------------------------\n");
	printf("Inflight events: %"PRIu64"/%"PRIu64"\n", events_inflight, nb_events_limit);
	printf("Active Atomic Flows: %lu\n", dev_xstat_vals[CFG_FID_INF_CNT]);
	printf("Dir enq events: %"PRIu64"\n", tot_dir_enq);
	printf("Atm sch events: %"PRIu64"\n", hcw_atm_sch);
	printf("AQED storage events: %"PRIu64"/%"PRIu64"\n", aqed_enq_cnt, aqed_enq_limit);
	if (ver == VER_DLB2) {
		ldb_rate = (dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB] - hcw_ldb_prev) / (time_elapsed * 1000000);
		hcw_ldb_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB];
	} else {
		ldb_rate = (tot_ldb_enq - hcw_ldb_prev) / (time_elapsed * 1000000);
		hcw_ldb_prev = tot_ldb_enq;
	}
	printf("LDB QE Rate: %3.2f  mpps\n", ldb_rate);

	if (ver == VER_DLB2) {
		aldb_rate = (dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM] - hcw_atm_prev)
		/ (time_elapsed * 1000000);
		hcw_atm_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM];
	} else {
		aldb_rate = (hcw_atm_sch - hcw_atm_prev) / (time_elapsed * 1000000);
		hcw_atm_prev = hcw_atm_sch;
	}
	printf("Atomic LDB QE Rate: %3.2f  mpps\n", aldb_rate);

	if (ver == VER_DLB2) {
		dir_rate = (dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR] - hcw_dir_prev) / (time_elapsed * 1000000);
		hcw_dir_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR];
	} else {
		dir_rate = (tot_dir_enq - hcw_dir_prev) / (time_elapsed * 1000000);
		hcw_dir_prev = tot_dir_enq;
	}
	printf("Directed QE Rate: %3.2f  mpps\n", dir_rate);

	printf("\n-----------------------------------------------------------\n");

	/* 
	 * write to csv file
	 */
	if(out_file) {
		//time,hcw_atm_enq,hcw_atm_deq,hcw_dir_enq,hcw_dir_deq,hcw_nalb_enq,hcw_nalb_deq,inf_evt,event_limit,nldb_rate,aldb_rate,dir_rate\n
		fprintf(fp, "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%3.2f,%3.2f,%3.2f\n", timestamp,
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM],dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_ATM],
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR],dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_DIR],
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB],dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_NALB],
			events_inflight, nb_events_limit,ldb_rate, aldb_rate, dir_rate
			);
		fflush(fp);
		timestamp = timestamp + interval;
	}
}

static void
display_stats(void)
{
	/*TODO*/
	/*display_port_dequeue_stats();*/

	/*TODO*/
	/*display_port_enqueue_stats();*/

	display_queue_stats();

	if (base) display_device_stats();

	printf("Note: scheduling throughput measured over a duration of %us. All other stats are instantaneous samples.\n",
			measure_time_us / US_PER_S);
	printf("\n");
}

/* Sleep function to wait for the time interval between data display*/
void
dlb_delay_us_sleep(unsigned int us)
{
	struct timespec wait[2] = {0};
	int ind = 0;

	wait[0].tv_sec = 0;
	if (us >= US_PER_S) {
		wait[0].tv_sec = us / US_PER_S;
		us -= wait[0].tv_sec * US_PER_S;
	}
	wait[0].tv_nsec = 1000 * us;

	while (nanosleep(&wait[ind], &wait[1 - ind]) && errno == EINTR) {
		/*
		 * Sleep was interrupted. Flip the index, so the 'remainder'
		 * will become the 'request' for a next call.
		 */
		ind = 1 - ind;
	}

	return;
}
#define GET_XSTATS(type, offset, id)                                                               \
	do {                                                                                       \
		if (base) {                                                                        \
			char xstatname_buf[DLB_DEV_XSTATS_NAME_SIZE];                              \
			sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[type], id);              \
			get_xstats(base, &queue_xstat_vals[id + offset][type], xstatname_buf);     \
		} else {                                                                           \
			dlb_get_xstats(dlb, DLB_##type, id, &queue_xstat_vals[id + offset][type]); \
		}                                                                                  \
	} while (0)

/* Collect stats periodically from the DLB device registers*/
static void
collect_stats(void)
{
	char xstatname_buf[DLB_DEV_XSTATS_NAME_SIZE];
	unsigned int i, off = 0;
	int ret;

	/* Wait while the eventdev application executes */
	dlb_delay_us_sleep(measure_time_us);

	get_xstats(base, &dev_xstat_vals[DEV_INFL_EVENTS], dev_xstat_strs[DEV_INFL_EVENTS]);
	get_xstats(base, &dev_xstat_vals[DEV_NB_EVENTS_LIMIT], dev_xstat_strs[DEV_NB_EVENTS_LIMIT]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_ATM], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_ATM]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_ATM]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_DIR], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_DIR]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_DIR]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_NALB], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_NALB]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_NALB]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_LDB_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_LDB_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_L], dev_xstat_strs[CFG_CHP_CNT_ATM_QE_SCH_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_H], dev_xstat_strs[CFG_CHP_CNT_ATM_QE_SCH_H]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_L]);
	get_xstats(base, &dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H], dev_xstat_strs[CFG_CHP_CNT_DIR_HCW_ENQ_H]);
	get_xstats(base, &dev_xstat_vals[CFG_FID_INF_CNT], dev_xstat_strs[CFG_FID_INF_CNT]);

	if (clock_gettime(CLOCK_REALTIME, &stop_time) == -1) {
		perror("ERROR: clock_gettime()"); exit(-1);
	}
	time_elapsed = (stop_time.tv_sec - start_time.tv_sec) +
		(stop_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
	if (clock_gettime(CLOCK_REALTIME, &start_time) == -1) {
		perror("ERROR: clock_gettime()"); exit(-1);
	}
	/* Collect LDB Queue stats */
	for (i = 0; i < num_ldb_queues; i++) {
		GET_XSTATS(CFG_QID_LDB_INFLIGHT_COUNT, off, i);
		GET_XSTATS(CFG_QID_LDB_INFLIGHT_LIMIT, off, i);
		GET_XSTATS(CFG_QID_ATM_ACTIVE, off, i);
		GET_XSTATS(CFG_QID_ATM_DEPTH_THRSH, off, i);
		GET_XSTATS(CFG_QID_NALB_DEPTH_THRSH, off, i);
		GET_XSTATS(CFG_QID_ATQ_ENQ_CNT, off, i);
		GET_XSTATS(CFG_QID_LDB_ENQ_CNT, off, i);
	}

	off += num_ldb_queues;
	/* Collect DIR QUEUE stats */
	for (i = 0; i < num_dir_ports; i++) {
		GET_XSTATS(CFG_QID_DIR_DEPTH_THRSH, off, i);
		GET_XSTATS(CFG_QID_DIR_ENQ_CNT, off, i);
	}
	off += num_dir_ports;

	/* Collect LDB CQ stats */
	for (i = 0; i < num_ldb_ports; i++) {
		GET_XSTATS(CFG_CQ_LDB_DEPTH, off, i);
		GET_XSTATS(CFG_CQ_LDB_TOKEN_COUNT, off, i);
		GET_XSTATS(CFG_CQ_LDB_TOKEN_DEPTH_SELECT, off, i);
		GET_XSTATS(CFG_CQ_LDB_INFLIGHT_COUNT, off, i);
	}
	off += num_ldb_ports;
	/* Collect DIR CQ stats */
	for (i = 0; i < num_dir_ports; i++) {
		GET_XSTATS(CFG_CQ_DIR_DEPTH, off, i);
		GET_XSTATS(CFG_CQ_DIR_TOKEN_DEPTH_SELECT, off, i);
	}
}

/* Display queue Stats */
static void
display_queue_stats(void)
{
	unsigned int i, off = 0;

	if(prt_ldb) {
	printf("\n");
	printf("               Per QID Configuration and stats\n");
	printf("--------------------------------------------------------\n");
	printf("\n");
	printf("   LDB QUEUE stats\n");
	printf("--------------------\n");
	printf("Queue|Type|ldb_inf|inf_limit|atm_active|atm_th |naldb_th|atq_enq|naldb_enq|\n");
	printf("-----|----|-------|---------|----------|-------|--------|-------|---------|\n");
	for (i = 0; i < num_ldb_queues; i++) {
		if (!queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_LIMIT] && skip_zero)
			continue;
		printf(" %3u |%s|%7"PRIu64"|%9"PRIu64"|%10"PRIu64"|%7"PRIu64"|%8"PRIu64"|%7"PRIu64"|%9"PRIu64"|\n",
				i,
				" LDB",
				queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_COUNT],
				queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_LIMIT],
				queue_xstat_vals[i][CFG_QID_ATM_ACTIVE],
				queue_xstat_vals[i][CFG_QID_ATM_DEPTH_THRSH],
				queue_xstat_vals[i][CFG_QID_NALB_DEPTH_THRSH],
				queue_xstat_vals[i][CFG_QID_ATQ_ENQ_CNT],
				queue_xstat_vals[i][CFG_QID_LDB_ENQ_CNT]);
	}

	printf("-------------------------------------------------------------------------------\n");
	}

	off += num_ldb_queues;
	if(prt_dir) {
	printf("\n------------------------------\n");
	printf("         DIR QUEUE stats\n");
	printf("------------------------------\n");
	printf("Queue|Type|depth_th|enq_count|\n");
	printf("-----|----|--------|---------|\n");
	for (i = 0; i < num_dir_ports; i++, off++) {
		if (!queue_xstat_vals[off][CFG_QID_DIR_DEPTH_THRSH] && skip_zero)
			continue;
		printf(" %3u |%s|%8"PRIu64"|%9"PRIu64"\n",
				i,
				" DIR",
				queue_xstat_vals[off][CFG_QID_DIR_DEPTH_THRSH],
				queue_xstat_vals[off][CFG_QID_DIR_ENQ_CNT]);
	}
	printf("-----------------------------------------------------------\n");
	if (ver == VER_DLB2_5) /* DIR enqueue depth MSB is not accessible */
  	{
		printf("WARNING: DIR enq_count only shows lower 12 bits. \n If current depth is > 8192, displayed value will be incorrect\n");
	printf("-----------------------------------------------------------\n");
	}
	}

	if(prt_cq) {
	printf("\n");
	printf(" Per Port CQ stats\n");
	printf("-------------------------------------------------\n");
	printf("  CQ |type|size|ldb_cq_depth|dir_cq_depth|cq_ldb_token|cq_infl_cnt|\n");
	printf("-----|----|----|------------|------------|------------|-----------|\n");
	for (i = 0; i < num_ldb_ports + num_dir_ports; i++, off++) {
		bool is_ldb = i < num_ldb_ports? true : false;
		if ((!queue_xstat_vals[off][CFG_CQ_LDB_DEPTH] &&
					!queue_xstat_vals[off][CFG_CQ_DIR_DEPTH]) && skip_zero)
			continue;
		printf(" %3u |%s|%4"PRIu32"|%12"PRIu64"|%12"PRIu64"|%12"PRIu64"|%12"PRIu64"|\n",
				i,
				is_ldb ? " LDB" : " DIR",
				2 << (queue_xstat_vals[off][
					is_ldb ? CFG_CQ_LDB_TOKEN_DEPTH_SELECT : CFG_CQ_DIR_TOKEN_DEPTH_SELECT]+1),
				queue_xstat_vals[off][CFG_CQ_LDB_DEPTH],
				queue_xstat_vals[off][CFG_CQ_DIR_DEPTH],
				queue_xstat_vals[off][CFG_CQ_LDB_TOKEN_COUNT],
				queue_xstat_vals[off][CFG_CQ_LDB_INFLIGHT_COUNT]);
	}
	printf("-------------------------------------------------------------------------------\n");
	}

	if( prt_glb && base) {
	printf("\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("           DLB Global Stats\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("cfg_counter_enqueue_hcw_atm        %12"PRIu64"  Total Atomic HCW enqueued\n",
			ver == VER_DLB2 ? dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM] :
			(dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_H] << 16) + dev_xstat_vals[CFG_CHP_CNT_ATM_QE_SCH_L]);
	if (ver == VER_DLB2) printf("cfg_counter_dequeue_hcw_atm        %12"PRIu64"  Total Atomic HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_ATM]);
	printf("cfg_counter_enqueue_hcw_dir        %12"PRIu64"  Total DIR HCW enqueued\n",
			ver == VER_DLB2 ? dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR] :
			(dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_H] << 16) + dev_xstat_vals[CFG_CHP_CNT_DIR_HCW_ENQ_L]);
	if (ver == VER_DLB2) printf("cfg_counter_dequeue_hcw_dir        %12"PRIu64"  Total DIR HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_DIR]);
	printf("cfg_counter_enqueue_hcw_ldb        %12"PRIu64"  Total LDB HCW enqueued\n",
			ver == VER_DLB2 ? dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB] :
			(dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_H] << 16) + dev_xstat_vals[CFG_CHP_CNT_LDB_HCW_ENQ_L]);
	if (ver == VER_DLB2) printf("cfg_counter_dequeue_hcw_ldb        %12"PRIu64"  Total LDB HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_NALB]);

	printf("-------------------------------------------------------------------------------\n");
	printf("\n");
	}
}
