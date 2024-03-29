/*
 * Copyright (c) 2015 Mellanox Technologies, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <signal.h>
#include "ec.h"
#include "doorbell.h"

static int ec_post_recv(struct ibv_qp *qp,
			struct ibv_sge *sge,
			struct mlx5_ec_comp *comp);

static inline void
mlx5_multi_comp_set_status_check_done(struct mlx5_ec_multi_comp *comp,
				      int status);

static void
mlx5_multi_done(struct ibv_exp_ec_comp *comp);

static struct mlx5_ec_mat *
mlx5_get_ec_decode_mat(struct mlx5_ec_calc *calc,
		       uint8_t *decode_matrix,
		       int k, int m, int curr_cols, int offset)
{
	struct mlx5_ec_mat_pool *pool = &calc->mat_pool;
	struct mlx5_ec_mat *decode;
	uint8_t *buf;
	int cols = MLX5_EC_NOUTPUTS(curr_cols);
	int i, j;

	mlx5_lock(&pool->lock);
	decode = list_first_entry(&pool->list, struct mlx5_ec_mat, node);
	list_del(&decode->node);
	mlx5_unlock(&pool->lock);

	buf = (uint8_t *)(uintptr_t)decode->sge.addr;
	for (i = 0; i < k; i++) {
		for (j = 0; j < cols; j++) {
			buf[i * cols + j] = decode_matrix[i * m + j + offset];
			if (calc->w != 8)
				/* Crazy HW formatting, bit 5 is on */
				buf[i * cols + j] |= 0x10;
		}
	}
	/* Three outputs, zero the last column */
	if (curr_cols == 3)
		for (i = 0; i < k; i++)
			buf[i*cols+3] = 0x0;

	return decode;
}

static struct mlx5_ec_mat *
mlx5_get_ec_update_mat(struct mlx5_ec_calc *calc,
		       struct ibv_exp_ec_mem *ec_mem,
		       uint8_t *data_updates,
		       uint8_t *code_updates,
		       int code_start_idx,
		       int code_end_idx)
{
	struct mlx5_ec_mat_pool *pool = &calc->mat_pool;
	struct mlx5_ec_mat *update_matrix;
	uint8_t *update_mat;
	uint8_t *encode_mat = calc->mat;
	int k = ec_mem->num_data_sge;
	int m = ec_mem->num_code_sge;
	int cols = MLX5_EC_NOUTPUTS(m);
	int en_cols = MLX5_EC_NOUTPUTS(calc->m);
	int uraw, ucol, i, j, mat_offset, curr_cols, en_idx;

	mlx5_lock(&pool->lock);
	if (list_empty(&pool->list)) {
		fprintf(stderr, "pool of matrices is empty\n");
		mlx5_unlock(&pool->lock);
		return NULL;
	}

	update_matrix = list_first_entry(&pool->list, struct mlx5_ec_mat, node);
	list_del_init(&update_matrix->node);
	mlx5_unlock(&pool->lock);
	update_mat = (uint8_t *)(uintptr_t)update_matrix->sge.addr;

	/* We first constract identity matrix */
	for (uraw = 0; uraw < m; uraw++) {
		for (ucol = 0; ucol < m; ucol++) {
			if (uraw == ucol) {
				update_mat[uraw * cols + ucol] = 1;
				if (calc->w != 8)
					/* Crazy HW formatting, bit 5 is on */
					update_mat[uraw * cols + ucol] |= 0x10;
			} else {
				update_mat[uraw * cols + ucol] = 0;
				if (calc->w != 8)
					/* Crazy HW formatting, bit 5 is on */
					update_mat[uraw * cols + ucol] |= 0x10;
			}
		}
	}

	ucol = 0;
	/* Now we copy appropriate entries from encode matrix */
	for (i = 0; i < calc->k; i++) {
		if (data_updates[i]) {
			for (j = 0; j < calc->m; j++) {
				if (j >= code_start_idx && j <= code_end_idx &&
				    code_updates[j]) {
				/*
				* We update block number i
				* and we want to compute code block number j.
				* So we copy entry from encode_matrix[i][j]
				* to update matrix[uraw][ucal] and
				* to update_matrix[uraw+1][ucal].
				* Note that in update matrix raw 2*i+1
				* duplicates raw i
				*/
					en_idx = j;
					if (calc->m > MLX5_EC_NUM_OUTPUTS) {
						mat_offset =
						    j / MLX5_EC_NUM_OUTPUTS;
						encode_mat =
						    calc->matrices[mat_offset];
						curr_cols =
						    MLX5_EC_COLS(
							mat_offset,
							calc->mult_num,
							calc->m);
						en_cols =
						    MLX5_EC_NOUTPUTS(curr_cols);
						en_idx =
						    j % MLX5_EC_NUM_OUTPUTS;
					}
					update_mat[uraw * cols + ucol] =
					    encode_mat[i * en_cols + en_idx];
					update_mat[(uraw + 1) * cols + ucol] =
					    encode_mat[i * en_cols + en_idx];

					/*
					 * We filled raw entry ucol,
					 * and increase the counter
					 */
					ucol++;
				}
			}
			/*
			 * We copied required entries for data update i
			 * to update matrix raws uraw and uraw+1.
			 * Now we increase the uraw counter and
			 * zero cols counter for next iteration.
			 */
			ucol = 0;
			uraw = uraw + 2;
		}
	}

	/* Three outputs, zero the last column */
	if (m == 3)
		for (uraw = 0; uraw < k; uraw++)
			update_mat[uraw * cols + 3] = 0x0;

	return update_matrix;
}

static void
mlx5_put_ec_mat(struct mlx5_ec_calc *calc,
		struct mlx5_ec_mat *mat)
{
	struct mlx5_ec_mat_pool *pool = &calc->mat_pool;

	mlx5_lock(&pool->lock);
	list_add(&mat->node, &pool->list);
	mlx5_unlock(&pool->lock);
}

static struct mlx5_ec_comp *
mlx5_get_ec_comp(struct mlx5_ec_calc *calc,
		 struct mlx5_ec_mat *ec_mat,
		 struct ibv_exp_ec_comp *ec_comp)
{
	struct mlx5_ec_comp_pool *pool = &calc->comp_pool;
	struct mlx5_ec_comp *comp;

	mlx5_lock(&pool->lock);
	if (list_empty(&pool->list)) {
		fprintf(stderr, "pool of comps is empty\n");
		mlx5_unlock(&pool->lock);
		return NULL;
	}
	comp = list_first_entry(&pool->list, struct mlx5_ec_comp, node);
	list_del_init(&comp->node);
	mlx5_unlock(&pool->lock);

	comp->ec_mat = ec_mat;
	comp->comp = ec_comp;

	return comp;
}

static void
mlx5_put_ec_comp(struct mlx5_ec_calc *calc,
		 struct mlx5_ec_comp *comp)
{
	struct mlx5_ec_comp_pool *pool = &calc->comp_pool;

	comp->comp = NULL;
	comp->ec_mat = NULL;
	mlx5_lock(&pool->lock);
	list_add(&comp->node, &pool->list);
	mlx5_unlock(&pool->lock);
}

static struct mlx5_ec_multi_comp *
mlx5_get_ec_multi_comp(struct mlx5_ec_calc *calc,
		       struct ibv_exp_ec_comp *ec_comp,
		       int num_calcs)
{
	struct mlx5_ec_multi_comp_pool *pool = &calc->multi_comp_pool;
	struct mlx5_ec_multi_comp *comp;

	mlx5_lock(&pool->lock);
	if (list_empty(&pool->list)) {
		fprintf(stderr, "pool of multi comps is empty\n");
		mlx5_unlock(&pool->lock);
		return NULL;
	}
	comp = list_first_entry(&pool->list, struct mlx5_ec_multi_comp, node);
	list_del_init(&comp->node);
	mlx5_unlock(&pool->lock);

	comp->orig_comp = ec_comp;
	/* init original status to success */
	comp->orig_comp->status = IBV_EXP_EC_CALC_SUCCESS;
	comp->counter = num_calcs;
	memset(comp->data_update, 0, calc->k * sizeof(struct ibv_sge));

	return comp;
}

static void
mlx5_put_ec_multi_comp(struct mlx5_ec_calc *calc,
		       struct mlx5_ec_multi_comp *comp)
{
	struct mlx5_ec_multi_comp_pool *pool = &calc->multi_comp_pool;

	comp->orig_comp = NULL;
	mlx5_lock(&pool->lock);
	list_add(&comp->node, &pool->list);
	mlx5_unlock(&pool->lock);
}

static int is_post_recv(struct mlx5_ec_calc *calc, struct ibv_wc *wc)
{
	int num_comps = calc->max_inflight_calcs;
	int size = sizeof(struct mlx5_ec_comp);
	uint64_t start = (uintptr_t)calc->comp_pool.comps;
	uint64_t end = (uintptr_t)calc->comp_pool.comps + num_comps * size;

	if (wc->wr_id >= start && wc->wr_id < end)
		return 1;

	return 0;
}

static void handle_ec_comp(struct mlx5_ec_calc *calc, struct ibv_wc *wc)
{
	struct mlx5_ec_comp *comp;
	struct ibv_exp_ec_comp *ec_comp;
	int post_recv_err;
	enum ibv_exp_ec_status status = IBV_EXP_EC_CALC_SUCCESS;


	if (unlikely(wc->status != IBV_WC_SUCCESS)) {
		status = IBV_EXP_EC_CALC_FAIL;
		post_recv_err = is_post_recv(calc, wc);

		if (wc->wr_id == EC_BEACON_WRID) {
			pthread_mutex_lock(&calc->beacon_mutex);
			pthread_cond_signal(&calc->beacon_cond);
			pthread_mutex_unlock(&calc->beacon_mutex);
			return;
		} else if (!post_recv_err) {
			if (wc->status == IBV_WC_WR_FLUSH_ERR)
				fprintf(stderr, "calc on qp 0x%x was flushed.\
					did you close context with active calcs?\n",
					wc->qp_num);
			else
				fprintf(stderr, "failed calc on qp 0x%x: \
					got completion with status %s(%d) vendor_err %d\n",
					wc->qp_num, ibv_wc_status_str(wc->status),
					wc->status, wc->vendor_err);
			return;
		}
		/* For failed post_recv we return bad status within ec_comp */
	}

	comp = (struct mlx5_ec_comp *)(uintptr_t)wc->wr_id;
	if (comp->ec_mat)
		mlx5_put_ec_mat(calc, comp->ec_mat);

	ec_comp = comp->comp;
	mlx5_put_ec_comp(calc, comp);

	if (ec_comp) {
		ec_comp->status = status;
		ec_comp->done(ec_comp);
	}
}

static int ec_poll_cq(struct mlx5_ec_calc *calc, int budget)
{
	struct ibv_wc wcs[EC_POLL_BATCH];
	int poll_batch = min(EC_POLL_BATCH, budget);
	int i, n, count = 0;

	while ((n = ibv_poll_cq(calc->cq, poll_batch, wcs)) > 0) {
		if (unlikely(n < 0)) {
			fprintf(stderr, "poll CQ failed\n");
			return n;
		}

		for (i = 0; i < n; i++)
			handle_ec_comp(calc, &wcs[i]);

		count += n;
		if (count >= budget)
			break;
	}

	return count;
}

static int mlx5_ec_poll_cq(struct mlx5_ec_calc *calc)
{
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int err, count;

	err = ibv_get_cq_event(calc->channel, &ev_cq, &ev_ctx);
	if (unlikely(err))
		return err;

	if (unlikely(ev_cq != calc->cq)) {
		fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
		return -1;
	}

	if (ibv_req_notify_cq(calc->cq, 0)) {
		fprintf(stderr, "Couldn't request CQ notification\n");
		return -1;
	}

	do {
		count = ec_poll_cq(calc, EC_POLL_BUDGET);
	} while (count > 0);

	return 0;
}

static void ec_sig_handler(int signo)
{
}

void *handle_comp_events(void *data)
{
	struct mlx5_ec_calc *calc = data;
	int n = 0;
	struct sigaction sa = { };

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = ec_sig_handler;
	sigaction(SIGINT, &sa, 0);

	while (!calc->stop_ec_poller) {
		if(unlikely(mlx5_ec_poll_cq(calc)))
			break;
		if (n++ == EC_ACK_NEVENTS) {
			ibv_ack_cq_events(calc->cq, n);
			n = 0;
		}
	}

	ibv_ack_cq_events(calc->cq, n);

	return NULL;
}

struct ibv_qp *alloc_calc_qp(struct mlx5_ec_calc *calc)
{
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_qp_attr qp_attr;
	struct ibv_qp *ibqp;
	struct mlx5_qp *qp;
	struct ibv_port_attr attr;
	union ibv_gid gid;
	int err;

	memset(&attr, 0, sizeof(attr));
	err = ibv_query_port(calc->pd->context, 1, &attr);
	if (err) {
		perror("failed to query port");
		return NULL;
	};

	err = ibv_query_gid(calc->pd->context, 1, 0, &gid);
	if (err) {
		perror("failed to query gid");
		return NULL;
	};

	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	qp_init_attr.send_cq = calc->cq;
	qp_init_attr.recv_cq = calc->cq;
	/* FIXME: should really communicate that we do UMRs */
	qp_init_attr.cap.max_send_wr = calc->max_inflight_calcs * MLX5_EC_MAX_WQE_BBS;
	qp_init_attr.cap.max_recv_wr = calc->max_inflight_calcs;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	qp_init_attr.qp_type = IBV_QPT_RC;
	ibqp = ibv_create_qp(calc->pd, &qp_init_attr);
	if (!ibqp) {
		fprintf(stderr, "failed to alloc calc qp\n");
		return NULL;
	};

	/* modify to INIT */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.port_num = 1;
	qp_attr.pkey_index = 0;
	qp_attr.qp_access_flags = 0;
	err = ibv_modify_qp(ibqp, &qp_attr, IBV_QP_STATE      |
					    IBV_QP_PORT	      |
					    IBV_QP_PKEY_INDEX |
					    IBV_QP_ACCESS_FLAGS);
	if (err) {
		perror("failed to modify calc qp to INIT");
		goto clean_qp;
	}

	qp = to_mqp(ibqp);
	/* Don't track SQ overflow - we are covered with the RQ flow-ctrl */
	qp->gen_data.create_flags |= IBV_EXP_QP_CREATE_IGNORE_SQ_OVERFLOW;

	/* modify to RTR */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTR;
	/* FIXME: increasing qp_attr.path_mtu improves performance
	 * But today on RoCE we got default port mtu (and thus rxb) 1500
	 * We cannot have qp mtu greater than rxb
	 * Waiting for FW to fix it
	 */
	qp_attr.path_mtu = IBV_MTU_1024;
	qp_attr.dest_qp_num = ibqp->qp_num;
	qp_attr.rq_psn = 0;
	qp_attr.max_dest_rd_atomic = 0;
	qp_attr.min_rnr_timer = 12;
	qp_attr.ah_attr.is_global = 1;
	qp_attr.ah_attr.grh.hop_limit = 1;
	qp_attr.ah_attr.grh.dgid = gid;
	qp_attr.ah_attr.grh.sgid_index = 0;
	qp_attr.ah_attr.dlid = attr.lid;
	qp_attr.ah_attr.sl = 0;
	qp_attr.ah_attr.src_path_bits = 0;
	qp_attr.ah_attr.port_num = 1;
	err = ibv_modify_qp(ibqp, &qp_attr, IBV_QP_STATE	      |
					    IBV_QP_AV		      |
					    IBV_QP_PATH_MTU	      |
					    IBV_QP_DEST_QPN	      |
					    IBV_QP_RQ_PSN	      |
					    IBV_QP_MAX_DEST_RD_ATOMIC |
					    IBV_QP_MIN_RNR_TIMER);
	if (err) {
		perror("failed to modify calc qp to RTR");
		goto clean_qp;
	}
	calc->log_chunk_size = 0;

	/* modify to RTS */
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.timeout = 14;
	qp_attr.retry_cnt = 7;
	qp_attr.rnr_retry = 7;
	qp_attr.sq_psn = 0;
	qp_attr.max_rd_atomic  = 1;
	err = ibv_modify_qp(ibqp, &qp_attr, IBV_QP_STATE     |
					    IBV_QP_TIMEOUT   |
					    IBV_QP_RETRY_CNT |
					    IBV_QP_RNR_RETRY |
					    IBV_QP_SQ_PSN    |
					    IBV_QP_MAX_QP_RD_ATOMIC);
	if (err) {
		perror("failed to modify calc qp to RTS");
		goto clean_qp;
	}

	return ibqp;

clean_qp:
	ibv_destroy_qp(ibqp);

	return NULL;
}

static void dereg_encode_matrix(struct mlx5_ec_calc *calc)
{
	ibv_dereg_mr(calc->mat_mr);
	free(calc->matrices);
	free(calc->mat);
}

static int reg_encode_matrix(struct mlx5_ec_calc *calc, uint8_t *matrix)
{
	int k = calc->k;
	int m = calc->m;
	int cols, mi, i, j, err, curr_cols, curr_m, offset;
	uint8_t *curr_mat;
	int last_m = MLX5_EC_LAST_COLS(m);

	cols = (m / MLX5_EC_NUM_OUTPUTS) * MLX5_EC_NUM_OUTPUTS +
		MLX5_EC_NOUTPUTS(last_m);
	/* mat holds the whole memory for all matrices */
	calc->mat = calloc(1, cols * k);
	if (!calc->mat) {
		fprintf(stderr, "failed to alloc calc matrix\n");
		return ENOMEM;
	}
	calc->matrices = calloc(calc->mult_num, sizeof(uint8_t *));
	if (!calc->matrices) {
		fprintf(stderr, "failed to alloc encode calc matrices\n");
		err = -ENOMEM;
		goto free_mat;
	}
	calc->mat_mr = ibv_reg_mr(calc->pd, calc->mat,
				  cols * k, IBV_ACCESS_LOCAL_WRITE);
	if (!calc->mat_mr) {
		fprintf(stderr, "failed to alloc calc encode matrix mr\n");
		err = errno;
		goto free_matrices;
	}
	for (mi = 0; mi < calc->mult_num; mi++) {
		curr_m = MLX5_EC_COLS(mi, calc->mult_num, m);
		offset = mi * MLX5_EC_NUM_OUTPUTS;
		/* add sizes of previous matrices */
		calc->matrices[mi] = calc->mat + offset * k;
		curr_mat = calc->matrices[mi];
		curr_cols = MLX5_EC_NOUTPUTS(curr_m);
		for (i = 0; i < k; i++)
			for (j = 0; j < curr_cols; j++) {
				/* 3 outputs, don't set HW format */
				if (j == 3 && curr_m == 3)
					continue;

				/* not i in right matrix */
				curr_mat[i * curr_cols + j] =
					matrix[i * m + j + offset];
				if (calc->w != 8)
					/* Crazy HW formatting, bit 5 is on */
					curr_mat[i * curr_cols + j] |= 0x10;
			}
	}

	return 0;

free_matrices:
	free(calc->matrices);
free_mat:
	free(calc->mat);

	return err;
}

struct ibv_mr *reg_umr(struct ibv_pd *pd, int num_blocks)
{
	struct ibv_exp_create_mr_in in;

	memset(&in, 0, sizeof(in));
	in.pd = pd;
	in.attr.create_flags = IBV_EXP_MR_INDIRECT_KLMS;
	in.attr.exp_access_flags = IBV_EXP_ACCESS_LOCAL_WRITE;
	in.attr.max_klm_list_size = align(num_blocks, 4);

	return mlx5_create_mr(&in);
}

static void dereg_pad_umr(struct mlx5_ec_calc *calc,
			   struct mlx5_ec_comp *comp)
{
	int i, m = MLX5_EC_PADDED_OUTPUTS(calc->m);

	for (i = 0; i < m; i++)
		mlx5_dereg_mr(comp->pad_mrs[i]);
}

static int reg_pad_umr(struct mlx5_ec_calc *calc,
			struct mlx5_ec_comp *comp)
{
	int i, j, m = MLX5_EC_PADDED_OUTPUTS(calc->m);

	for (i = 0; i < m; i++) {
		comp->pad_mrs[i] = reg_umr(calc->pd, 2);
		if (!comp->pad_mrs[i]) {
			fprintf(stderr, "calc %p failed to register padding umr\n",
				calc);
			goto dereg;
		}
	}
	return 0;
dereg:
	for (j = 0; j < i; j++)
		mlx5_dereg_mr(comp->pad_mrs[j]);

	return -EINVAL;
}

static void free_comps(struct mlx5_ec_calc *calc)
{
	int comp_num = calc->max_inflight_calcs;
	struct mlx5_ec_comp_pool *pool = &calc->comp_pool;
	int i;

	for (i = 0; i < comp_num; i++) {
		free(pool->comps[i].data);
		mlx5_dereg_mr(pool->comps[i].inumr);
		mlx5_dereg_mr(pool->comps[i].outumr);
		dereg_pad_umr(calc, &pool->comps[i]);
	}
	free(calc->comp_pool.comps);
}

static int alloc_comps(struct mlx5_ec_calc *calc)
{
	struct mlx5_ec_comp_pool *pool = &calc->comp_pool;
	int comp_num = calc->max_inflight_calcs;
	int i, j;

	INIT_LIST_HEAD(&pool->list);
	mlx5_lock_init(&pool->lock, 1, mlx5_get_locktype());

	pool->comps = calloc(comp_num, sizeof(*pool->comps));
	if (!pool->comps) {
		fprintf(stderr, "failed to allocate comps\n");
		return -ENOMEM;
	}

	for (i = 0; i < comp_num; i++) {
		pool->comps[i].inumr = reg_umr(calc->pd, calc->k << 1);
		if (!pool->comps[i].inumr) {
			fprintf(stderr, "calc %p failed to register inumr\n", calc);
			goto free_umrs;
		}
		pool->comps[i].outumr = reg_umr(calc->pd, calc->m);
		if (!pool->comps[i].outumr) {
			fprintf(stderr, "calc %p failed to register outumr\n", calc);
			goto free_inumr;
		}
		pool->comps[i].data = calloc(calc->k << 1,
				sizeof(struct ibv_mr));
		if (!pool->comps[i].data) {
			fprintf(stderr, "failed to alloc padding umr\n");
			goto free_outumr;
		}
		if (reg_pad_umr(calc, &pool->comps[i])) {
			fprintf(stderr, "calc %p failed to register padding umr\n",
				calc);
			goto free_data;
		}
		list_add_tail(&pool->comps[i].node, &pool->list);
	}
	return 0;

free_data:
	free(pool->comps[i].data);
free_outumr:
	mlx5_dereg_mr(pool->comps[i].outumr);
free_inumr:
	mlx5_dereg_mr(pool->comps[i].inumr);
free_umrs:
	for (j = 0 ; j < i ; j++) {
		list_del_init(&pool->comps[j].node);
		mlx5_dereg_mr(pool->comps[j].inumr);
		mlx5_dereg_mr(pool->comps[j].outumr);
		free(pool->comps[i].data);
	}
	free(pool->comps);

	return -ENOMEM;

}

static void free_multi_comps(struct mlx5_ec_calc *calc)
{
	int comp_num = calc->user_max_inflight_calcs;
	int i;

	for (i = 0; i < comp_num; i++)
		free(calc->multi_comp_pool.comps[i].data_update);

	free(calc->multi_comp_pool.comps);
}

static int alloc_multi_comps(struct mlx5_ec_calc *calc)
{
	struct mlx5_ec_multi_comp_pool *pool = &calc->multi_comp_pool;
	int comp_num = calc->user_max_inflight_calcs;
	int i, j;

	INIT_LIST_HEAD(&pool->list);
	mlx5_lock_init(&pool->lock, 1, mlx5_get_locktype());

	/* we have one multi_comp per user max_inflight_calcs */
	pool->comps = calloc(comp_num, sizeof(*pool->comps));
	if (!pool->comps) {
		fprintf(stderr, "failed to allocate multi comps\n");
		return -ENOMEM;
	}

	for (i = 0; i < comp_num; i++) {
		pool->comps[i].calc = calc;
		pool->comps[i].comp.done = mlx5_multi_done;
		pool->comps[i].data_update =
				calloc(calc->k, sizeof(struct ibv_sge));
		if (!pool->comps[i].data_update) {
			fprintf(stderr, "failed to allocate update data\n");
			goto free_comps;
		}
		pthread_mutex_init(&(pool->comps[i].mutex), NULL);
		list_add_tail(&pool->comps[i].node, &pool->list);
	}

	return 0;

free_comps:
	for (j = 0; j < i; j++) {
		list_del_init(&pool->comps[j].node);
		free(pool->comps[i].data_update);
	}
	free(pool->comps);

	return -ENOMEM;
}

static void free_matrices(struct mlx5_ec_calc *calc)
{
	struct mlx5_ec_mat_pool *pool = &calc->mat_pool;

	free(pool->matrices);
	ibv_dereg_mr(pool->mat_mr);
	free(pool->mat_buf);
}

static int alloc_matrices(struct mlx5_ec_calc *calc)
{
	struct mlx5_ec_mat_pool *pool = &calc->mat_pool;
	int mat_num = calc->max_inflight_calcs;
	int mat_size;
	int cols;
	int i, err;

	cols = MLX5_EC_NOUTPUTS(calc->m);
	mat_size = calc->k * cols;

	INIT_LIST_HEAD(&pool->list);
	mlx5_lock_init(&pool->lock, 1, mlx5_get_locktype());

	pool->mat_buf = calloc(mat_num, mat_size);
	if (!pool->mat_buf) {
		fprintf(stderr, "failed to allocate matrix buffer\n");
		return ENOMEM;
	}

	pool->mat_mr = ibv_reg_mr(calc->pd, pool->mat_buf,
				  mat_size * mat_num,
				  IBV_ACCESS_LOCAL_WRITE);
	if (!pool->mat_mr) {
		fprintf(stderr, "failed to alloc calc decode matrix mr\n");
		err = errno;
		goto err_mat_buf;
	}

	pool->matrices = calloc(mat_num, sizeof(*pool->matrices));
	if (!pool->matrices) {
		fprintf(stderr, "failed to allocate matrix bufs\n");
		err = ENOMEM;
		goto err_mat_mr;
	}

	for (i = 0; i < mat_num; i++) {
		struct mlx5_ec_mat *mat = &pool->matrices[i];

		mat->sge.lkey = pool->mat_mr->lkey;
		mat->sge.length = mat_size;
		mat->sge.addr = (uintptr_t)(pool->mat_buf + i * mat_size);
		list_add_tail(&mat->node, &pool->list);
	}

	return 0;

err_mat_mr:
	ibv_dereg_mr(pool->mat_mr);
err_mat_buf:
	free(pool->mat_buf);

	return err;
}

static void free_dump(struct mlx5_ec_calc *calc)
{
	ibv_dereg_mr(calc->dump_mr);
	free(calc->dump);
}

static int alloc_dump(struct mlx5_ec_calc *calc)
{
	int chunk_size = MLX5_CHUNK_SIZE(calc);
	int err;

	calc->dump = calloc(1, chunk_size);
	if (!calc->dump)
		return ENOMEM;

	calc->dump_mr = ibv_reg_mr(calc->pd, calc->dump,
	                           chunk_size, IBV_ACCESS_LOCAL_WRITE);
	if (!calc->dump_mr) {
		fprintf(stderr, "failed to alloc calc dump mr\n");
		err = errno;
		goto free_dump;
	}

	return 0;

free_dump:
	free(calc->dump);

	return err;
}

static int
ec_attr_sanity_checks(struct ibv_exp_ec_calc_init_attr *attr)
{
	if (attr->k <= 0 || attr->k >= 256) {
		fprintf(stderr, "Bad K arg (%d)\n", attr->k);
		return EINVAL;
	}

	if (attr->m <= 0 || attr->m >= 256) {
		fprintf(stderr, "Bad M arg (%d)\n", attr->m);
		return EINVAL;
	}

	if (attr->w != 1 && attr->w != 2 && attr->w != 4 && attr->w != 8) {
		fprintf(stderr, "bad W arg (%d)\n", attr->w);
		return EINVAL;
	}

	/* check that m + k <= 2^w */
	if (attr->m + attr->k > (1 << attr->w)) {
		fprintf(stderr, "Bad arguments. \
			Arguments K (%d), M (%d), W (%d), violate \
			K + M <= 2^W\n", attr->k, attr->m, attr->w);
		return EINVAL;
	}

	if (attr->max_data_sge != attr->k) {
		fprintf(stderr, "Unsupported max_data_sge (%d) != k (%d)\n",
			attr->max_data_sge, attr->k);
		return EINVAL;
	}

	if (attr->max_code_sge != attr->m) {
		fprintf(stderr, "Unsupported max_code_sge (%d) != m (%d)\n",
			attr->max_code_sge, attr->m);
		return EINVAL;
	}
	return 0;
}

struct ibv_exp_ec_calc *
mlx5_alloc_ec_calc(struct ibv_pd *pd,
		   struct ibv_exp_ec_calc_init_attr *attr)
{
	struct mlx5_ec_calc *calc;
	struct ibv_exp_ec_calc *ibcalc;
	void *status;
	int err;

	err = ec_attr_sanity_checks(attr);
	if (err) {
		errno = err;
		return NULL;
	}

	calc = calloc(1, sizeof(*calc));
	if (!calc) {
		fprintf(stderr, "failed to alloc calc\n");
		return NULL;
	}
	ibcalc = (struct ibv_exp_ec_calc *)&calc->ibcalc;

	calc->pd = ibcalc->pd = pd;
	/* calculate number of vector calcs operations needed for given m */
	calc->mult_num = MLX5_EC_MULT_NUM(attr->m);

	/* we need extra inflights for encode_send operation
	 * upper limit of m/4
	 */
	calc->user_max_inflight_calcs =
				attr->max_inflight_calcs + EC_POLL_BATCH;
	calc->max_inflight_calcs =
				calc->mult_num * calc->user_max_inflight_calcs;
	calc->k = attr->k;
	calc->m = attr->m;
	calc->w = attr->w;
	calc->polling = attr->polling;

	calc->channel = ibv_create_comp_channel(calc->pd->context);
	if (!calc->channel) {
		fprintf(stderr, "failed to alloc calc channel\n");
		goto free_calc;
	};

	calc->cq = ibv_create_cq(calc->pd->context,
				 calc->max_inflight_calcs * MLX5_EC_CQ_FACTOR,
				 NULL, calc->channel, attr->affinity_hint);
	if (!calc->cq) {
		fprintf(stderr, "failed to alloc calc cq\n");
		goto free_channel;
	};

	if (!calc->polling) {
		err = ibv_req_notify_cq(calc->cq, 0);
		if (err) {
			fprintf(stderr, "failed to req notify cq\n");
			goto free_cq;
		}

		err = pthread_create(&calc->ec_poller, NULL,
				     handle_comp_events, calc);
		if (err) {
			fprintf(stderr, "failed to create ec_poller\n");
			goto free_cq;
		}
	}

	err = reg_encode_matrix(calc, attr->encode_matrix);
	if (err)
		goto free_ec_poller;

	calc->qp = alloc_calc_qp(calc);
	if (!calc->qp)
		goto encode_matrix;

	err = alloc_matrices(calc);
	if (err)
		goto calc_qp;

	err = alloc_dump(calc);
	if (err)
		goto free_mat;

	err = alloc_comps(calc);
	if (err)
		goto free_dump;

	err = alloc_multi_comps(calc);
	if (err)
		goto free_comps;

	return ibcalc;

free_comps:
	free_comps(calc);
free_dump:
	free_dump(calc);
free_mat:
	free_matrices(calc);
calc_qp:
	ibv_destroy_qp(calc->qp);
encode_matrix:
	dereg_encode_matrix(calc);
free_ec_poller:
	if (!calc->polling) {
		calc->stop_ec_poller = 1;
		wmb();
		pthread_kill(calc->ec_poller, SIGINT);
		pthread_join(calc->ec_poller, &status);
	}
free_cq:
	ibv_destroy_cq(calc->cq);
free_channel:
	ibv_destroy_comp_channel(calc->channel);
free_calc:
	free(calc);

	return NULL;
}

void
mlx5_dealloc_ec_calc(struct ibv_exp_ec_calc *ec_calc)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);
	struct ibv_qp_attr qp_attr;
	void *status;
	int err;

	qp_attr.qp_state = IBV_QPS_ERR;
	err = ibv_modify_qp(calc->qp, &qp_attr, IBV_QP_STATE);
	if (err) {
		perror("failed to modify calc qp to ERR");
		return;
	}

	if (!calc->polling) {
		pthread_mutex_init(&calc->beacon_mutex, NULL);
		pthread_cond_init(&calc->beacon_cond, NULL);

		err = ec_post_recv(calc->qp, NULL, (void *)EC_BEACON_WRID);
		if (err) {
			perror("failed to post beacon\n");
			goto free;
		}

		pthread_mutex_lock(&calc->beacon_mutex);
		pthread_cond_wait(&calc->beacon_cond, &calc->beacon_mutex);
		pthread_mutex_unlock(&calc->beacon_mutex);
	}

free:
	free_multi_comps(calc);
	free_comps(calc);
	free_dump(calc);
	free_matrices(calc);
	ibv_destroy_qp(calc->qp);
	dereg_encode_matrix(calc);

	if (!calc->polling) {
		calc->stop_ec_poller = 1;
		wmb();
		pthread_kill(calc->ec_poller, SIGINT);
		pthread_join(calc->ec_poller, &status);
	}

	ibv_destroy_cq(calc->cq);
	ibv_destroy_comp_channel(calc->channel);
	free(calc);
}

static void
set_ec_umr_ctrl_seg(struct mlx5_ec_calc *calc, int nklms,
		    int pat, struct mlx5_wqe_umr_ctrl_seg *umr)
{
	memset(umr, 0, sizeof(*umr));

	umr->flags = MLX5_UMR_CTRL_INLINE;
	umr->klm_octowords = htons(align(nklms + pat, 4));
	umr->mkey_mask =  htonll(MLX5_MKEY_MASK_LEN		|
				 MLX5_MKEY_MASK_START_ADDR	|
				 MLX5_MKEY_MASK_KEY		|
				 MLX5_MKEY_MASK_FREE		|
				 MLX5_MKEY_MASK_LR		|
				 MLX5_MKEY_MASK_LW);
}

static void
set_ec_mkey_seg(struct mlx5_ec_calc *calc,
		struct ibv_sge *klms,
		int nklms,
		uint32_t umr_key,
		int pat,
		struct mlx5_mkey_seg *seg)
{
	uint32_t len = 0;
	int i;

	memset(seg, 0, sizeof(*seg));

	seg->flags = MLX5_PERM_LOCAL_READ  |
		     MLX5_PERM_LOCAL_WRITE |
		     MLX5_PERM_UMR_EN	   |
		     MLX5_ACCESS_MODE_KLM;
	seg->qpn_mkey7_0 = htonl(0xffffff00 | (umr_key & 0xff));
	seg->flags_pd = htonl(to_mpd(calc->pd)->pdn);
	seg->start_addr = htonll((uintptr_t)klms[0].addr);
	/* For pattern case all klms have same length */
	if (pat) {
		len = klms[0].length * nklms;
	} else {
		for (i = 0; i < nklms; i++)
			len += klms[i].length;
	}
	seg->len = htonll(len);
	seg->xlt_oct_size = htonl(align(nklms + pat, 4));
}

static inline void *
rewind_sq(struct mlx5_qp *qp, void **seg, int *size, int *inc)
{
	void *start = mlx5_get_send_wqe(qp, 0);

	*seg = start;
	*size += MLX5_SEND_WQE_BB / 16;
	*inc -= MLX5_SEND_WQE_BB;

	return start;
}

static void
set_ec_umr_pattern_ds(struct mlx5_ec_calc *calc,
		      struct ibv_sge *klms,
		      int nklms, int nrklms,
		      void **seg, int *size)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_seg_repeat_block *rb;
	struct mlx5_seg_repeat_ent *re;
	int set, i, inc_size;
	int chunk_size = min(klms[0].length, MLX5_CHUNK_SIZE(calc));

	inc_size = align(sizeof(*rb) + nrklms * sizeof(*re), MLX5_SEND_WQE_BB);

	rb = *seg;
	rb->const_0x400 = htonl(0x400);
	rb->reserved = 0;
	rb->num_ent = htons(nrklms);
	rb->repeat_count = htonl(DIV_ROUND_UP(klms[0].length * nrklms,
				 chunk_size * nrklms));
	rb->byte_count = htonl(chunk_size * nrklms);
	re = rb->entries;
	for (i = 0; i < nklms; i++, re++) {
		if (re == qp->gen_data.sqend)
			re = rewind_sq(qp, seg, size, &inc_size);

		re->va = htonll(klms[i].addr);
		re->byte_count = htons(chunk_size);
		re->stride = htons(chunk_size);
		re->memkey = htonl(klms[i].lkey);
	}

	/* 3 outputs, set last KLM to our dump lkey */
	if (nklms == 3) {
		if (re == qp->gen_data.sqend)
			re = rewind_sq(qp, seg, size, &inc_size);

		re->va = htonll((uintptr_t)calc->dump);
		re->byte_count = htons(chunk_size);
		re->stride = 0;
		re->memkey = htonl(calc->dump_mr->lkey);
		re++;
	}

	set = align((nrklms + 1), 4) - nrklms - 1;
	if (set)
		memset(re, 0, set * sizeof(*re));

	*seg += inc_size;
	*size += inc_size / 16;
	if (unlikely((*seg == qp->gen_data.sqend)))
		*seg = mlx5_get_send_wqe(qp, 0);
}

static void
set_ec_umr_klm_ds(struct mlx5_ec_calc *calc,
		  struct ibv_sge *klms,
		  int nklms,
		  void **seg, int *size)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_klm *klm;
	int set, i, inc_size;

	inc_size = align(nklms * sizeof(*klm), MLX5_SEND_WQE_BB);

	klm = *seg;
	for (i = 0; i < nklms; i++, klm++) {
		if (klm == qp->gen_data.sqend)
			klm = rewind_sq(qp, seg, size, &inc_size);

		klm->va = htonll(klms[i].addr);
		klm->byte_count = htonl(klms[i].length);
		klm->key = htonl(klms[i].lkey);
	}

	set = align(nklms, 4) - nklms;
	if (set)
		memset(klm, 0, set * sizeof(*klm));

	*seg += inc_size;
	*size += inc_size / 16;
	if (unlikely((*seg == qp->gen_data.sqend)))
		*seg = mlx5_get_send_wqe(qp, 0);
}

static void
post_ec_umr(struct mlx5_ec_calc *calc,
	    struct ibv_sge *klms,
	    int nklms,
	    int pattern,
	    uint32_t umr_key,
	    void **seg, int *size)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_wqe_ctrl_seg *ctrl;
	int nrklms = MLX5_EC_NOUTPUTS(nklms);

	ctrl = *seg;
	*seg += sizeof(*ctrl);
	*size = sizeof(*ctrl) / 16;
	if (unlikely((*seg == qp->gen_data.sqend)))
		*seg = mlx5_get_send_wqe(qp, 0);

	set_ec_umr_ctrl_seg(calc, nrklms, pattern, *seg);
	*seg += sizeof(struct mlx5_wqe_umr_ctrl_seg);
	*size += sizeof(struct mlx5_wqe_umr_ctrl_seg) / 16;
	if (unlikely((*seg == qp->gen_data.sqend)))
		*seg = mlx5_get_send_wqe(qp, 0);

	set_ec_mkey_seg(calc, klms, nrklms, umr_key, pattern, *seg);
	*seg += sizeof(struct mlx5_mkey_seg);
	*size += (sizeof(struct mlx5_mkey_seg) / 16);
	if (unlikely((*seg == qp->gen_data.sqend)))
		*seg = mlx5_get_send_wqe(qp, 0);

	if (pattern)
		set_ec_umr_pattern_ds(calc, klms, nklms, nrklms, seg, size);
	else
		set_ec_umr_klm_ds(calc, klms, nklms, seg, size);

	set_ctrl_seg((uint32_t *)ctrl, &qp->ctrl_seg,
		     MLX5_OPCODE_UMR, qp->gen_data.scur_post, 0, *size,
		     0, htonl(umr_key));

	qp->gen_data.fm_cache = MLX5_FENCE_MODE_INITIATOR_SMALL;
}

static void
post_ec_vec_calc(struct mlx5_ec_calc *calc,
		 struct ibv_sge *klm,
		 int block_size,
		 int nvecs,
		 int noutputs,
		 void *matrix_addr,
		 uint32_t matrix_key,
		 int signal,
		 void *seg, int *size)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_wqe_ctrl_seg *ctrl;
	struct mlx5_vec_calc_seg *vc;
	uint8_t fm_ce_se;
	int i;

	ctrl = seg;
	vc = seg + sizeof(*ctrl);

	memset(vc, 0, sizeof(*vc));
	for (i = 0; i < noutputs; i++)
		vc->calc_op[i] = MLX5_CALC_OP_XOR;

	vc->mat_le_tag_cs = MLX5_CALC_MATRIX | calc->log_chunk_size;
	if (calc->w == 8)
		vc->mat_le_tag_cs |= MLX5_CALC_MATRIX_8BIT;

	vc->vec_count = (uint8_t)nvecs;
	vc->cm_lkey = htonl(matrix_key);
	vc->cm_addr = htonll((uintptr_t)matrix_addr);
	vc->vec_size = htonl((block_size >> 2) << 2);
	vc->vec_lkey = htonl(klm->lkey);
	vc->vec_addr = htonll(klm->addr);

	*size = (sizeof(*ctrl) + sizeof(*vc)) / 16;

	fm_ce_se = qp->gen_data.fm_cache;
	if (signal)
		fm_ce_se |= MLX5_WQE_CTRL_CQ_UPDATE;

	set_ctrl_seg((uint32_t *)ctrl, &qp->ctrl_seg,
		     MLX5_OPCODE_SEND, qp->gen_data.scur_post, 0xff, *size,
		     fm_ce_se, 0);

	qp->gen_data.fm_cache = 0;
}

static int ec_post_recv(struct ibv_qp *qp,
			struct ibv_sge *sge,
			struct mlx5_ec_comp *comp)
{
	struct ibv_recv_wr wr, *bad_wr;

	wr.next = NULL;
	wr.wr_id = (uintptr_t)comp;
	wr.sg_list = sge;
	if (likely((uintptr_t)sge))
		wr.num_sge = 1;
	else
		wr.num_sge = 0;
#ifdef USE_MPRUD
	return mlx5_post_recv(qp, &wr, &bad_wr, 1);
#else
	return mlx5_post_recv(qp, &wr, &bad_wr);
#endif
}

static unsigned begin_wqe(struct mlx5_qp *qp, void **seg)
{
	int idx;

	idx = qp->gen_data.scur_post & (qp->sq.wqe_cnt - 1);
	*seg = mlx5_get_send_wqe(qp, idx);

	return idx;
}

static void finish_wqe(struct mlx5_qp *qp, int idx,
		       int size, void *wrid)
{
	qp->sq.wrid[idx] = (uintptr_t)wrid;
	qp->gen_data.wqe_head[idx] = qp->sq.head + 1;
	qp->gen_data.scur_post += DIV_ROUND_UP(size * 16, MLX5_SEND_WQE_BB);
}

static int ec_post_pad_umrs(struct mlx5_ec_calc *calc,
			    struct mlx5_ec_comp *comp,
			    struct ibv_exp_ec_mem *ec_mem,
			    int padding,
			    unsigned int *idx,
			    void *seg,
			    int *size)
{
	int i, j;
	int m = ec_mem->num_code_sge;
	struct ibv_sge padded_klms[m << 1];
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct ibv_sge *code = ec_mem->code_blocks;

	if (likely(!padding))
		return 0;

	for (i = 0; i < m; i++) {
		j =  i << 1;
		padded_klms[j].addr = code[i].addr;
		padded_klms[j].lkey = code[i].lkey;
		padded_klms[j].length = code[i].length;
		padded_klms[j + 1].addr = (uintptr_t)calc->dump;
		padded_klms[j + 1].lkey = calc->dump_mr->lkey;
		padded_klms[j + 1].length = padding;
		*idx = begin_wqe(qp, &seg);
		post_ec_umr(calc, padded_klms + j, 2, 0,
				comp->pad_mrs[i]->lkey, &seg, size);
		finish_wqe(qp, *idx, *size, NULL);
	}
	return m;
}

static int mlx5_set_encode_code(struct mlx5_ec_calc *calc,
				struct mlx5_ec_comp *comp,
				struct ibv_exp_ec_mem *ec_mem,
				struct ibv_sge *klms,
				struct ibv_sge *out,
				struct ibv_sge **out_ptr,
				int padding)
{
	int i;
	int m = ec_mem->num_code_sge;
	struct ibv_sge *code = ec_mem->code_blocks;

	/* Single output, just point to it */
	if (m == 1 && !padding) {
		*out_ptr = code;
		goto out;
	}

	for (i = 0; i < m; i++) {
		if (code[i].length != ec_mem->block_size) {
			fprintf(stderr, "Unsupported code_block[%d] length %d\n",
				i, klms[i].length);
			return EINVAL;
		}
		klms[i].addr = code[i].addr;
		klms[i].length = code[i].length + padding;
		if (unlikely(padding))
			klms[i].lkey = comp->pad_mrs[i]->lkey;
		else
			klms[i].lkey = code[i].lkey;
	}

	/* Take care of 3 outputs dumping */
	if (m == 3) {
		klms[3].addr = (uintptr_t)calc->dump;
		klms[3].lkey = calc->dump_mr->lkey;
		klms[3].length = ec_mem->block_size + padding;
	}

	out->addr = klms[0].addr;
	out->length = (ec_mem->block_size + padding) * MLX5_EC_NOUTPUTS(m);
	out->lkey = comp->outumr->lkey;
	*out_ptr = out;
out:
	return 0;
}

static inline int is_contig_sge(struct ibv_sge *data, int i)
{
	return !i || ((data[i].lkey == data[i - 1].lkey) &&
		(data[i].addr == data[i - 1].addr + data[i - 1].length));
}

static int mlx5_set_encode_data(struct mlx5_ec_calc *calc,
				struct mlx5_ec_comp *comp,
				struct ibv_exp_ec_mem *ec_mem,
				struct ibv_sge *in,
				int *contig,
				struct ibv_sge **set_data,
				int padding)
{
	uint32_t lkey;
	struct ibv_sge *data = ec_mem->data_blocks;
	int k = ec_mem->num_data_sge;
	int i, j;

	if (unlikely(padding)) {
		*contig = 0;
		*set_data = comp->data;
		lkey = comp->inumr->lkey;
	} else {
		*contig = 1;
		*set_data = data;
		lkey = data[0].lkey;
	}

	for (i = 0; i < k; i++) {
		if (data[i].length != ec_mem->block_size) {
			fprintf(stderr, "Unsupported data_block[%d] length %d\n",
				i, ec_mem->data_blocks[i].length);
			return EINVAL;
		}

		if (unlikely(*contig && !is_contig_sge(data, i))) {
			*contig = 0;
			lkey = comp->inumr->lkey;
		}

		if (unlikely(padding)) {
			j = i << 1;
			comp->data[j].length = ec_mem->block_size;
			comp->data[j].addr = data[i].addr;
			comp->data[j].lkey = data[i].lkey;
			comp->data[j + 1].length = padding;
			comp->data[j + 1].addr = (uintptr_t)calc->dump;
			comp->data[j + 1].lkey = calc->dump_mr->lkey;
		}
	}

	in->addr = data[0].addr;
	in->length = (ec_mem->block_size + padding) * k;
	in->lkey = lkey;

	return 0;
}

static int __mlx5_ec_encode_async(struct mlx5_ec_calc *calc,
				  int k, int m,
				  uint8_t *mat, uint32_t mat_lkey,
				  struct ibv_exp_ec_mem *ec_mem,
				  struct ibv_exp_ec_comp *ec_comp,
				  struct mlx5_ec_mat *ec_mat)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_ec_comp *comp;
	struct ibv_sge klms[MLX5_EC_NUM_OUTPUTS];
	struct ibv_sge in, out, *out_ptr = NULL;
	void *uninitialized_var(seg);
	unsigned idx;
	int size, err, blocks, contig = 0, wqe_count = 0;
	struct ibv_sge *data = NULL;
	int chunk_size = MLX5_CHUNK_SIZE(calc);
	int padding = 0;

	if (ec_mem->block_size % chunk_size != 0)
		padding = chunk_size - ec_mem->block_size % chunk_size;

	comp = mlx5_get_ec_comp(calc, ec_mat, ec_comp);
	if (unlikely(!comp)) {
		fprintf(stderr, "Failed to get comp from pool. \
				Do not activate more then %d inflight calculations \
				on this calc context.\n",
				calc->user_max_inflight_calcs);
		err = -EOVERFLOW;
		goto error;
	}

	err = mlx5_set_encode_code(calc, comp, ec_mem, klms,
			&out, &out_ptr, padding);
	if (unlikely(err))
		goto comp_error;

	err = mlx5_set_encode_data(calc, comp, ec_mem, &in,
			&contig, &data, padding);
	if (unlikely(err))
		goto comp_error;

	/* post recv for calc SEND */
	err = ec_post_recv((struct ibv_qp *)&qp->verbs_qp, out_ptr, comp);
	if (unlikely(err)) {
		fprintf(stderr, "failed to post recv calc\n");
		goto comp_error;
	}

	wqe_count += ec_post_pad_umrs(calc, comp, ec_mem, padding,
			&idx, seg, &size);

	if (m > 1 || padding) {
		/* post pattern KLM - non-signaled */
		idx = begin_wqe(qp, &seg);
		post_ec_umr(calc, klms, m, 1, comp->outumr->lkey, &seg, &size);
		finish_wqe(qp, idx, size, NULL);
		wqe_count++;
	}

	if (!contig) {
		/* post UMR of input - non-signaled */
		idx = begin_wqe(qp, &seg);
		blocks = padding ? k << 1 : k;
		post_ec_umr(calc, data, blocks, 0,
			    comp->inumr->lkey, &seg, &size);
		finish_wqe(qp, idx, size, NULL);
		wqe_count++;
	}

	/* post vec_calc SEND - non-signaled */
	idx = begin_wqe(qp, &seg);
	post_ec_vec_calc(calc, &in, ec_mem->block_size + padding,
			 k, m, mat, mat_lkey,
			 0, seg, &size);
	finish_wqe(qp, idx, size, NULL);
	wqe_count++;

	/* ring the DB */
	qp->sq.head += wqe_count;
	__ring_db(qp, qp->gen_data.bf->db_method,
		  qp->gen_data.scur_post & 0xffff,
		  seg, (size + 3) / 4);

	calc->cq_count += 1;

	return 0;

comp_error:
	mlx5_put_ec_comp(calc, comp);
error:
	errno = err;

	return err;
}

/*
*  Check that matrix and vectors dimensions are consistent
 */
static int check_sge(struct mlx5_ec_calc *calc,
		      struct ibv_exp_ec_mem *ec_mem)
{
	if (unlikely(ec_mem->num_data_sge != calc->k)) {
		fprintf(stderr, "Unsupported num_data_sge %d != %d\n",
			ec_mem->num_data_sge, calc->k);
		return -EINVAL;
	}

	if (unlikely(ec_mem->num_code_sge != calc->m)) {
		fprintf(stderr, "Unsupported num_code_sge %d != %d\n",
			ec_mem->num_code_sge, calc->m);
		return -EINVAL;
	}

	return 0;
}

static inline void
mlx5_multi_comp_set_status_check_done(struct mlx5_ec_multi_comp *comp,
				      int status)
{
	struct ibv_exp_ec_comp *ec_comp;

	/* critical area that changes status and counter */
	pthread_mutex_lock(&comp->mutex);

	ec_comp = comp->orig_comp;
	/* remember failure */
	if (status && ec_comp &&
	    ec_comp->status == IBV_EXP_EC_CALC_SUCCESS)
		ec_comp->status = IBV_EXP_EC_CALC_FAIL;

	comp->counter--;
	if (comp->counter == 0) {
		pthread_mutex_unlock(&comp->mutex);
		mlx5_put_ec_multi_comp(comp->calc, comp);
		if (ec_comp)
			ec_comp->done(ec_comp);
		return;
	}

	pthread_mutex_unlock(&comp->mutex);
}

static void
mlx5_multi_done(struct ibv_exp_ec_comp *comp)
{
	struct mlx5_ec_multi_comp *def_comp = to_multicomp(comp);

	mlx5_multi_comp_set_status_check_done(def_comp, comp->status);
}

int mlx5_ec_encode_async_big_m(struct mlx5_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       struct ibv_exp_ec_comp *ec_comp)
{
	int ret = 0, i;
	struct ibv_exp_ec_mem curr_ec_mem;
	uint8_t *curr_mat;
	struct mlx5_ec_multi_comp *def_comp;

	def_comp = mlx5_get_ec_multi_comp(calc, ec_comp, calc->mult_num);
	if (unlikely(!def_comp)) {
		fprintf(stderr, "Failed to get multi comp from pool. \
			Do not activate more then %d \
			inflight calculations on this calc context.\n",
			calc->user_max_inflight_calcs);
		return -EOVERFLOW;
	}

	/* data blocks are the same */
	curr_ec_mem.data_blocks = ec_mem->data_blocks;
	curr_ec_mem.num_data_sge = ec_mem->num_data_sge;
	curr_ec_mem.block_size = ec_mem->block_size;

	for (i = 0; i < calc->mult_num; i++) {
		/* take suitable code blocks */
		curr_ec_mem.code_blocks = ec_mem->code_blocks +
						i * MLX5_EC_NUM_OUTPUTS;
		curr_ec_mem.num_code_sge = MLX5_EC_COLS(i,
							calc->mult_num,
							ec_mem->num_code_sge);
		curr_mat = calc->matrices[i];
		ret = __mlx5_ec_encode_async(calc, calc->k,
					     curr_ec_mem.num_code_sge, curr_mat,
					     calc->mat_mr->lkey, &curr_ec_mem,
					     &def_comp->comp, NULL);
		if (ret)
			mlx5_multi_comp_set_status_check_done(def_comp, ret);
	}
	return ret;
}

int mlx5_ec_encode_async(struct ibv_exp_ec_calc *ec_calc,
			 struct ibv_exp_ec_mem *ec_mem,
			 struct ibv_exp_ec_comp *ec_comp)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);
	struct mlx5_qp *qp = to_mqp(calc->qp);
	int ret;

	ret = check_sge(calc, ec_mem);
	if (ret < 0)
		return ret;

	mlx5_lock(&qp->sq.lock);
	if (calc->m <= MLX5_EC_NUM_OUTPUTS)
		ret = __mlx5_ec_encode_async(calc, calc->k, calc->m, calc->mat,
					     calc->mat_mr->lkey, ec_mem,
					     ec_comp, NULL);
	else
		ret = mlx5_ec_encode_async_big_m(calc, ec_mem, ec_comp);
	mlx5_unlock(&qp->sq.lock);

	return ret;
}

/*
 * Fail if update matrix is of bigger dimension then encode matrix
 * num - return value of number of updated data blocks,
 * function returns 0 for parameters that don't suit update
 * function return 1 for parameters that suit update
 */
static int check_update_params(int k, int m, uint8_t *data_updates, int *num)
{
	int i, raws, num_updates = 0;

	for (i = 0; i < k; i++)
		if (data_updates[i])
			++num_updates;
	*num = num_updates;
	raws = m + 2 * num_updates;
	if (raws >= k)
		return 0;
	return 1;
}

static int __mlx5_ec_update_async(struct mlx5_ec_calc *calc,
				  struct ibv_exp_ec_mem *ec_mem,
				  uint8_t *data_updates,
				  uint8_t *code_updates,
				  struct ibv_exp_ec_comp *ec_comp,
				  int code_start_idx,
				  int code_end_idx)
{
	int ret;
	struct mlx5_ec_mat *update_mat;

	/* Get update matrix */
	update_mat = mlx5_get_ec_update_mat(calc, ec_mem,
					    data_updates, code_updates,
					    code_start_idx, code_end_idx);
	if (!update_mat) {
		fprintf(stderr, "Failed to get matrix from pool\n");
		return -EINVAL;
	}

	/* Get new code */
	ret = __mlx5_ec_encode_async(calc, ec_mem->num_data_sge,
				     ec_mem->num_code_sge,
				     (uint8_t *)(uintptr_t)update_mat->sge.addr,
				     update_mat->sge.lkey,
				     ec_mem, ec_comp, update_mat);

	return ret;
}

int mlx5_ec_update_async_big_m(struct mlx5_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       uint8_t *data_updates,
			       uint8_t *code_updates,
			       struct ibv_exp_ec_comp *ec_comp,
			       int num_updated_data)
{
	int ret = 0, i, j = 0, total = 0;
	int start_idx = 0;
	int end_idx = calc->m - 1;
	int num_updated_codes = ec_mem->num_code_sge;
	struct ibv_exp_ec_mem curr_ec_mem;
	struct mlx5_ec_multi_comp *def_comp;
	int num_calcs = MLX5_EC_MULT_NUM(num_updated_codes);
	int curr_codes = 0;

	def_comp = mlx5_get_ec_multi_comp(calc, ec_comp, num_calcs);
	if (unlikely(!def_comp)) {
		fprintf(stderr, "Failed to get multi comp from pool. \
			Do not activate more then %d \
			inflight calculations on this calc context.\n",
			calc->user_max_inflight_calcs);
		return -EOVERFLOW;
	}
	curr_ec_mem.block_size = ec_mem->block_size;
	/* code blocks point at the same code blocks at the beginning */
	curr_ec_mem.code_blocks = ec_mem->code_blocks;

	/*
	 * go over original code updates and look each time at a different
	 * section of updated blocks.
	 * start_idx and end_idx define current section.
	 */
	for (i = 0; i < calc->m && total < num_updated_codes; i++) {
		if (code_updates[i]) {
			def_comp->data_update[j] = ec_mem->data_blocks[total];
			j++;
			total++;
		}
		if (j == MLX5_EC_NUM_OUTPUTS || total == num_updated_codes) {
			memcpy(def_comp->data_update + j,
			       ec_mem->data_blocks + num_updated_codes,
			       sizeof(struct ibv_sge) * 2 * num_updated_data);
			end_idx = i;
			/* current number of codes sent for calculation */
			curr_codes = j;
			curr_ec_mem.num_code_sge = curr_codes;
			curr_ec_mem.data_blocks = def_comp->data_update;
			curr_ec_mem.num_data_sge = curr_codes +
						   2 * num_updated_data;

			ret = __mlx5_ec_update_async(calc,
						     &curr_ec_mem,
						     data_updates,
						     code_updates,
						     &def_comp->comp,
						     start_idx,
						     end_idx);
			if (ret)
				mlx5_multi_comp_set_status_check_done(def_comp,
								      ret);
			/* for the next calculation */
			start_idx = i + 1;
			curr_ec_mem.code_blocks = curr_ec_mem.code_blocks +
						  curr_codes;
			j = 0;
		}
	}
	return ret;
}

int mlx5_ec_update_async(struct ibv_exp_ec_calc *ec_calc,
			 struct ibv_exp_ec_mem *ec_mem,
			 uint8_t *data_updates,
			 uint8_t *code_updates,
			 struct ibv_exp_ec_comp *ec_comp)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);
	struct mlx5_qp *qp = to_mqp(calc->qp);
	int ret, num_updates = 0;

	/* Check that update is worth an effort */
	if (!check_update_params(calc->k, calc->m,
				 data_updates, &num_updates)) {
		fprintf(stderr, "Update not supported: encode preferred\n");
		return -EINVAL;
	}


	mlx5_lock(&qp->sq.lock);
	if (ec_mem->num_code_sge <= MLX5_EC_NUM_OUTPUTS)
		ret = __mlx5_ec_update_async(calc, ec_mem, data_updates,
					     code_updates, ec_comp,
					     0, calc->m - 1);
	else
		ret = mlx5_ec_update_async_big_m(calc, ec_mem, data_updates,
						 code_updates, ec_comp,
						 num_updates);
	mlx5_unlock(&qp->sq.lock);

	return ret;
}

static struct ibv_sge *get_block_for_index(struct mlx5_ec_calc *calc,
		struct ibv_exp_ec_mem *ec_mem,
		struct ibv_sge *data,
		struct ibv_sge *code,
		int i)
{
	if (i < calc->k) {
		if (data[i].length != ec_mem->block_size) {
			fprintf(stderr, "Unsupported data_block[%d] length %d\n",
				i, data[i].length);
			return NULL;
		}
		return data + i;
	} else if (i < calc->k + calc->m) {
		if (code[i - calc->k].length != ec_mem->block_size) {
			fprintf(stderr, "Unsupported code_block[%d] length %d\n",
				i - calc->k, code[i - calc->k].length);
			return NULL;
		}
		return code + (i - calc->k);
	}
	fprintf(stderr, "bad erasure %d\n", i);
	return NULL;
}

static int set_decode_klms(struct mlx5_ec_calc *calc,
			   struct mlx5_ec_comp *comp,
			   struct ibv_exp_ec_mem *ec_mem,
			   uint8_t *erasures,
			   struct ibv_sge *in,
			   struct ibv_sge *iklms,
			   int *in_num,
			   struct ibv_sge *out,
			   struct ibv_sge *oklms,
			   int *out_num,
			   struct ibv_sge *padded_klms,
			   int start_idx,
			   int end_idx,
			   int padding)
{
	struct ibv_sge *data = ec_mem->data_blocks;
	struct ibv_sge *code = ec_mem->code_blocks;
	struct ibv_sge *current_block;
	int i, j, k = 0, m = 0;

	/* XXX: This is just to make the compiler happy */
	iklms[0].addr = 0;

	for (i = 0; i < calc->k + calc->m; i++) {
		/*
		 * here we map the erased blocks.
		 * look only in the given range between start_idx and end_idx.
		 */
		if (erasures[i] && (i >= start_idx && i <= end_idx)) {
			current_block = get_block_for_index(calc, ec_mem,
					data, code, i);
			if (!current_block)
				return EINVAL;

			oklms[m].length = current_block->length + padding;
			if (unlikely(padding)) {
				j =  m << 1;
				padded_klms[j].addr = current_block->addr;
				padded_klms[j].lkey = current_block->lkey;
				padded_klms[j].length = current_block->length;
				padded_klms[j + 1].addr = (uintptr_t)calc->dump;
				padded_klms[j + 1].lkey = calc->dump_mr->lkey;
				padded_klms[j + 1].length = padding;
				oklms[m].addr = current_block->addr;
				oklms[m].lkey = comp->pad_mrs[m]->lkey;
			} else {
				oklms[m].lkey = current_block->lkey;
				oklms[m].addr = current_block->addr;
			}
			m++;

			if (unlikely(m > MLX5_EC_NUM_OUTPUTS)) {
				fprintf(stderr, "more than %d erasures \
					are not supported\n",
					MLX5_EC_NUM_OUTPUTS);
				return EINVAL;
			}
		} else if (!erasures[i] && k < calc->k) {
			/*
			 * here we want to map all the blocks that can work as
			 * k data blocks.
			 */
			current_block = get_block_for_index(calc, ec_mem,
					data, code, i);
			if (!current_block)
				return EINVAL;

			if (unlikely(padding)) {
				j = k << 1;
				iklms[j].length = current_block->length;
				iklms[j].addr = current_block->addr;
				iklms[j].lkey = current_block->lkey;
				iklms[j + 1].length = padding;
				iklms[j + 1].addr = (uintptr_t)calc->dump;
				iklms[j + 1].lkey = calc->dump_mr->lkey;
			} else {
				iklms[k].length = current_block->length;
				iklms[k].addr = current_block->addr;
				iklms[k].lkey = current_block->lkey;
			}
			k++;
		}
	}

	in->lkey = comp->inumr->lkey;
	in->addr = iklms[0].addr;
	in->length = (ec_mem->block_size + padding) * k;
	*in_num = k;

	if (m > 1 || padding)
		out->lkey = comp->outumr->lkey;
	else
		out->lkey = oklms[0].lkey;
	out->addr = oklms[0].addr;
	out->length = (ec_mem->block_size + padding) * MLX5_EC_NOUTPUTS(m);
	*out_num = m;

	return 0;
}

static int __mlx5_ec_decode_async(struct mlx5_ec_calc *calc,
			 struct ibv_exp_ec_mem *ec_mem,
			 uint8_t *erasures,
			 uint8_t *decode_matrix,
			 struct ibv_exp_ec_comp *ec_comp,
			 int num_mat_cols,
			 int curr_cols,
			 int mat_offset,
			 int start_idx,
			 int end_idx)
{
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct mlx5_ec_mat *decode;
	struct mlx5_ec_comp *comp;
	struct ibv_sge in_klms[calc->k << 1];
	struct ibv_sge out_klms[MLX5_EC_NUM_OUTPUTS];
	struct ibv_sge out, in;
	struct ibv_sge padded_klms[calc->m << 1];
	void *uninitialized_var(seg);
	unsigned idx;
	int err, size, blocks, i, j, k = 0, m = 0, wqe_count = 0;
	int chunk_size = MLX5_CHUNK_SIZE(calc);
	int padding = 0;

	if (ec_mem->block_size % chunk_size != 0)
		padding = chunk_size - ec_mem->block_size % chunk_size;

	decode = mlx5_get_ec_decode_mat(calc, decode_matrix,
					calc->k, num_mat_cols,
					curr_cols, mat_offset);

	comp = mlx5_get_ec_comp(calc, decode, ec_comp);
	if (unlikely(!comp)) {
		fprintf(stderr, "Failed to get comp from pool. \
				Do not activate more then %d inflight calculations \
				on this calc context.\n",
				calc->user_max_inflight_calcs);
		fprintf(stderr, "Failed to get comp from pool\n");
		err = -EOVERFLOW;
		goto mat_error;
	}

	err = set_decode_klms(calc, comp, ec_mem, erasures,
			      &in, in_klms, &k, &out, out_klms, &m,
				  padded_klms, start_idx, end_idx, padding);
	if (unlikely(err) || unlikely(m == 0))
		goto comp_error;

	/* post recv for calc SEND */
	err = ec_post_recv(calc->qp, &out, comp);
	if (unlikely(err)) {
		fprintf(stderr, "failed to post recv calc\n");
		goto comp_error;
	}

	if (unlikely(padding)) {
		for (i = 0; i < m; i++) {
			j =  i << 1;
			idx = begin_wqe(qp, &seg);
			post_ec_umr(calc, padded_klms + j, 2, 0,
					comp->pad_mrs[i]->lkey, &seg, &size);
			finish_wqe(qp, idx, size, NULL);
		}
		wqe_count += m;
	}

	if (m > 1 || padding) {
		/* post pattern KLM of output - non-signaled */
		idx = begin_wqe(qp, &seg);
		post_ec_umr(calc, out_klms, m, 1, comp->outumr->lkey, &seg, &size);
		finish_wqe(qp, idx, size, NULL);
		wqe_count++;
	}

	/* post UMR of input - non-signaled */
	idx = begin_wqe(qp, &seg);
	blocks = padding ? k << 1 : k;
	post_ec_umr(calc, in_klms, blocks, 0, comp->inumr->lkey, &seg, &size);
	finish_wqe(qp, idx, size, NULL);
	wqe_count++;

	/* post vec_calc SEND - non-signaled */
	idx = begin_wqe(qp, &seg);
	post_ec_vec_calc(calc, &in, ec_mem->block_size + padding, k, m,
			 (void *)(uintptr_t)decode->sge.addr, decode->sge.lkey,
			 0, seg, &size);
	finish_wqe(qp, idx, size, NULL);
	wqe_count++;

	/* ring the DB */
	qp->sq.head += wqe_count;
	__ring_db(qp, qp->gen_data.bf->db_method,
		  qp->gen_data.scur_post & 0xffff,
		  seg, (size + 3) / 4);

	calc->cq_count += 2;

	return 0;

comp_error:
	mlx5_put_ec_comp(calc, comp);
mat_error:
	mlx5_put_ec_mat(calc, decode);

	errno = err;

	return err;
}

int mlx5_ec_decode_async_big_m(struct mlx5_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       uint8_t *erasures,
			       uint8_t *decode_matrix,
			       struct ibv_exp_ec_comp *ec_comp,
			       int num_erasures)
{
	int ret = 0, i;
	int j = 0;
	int total = 0;
	int offset = 0;
	int start_idx = 0;
	int end_idx = calc->k + calc->m - 1;
	struct mlx5_ec_multi_comp *def_comp;
	int num_calcs = MLX5_EC_MULT_NUM(num_erasures);

	def_comp = mlx5_get_ec_multi_comp(calc, ec_comp, num_calcs);
	if (unlikely(!def_comp)) {
		fprintf(stderr, "Failed to get multi comp from pool. \
			Do not activate more then %d \
			inflight calculations on this calc context.\n",
			calc->user_max_inflight_calcs);
		return -EOVERFLOW;
	}

	/*
	 * go over original erasures and look each time at a different
	 * section of erased blocks.
	 * start_idx and end_idx define current section.
	 */
	for (i = 0; i < calc->k + calc->m && total < num_erasures; i++) {
		if (erasures[i]) {
			/* count how many erased blocks in current section */
			j++;
			/* count how many erased blocks so far */
			total++;
		}
		/* we have enough for another calculation */
		if (j == MLX5_EC_NUM_OUTPUTS || total == num_erasures) {
			/*
			 * remember the current index as the end
			 * of the current calculation.
			 */
			end_idx = i;
			ret = __mlx5_ec_decode_async(calc,
						     ec_mem,
						     erasures,
						     decode_matrix,
						     &def_comp->comp,
						     num_erasures,
						     j,
						     offset,
						     start_idx,
						     end_idx);
			if (ret)
				mlx5_multi_comp_set_status_check_done(def_comp,
								      ret);
			/* offset in columns of original matrix */
			offset = total;
			/* the next section starts */
			start_idx = i + 1;
			j = 0;
		}
	}
	return ret;
}

int mlx5_ec_decode_async(struct ibv_exp_ec_calc *ec_calc,
			 struct ibv_exp_ec_mem *ec_mem,
			 uint8_t *erasures,
			 uint8_t *decode_matrix,
			 struct ibv_exp_ec_comp *ec_comp)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);
	struct mlx5_qp *qp = to_mqp(calc->qp);
	int ret, i, num_erasures = 0;

	for (i = 0; i < calc->k + calc->m; i++)
		if (erasures[i])
			num_erasures++;

	mlx5_lock(&qp->sq.lock);
	if (num_erasures <= MLX5_EC_NUM_OUTPUTS)
		ret = __mlx5_ec_decode_async(calc, ec_mem, erasures,
					     decode_matrix, ec_comp,
					     num_erasures, num_erasures,
					     0, 0, calc->k + calc->m - 1);
	else
		ret = mlx5_ec_decode_async_big_m(calc, ec_mem, erasures,
						 decode_matrix, ec_comp,
						 num_erasures);
	mlx5_unlock(&qp->sq.lock);

	return ret;
}

static void
mlx5_sync_done(struct ibv_exp_ec_comp *comp)
{
	struct mlx5_ec_sync_comp *def_comp = to_mcomp(comp);

	pthread_mutex_lock(&def_comp->mutex);
	pthread_cond_signal(&def_comp->cond);
	pthread_mutex_unlock(&def_comp->mutex);
}

int mlx5_ec_encode_sync(struct ibv_exp_ec_calc *ec_calc,
			struct ibv_exp_ec_mem *ec_mem)
{
	int err;
	struct mlx5_ec_sync_comp def_comp = {
		.comp = {.done = mlx5_sync_done},
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
	};

	pthread_mutex_lock(&def_comp.mutex);
	err = mlx5_ec_encode_async(ec_calc, ec_mem, &def_comp.comp);
	if (err) {
		fprintf(stderr, "%s: failed\n", __func__);
		pthread_mutex_unlock(&def_comp.mutex);
		errno = err;
		return err;
	}

	pthread_cond_wait(&def_comp.cond, &def_comp.mutex);
	pthread_mutex_unlock(&def_comp.mutex);

	return (int)def_comp.comp.status;
}

int mlx5_ec_update_sync(struct ibv_exp_ec_calc *ec_calc,
			struct ibv_exp_ec_mem *ec_mem,
			uint8_t *data_updates,
			uint8_t *code_updates)
{
	int err;
	struct mlx5_ec_sync_comp def_comp = {
		.comp = {.done = mlx5_sync_done},
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
	};

	pthread_mutex_lock(&def_comp.mutex);
	err = mlx5_ec_update_async(ec_calc, ec_mem,
				   data_updates, code_updates,
				   &def_comp.comp);
	if (err) {
		fprintf(stderr, "%s: failed\n", __func__);
		pthread_mutex_unlock(&def_comp.mutex);
		errno = err;
		return err;
	}

	pthread_cond_wait(&def_comp.cond, &def_comp.mutex);
	pthread_mutex_unlock(&def_comp.mutex);

	return (int)def_comp.comp.status;
}

int mlx5_ec_decode_sync(struct ibv_exp_ec_calc *ec_calc,
			struct ibv_exp_ec_mem *ec_mem,
			uint8_t *erasures,
			uint8_t *decode_matrix)
{
	int err;
	struct mlx5_ec_sync_comp def_comp = {
		.comp = {.done = mlx5_sync_done},
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
	};

	pthread_mutex_lock(&def_comp.mutex);
	err = mlx5_ec_decode_async(ec_calc, ec_mem, erasures,
				   decode_matrix, &def_comp.comp);
	if (err) {
		fprintf(stderr, "%s: failed\n", __func__);
		pthread_mutex_unlock(&def_comp.mutex);
		errno = err;
		return err;
	}

	pthread_cond_wait(&def_comp.cond, &def_comp.mutex);
	pthread_mutex_unlock(&def_comp.mutex);

	return (int)def_comp.comp.status;
}

int mlx5_ec_poll(struct ibv_exp_ec_calc *ec_calc, int n)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);

	return ec_poll_cq(calc, n);
}

int mlx5_ec_encode_send(struct ibv_exp_ec_calc *ec_calc,
			struct ibv_exp_ec_mem *ec_mem,
			struct ibv_exp_ec_stripe *data_stripes,
			struct ibv_exp_ec_stripe *code_stripes)
{
	struct mlx5_ec_calc *calc = to_mcalc(ec_calc);
	struct mlx5_qp *qp = to_mqp(calc->qp);
	struct ibv_exp_send_wr wait_wr;
	struct ibv_exp_send_wr *bad_exp_wr;
	struct ibv_send_wr *bad_wr;
	int i, err;

	if (calc->polling) {
		fprintf(stderr, "encode_send is not supported in polling mode\n");
		return -EINVAL;
	}

	/* stripe data */
	for (i = 0; i < calc->k; i++) {
		err = ibv_post_send(data_stripes[i].qp,
				    data_stripes[i].wr, &bad_wr);
		if (unlikely(err)) {
			fprintf(stderr, "ibv_post_send(%d) failed\n", i);
			return err;
		}
	}

	/*
	 * In encode_send operation we don't indicate the user whether
	 * the calculation was over nor the completion was consumed,
	 * therefore we must poll the cq to ensure we have resources for
	 * the next calculation.
	 */
	if (ec_poll_cq(calc, 1)) {
		err = ibv_req_notify_cq(calc->cq, 0);
		if (unlikely(err)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
			return err;
		}
	}
	mlx5_lock(&qp->sq.lock);
	/* post async encode */
	err = __mlx5_ec_encode_async(calc, calc->k, calc->m, calc->mat,
				     calc->mat_mr->lkey, ec_mem, NULL, NULL);
	if (unlikely(err)) {
		fprintf(stderr, "mlx5_ec_encode_async failed\n");
		goto out;
	}

	/* stripe code */
	wait_wr.exp_opcode = IBV_EXP_WR_CQE_WAIT;
	wait_wr.exp_send_flags = IBV_EXP_SEND_WAIT_EN_LAST;
	wait_wr.num_sge = 0;
	wait_wr.sg_list = NULL;
	wait_wr.task.cqe_wait.cq = calc->cq;
	wait_wr.task.cqe_wait.cq_count = calc->cq_count;
	calc->cq_count = 0;
	wait_wr.next = NULL;
	for (i = 0; i < calc->m; i++) {
		wait_wr.wr_id = code_stripes[i].wr->wr_id;

		/*
		 * XXX: I can't post a wr chain because mlx5_exp_post_send
		 * assumes ibv_exp_send_wr which is different than ibv_send_wr.
		 * Bleh...
		 */
		err = ibv_exp_post_send(code_stripes[i].qp,
					&wait_wr, &bad_exp_wr);
		if (unlikely(err)) {
			fprintf(stderr, "ibv_exp_post_send(%d) failed err=%d\n",
				i, err);
			goto out;
		}
		wait_wr.task.cqe_wait.cq_count = 0;

		err = ibv_post_send(code_stripes[i].qp,
				    code_stripes[i].wr, &bad_wr);
		if (unlikely(err)) {
			fprintf(stderr, "ibv_post_send(%d) failed err=%d\n",
				i, err);
			goto out;
		}
	}

out:
	mlx5_unlock(&qp->sq.lock);

	return err;
}
