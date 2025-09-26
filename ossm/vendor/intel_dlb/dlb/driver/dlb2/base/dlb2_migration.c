// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2016-2020 Intel Corporation

#include "uapi/linux/dlb2_user.h"

#include "dlb2_hw_types.h"
#include "dlb2_mbox.h"
#include "dlb2_osdep.h"
#include "dlb2_osdep_bitmap.h"
#include "dlb2_osdep_types.h"
#include "dlb2_regs.h"
#include "dlb2_resource.h"

#define DLB2_LM_DEBUG_ON	1
#define DRAIN_RESTORE_ORDERED_QID 1

#define DUMMY_CQ_HIST_LIST_BASE(state) state->dummy_cq_hist_list_base
#define DUMMY_CQ_HIST_LIST_LIMIT(state) state->dummy_cq_hist_list_limit

#if (DLB2_LM_DEBUG_ON)
#define LM_DEBUG_print(...) printk(__VA_ARGS__)
#else
#define LM_DEBUG_print(...)
#endif

static dma_addr_t dummy_cq_dma_base = 0;
static void *dummy_cq_base = 0;

static uint32_t read_hist_list_pop_ptr (struct dlb2_hw *hw, uint32_t cq)
{
	uint32_t pop_ptr;

	pop_ptr = DLB2_CSR_RD(hw, CHP_HIST_LIST_POP_PTR(hw->ver, cq));
	return (pop_ptr & 0x3fff);
}

static uint32_t read_hist_list_push_ptr (struct dlb2_hw *hw, uint8_t cq)
{
	uint32_t push_ptr;

	push_ptr = DLB2_CSR_RD(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, cq));
	return (push_ptr & 0x3fff);
}

/* This function sets the same value for CQ HL Pop/Push ptr */
static int set_hl_pop_push_ptr(struct dlb2_hw *hw, uint8_t cq, uint16_t idx)
{
	DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, cq), idx);
	DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, cq), idx);
	DLB2_CSR_RD(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, cq));
	return 0;
}

/**
 * @brief Determines number of inflights for a CQ.
 *
 * This function determines the number of inflights by reading
 * register = hqm_list_sel_pipe.cfg_cq_ldb_inflight_count.
 *
 * @param cq_id is the CQ identifier.
 * @return num inflights pending for CQ.
 **/
/* Can be replaced with dlb2_ldb_cq_inflight_count() */
static uint16_t num_ldb_inflights (struct dlb2_hw *hw, uint8_t cq)
{
	uint32_t infl_cnt;

	infl_cnt = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_CNT(hw->ver, cq));

	if (infl_cnt >= 0xffff) {
	  LM_DEBUG_print("[%s]CFG Read timeout reading inf count \n TEST Failed\n", __func__);
	}
	return infl_cnt;
}

static int dlb2_read_src_hl(struct dlb2_hw *hw,
			    struct dlb2_ldb_port *port,
			    struct dlb2_migration_state *state)
{
	uint16_t hl_size, hl_idx, inflights;
	uint32_t val0, val1, tkn_cnt;
	struct hl_t *hl_state;
	uint32_t i, j, cq, vcq;
	uint32_t wptr;

	cq = port->id.phys_id;
	vcq = port->id.virt_id;

	/* Read and save the pop/push ptrs */
	state->ldb_cq_state[vcq].pop_ptr_val = read_hist_list_pop_ptr(hw, cq);
	state->ldb_cq_state[vcq].pop_ptr_gen = read_hist_list_pop_ptr(hw, cq) >>
					       CHP_HIST_LIST_POP_PTR_GENERATION_LOC;
	state->ldb_cq_state[vcq].pop_ptr = BITS_GET(state->ldb_cq_state[vcq].pop_ptr_val,
						       CHP_HIST_LIST_POP_PTR_POP_PTR);

	state->ldb_cq_state[vcq].push_ptr_val = read_hist_list_push_ptr(hw, cq);
	state->ldb_cq_state[vcq].push_ptr_gen = read_hist_list_push_ptr(hw, cq) >>
						CHP_HIST_LIST_PUSH_PTR_GENERATION_LOC;
	state->ldb_cq_state[vcq].push_ptr = BITS_GET(state->ldb_cq_state[vcq].push_ptr_val,
						       CHP_HIST_LIST_PUSH_PTR_PUSH_PTR);
	state->ldb_cq_state[vcq].hist_list_entry_base = port->hist_list_entry_base;
	state->ldb_cq_state[vcq].hist_list_entry_limit = port->hist_list_entry_limit;

	state->ldb_cq_state[vcq].inflights = num_ldb_inflights(hw, cq);
	wptr = DLB2_CSR_RD(hw, CHP_LDB_CQ_WPTR(hw->ver, cq));
	state->ldb_cq_wptr[vcq] = wptr;

	/* Obtain the configured size of the hl */
	hl_size = state->ldb_cq_state[vcq].hist_list_entry_limit -
			state->ldb_cq_state[vcq].hist_list_entry_base;
	hl_idx = state->ldb_cq_state[vcq].pop_ptr;
	inflights = state->ldb_cq_state[vcq].inflights;
	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

	LM_DEBUG_print("Reading CQ[%d] : push_ptr = 0x%08x , gen = %d \n", cq,
		state->ldb_cq_state[vcq].push_ptr, state->ldb_cq_state[vcq].push_ptr_gen);
	LM_DEBUG_print("Reading CQ[%d] : pop_ptr  = 0x%08x , gen = %d \n", cq,
		state->ldb_cq_state[vcq].pop_ptr, state->ldb_cq_state[vcq].pop_ptr_gen);
	LM_DEBUG_print("HL Base : 0x%08x , HL Limit : 0x%08x\n", port->hist_list_entry_base,
		port->hist_list_entry_limit - 1);
	LM_DEBUG_print("HL size : %d, HL Idx : %d, Inflights : %d, Token cnt: %d\n\n",
		hl_size, hl_idx, inflights, tkn_cnt);

	/* Loop through the CQ's hl and read the content */
	LM_DEBUG_print ("CQ   idx  qid  qt   hid    sn \n");
	for (j = 0; j < inflights; j++) {
		/* start from the HL base once the limit is reached */
		mb(); /* to prevent Spectra Vulnerability */
		if (hl_idx >  port->hist_list_entry_limit - 1)  {
			hl_idx = port->hist_list_entry_base;
		}
		val0 = DLB2_CSR_RD(hw, CHP_HIST_LIST_0(hl_idx));
		val1 = DLB2_CSR_RD(hw, CHP_HIST_LIST_1(hl_idx));

		hl_state = &state->hlist_state[hl_idx];

		hl_state->qid =  (val0 >> 23) & 0x7f;
		hl_state->qtype = (val1 >> 1) & 0x3;
		hl_state->hid = (val1 >> 8) & 0xffff;
		hl_state->sn_fid = val0 & 0xfff;

		/* Convert the physical QID to virtual QID */
		for (i = 0; i < state->num_ldb_queues; i++) {
			if (state->ldb_queue[i]->id.phys_id == hl_state->qid) {
				hl_state->qid = state->ldb_queue[i]->id.virt_id;
				break;
			}
		}

		if (i == state->num_ldb_queues) {
			LM_DEBUG_print("%s: Invalid qid from hist list! \n", __func__);
			return EINVAL;
		}

		if (hl_state->qtype == ORDERED) {
			state->ldb_qid_state[hl_state->qid].sn_state[hl_state->sn_fid].hl_valid = 1;
			state->ldb_qid_state[hl_state->qid].sn_state[hl_state->sn_fid].rob_valid = 0;
			state->ldb_qid_state[hl_state->qid].sn_state[hl_state->sn_fid].hl_port_id = vcq;
			state->ldb_qid_state[hl_state->qid].sn_state[hl_state->sn_fid].hl_idx = hl_idx;
		}

		LM_DEBUG_print("%2d  %4d  %3d  %2d  %04x  %4d \n", cq, hl_idx,
				hl_state->qid, hl_state->qtype,
				hl_state->hid, hl_state->sn_fid );

		hl_idx++;
	}
	return 0;
}


static int dlb2_read_src_queue_state(struct dlb2_hw *hw,
				     struct dlb2_ldb_queue *queue,
				     struct dlb2_migration_state *src_state)
{
	uint32_t vqid = queue->id.virt_id;
	uint32_t qid = queue->id.phys_id;
	uint32_t slot_shift, j, k, val;
	struct dlb2_sn_group *group;
	uint16_t sn_min, sn_idx;
	uint32_t num_sn;

	src_state->ldb_qid_state[vqid].num_drain_hcws = 0;
	if (queue->sn_cfg_valid) {
		/* Since queues and domain have been configured, each ordered
		 * queue should have a valid sn group and corresponding
		 * sequeuece_number_per_queue.
		 */
		group = &hw->rsrcs.sn_groups[queue->sn_group];
		num_sn = group->sequence_numbers_per_queue;
		LM_DEBUG_print("MIG: sn_group = %d, sn_per_queue = %d",
				queue->sn_group, num_sn);

		sn_min = queue->sn_slot * num_sn;
		slot_shift =  DLB2_CSR_RD(hw, RO_GRP_0_SLT_SHFT(hw->ver, queue->sn_slot));
		sn_idx = sn_min + slot_shift;
		LM_DEBUG_print("QID[%d] Group %d  Slot %d slot_shift: 0x%x %u sn_min: %u (oldest sn_idx = %u)",
			vqid, queue->sn_group,queue->sn_slot, slot_shift, slot_shift, sn_min, sn_idx);

		src_state->ldb_qid_state[vqid].num_sn_in_hl = 0;
		src_state->ldb_qid_state[vqid].num_sn_in_rob = 0;

		for (j = 0; j < num_sn; j++) {
			if (sn_idx >= (sn_min + num_sn)) {
				sn_idx = sn_min;
			}
			/* Create a sorted SN list starting at the oldest */
			if (src_state->ldb_qid_state[vqid].sn_state[sn_idx].hl_valid) {
				k = src_state->ldb_qid_state[vqid].num_sn_in_hl;
				src_state->ldb_qid_state[vqid].sn_list[k] = sn_idx;
				src_state->ldb_qid_state[vqid].num_sn_in_hl++;
				LM_DEBUG_print("%d (%d) ", sn_idx,
					src_state->ldb_qid_state[vqid].sn_state[sn_idx].hl_port_id);
			} else {
				/* It is in ROB */
				val =  DLB2_CSR_RD(hw, RO_REORDER_STATE_QID_QIDIX_CQ(sn_idx));
				LM_DEBUG_print("sn_idx: %d, val: 0x%x", sn_idx, val);
				/* 23rd bit of RO_REORDER_STATE_QID_QIDIX_CQ is Reorder St Vld (VLD)
				 * This indicates the SN entry is active.
				 */
				if ((val & RO_REORDER_STATE_QID_QIDIX_CQ_VLD) == 0) {
					src_state->ldb_qid_state[vqid].num_sn_in_rob++;
					LM_DEBUG_print("* ");
				}
			}
			sn_idx++;
		}
		LM_DEBUG_print("\n");
		LM_DEBUG_print("MIG: QID[%d] Found %d in HL and %d in ROB \n", qid,
			src_state->ldb_qid_state[vqid].num_sn_in_hl,
			src_state->ldb_qid_state[vqid].num_sn_in_rob);
	}
	return 0;
}

static int dlb2_read_sn_state(struct dlb2_hw *hw)
{
	uint8_t slot, group, max_group=2;
	uint32_t i, j, val, val0;
	uint32_t slot_shift;

	for (group = 0; group < max_group; group++) {
		for (i = 0; i < 16; i++) { //Needs to be generalized for all slot types
			slot = i;
			if (group == 0) {
				val =  DLB2_CSR_RD(hw, RO_HEALTH_SEQNUM_STATE_GRP0(i));
				slot_shift =  DLB2_CSR_RD(hw, RO_GRP_0_SLT_SHFT(hw->ver, slot));
			} else {
				val =  DLB2_CSR_RD(hw, RO_HEALTH_SEQNUM_STATE_GRP1(i));
				slot_shift =  DLB2_CSR_RD(hw, RO_GRP_1_SLT_SHFT(hw->ver, slot));
			}
			if (val > 0) {
				LM_DEBUG_print("MIG: GRP%d[%2d] = 0x%08x ", group, i, val);
				for (j = 0; j < 16; j++) {
					val0 = val & (1 << j);
					if (val0 > 0)
						LM_DEBUG_print("%d, ", (j + i * 16) + slot_shift);
				}
				LM_DEBUG_print("\n");
			}
		}
	}
#if 0
	uint32_t max = 192; //For fewer log prints
	/* Go through all SNs - Max 2048 */
	for (i = 0; i < max; i++) {
		val =  DLB2_CSR_RD(hw, RO_REORDER_STATE_QID_QIDIX_CQ(i));
		if (val != 0x00800000 && val > 0x0)
			LM_DEBUG_print("MIG: SN_STATE[%4d] = %08x \n", i, val);
	}
#endif
	return 0;
}

static bool dlb2_return_token(struct dlb2_hw *hw, uint8_t cq, bool is_ldb)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;

	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
	memset(hcw, 0, 4 * sizeof(*hcw));

	hcw->cq_token = 1;

	pp_addr = os_map_producer_port_maskable(hw, cq, is_ldb);
	os_enqueue_four_hcws(hw, hcw, pp_addr);

	os_fence_hcw(hw, pp_addr);
	os_unmap_producer_port(hw, pp_addr);

	return 0;
}

static int dlb2_send_src_cq_comps(struct dlb2_hw *hw,
				  struct dlb2_ldb_port *port,
				  struct dlb2_migration_state *src_state)
{
	uint8_t pop_ptr_gen, src_cq, src_vcq;
	struct dlb2_hcw hcw_mem[8], *hcw;
	uint16_t pop_ptr, inflight_limit;
	uint16_t inf, entries, tkn_cnt;
	uint16_t hl_idx, tokens_returned;
	uint32_t i, val, k = 0;
	void __iomem *pp_addr;

	src_cq = port->id.phys_id;
	src_vcq = port->id.virt_id;

	pop_ptr = src_state->ldb_cq_state[src_vcq].pop_ptr;
	pop_ptr_gen = src_state->ldb_cq_state[src_vcq].pop_ptr_gen & 1;
	entries =  src_state->ldb_cq_state[src_vcq].inflights;

	/* The first HL entry to read to the SRC HL pop_ptr */
	hl_idx = pop_ptr;
	src_state->hl_ptr[hl_idx] = hl_idx |
				    (pop_ptr_gen << CHP_HIST_LIST_POP_PTR_GENERATION_LOC);

	inf = num_ldb_inflights(hw, src_cq);

	/* Read and store the SRC CQ token count */
	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);
	src_state->ldb_cq_state[src_vcq].tkn_cnt = tkn_cnt;

	/* Read and store the SRC CQ inflights limit */
	inflight_limit = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, src_cq));
	src_state->ldb_cq_state[src_vcq].inflights_limit = inflight_limit;

	LM_DEBUG_print("Processing SRC HL COMPS: SRC_CQ = %d hl_idx = %d Inflights = %d inf: "
		"%d, Inflight_limit: %d, tkn_cnt: %d\n",
		src_cq, hl_idx, entries, inf, src_state->ldb_cq_state[src_vcq].inflights_limit,
		src_state->ldb_cq_state[src_vcq].tkn_cnt);

	tokens_returned = 0;

	for (i = 0; i < entries; i++) {
		src_state->hl_ptr[hl_idx] = hl_idx |
					    (pop_ptr_gen << CHP_HIST_LIST_POP_PTR_GENERATION_LOC);

		if (src_state->hlist_state[hl_idx].qtype == ORDERED) {
			/* Do not send a RENQ/COMP yet */
			/* Setup the next index to pop */
			tkn_cnt = dlb2_ldb_cq_token_count(hw, port);
			LM_DEBUG_print("CQ[%d] IDX = %d, ORD QE -- SKIP remaining inflights = %d: "
				"remaining entries = %d tkn_cnt = %d\n", src_cq, hl_idx, inf, (entries-i-1), tkn_cnt);
			hl_idx++;
			/* start from the HL base once the limit is reached */
			if (hl_idx >  port->hist_list_entry_limit - 1)  {
				hl_idx = port->hist_list_entry_base;
				pop_ptr_gen = pop_ptr_gen ^ 1;
				LM_DEBUG_print("CQ_HL[%d] gen change hl_idx = 0x%08x \n", src_cq, hl_idx);
			}
			pop_ptr = hl_idx | (pop_ptr_gen << CHP_HIST_LIST_POP_PTR_GENERATION_LOC);

			DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, src_cq), pop_ptr);

			/* Reading the pop ptr to make sure the above write was successful */
			val = read_hist_list_pop_ptr(hw, src_cq);
			if (val != pop_ptr)
				LM_DEBUG_print("MIG: Pop_ptr = %08x pop_ptr update NOT successful \n", val);

		} else { /* All other sched types */

			val = read_hist_list_pop_ptr(hw, src_cq);

			pp_addr = os_map_producer_port_maskable(hw, src_cq, true);

			/* Point hcw to a 64B-aligned location */
			hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

			/*
			 * Program the first HCW for a completion and token return and
			 * the other HCWs as NOOPS
			 */
			memset(hcw, 0, 4 * sizeof(*hcw));

			/* Set the qe_comp to send completions upto inflights count */
			hcw->qe_comp = 1;

			/* Set the cq_token to return tokens upto token count(or single BAT_T) */
			if (tkn_cnt) {
				hcw->cq_token = 1;
				tokens_returned++;
			}

			/* Return tokens in the first HCW */
			os_enqueue_four_hcws(hw, hcw, pp_addr);

			inf = num_ldb_inflights(hw, src_cq);

			while(k < 1000) {
				/* loop until new inflight is less than old one
				 * indicating COMP_T has been seen by HQM */
				if (inf > num_ldb_inflights(hw, src_cq)) {
				  break;
				}
				k++;
			}
			os_fence_hcw(hw, pp_addr);
			os_unmap_producer_port(hw, pp_addr);

			inf = num_ldb_inflights(hw, src_cq);
			tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

			LM_DEBUG_print("CQ[%d] IDX = %d, pop_ptr: %08x, remaining inflights = %d:  "
				"remaining entries = %d tkn_cnt: %d \n", src_cq, hl_idx, val,
				inf, (entries-i-1), tkn_cnt);
			hl_idx++;
			/* start from the HL base once the limit is reached */
			if (hl_idx >  port->hist_list_entry_limit - 1)
				hl_idx = port->hist_list_entry_base;
		}
	}
	LM_DEBUG_print("Num of tokens returned: %d\n", tokens_returned);
	return 0;
}

#if 0
static int dlb2_send_src_cq_tokens(struct dlb2_hw *hw,
				  struct dlb2_dir_pq_pair *port,
				  struct dlb2_migration_state *src_state)
{
	uint32_t tkn_cnt;
	uint8_t cq;

	cq = port->id.phys_id;
	tkn_cnt = dlb2_dir_cq_token_count(hw, port);
	LM_DEBUG_print("CQ[%d]Number of tokens to be returned : %d\n", cq, tkn_cnt);

	while (dlb2_dir_cq_token_count(hw, port) > 0) {
		if (dlb2_return_token(hw, cq, 0))
			LM_DEBUG_print("DIR Token Return Failure\n");
		else
			LM_DEBUG_print("Remaining tokens: %d\n", dlb2_dir_cq_token_count(hw, port));
	}

	return 0;
}
#endif

static int dlb2_rerun_pending_src_comps(struct dlb2_hw *hw,
					struct dlb2_migration_state *src_state)
{
	struct dlb2_ldb_port *port;
	uint32_t i;

	/* Send COMP_Ts for all pending HL entries except for the ORD ones */
	for (i = 0; i < src_state->num_ldb_ports; i++) {
		port = src_state->ldb_port[i];
		/* Send COMP_Ts until all pending COMPs are received */
		if (dlb2_send_src_cq_comps(hw, port, src_state))
			LM_DEBUG_print("Drain CQ COMP/TOK Send FAIL \n");

	}
	udelay(5000);
	return 0;
}

/* This function sets the base and limit of HL-either its own or from a temporary cq */
static int set_hl_base_limit(struct dlb2_hw *hw,
			     struct dlb2_migration_state *state,
			     struct dlb2_ldb_port *port,
			     struct dlb2_ldb_port *buddy_port)
{
	uint8_t cq;

	cq = port->id.phys_id;

	if (buddy_port) {
		DLB2_CSR_WR(hw, CHP_HIST_LIST_LIM(hw->ver, cq), buddy_port->hist_list_entry_limit - 1);
		DLB2_CSR_WR(hw, CHP_HIST_LIST_BASE(hw->ver, cq), buddy_port->hist_list_entry_base);
		DLB2_CSR_RD(hw, CHP_HIST_LIST_BASE(hw->ver, cq));
	} else {
		DLB2_CSR_WR(hw, CHP_HIST_LIST_LIM(hw->ver, cq), DUMMY_CQ_HIST_LIST_LIMIT(state) - 1);
		DLB2_CSR_WR(hw, CHP_HIST_LIST_BASE(hw->ver, cq), DUMMY_CQ_HIST_LIST_BASE(state));
		DLB2_CSR_RD(hw, CHP_HIST_LIST_BASE(hw->ver, cq));
	}

	return 0;
}

static int pf_send_COMP_T(struct dlb2_hw *hw, uint8_t cq)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;

	pp_addr = os_map_producer_port_maskable(hw, cq, true);

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

	/*
	 * Program the first HCW for a completion and token return and
	 * the other HCWs as NOOPS
	 */
	memset(hcw, 0, 4 * sizeof(*hcw));
	hcw->qe_comp = 1;
	hcw->cq_token = 1;

	/* Return tokens in the first HCW */
	os_enqueue_four_hcws(hw, hcw, pp_addr);

	os_fence_hcw(hw, pp_addr);

	os_unmap_producer_port(hw, pp_addr);

	return 0;
}

static int pf_send_comp_token(struct dlb2_hw *hw, struct dlb2_ldb_port *port)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;
	uint16_t tkn_cnt;
	uint8_t cq;

	cq = port->id.phys_id;
	pp_addr = os_map_producer_port_maskable(hw, cq, true);

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

	/*
	 * Program the first HCW for a completion and token return and
	 * the other HCWs as NOOPS
	 */
	memset(hcw, 0, 4 * sizeof(*hcw));
	hcw->qe_comp = 1;

	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);
	if (tkn_cnt)
		hcw->cq_token = 1;

	/* Return tokens in the first HCW */
	os_enqueue_four_hcws(hw, hcw, pp_addr);

	os_fence_hcw(hw, pp_addr);

	os_unmap_producer_port(hw, pp_addr);

	return 0;
}

static int dlb2_drain_src_vas(struct dlb2_hw *hw,
			      bool drain_type,
			      struct dlb2_migration_state *src_state)
{
	uint32_t i, k, na_enq, at_ac, aq_ac;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_port *ldb_port;
	int cnt, loop_cnt, tkn_cnt;
	uint8_t cq, qid, vcq, vqid;
	struct dlb2_hcw *hcw;
	uint16_t inflights;
	bool found;

	LM_DEBUG_print("num_ldb_ports : %d,num_dir_ports = %d size of mig_state: %ld\n",
		src_state->num_ldb_ports, src_state->num_dir_ports, sizeof(*src_state));

	/* Get the QE count in the internal queues */
	for (i = 0; i < src_state->num_ldb_queues; i++) {
		qid = src_state->ldb_queue[i]->id.phys_id;
		vqid = src_state->ldb_queue[i]->id.virt_id;
		na_enq = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, qid));
		at_ac = DLB2_CSR_RD(hw, LSP_QID_ATM_ACTIVE(hw->ver, qid));
		aq_ac = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, qid));
		LM_DEBUG_print("[PANEL]LDB QID : %d(%d), na_eq: %d, at_ac: %d, aq_ac: %d\n",
			qid, vqid,  na_enq, at_ac, aq_ac);
	}

	for (i = 0; i < src_state->num_ldb_ports; i++) {
		ldb_port = src_state->ldb_port[i];
		cq = ldb_port->id.phys_id;

		inflights = num_ldb_inflights(hw, cq);
		tkn_cnt = dlb2_ldb_cq_token_count(hw, ldb_port);

		/* Increase inflight limit by 1 more than the current pending
		 * inflights such that CQ can schedule 1 more */
		DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, cq), inflights + 1);
		LM_DEBUG_print("Draining CQ = %d, Token count: %d, Setting CQ[%d] Inflight Limit to %d \n",
			cq, tkn_cnt, cq, inflights + 1);

		/* Temporarily Assign a new HL for the drained CQ */
		set_hl_base_limit(hw, src_state, ldb_port, NULL);
		DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, cq), DUMMY_CQ_HIST_LIST_BASE(src_state));
		DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, cq), DUMMY_CQ_HIST_LIST_BASE(src_state));

		read_hist_list_push_ptr(hw, cq);

		/* CQ write pointer is set to 0. Every new QE scheduled to the CQ will then
		 * arrive with CQ gen (hcw->cq_token) bit set.
		 */
		DLB2_CSR_WR(hw, CHP_LDB_CQ_WPTR(hw->ver, cq), CHP_LDB_CQ_WPTR_RST);

		DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(cq), dummy_cq_dma_base & 0xffffffc0);
		DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(cq), dummy_cq_dma_base >> 32);

		/* Reset PASID for HCW draining in PF host driver */
		DLB2_CSR_WR(hw,
			    SYS_LDB_CQ_PASID(hw->ver, cq),
			    SYS_LDB_CQ_PASID_RST);

		ndelay(500);
		/* Enable the LDB port */
		dlb2_ldb_port_cq_enable(hw, ldb_port);

		ndelay(500);

		LM_DEBUG_print("%s: dummy_cq_dma_base = 0x%016llx, dummy_cq_base = 0x%016llx \n", __func__,
				dummy_cq_dma_base, (uint64_t)dummy_cq_base);

		cnt = 0;
		loop_cnt = 0;
		while (loop_cnt < 10000) {
			found = false;
			if (num_ldb_inflights(hw, cq) > inflights) {

				/* one new QE has been scheduled */
				 hcw = (struct dlb2_hcw *)((uintptr_t)dummy_cq_base);

				 if (hcw->cq_token == 1) {
					vqid = hcw->qid; /* FIXME: vqid->qid translation required */
					if (vqid >= DLB2_MAX_NUM_LDB_QUEUES) return EINVAL;

					if (cnt % 1000 == 0 || cnt < 16)
						LM_DEBUG_print("[%d]Reading SRC HCW[%d]: 0x%016llx 0x%016llx"
							" at CQ=%d QID = %d qType = %d udata64: %llx\n",
							src_state->ldb_qid_state[vqid].num_drain_hcws, cnt,
							*(uint64_t *)hcw, *((uint64_t *)hcw + 1),
							cq, hcw->qid, hcw->sched_type, hcw->data);

					if (drain_type == 0 || hcw->sched_type == ORDERED) {
						k = src_state->ldb_qid_state[vqid].num_drain_hcws;
						src_state->ldb_qid_state[vqid].drain_hcw[k] = *hcw;
						src_state->ldb_qid_state[vqid].num_drain_hcws++;
					} else {
						k = src_state->ldb_qid_state[vqid].num_drain_rob_hcws;
						src_state->ldb_qid_state[vqid].drain_rob_hcw[k] = *hcw;
						src_state->ldb_qid_state[vqid].num_drain_rob_hcws++;
					}
					cnt++;
					DLB2_CSR_WR(hw, CHP_LDB_CQ_WPTR(hw->ver, ldb_port->id.phys_id), CHP_LDB_CQ_WPTR_RST);

					if (pf_send_COMP_T(hw, cq))
						LM_DEBUG_print("Drain CQ COMP/TOK Send FAIL \n");

					ndelay(5000);
					found = true;
				 }
			}
			loop_cnt++;
		}

		/* Disable the ldb port again */
		dlb2_ldb_port_cq_disable(hw, ldb_port);

		if (found == false) { /* No more SCHs from this CQ */
			if (drain_type == 0)
				LM_DEBUG_print("CQ[%d] drained - but QEs may still exist for ORD QIDs \n", cq);
		}
		/* Restore the original HL for the drained CQ */
		set_hl_base_limit(hw, src_state, ldb_port, ldb_port);
		ndelay(10000);
	}

	for (i = 0; i < src_state->num_ldb_queues; i++) {
		qid = src_state->ldb_queue[i]->id.phys_id;
		vqid = src_state->ldb_queue[i]->id.virt_id;
		na_enq = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, qid));
		at_ac = DLB2_CSR_RD(hw, LSP_QID_ATM_ACTIVE(hw->ver, qid));
		aq_ac = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, qid));
		LM_DEBUG_print("[PANEL]LDB QID : %d(%d), na_eq: %d, at_ac: %d, aq_ac: %d\n",
			qid, vqid, na_enq, at_ac, aq_ac);
	}

	/* DIR CQs */
	for (i = 0; i < src_state->num_dir_ports && drain_type == 0; i++) {
		dir_port = src_state->dir_port[i];
		cq = dir_port->id.phys_id;
		vcq = dir_port->id.virt_id;

		na_enq = DLB2_CSR_RD(hw, LSP_QID_DIR_ENQUEUE_CNT(hw->ver, cq));
		LM_DEBUG_print("[PANEL]DIR QID: %d, na_eq: %d\n", cq, na_enq);


		tkn_cnt = dlb2_dir_cq_token_count(hw, dir_port);
		src_state->dir_cq_state[vcq].tkn_cnt = tkn_cnt;

		LM_DEBUG_print("Draining CQ = %d, Token count: %d \n", cq, tkn_cnt);

		/* Return a token to make space to schedule one new QE at a time */
		if (tkn_cnt != 0 && na_enq != 0) {
		     if (dlb2_return_token(hw, cq, 0)) {
			LM_DEBUG_print("DIR Token Return Failure\n");
		     } else {
			tkn_cnt = dlb2_dir_cq_token_count(hw, dir_port);
			LM_DEBUG_print("Returned 1 token, new tkn_cnt: %d\n", tkn_cnt);
		     }
		}


		/* CQ write pointer is set to 0. Every new QE scheduled to the CQ will then
		 * arrive with CQ gen (hcw->cq_token) bit set.
		 */
		DLB2_CSR_WR(hw, CHP_DIR_CQ_WPTR(hw->ver, cq), CHP_DIR_CQ_WPTR_RST);

		DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_L(cq), dummy_cq_dma_base & 0xffffffc0);
		DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_U(cq), dummy_cq_dma_base >> 32);

		src_state->dir_qid_state[vcq].num_drain_hcws = 0;

		/* Reset PASID for HCW draining in PF host driver */
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_PASID(hw->ver, cq),
			    SYS_DIR_CQ_PASID_RST);

		/* Enable the DIR port */
		dlb2_dir_port_cq_enable(hw, dir_port);

		cnt = 0;
		loop_cnt = 0;
		while (loop_cnt < 10000) {
			found = false;
			if (dlb2_dir_cq_token_count(hw, dir_port) > tkn_cnt) {
				/* one new QE has been scheduled */
				hcw = (struct dlb2_hcw *)((uintptr_t)dummy_cq_base);

				vqid = vcq;//hcw->qid;

				if (cnt % 500 == 0)
					LM_DEBUG_print("[%d]Reading SRC HCW[%d]: 0x%016llx 0x%016llx"
						" at CQ=%d VQID = %d qType = %d udata64: %llx\n",
						src_state->dir_qid_state[vqid].num_drain_hcws, cnt,
						*(uint64_t *)hcw, *((uint64_t *)hcw + 1),
						cq, vqid, hcw->sched_type, hcw->data);

				k = src_state->dir_qid_state[vqid].num_drain_hcws;
				src_state->dir_qid_state[vqid].drain_hcw[k] = *hcw;
				src_state->dir_qid_state[vqid].num_drain_hcws++;
				cnt++;
				DLB2_CSR_WR(hw, CHP_DIR_CQ_WPTR(hw->ver, cq), CHP_DIR_CQ_WPTR_RST);

				if (dlb2_return_token(hw, cq, 0))
					LM_DEBUG_print("DIR Token Return Failure\n");

				found = true;
			}
			loop_cnt++;
		}
		/* Another option is to send BAT_T (Batch token return) */

		tkn_cnt = dlb2_dir_cq_token_count(hw, dir_port);
		LM_DEBUG_print("Token count after QID drain: %d\n", tkn_cnt);

		/* Disable the dir port again */
		dlb2_dir_port_cq_disable(hw, dir_port);

		na_enq = DLB2_CSR_RD(hw, LSP_QID_DIR_ENQUEUE_CNT(hw->ver, cq));
		LM_DEBUG_print("[PANEL]DIR QID: %d, na_eq: %d\n", cq, na_enq);

		if (found == false) /* No more SCHs from this CQ */
			LM_DEBUG_print("CQ[%d] draining complete \n", cq);
	}

	return 0;
}

static void dlb2_get_queue_status(struct dlb2_hw *hw,
				  struct dlb2_migration_state *src_state,
				  uint16_t *na_enq,
				  uint16_t *at_ac,
				  uint16_t *aq_ac)
{
	struct dlb2_ldb_queue *queue;
	int i, qid;

	for (i = 0; i < src_state->num_ldb_queues; i++) {
		queue = src_state->ldb_queue[i];
		qid  = queue->id.phys_id;

		na_enq[i] = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, qid));
		at_ac[i] = DLB2_CSR_RD(hw, LSP_QID_ATM_ACTIVE(hw->ver, qid));
		aq_ac[i] = DLB2_CSR_RD(hw, LSP_QID_AQED_ACTIVE_CNT(hw->ver, qid));
		LM_DEBUG_print("%s: qid = %d, na_enq = %d, at_ac = %d, aq_ac = %d\n",
				__func__, qid, na_enq[i], at_ac[i], aq_ac[i]);
	}
}

static int dlb2_drain_src_ord_qid(struct dlb2_hw *hw,
				  struct dlb2_migration_state *src_state)
{
	uint16_t hl_idx, sn, tkn_cnt, inflights;
	mig_ldb_qid_state_t *qid_state;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	uint8_t cq, qid, vcq, vqid;
	uint32_t i, k, val;

	/* Disable scheduling such that no further schedules
	 * take place until we clear out the ROB */
	for (i = 0; i < src_state->num_ldb_ports; i++) {
		port = src_state->ldb_port[i];
		cq  = port->id.phys_id;
		dlb2_ldb_port_cq_disable(hw, port);
	}

	for (i = 0; i < src_state->num_ldb_queues; i++) {
		queue = src_state->ldb_queue[i];
		qid  = queue->id.phys_id;
		vqid  = queue->id.virt_id;
		qid_state = &src_state->ldb_qid_state[vqid];

		/* Process only the ORD QIDs */
		if (queue->sn_cfg_valid == 1) {
			uint16_t na_enq[32], at_ac[32], aq_ac[32];

			dlb2_get_queue_status(hw, src_state, na_enq, at_ac, aq_ac);

			for (k = 0; k < qid_state->num_sn_in_hl; k++) {
				sn = qid_state->sn_list[k];
				vcq = qid_state->sn_state[sn].hl_port_id;
				port = src_state->ldb_port[vcq];
				cq = port->id.phys_id;
				if (vcq != port->id.virt_id)
					LM_DEBUG_print("%s: virtial port ID does not match %d, %d! \n",
							__func__, vcq, port->id.virt_id);
				hl_idx = qid_state->sn_state[sn].hl_idx;

				DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, cq),
					    src_state->hl_ptr[hl_idx]);
				val = read_hist_list_pop_ptr(hw, cq);
				tkn_cnt = dlb2_ldb_cq_token_count(hw, port);
				inflights = num_ldb_inflights(hw, cq);
				LM_DEBUG_print("MIG: QID [%d(%d)] Sending in a COMP for ORD Entry"
					" SN = %d via CQ = %d hl_idx = %d pop_ptr = 0x%08x"
					" inflights: %d tkn_cnt: %d\n", qid, vqid, sn, cq, hl_idx,
					 val, inflights, tkn_cnt);

				if (pf_send_comp_token(hw, port))
					LM_DEBUG_print("Drain CQ COMP/TOK Send FAIL %x\n", val);
			}

			ndelay(100);
			/* Find the re-enqueue QID of HCWs in ROB of by checking
			 * the qeueue activities.
			 */
			for (k = 0; k < src_state->num_ldb_queues; k++) {
				struct dlb2_ldb_queue *renq_queue;
				uint16_t renq_qid;
				uint16_t na_enq_1, at_ac_1, aq_ac_1;

				renq_queue = src_state->ldb_queue[k];
				renq_qid  = renq_queue->id.phys_id;

				na_enq_1 = DLB2_CSR_RD(hw,
						LSP_QID_LDB_ENQUEUE_CNT(hw->ver, renq_qid));
				at_ac_1 = DLB2_CSR_RD(hw,
						LSP_QID_ATM_ACTIVE(hw->ver, renq_qid));
				aq_ac_1 = DLB2_CSR_RD(hw,
						LSP_QID_AQED_ACTIVE_CNT(hw->ver, renq_qid));

				if ((na_enq_1 != na_enq[k]) || (at_ac_1 != at_ac[k])
				    || (aq_ac_1 != aq_ac[k])) {
					LM_DEBUG_print("renq_qid found: renq_qid = %d, qid = %d,"
						" na_enq = %d, at_ac = %d," " aq_ac = %d\n",
						renq_qid, qid, na_enq_1, at_ac_1, aq_ac_1);

					qid_state->renq_qid = renq_queue->id.virt_id;
					break;
				}
			}
		}
	}

	return 0;
}

static bool pf_sch_dummy_ord_hcw(struct dlb2_hw *hw,
			  struct dlb2_ldb_port *port,
			  struct dlb2_ldb_queue *queue)
{
	uint16_t exp_inflights = 0, cur_inflights = 0;
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;
	uint8_t cq, qid, vqid;
	int loop_cnt, tkn_cnt;

	cq = port->id.phys_id;
	qid  = queue->id.phys_id;
	vqid  = queue->id.virt_id;

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
	pp_addr = os_map_producer_port_maskable(hw, cq, true);

	/* Setup the required HCW fields from the SRC HL Entry */
	memset(hcw, 0, 4 * sizeof(*hcw));
	hcw->qe_valid = 1;
	/* Set the vqid for new CQ */
	hcw->qid =  vqid;
	hcw->sched_type = 2;
	hcw->data =  0xaaaa;

	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

	if (tkn_cnt && tkn_cnt >= port->cq_depth)
		hcw->cq_token = 1;

	os_enqueue_four_hcws(hw, hcw, pp_addr);
	os_fence_hcw(hw, pp_addr);
	os_unmap_producer_port(hw, pp_addr);

	cur_inflights = num_ldb_inflights(hw, cq);

	/* Enable the port */
	dlb2_ldb_port_cq_enable(hw, port);

	loop_cnt = 0;
	exp_inflights = cur_inflights + 1;
	while (cur_inflights < exp_inflights) {
		loop_cnt++;
		if (loop_cnt > 100000) {
			LM_DEBUG_print("CQ[%d]: SCH HCW failed at destination: "
			       "exp = %d, actual = %d, port->num_pending_removals = %d\n",
				cq, exp_inflights, cur_inflights, port->num_pending_removals);
			return 1;
		}
		cur_inflights = num_ldb_inflights(hw, cq);
	}

	/* Disable the port */
	dlb2_ldb_port_cq_disable(hw, port);

	return 0;
}

/* Insert a RENQ for ENQed QEs as part of the RENQs from completed ORD QEs.
 * Note: qid passed in here is the original src_qid
 */
static bool fill_dest_rob_renq(struct dlb2_hw *hw, struct dlb2_ldb_port *port,
		        uint8_t qid, uint16_t idx,
			struct dlb2_migration_state *dst_state)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;
	uint32_t tkn_cnt;
	uint8_t cq;

	cq = port->id.phys_id;

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

	/* Setup the required HCW fields from the SRC HL Entry */
	memset(hcw, 0, 4 * sizeof(*hcw));

	hcw->qe_valid = 1;
	hcw->qe_comp = 1;

	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);
	//if (tkn_cnt)
	//	hcw->cq_token = 1;
	hcw->cq_token = 1;

	qid = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].qid;
	LM_DEBUG_print("[%s]Using re-enq QID : %d\n", __func__, qid);

	hcw->qid = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].qid;
	hcw->sched_type = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].sched_type;
	hcw->lock_id =  dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].lock_id;
	hcw->data = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].data;
	hcw->opaque = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].opaque;
	hcw->priority = dst_state->ldb_qid_state[qid].drain_rob_hcw[idx].priority;

	pp_addr = os_map_producer_port_maskable(hw, cq, true);
	os_enqueue_four_hcws(hw, hcw, pp_addr);
	LM_DEBUG_print("PP[%d] Writing RENQ HCW[%d] QID = %d qType = %d 0x%04x data=0x%llx\n",
			cq, idx, hcw->qid, hcw->sched_type, hcw->lock_id, hcw->data);
	return 0;
}

static struct dlb2_ldb_port *dlb2_find_renq_port(struct dlb2_hw *hw,
					struct dlb2_migration_state *dst_state,
					struct dlb2_ldb_queue *queue)
{
	enum dlb2_qid_map_state map_state;
	struct dlb2_ldb_port *port;
	uint8_t cq, vcq;
	int i, slot;
	u32 qlen;

	for (i = 0; i < dst_state->num_ldb_ports; i++) {
		port = dst_state->ldb_port[i];
		vcq = port->id.virt_id;
		cq = port->id.phys_id;

		qlen = DLB2_CSR_RD(hw, CHP_LDB_CQ_DEPTH(hw->ver, cq));
		qlen = qlen & CHP_LDB_CQ_DEPTH_DEPTH;

		map_state = DLB2_QUEUE_MAPPED;
		if (dlb2_port_find_slot_queue(port, map_state, queue, &slot)) {
			u32 infl_cnt = num_ldb_inflights(hw, cq);

			/* Make sure the CQ has at least one space for ROB hcw re-enqueue,
			 * and the CQ is not full.
			 */
			if (infl_cnt < dst_state->ldb_cq_state[vcq].inflights_limit  &&
                            qlen < port->cq_depth) {
				LM_DEBUG_print("MIG: i = %d, re-enqueue port = %d, qlen = %d, infl_cnt = %d\n",
						i, port->id.phys_id, qlen, infl_cnt);
				return port;
			}
		}
	}

	return NULL;
}

static int dlb2_fill_dest_rob_hl(struct dlb2_hw *hw,
				 struct dlb2_migration_state *dst_state)
{
	uint16_t hl_idx, sn, num_sn, sn_min, sn_max, num_drained;
	struct dlb2_ldb_port *dst_port, *renq_port;
	uint8_t dst_cq, dst_qid, qid2, dst_vqid;
	struct dlb2_ldb_queue *dst_queue;
	struct dlb2_sn_group *group;
	uint8_t dst_vcq;
	uint32_t i, j;

	for (i = 0; i < dst_state->num_ldb_queues; i++) {
		dst_queue = dst_state->ldb_queue[i];
		dst_qid = dst_queue->id.phys_id;
		dst_vqid = dst_queue->id.virt_id;

		/* Process only the ORD QIDs */
		if (dst_queue->sn_cfg_valid == 1 &&
		    dst_state->ldb_qid_state[dst_vqid].num_sn_in_hl > 0) {
			/* Since queues and domain have been configured, each ordered
			 * queue should have a valid sn group and corresponding
			 * sequeuece_number_per_queue.
			 */
			group = &hw->rsrcs.sn_groups[dst_queue->sn_group];
			num_sn = group->sequence_numbers_per_queue;
			LM_DEBUG_print("MIG: sn_group = %d, sn_per_queue = %d",
					dst_queue->sn_group, num_sn);

			/* Get the oldest SN  and the CQ */
			/* if not there, skip the QID as there are no ORD QEs in ROB! */
			sn_min = dst_queue->sn_slot * num_sn;
			sn_max = num_sn + sn_min;
			num_drained = 0;
			qid2 = dst_state->ldb_qid_state[dst_vqid].renq_qid;
			sn = dst_state->ldb_qid_state[dst_vqid].sn_list[0];

			for (j = 0; j < num_sn; j++) {
				LM_DEBUG_print("MIG: ROB Processing: num_sn:%d sn_min = %d, j = %d,"
					"  SN = %d QID = %d(%d) \n", num_sn, sn_min, j, sn, dst_qid, dst_vqid);
				if (dst_state->ldb_qid_state[dst_vqid].sn_state[sn].hl_valid) {
					dst_vcq = dst_state->ldb_qid_state[dst_vqid].sn_state[sn].hl_port_id;
					dst_port = dst_state->ldb_port[dst_vcq];
					dst_cq = dst_port->id.phys_id;
					hl_idx = dst_state->ldb_qid_state[dst_vqid].sn_state[sn].hl_idx;

					/* Copy the src inflights limit to dest CQ if not already done*/
					DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, dst_cq),
						 dst_state->ldb_cq_state[dst_vcq].inflights_limit);

					/* Restore the original HL for the drained CQ */
					set_hl_base_limit(hw, dst_state, dst_port, dst_port);
					set_hl_pop_push_ptr(hw, dst_cq, dst_port->hist_list_entry_base +
						(hl_idx - dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base));

					LM_DEBUG_print("MIG: ROB FILL: Writing Dummy ORD HCW to CQ=%d(%d)"
						" hl_port_id = %d hl_idx = %d"
						" (0x%04x) SN = %d QID = %d(%d) \n",
						dst_cq, dst_port->id.virt_id, dst_vcq, hl_idx, hl_idx,
						sn, dst_qid, dst_vqid);

					DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(dst_cq), dummy_cq_dma_base & 0xffffffc0);
					DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(dst_cq), dummy_cq_dma_base >> 32);

					if (pf_sch_dummy_ord_hcw(hw, dst_port, dst_queue))
						LM_DEBUG_print("ERR: sch_dummy_ord_hcw Failed \n");

				}
				else { /* RENQ is already in ROB - use the drain_rob list to ENQ/SCH */
					renq_port = dlb2_find_renq_port(hw, dst_state, dst_queue);
					if (!renq_port) {
						LM_DEBUG_print("No port is linked to queue:%d\n", dst_qid);
						LM_DEBUG_print("Use port 0 for re-enqueue\n");
						renq_port = dst_state->ldb_port[0];
					}
					dst_port = renq_port;
					dst_cq = dst_port->id.phys_id;
					dst_vcq = dst_port->id.virt_id;

					LM_DEBUG_print("MIG: ROB FILL (NOT IN HL):  SN = %d QID = %d"
						 "dst_cq: %d,num_drained: %d, num_drain_rob_hcws: %d,"
						 " inflight_limit: %d\n",
						 sn, dst_qid, dst_cq,  num_drained,
						 dst_state->ldb_qid_state[qid2].num_drain_rob_hcws,
						 dst_state->ldb_cq_state[dst_vcq].inflights_limit);
					/* Point the HL to dummy area */
					if (num_drained < dst_state->ldb_qid_state[qid2].num_drain_rob_hcws) {
					    /* first ENQ/SCH ORD QE to establish ORD SN */
					    /* Copy the src inflights limit to dest CQ if not already done */
					    DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, dst_cq),
						      dst_state->ldb_cq_state[dst_vcq].inflights_limit);

					/* Use dummy cq history list */
					set_hl_base_limit(hw, dst_state, dst_port, NULL);
					set_hl_pop_push_ptr(hw, dst_cq, DUMMY_CQ_HIST_LIST_BASE(dst_state));

					DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(dst_cq), dummy_cq_dma_base & 0xffffffc0);
					DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(dst_cq), dummy_cq_dma_base >> 32);

					if (pf_sch_dummy_ord_hcw(hw, dst_port, dst_queue))
						LM_DEBUG_print("ERR: sch_dummy_ord_hcw Failed \n");
					/* second, insert a RENQ to complete the first pass */
					if (fill_dest_rob_renq(hw, dst_port, qid2, num_drained, dst_state))
						LM_DEBUG_print("ERR: fill_rob_renq Failed \n");

					num_drained ++;

					}
				}
				sn++;
				if (sn >= sn_max) {
				  sn = sn_min;
				}
			}
		}
	}

	return 0;
}

static bool pf_sch_dummy_hcw(struct dlb2_hw *hw, struct dlb2_ldb_port *dst_port, uint16_t hl_idx,
		      uint16_t tkn_cnt, struct dlb2_migration_state *dst_state)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	uint8_t dst_vcq, dst_cq;
	void __iomem *pp_addr;
	uint8_t qid;

	dst_cq = dst_port->id.phys_id;

	dst_vcq = dst_port->id.virt_id;

	/* If ord save the ORD Info and skip the HL entry */
	/* Note hl_idx (src pop_ptr) does not have to be at idx=0 for SRC -
	 * but in dst the pop_ptr will be at 0
	 */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
	memset(hcw, 0, 4 * sizeof(*hcw));

	/* Setup the required HCW fields from the SRC HL Entry */
	hcw->qe_valid = 1;

	/* If all the CQ entries are restored, but few HL entries are yet to
	 * be restored, set the cq_token bit for such entries
	 */
	LM_DEBUG_print("tkn_cnt: %d, dst_state->ldb_cq_state[dst_vcq].tkn_cnt: %d\n",
			tkn_cnt, dst_state->ldb_cq_state[dst_vcq].tkn_cnt);
	if (tkn_cnt && tkn_cnt >= dst_state->ldb_cq_state[dst_vcq].tkn_cnt)
		hcw->cq_token = 1;

	qid = dst_state->hlist_state[hl_idx].qid;
	hcw->qid = qid;
	hcw->sched_type = dst_state->hlist_state[hl_idx].qtype;
	hcw->lock_id = dst_state->hlist_state[hl_idx].hid;
	hcw->data =  0;

	pp_addr = os_map_producer_port_maskable(hw, dst_cq, true);
	LM_DEBUG_print("Writing SRC HCW at HL[%d] HCW: 0x%016llx 0x%016llx to PP =%d(%d) "
		"PP addr:%p QID = %d qType = %d, udata64: %llx\n",
		hl_idx, *(uint64_t *)hcw, *((uint64_t *)hcw + 1), dst_cq, dst_vcq, pp_addr,
		hcw->qid, hcw->sched_type, hcw->data);
	os_enqueue_four_hcws(hw, hcw, pp_addr);
	os_fence_hcw(hw, pp_addr);
	os_unmap_producer_port(hw, pp_addr);

	return 0;
}

static bool fill_dest_hl(struct dlb2_hw *hw,
		  struct dlb2_ldb_port *dst_port,
		  struct dlb2_migration_state *dst_state)
{
	uint16_t inflights, exp_inflights = 0, cur_inflights = 0;
	uint32_t infl_cnt, i, val, loop_cnt;
	uint16_t hl_idx, tkn_cnt;
	uint8_t dst_vcq, dst_cq;

	dst_cq = dst_port->id.phys_id;

	dst_vcq = dst_port->id.virt_id;

	/* Read the number of COMPs the HW is waiting on */
	inflights =  dst_state->ldb_cq_state[dst_vcq].inflights;
	if (inflights == 0) {
		LM_DEBUG_print("Nothing to be done with Dest CQ[%d] Inflights = %d \n", dst_cq, inflights);
		return 0;
	}
	/* Copy the src inflights limit to dest CQ */
	DLB2_CSR_WR(hw, LSP_CQ_LDB_INFL_LIM(hw->ver, dst_cq),
		    dst_state->ldb_cq_state[dst_vcq].inflights_limit);

	tkn_cnt = dlb2_ldb_cq_token_count(hw, dst_port);

	/* The first HL entry to read to the SRC HL pop_ptr and set this as dest cq pop/push ptr */
	set_hl_base_limit(hw, dst_state, dst_port, dst_port);

	hl_idx = dst_state->ldb_cq_state[dst_vcq].pop_ptr;


	/* Setting Push and Pop Ptrs */
	DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver,
		    dst_cq), dst_port->hist_list_entry_base +
		    (hl_idx - dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base));

	val = DLB2_CSR_RD(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, dst_cq));

	DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver,
		    dst_cq), dst_port->hist_list_entry_base +
		    (hl_idx - dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base));

	val = DLB2_CSR_RD(hw, CHP_HIST_LIST_POP_PTR(hw->ver, dst_cq));

	/* Need to start writing the destination CQ with the
	 * current GEN bit from the SRC
	 */
	DLB2_CSR_WR(hw, CHP_LDB_CQ_WPTR(hw->ver, dst_cq),
			(dst_state->ldb_cq_wptr[dst_vcq] & 0x800));

	DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(dst_cq), dummy_cq_dma_base & 0xffffffc0);
	DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(dst_cq), dummy_cq_dma_base >> 32);

	val = DLB2_CSR_RD(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, dst_cq));

	LM_DEBUG_print("Filling Dest CQ[%d] SRC_HL_IDX/POP_PTR = 0x%08x push_ptr = 0x%08x"
		" Inflights = %d, Token cnt : %d\n", dst_cq, hl_idx, val, inflights, tkn_cnt);
	LM_DEBUG_print("dst_port->hist_list_entry_limit : 0x%08x ,"
		" dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base : 0x%08x\n",
		dst_port->hist_list_entry_limit - 1,
		dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base);

	/* Reset PASID for HCW draining in PF host driver */
	DLB2_CSR_WR(hw,
		    SYS_LDB_CQ_PASID(hw->ver, dst_cq),
		    SYS_LDB_CQ_PASID_RST);

	LM_DEBUG_print("%s: dummy_cq_dma_base = 0x%016llx, dummy_cq_base = 0x%016llx \n", __func__,
			dummy_cq_dma_base, (uint64_t)dummy_cq_base);

	/* Send dummy QEs to fill in the dest HL
	 * Content of the dummy QE is obtained from the src HL
	 */
	for (i = 0; i < inflights; i++) {
		/* start from the HL base once the limit is reached */
		if (hl_idx >  dst_port->hist_list_entry_limit - 1)
			hl_idx = dst_port->hist_list_entry_base;

		if (dst_state->hlist_state[hl_idx].qtype != ORDERED) {

			DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver,
				    dst_cq), dst_port->hist_list_entry_base +
				    (hl_idx - dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base));

			val = DLB2_CSR_RD(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, dst_cq));

			LM_DEBUG_print("Filling Dest CQ[%d] SRC_CQ = %d SRC_HL_IDX = %d push_ptr = 0x%08x\n",
				dst_cq, dst_cq, hl_idx, val);

			/* Enable scheduling to send the dummy QE to fill dest HL*/
			dlb2_ldb_port_cq_enable(hw, dst_port);

			if (pf_sch_dummy_hcw(hw, dst_port, hl_idx, tkn_cnt, dst_state))
				LM_DEBUG_print("Fill Dummy HCW failed \n");

			exp_inflights++;
			loop_cnt = 0;
			cur_inflights = num_ldb_inflights(hw, dst_cq);
			tkn_cnt = dlb2_ldb_cq_token_count(hw, dst_port);
			LM_DEBUG_print("exp inflights = %d, current inflights = %d, token count: %d\n",
				exp_inflights, cur_inflights, tkn_cnt);

			while (cur_inflights < exp_inflights) {
				loop_cnt++;
				if (loop_cnt > 10000) {
					LM_DEBUG_print("CQ[%d]: insert HCW failed at destination: exp = %d, actual = %d\n",
						dst_cq, exp_inflights, cur_inflights);
					return 1;
				}
				cur_inflights = num_ldb_inflights(hw, dst_cq);
			}
		}
		/* Disable the CQ */
		dlb2_ldb_port_cq_disable(hw, dst_port);
		hl_idx++;
	}

	infl_cnt = DLB2_CSR_RD(hw, LSP_CQ_LDB_INFL_CNT(hw->ver, dst_cq));

	LM_DEBUG_print("Curr infl_cnt = %d, Curr token count: %d, Src token count: %d\n",
			infl_cnt, dlb2_ldb_cq_token_count(hw, dst_port),
			dst_state->ldb_cq_state[dst_vcq].tkn_cnt);

	/* Restore Token Count at DEST */
	while (dlb2_ldb_cq_token_count(hw, dst_port) > dst_state->ldb_cq_state[dst_vcq].tkn_cnt)
		dlb2_return_token(hw, dst_cq, 1);

	//DLB2_CSR_WR(hw, LSP_CQ_LDB_TKN_CNT(hw->ver, dst_cq), dst_state->ldb_cq_state[dst_vcq].tkn_cnt + dst_port->init_tkn_cnt);
	tkn_cnt = dlb2_ldb_cq_token_count(hw, dst_port);

	inflights = num_ldb_inflights(hw, dst_cq);
	LM_DEBUG_print("Filled Dest CQ[%d] SRC_CQ = %d Inflights = %d, Token Count: %d\n",
			dst_cq, dst_cq, inflights, tkn_cnt);

	/* Disable the CQ */
	dlb2_ldb_port_cq_disable(hw, dst_port);

	return 0;
}

static bool pf_sch_dummy_dir_hcw(struct dlb2_hw *hw,
			  struct dlb2_dir_pq_pair *port)
{
	struct dlb2_hcw hcw_mem[8], *hcw;
	void __iomem *pp_addr;
	uint32_t tkn_cnt;
	uint8_t cq, vcq;

	cq = port->id.phys_id;
	vcq = port->id.virt_id;

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
	pp_addr = os_map_producer_port_maskable(hw, cq, 0);

	/* Setup the required HCW fields from the SRC HL Entry */
	memset(hcw, 0, 4 * sizeof(*hcw));
	hcw->qe_valid = 1;
	/* Set the vqid for new CQ */
	hcw->qid = vcq;
	hcw->sched_type = 3;
	hcw->data = 0;

	os_enqueue_four_hcws(hw, hcw, pp_addr);
	os_fence_hcw(hw, pp_addr);
	os_unmap_producer_port(hw, pp_addr);

	tkn_cnt = dlb2_dir_cq_token_count(hw, port);
	LM_DEBUG_print("Writing HCW: 0x%016llx 0x%016llx to PP =%d "
		"PP addr:%p QID = %d qType = %d, new tkn_cnt: %d\n",
		*(uint64_t *)hcw, *((uint64_t *)hcw + 1), cq, pp_addr,
		hcw->qid, hcw->sched_type, tkn_cnt);

	return 0;
}

static bool restore_dest_tokens(struct dlb2_hw *hw,
			 struct dlb2_dir_pq_pair *dst_port,
			 struct dlb2_migration_state *dst_state)
{
	uint16_t cur_tkn_cnt, expected_tkn_cnt;
	uint8_t dst_cq, dst_vcq;

	dst_cq = dst_port->id.phys_id;
	dst_vcq = dst_port->id.virt_id;

	DLB2_CSR_WR(hw, CHP_DIR_CQ_WPTR(hw->ver, dst_cq), CHP_DIR_CQ_WPTR_RST);
	DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_L(dst_cq), dummy_cq_dma_base & 0xffffffc0);
	DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_U(dst_cq), dummy_cq_dma_base >> 32);

	expected_tkn_cnt = dst_state->dir_cq_state[dst_vcq].tkn_cnt;
	cur_tkn_cnt = dlb2_dir_cq_token_count(hw, dst_port);
	LM_DEBUG_print("Restoring Token count(%d) at DST to : %d\n", cur_tkn_cnt, expected_tkn_cnt);

	/* Reset PASID for HCW draining in PF host driver */
	DLB2_CSR_WR(hw,
		    SYS_DIR_CQ_PASID(hw->ver, dst_cq),
		    SYS_DIR_CQ_PASID_RST);

	/* Enable the DIR port scheduling to send the dummy QE  */
	dlb2_dir_port_cq_enable(hw, dst_port);

	while(cur_tkn_cnt < expected_tkn_cnt) {
		if (pf_sch_dummy_dir_hcw(hw, dst_port))
			LM_DEBUG_print("Fill Dummy HCW failed \n");
		cur_tkn_cnt = dlb2_dir_cq_token_count(hw, dst_port);
	}

	cur_tkn_cnt = dlb2_dir_cq_token_count(hw, dst_port);
	LM_DEBUG_print("Updated Token cnt at DST: %d\n", cur_tkn_cnt);

	/* Disable the port */
	dlb2_dir_port_cq_disable(hw, dst_port);

	return 0;
}

/* Read the number of COMPs the HW is waiting on for LDB.
 * For DIR, restore the token count.
 */
static bool dlb2_fill_dest_vas_hl(struct dlb2_hw *hw,
			   struct dlb2_migration_state *dst_state)
{
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_port *port;
	uint8_t cq;
	int i;

	LM_DEBUG_print("MIG: Preparing to FILL the Dest CQ HL \n");
	for (i = 0; i < dst_state->num_ldb_ports; i++) {
		port = dst_state->ldb_port[i];
		cq  = port->id.phys_id;
		LM_DEBUG_print("MIG: Disabling DST CQ: %2d \n", cq);
		dlb2_ldb_port_cq_disable(hw, port);
	}
	LM_DEBUG_print("\n");

	LM_DEBUG_print("MIG: Enqueue/Schedule Dummy QEs that match the SRC HL \n");
	for(i = 0; i < dst_state->num_ldb_ports; i++) {
		if (fill_dest_hl(hw, dst_state->ldb_port[i], dst_state))
			LM_DEBUG_print("FILL HL FAIL \n");
	}

	LM_DEBUG_print("MIG: Preparing to restore DIR Tokens at destination \n");
	for (i = 0; i < dst_state->num_dir_ports; i++) {
		dir_port = dst_state->dir_port[i];
		cq  = dir_port->id.phys_id;
		LM_DEBUG_print("MIG: Disabling DST CQ: %2d \n", cq);
		dlb2_dir_port_cq_disable(hw, dir_port);
		restore_dest_tokens(hw, dir_port, dst_state);
	}
	LM_DEBUG_print("\n");

	return 0;
}

static bool fill_dest_qes_dir(struct dlb2_hw *hw,
		       uint8_t pp,
		       struct dlb2_dir_pq_pair *dst_queue,
		       struct dlb2_migration_state *dst_state)
{
	struct dlb2_hcw hcw_mem[8], *hcw_out;
	uint8_t dst_qid, dst_vqid;
	void __iomem *pp_addr;
	uint16_t i, domain_id;
	uint32_t enq;
	int val;

	domain_id = dst_state->domain->id.phys_id;

	dst_qid = dst_queue->id.phys_id;
	dst_vqid = dst_queue->id.virt_id;

	pp_addr = os_map_producer_port_maskable(hw, pp, 0);

	LM_DEBUG_print("QID : %d(%d)  num_drain_hcws: %d\n", dst_qid, dst_vqid,
			dst_state->dir_qid_state[dst_vqid].num_drain_hcws);
	for(i = 0; i < dst_state->dir_qid_state[dst_vqid].num_drain_hcws; i++) {
		hcw_out = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
		memset(hcw_out, 0, 4 * sizeof(*hcw_out));

		/* Insert the QE to the Dest VAS */
		hcw_out->data = dst_state->dir_qid_state[dst_vqid].drain_hcw[i].data;
		hcw_out->opaque = dst_state->dir_qid_state[dst_vqid].drain_hcw[i].opaque;
		hcw_out->qe_valid = 1;

		/* QID field in HCW may not have been populated. For DIR, CQid == Qid */
		hcw_out->qid = dst_vqid; //dst_state->dir_qid_state[dst_vqid].drain_hcw[i].qid;
		hcw_out->sched_type =  dst_state->dir_qid_state[dst_vqid].drain_hcw[i].sched_type;
		hcw_out->lock_id =  dst_state->dir_qid_state[dst_vqid].drain_hcw[i].lock_id;
		hcw_out->priority = dst_state->dir_qid_state[dst_vqid].drain_hcw[i].priority;

		os_enqueue_four_hcws(hw, hcw_out, pp_addr);
		os_fence_hcw(hw, pp_addr);
		os_unmap_producer_port(hw, pp_addr);

		if (i % 500 == 0 || i == dst_state->dir_qid_state[dst_vqid].num_drain_hcws - 1) {
			enq = DLB2_CSR_RD(hw, LSP_QID_DIR_ENQUEUE_CNT(hw->ver, dst_qid));
			val = DLB2_CSR_RD(hw, CHP_CFG_DIR_VAS_CRD(domain_id));
			LM_DEBUG_print("[%d]After writing SRC HCW: 0x%016llx 0x%016llx using PP: %d with QID = %d"
				" qType = %d udata64: %llx :: na_eq: %d, VAS DIR Credits: %d \n", i,
				*(uint64_t *)hcw_out, *((uint64_t *)hcw_out + 1), pp, hcw_out->qid,
				hcw_out->sched_type, hcw_out->data, enq, val);
		}
	}

	LM_DEBUG_print("MIG: Copying from SRC QID %d to DST QID %d \n", dst_qid, dst_qid);
	return 0;
}

static bool fill_dest_qes_ldb(struct dlb2_hw *hw,
		       uint8_t pp,
		       struct dlb2_ldb_queue *dst_queue,
		       struct dlb2_migration_state *dst_state)
{
	struct dlb2_hcw hcw_mem[8], *hcw_out;
	uint8_t dst_qid, dst_vqid;
	void __iomem *pp_addr;
	uint16_t i, domain_id;
	uint32_t enq;
	int val;

	domain_id = dst_state->domain->id.phys_id;

	dst_qid = dst_queue->id.phys_id;
	dst_vqid = dst_queue->id.virt_id;

	pp_addr = os_map_producer_port_maskable(hw, pp, 1);

	LM_DEBUG_print("QID : %d  num_drain_hcws: %d\n", dst_qid,
			dst_state->ldb_qid_state[dst_vqid].num_drain_hcws);
	for(i = 0; i < dst_state->ldb_qid_state[dst_vqid].num_drain_hcws; i++) {
		hcw_out = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);
		memset(hcw_out, 0, 4 * sizeof(*hcw_out));

		/* Insert the QE to the Dest VAS */
		hcw_out->data = dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].data;
		hcw_out->opaque = dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].opaque;
		hcw_out->qe_valid = 1;
		hcw_out->qid = dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].qid;
		hcw_out->sched_type =  dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].sched_type;
		hcw_out->lock_id =  dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].lock_id;
		hcw_out->priority = dst_state->ldb_qid_state[dst_vqid].drain_hcw[i].priority;

		os_enqueue_four_hcws(hw, hcw_out, pp_addr);
		os_fence_hcw(hw, pp_addr);
		os_unmap_producer_port(hw, pp_addr);

		if (i % 500 == 0 || i == dst_state->ldb_qid_state[dst_vqid].num_drain_hcws - 1 || i < 16) {
			enq = DLB2_CSR_RD(hw, LSP_QID_LDB_ENQUEUE_CNT(hw->ver, dst_qid));
			val = DLB2_CSR_RD(hw, CHP_CFG_LDB_VAS_CRD(domain_id));
			LM_DEBUG_print("[%d]After writing SRC HCW: 0x%016llx 0x%016llx using PP: %d with QID = %d"
				" qType = %d udata64: %llx :: na_eq: %d, VAS LDB Credits: %d \n", i,
				*(uint64_t *)hcw_out, *((uint64_t *)hcw_out + 1), pp, hcw_out->qid,
				hcw_out->sched_type, hcw_out->data, enq, val);
		}
	}

	LM_DEBUG_print("MIG: Copying from SRC QID %d to DST QID %d \n", dst_qid, dst_qid);
	return 0;
}

static bool dlb2_fill_dest_vas_qes(struct dlb2_hw *hw,
			    struct dlb2_migration_state *dst_state)
{
	uint8_t pp;
	int i;

	LM_DEBUG_print("MIG: Enqueue all drained QES \n");
	for (i = 0; i < dst_state->num_ldb_queues; i++) {
		pp = dst_state->ldb_port[0]->id.phys_id; //Using first port to enqueue QEs
		if (fill_dest_qes_ldb(hw, pp, dst_state->ldb_queue[i], dst_state))
			LM_DEBUG_print("FILL QE FAIL \n");
	}
	for (i = 0; i < dst_state->num_dir_ports; i++) {
		pp = dst_state->dir_port[0]->id.phys_id; //Using first port to enqueue QEs
		if (fill_dest_qes_dir(hw, pp, dst_state->dir_port[i], dst_state))
			LM_DEBUG_print("FILL QE FAIL \n");
	}
	return 0;
}

static bool dlb2_restore_state_vas(struct dlb2_hw *hw,
			    struct dlb2_migration_state *dst_state)
{
	struct dlb2_dir_pq_pair *dst_dir_port;
	struct dlb2_ldb_port *dst_ldb_port;
	uint8_t dst_vcq, dst_cq;
	uint32_t wptr;
	uint32_t i;

	/* Copy the CQ addr and gen bit */
	for (i = 0; i < dst_state->num_ldb_ports; i++) {
		dst_ldb_port = dst_state->ldb_port[i];
		dst_cq = dst_ldb_port->id.phys_id;

		dst_vcq = dst_ldb_port->id.virt_id;

		wptr = dst_state->ldb_cq_wptr[dst_vcq];

		set_hl_base_limit(hw, dst_state, dst_ldb_port, dst_ldb_port);
		DLB2_CSR_WR(hw, CHP_HIST_LIST_POP_PTR(hw->ver, dst_cq),
				(dst_state->ldb_cq_state[dst_vcq].pop_ptr_val -
				dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base) +
				dst_ldb_port->hist_list_entry_base);

		DLB2_CSR_WR(hw, CHP_HIST_LIST_PUSH_PTR(hw->ver, dst_cq),
				(dst_state->ldb_cq_state[dst_vcq].push_ptr_val -
				dst_state->ldb_cq_state[dst_vcq].hist_list_entry_base) +
				dst_ldb_port->hist_list_entry_base);

		/* Setting the CQ ADDR */
		LM_DEBUG_print("MIG: Setting CQ_ADDR for DST_CQ[%d] upper=0x%x, lower= 0x%x WPTR = 0x%08x\n",
			dst_cq, dst_state->ldb_cq_addr[dst_vcq].up, dst_state->ldb_cq_addr[dst_vcq].low, wptr);

		//Eventually add BITS_SET
		DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_L(dst_cq), dst_state->ldb_cq_addr[dst_vcq].low);
		DLB2_CSR_WR(hw, SYS_LDB_CQ_ADDR_U(dst_cq), dst_state->ldb_cq_addr[dst_vcq].up);

		DLB2_CSR_WR(hw, CHP_LDB_CQ_WPTR(hw->ver, dst_cq), wptr);
	}

	/* Copy the CQ addr and gen bit */
	for (i = 0; i < dst_state->num_dir_ports; i++) {
		dst_dir_port = dst_state->dir_port[i];
		dst_cq = dst_dir_port->id.phys_id;

		dst_vcq = dst_dir_port->id.virt_id;

		wptr = dst_state->dir_cq_wptr[dst_vcq];

		/* Setting the CQ ADDR */
		LM_DEBUG_print("MIG: Setting CQ_ADDR for DST_CQ[%d] upper=0x%x, lower= 0x%x WPTR = 0x%08x\n",
			dst_cq, dst_state->dir_cq_addr[dst_vcq].up, dst_state->dir_cq_addr[dst_vcq].low, wptr);

		DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_L(dst_cq), dst_state->dir_cq_addr[dst_vcq].low);
		DLB2_CSR_WR(hw, SYS_DIR_CQ_ADDR_U(dst_cq), dst_state->dir_cq_addr[dst_vcq].up);

		DLB2_CSR_WR(hw, CHP_DIR_CQ_WPTR(hw->ver, dst_cq), wptr);
	}
	return 0;
}

static bool dlb2_resume_vas(struct dlb2_hw *hw,
		     bool vdev_req,
		     unsigned int vdev_id,
		     struct dlb2_migration_state *dst_state)
{
	uint32_t i, reg;
	uint8_t cq;

	for (i = 0; i < dst_state->num_ldb_ports; i++) {
		cq = dst_state->ldb_port[i]->id.phys_id;

		/* restore PASID */
		reg = 0;
		BITS_SET(reg, hw->pasid[vdev_id], SYS_LDB_CQ_PASID_PASID);
		BIT_SET(reg, SYS_LDB_CQ_PASID_FMT2);
		DLB2_CSR_WR(hw, SYS_LDB_CQ_PASID(hw->ver, cq), reg);

		/* print the dst HL status/infor for debug.
		 *
		 * dlb2_read_src_hl(hw, dst_state->ldb_port[i], dst_state);
		 */

		LM_DEBUG_print("MIG: Enabling DST CQ : %d \n ", cq);

		dlb2_ldb_port_cq_enable(hw, dst_state->ldb_port[i]);
	}

	/* Get the dst ldb queue status for debug
	 *
	 * for (i = 0; i < dst_state->num_ldb_queues; i++) {
	 *	dlb2_read_src_queue_state(hw, dst_state->ldb_queue[i], dst_state);
	 * }
	 */

	for (i = 0; i < dst_state->num_dir_ports; i++) {
		cq = dst_state->dir_port[i]->id.phys_id;

		/* restore PASID */
		reg = 0;
		BITS_SET(reg, hw->pasid[vdev_id], SYS_DIR_CQ_PASID_PASID);
		BIT_SET(reg, SYS_DIR_CQ_PASID_FMT2);
		DLB2_CSR_WR(hw, SYS_DIR_CQ_PASID(hw->ver, cq), reg);

		LM_DEBUG_print("MIG: Enabling DST CQ : %d \n ", cq);
		dlb2_dir_port_cq_enable(hw, dst_state->dir_port[i]);
	}
	return 0;
}

static void dlb2_print_mig_state(struct dlb2_hw *hw,
				 struct dlb2_migration_state *state)
{
	uint32_t i;

	/* LDB Port */
	LM_DEBUG_print("\n =====================MIG STATUS=====================\n");
	LM_DEBUG_print("MIG: Number of LDB CQs to migrate = %d \n", state->num_ldb_ports);
	LM_DEBUG_print("MIG: List of LDB CQs: (Phy ID, Virt ID) \n");
	for(i = 0; i < state->num_ldb_ports; i++)
		LM_DEBUG_print("%2d, %2d\n", state->ldb_port[i]->id.phys_id,
			state->ldb_port[i]->id.virt_id);
	LM_DEBUG_print("\n");

	/* DIR Port */
	LM_DEBUG_print("MIG: Number of DIR CQs to migrate = %d \n", state->num_dir_ports);
	LM_DEBUG_print("MIG: List of DIR CQs: (Phy ID, Virt ID) \n");
	for(i = 0; i < state->num_dir_ports; i++)
		LM_DEBUG_print("%2d, %2d\n", state->dir_port[i]->id.phys_id,
			state->dir_port[i]->id.virt_id);
	LM_DEBUG_print("\n");

	/* LDB Queue */
	LM_DEBUG_print("MIG: Number of LDB Queues to migrate = %d \n", state->num_ldb_queues);
	LM_DEBUG_print("MIG: List of LDB Queues: (Phy ID, Virt ID) \n");
	for(i = 0; i < state->num_ldb_queues; i++)
		LM_DEBUG_print("%2d, %2d\n", state->ldb_queue[i]->id.phys_id,
			state->ldb_queue[i]->id.virt_id);
	LM_DEBUG_print("\n");
	LM_DEBUG_print("====================================================\n");
}

static int dlb2_prepare_migration(struct dlb2_hw *hw,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_migration_state *src_state)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_hw_domain *domain;
	int num_ports, num_queues, i;
	struct dlb2_bitmap *bitmap;
	uint8_t cq, vcq, vqid;
	int base;

	LM_DEBUG_print("%s, vdev_req = %d, vdev_id = %d\n", __func__, vdev_req, vdev_id);
	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;
	if (!rsrcs) {
		LM_DEBUG_print("No vdev available vdev_id = %d...\n", vdev_id);
		return -EFAULT;
	}

	/* Save domain details */
	domain = DLB2_FUNC_LIST_HEAD(rsrcs->used_domains, typeof(*domain));
	if (!domain) {
		LM_DEBUG_print("No domain configured\n");
		return -EFAULT;
	}
	src_state->domain = domain;

	LM_DEBUG_print("[%s]Src domain phys id : %d, virt id: %d\n",
		__func__, src_state->domain->id.phys_id, src_state->domain->id.virt_id);

	/* Get the HL entries for LM */
	bitmap = rsrcs->avail_hist_list_entries;
	base = dlb2_bitmap_find_set_bit_range(bitmap,
					DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	if (base < 0) {
		LM_DEBUG_print("No hist list entry available\n");
		return -EFAULT;
	}

	src_state->dummy_cq_hist_list_base = base;
	src_state->dummy_cq_hist_list_limit = base + DLB2_HIST_LIST_ENTRIES_USED_BY_LM;

	dlb2_bitmap_clear_range(bitmap, base, DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	/* Get LDB Queue basic info */
	src_state->num_ldb_queues = 0;
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, ldb_queue, iter) {

		/* Step 1.2 Prepare Migration */
		num_queues = src_state->num_ldb_queues;
		vqid = ldb_queue->id.virt_id;

		if (vqid != num_queues)
			LM_DEBUG_print("%s: vqid != num_queues; %d != %d\n", __func__, vqid, num_queues);
		src_state->ldb_queue[num_queues] = ldb_queue;

		LM_DEBUG_print("[%s]Src LDB queue phys id : %d, virt id: %d\n",
			__func__, src_state->ldb_queue[num_queues]->id.phys_id,
			src_state->ldb_queue[num_queues]->id.virt_id);

		src_state->num_ldb_queues++;
	}

	/* Disable the DIR ports and save their details */
	src_state->num_dir_ports = 0;
	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_port, iter) {

		cq = dir_port->id.phys_id;
		vcq = dir_port->id.virt_id;

		/*
		 * Can't drain a port if it's not configured, and there's
		 * nothing to drain if its queue is unconfigured.
		 */
		if (!dir_port->port_configured || !dir_port->queue_configured) {
			LM_DEBUG_print("CQ %d not enabled/configured/rx_port, skipping... \n", cq);
			continue;
		}
		/* Step 1.1 Disable CQ */
		LM_DEBUG_print("[%s]Disabling DIR port phys id : %d, virt id: %d\n",
			__func__, dir_port->id.phys_id, dir_port->id.virt_id);

		dlb2_dir_port_cq_disable(hw, dir_port);

		/* Step 1.2 Prepare Migration */
		num_ports = src_state->num_dir_ports;

		src_state->dir_port[num_ports] = dir_port;

		/* save the SRC CQ cq_addr to be used for the dst CQ */
		src_state->dir_cq_addr[vcq].low = DLB2_CSR_RD(hw, SYS_DIR_CQ_ADDR_L(cq));
		src_state->dir_cq_addr[vcq].up = DLB2_CSR_RD(hw, SYS_DIR_CQ_ADDR_U(cq));
		LM_DEBUG_print("MIG: CQ_ADDR for SRC_CQ[%d] upper=0x%x, lower= 0x%x\n",
			dir_port->id.phys_id, src_state->dir_cq_addr[vcq].up,
			src_state->dir_cq_addr[vcq].low);
		src_state->dir_cq_wptr[vcq] = DLB2_CSR_RD(hw, CHP_DIR_CQ_WPTR(hw->ver, cq));

		src_state->num_dir_ports++;

	}

	/* Disable the LDB Ports and save their details */
	src_state->num_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
	    DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], ldb_port, iter) {

		cq = ldb_port->id.phys_id;
		vcq = ldb_port->id.virt_id;

		if (!ldb_port->enabled || !ldb_port->configured) {
			LM_DEBUG_print("CQ %d not enabled/configured/rx_port, skipping... \n", cq);
			continue;
		}

		/* Step 1.1 Disable CQ */
		LM_DEBUG_print("[%s]Disabling LDB port : %d\n", __func__, ldb_port->id.phys_id);

		dlb2_ldb_port_cq_disable(hw, ldb_port);

		/* Step 1.2 Prepare Migration */
		num_ports = src_state->num_ldb_ports;

		src_state->ldb_port[num_ports] = ldb_port;

		/* save the SRC CQ cq_addr to be used for the dst CQ */
		src_state->ldb_cq_addr[vcq].low = DLB2_CSR_RD(hw, SYS_LDB_CQ_ADDR_L(cq));
		src_state->ldb_cq_addr[vcq].up = DLB2_CSR_RD(hw, SYS_LDB_CQ_ADDR_U(cq));
		LM_DEBUG_print("MIG: CQ_ADDR for SRC_CQ[%d(%d)] upper=0x%x, lower= 0x%x\n",
			ldb_port->id.phys_id, ldb_port->id.virt_id, src_state->ldb_cq_addr[vcq].up,
			src_state->ldb_cq_addr[vcq].low);

		src_state->num_ldb_ports++;

		/* Step 1.3 Store history list state only for LDB ports*/
		dlb2_read_src_hl(hw, ldb_port, src_state);

	   }
	}

	/* Save LDB Queue details */
	for (i = 0; i < src_state->num_ldb_queues; i++) {
		ldb_queue = src_state->ldb_queue[i];

		/* Step 1.3 Store the queue state */
		dlb2_read_src_queue_state(hw, ldb_queue, src_state);
	}

	/* Return hist list entries to the function */
	dlb2_bitmap_set_range(rsrcs->avail_hist_list_entries,
				src_state->dummy_cq_hist_list_base,
				DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	/* Print the migration state saved */
	dlb2_print_mig_state(hw, src_state);

	return 0;
}

static int dlb2_prepare_resumption(struct dlb2_hw *hw,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_migration_state *dst_state)
{
	struct dlb2_list_entry *iter __attribute__((unused));
	struct dlb2_function_resources *rsrcs;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_hw_domain *domain;
	int num_ports, num_queues, i;
	struct dlb2_bitmap *bitmap;
	uint8_t cq, vcq, vqid;
	int base;

	LM_DEBUG_print("%s, vdev_req = %d, vdev_id = %d\n", __func__, vdev_req, vdev_id);
	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;
	if (!rsrcs) {
		LM_DEBUG_print("No vdev available vdev_id = %d...\n", vdev_id);
		return -EFAULT;
	}

	/* Save domain details */
	domain = DLB2_FUNC_LIST_HEAD(rsrcs->used_domains, typeof(*domain));
	if (!domain) {
		LM_DEBUG_print("No domain configured\n");
		return -EFAULT;
	}
	dst_state->domain = domain;

	LM_DEBUG_print("[%s]Dst domain phys id : %d, virt id: %d\n",
		__func__, dst_state->domain->id.phys_id, dst_state->domain->id.virt_id);

	/* Get the HL entries for LM */
	bitmap = rsrcs->avail_hist_list_entries;
	base = dlb2_bitmap_find_set_bit_range(bitmap,
					DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	if (base < 0) {
		LM_DEBUG_print("No hist list entry available\n");
		return -EFAULT;
	}

	dst_state->dummy_cq_hist_list_base = base;
	dst_state->dummy_cq_hist_list_limit = base + DLB2_HIST_LIST_ENTRIES_USED_BY_LM;

	dlb2_bitmap_clear_range(bitmap, base, DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	dst_state->num_dir_ports = 0;
	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_port, iter) {

		cq = dir_port->id.phys_id;
		vcq = dir_port->id.virt_id;

		/*
		 * Can't drain a port if it's not configured, and there's
		 * nothing to drain if its queue is unconfigured.
		 */
		if (!dir_port->port_configured || !dir_port->queue_configured) {
			LM_DEBUG_print("CQ %d not enabled/configured/rx_port, skipping... \n", cq);
			continue;
		}
		LM_DEBUG_print("[%s]Dst DIR port phys id : %d, virt id: %d\n",
			__func__, dir_port->id.phys_id, dir_port->id.virt_id);

		num_ports = dst_state->num_dir_ports;
		//dst_state->dir_port[vcq] = dir_port;
		dst_state->dir_port[num_ports] = dir_port;

		dst_state->num_dir_ports++;

		/* Reset PASID for HCW draining in PF host driver */
		DLB2_CSR_WR(hw,
			    SYS_DIR_CQ_PASID(hw->ver, cq),
			    SYS_DIR_CQ_PASID_RST);
	}

	/* Disable the LDB Ports and save their details */
	dst_state->num_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
	    DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], ldb_port, iter) {

		cq = ldb_port->id.phys_id;
		vcq = ldb_port->id.virt_id;

		if (!ldb_port->enabled || !ldb_port->configured) {
			LM_DEBUG_print("CQ %d not enabled/configured/rx_port, skipping... \n", cq);
			continue;
		}

		LM_DEBUG_print("[%s]Dst LDB port : phys id: %d, virt id: %d\n", __func__,
				ldb_port->id.phys_id, ldb_port->id.virt_id);

		num_ports = dst_state->num_ldb_ports;
		dst_state->ldb_port[num_ports] = ldb_port;

		dst_state->num_ldb_ports++;

		dlb2_ldb_port_cq_disable(hw, ldb_port);

		/* Reset PASID for HCW draining in PF host driver */
		DLB2_CSR_WR(hw,
			    SYS_LDB_CQ_PASID(hw->ver, cq),
			    SYS_LDB_CQ_PASID_RST);
	   }
	}

	/* Save LDB Queue details */
	dst_state->num_ldb_queues = 0;
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, ldb_queue, iter) {

		/* Step 1.2 Prepare Migration */
		num_queues = dst_state->num_ldb_queues;
		vqid = ldb_queue->id.virt_id;

		if (vqid != num_queues)
			LM_DEBUG_print("%s: vqid != num_queues; %d != %d\n", __func__, vqid, num_queues);

		dst_state->ldb_queue[num_queues] = ldb_queue;

		LM_DEBUG_print("[%s]Src LDB queue phys id : %d, virt id: %d\n",
			__func__, dst_state->ldb_queue[num_queues]->id.phys_id,
			dst_state->ldb_queue[num_queues]->id.virt_id);

		dst_state->num_ldb_queues++;
	}

	/* Return hist list entries to the function */
	dlb2_bitmap_set_range(rsrcs->avail_hist_list_entries,
				dst_state->dummy_cq_hist_list_base,
				DLB2_HIST_LIST_ENTRIES_USED_BY_LM);

	/* Print the migration state saved */
	dlb2_print_mig_state(hw, dst_state);

	return 0;
}

int dlb2_lm_pause_device(struct dlb2_hw *hw,
			 bool vdev_req,
			 unsigned int vdev_id,
			 struct dlb2_migration_state *src_state)
{
	struct dlb2 *dlb2;
	int ret;

	dlb2 = container_of(hw, struct dlb2, hw);

	dummy_cq_base = dma_alloc_attrs(&dlb2->pdev->dev,
				  DLB2_CQ_SIZE,
				  &dummy_cq_dma_base,
				  GFP_KERNEL,
				  DMA_ATTR_FORCE_CONTIGUOUS);

	LM_DEBUG_print("%s\n", __func__);
	LM_DEBUG_print("\n------------------------------------------------------------------\n");
	LM_DEBUG_print("Step 1: Traffic is Paused and HL/SN State Information is Collected \n");
	LM_DEBUG_print("--------------------------------------------------------------------\n");
	ret = dlb2_prepare_migration(hw, vdev_req, vdev_id, src_state);
	if (ret) {
		LM_DEBUG_print("Prepare Failed \n");
		return ret;
	}

#if DRAIN_RESTORE_ORDERED_QID
	if (dlb2_read_sn_state(hw))
		LM_DEBUG_print("SN Read Failed \n");
#endif
	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 2: Returning SRC Device COMP_Ts for all except ORD SCHs \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	ret = dlb2_rerun_pending_src_comps(hw, src_state);
	if (ret) {
		LM_DEBUG_print("Return Pending COMPs Failed\n");
		return ret;
	}

	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 3: Draining all SRC QES - ORD QEs may still be in queues\n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	ret = dlb2_drain_src_vas(hw, 0, src_state);
	if (ret) {
		LM_DEBUG_print("Clean Failed \n");
		return ret;
	}

#if DRAIN_RESTORE_ORDERED_QID
	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 4: Draining all QES in ROB by Sending in ORD QE COMPs\n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	 ret = dlb2_drain_src_ord_qid(hw, src_state);
	if (ret) {
		LM_DEBUG_print("Clean ORD Failed \n");
		return ret;
	}
#endif

#if DRAIN_RESTORE_ORDERED_QID
	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 5: Draining all QES in ROB along with ORD QID QEs \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	ret =  dlb2_drain_src_vas(hw, 1, src_state);
	if (ret) {
		LM_DEBUG_print("Drain ORD VAS Failed \n");
		return ret;
	}
#endif
	return 0;
}

int dlb2_lm_restore_device(struct dlb2_hw *hw,
			   bool vdev_req,
			   unsigned int vdev_id,
			   struct dlb2_migration_state *dst_state)
{
	struct dlb2 *dlb2;
	int ret, val;

	dlb2 = container_of(hw, struct dlb2, hw);

	dummy_cq_base = dma_alloc_attrs(&dlb2->pdev->dev,
				  DLB2_CQ_SIZE,
				  &dummy_cq_dma_base,
				  GFP_KERNEL,
				  DMA_ATTR_FORCE_CONTIGUOUS);

	LM_DEBUG_print("%s\n", __func__);
	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 6a: Prepare resumption \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	ret = dlb2_prepare_resumption(hw, vdev_req, vdev_id, dst_state);
	if (ret) {
		LM_DEBUG_print("Prepare Failed \n");
		return ret;
	}

#if DRAIN_RESTORE_ORDERED_QID
	LM_DEBUG_print("\n-----------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 6: Establish Ordering in DEST ROB entries using Drained ROB QEs \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	if (dlb2_fill_dest_rob_hl(hw, dst_state)) {
		LM_DEBUG_print("Fill ROB Failed \n");
		return 1;
	}
#endif
	LM_DEBUG_print("\n-----------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 7: Filling all DEST non-ORD HL entries using dummy ENQ/SCH QEs \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	/* Fill the DST CQ HLs - scheduling of QEs required - change the CQ addr to a tmp space */
	if  (dlb2_fill_dest_vas_hl(hw, dst_state)) {
		LM_DEBUG_print("Fill HL Failed \n");
		return 1;
	}
	val = DLB2_CSR_RD(hw, CHP_CFG_LDB_VAS_CRD(dst_state->domain->id.phys_id));
	LM_DEBUG_print("[AFTER HL Restore]VAS LDB CREDITS : %d\n", val);

	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 8: Filling Back Drained  QEs \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	if (dlb2_fill_dest_vas_qes(hw, dst_state)) {
		LM_DEBUG_print("Fill QE Failed \n");
		return 1;
	}
	val = DLB2_CSR_RD(hw, CHP_CFG_LDB_VAS_CRD(dst_state->domain->id.phys_id));
	LM_DEBUG_print("[AFTER QID QE Restore]VAS LDB CREDITS : %d\n", val);

	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 9: Restoring all CQ Ring Information at the DEST to match SRC \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	/* Copy any remaining state from src to dst */
	if (dlb2_restore_state_vas(hw, dst_state)) {
		LM_DEBUG_print("Copy Failed \n");
		return 1;
	}

	LM_DEBUG_print("\n---------------------------------------------------------------------\n");
	LM_DEBUG_print("MIG Step 10: Resuming DEST Scheduling and Threads \n");
	LM_DEBUG_print("-----------------------------------------------------------------------\n");
	/* Resume DST VAS */
	if (dlb2_resume_vas(hw, vdev_req, vdev_id, dst_state)) {
		LM_DEBUG_print("Resume Failed \n");
		return 1;
	}
	LM_DEBUG_print("DEST Resumimg done! \n");

	dma_free_attrs(&dlb2->pdev->dev,
		       DLB2_CQ_SIZE,
		       dummy_cq_base,
		       dummy_cq_dma_base,
		       DMA_ATTR_FORCE_CONTIGUOUS);
	return 0;
}
