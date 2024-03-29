/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <search.h>
#include <linux/ip.h>

#include "ibverbs.h"
#include "infiniband/verbs_exp.h"
#ifndef NRESOLVE_NEIGH
#include "neigh.h"
#endif

// MPRUD by mingman
#ifdef USE_MPRUD
#include <infiniband/mprud.h>
#endif

int ibv_rate_to_mult(enum ibv_rate rate)
{
	switch (rate) {
	case IBV_RATE_2_5_GBPS: return  1;
	case IBV_RATE_5_GBPS:   return  2;
	case IBV_RATE_10_GBPS:  return  4;
	case IBV_RATE_20_GBPS:  return  8;
	case IBV_RATE_30_GBPS:  return 12;
	case IBV_RATE_40_GBPS:  return 16;
	case IBV_RATE_60_GBPS:  return 24;
	case IBV_RATE_80_GBPS:  return 32;
	case IBV_RATE_120_GBPS: return 48;
	case IBV_RATE_28_GBPS:  return 11;
	case IBV_RATE_50_GBPS:  return 20;
	case IBV_RATE_400_GBPS: return 160;
	case IBV_RATE_600_GBPS: return 240;
	default:           return -1;
	}
}

enum ibv_rate mult_to_ibv_rate(int mult)
{
	switch (mult) {
	case 1:  return IBV_RATE_2_5_GBPS;
	case 2:  return IBV_RATE_5_GBPS;
	case 4:  return IBV_RATE_10_GBPS;
	case 8:  return IBV_RATE_20_GBPS;
	case 12: return IBV_RATE_30_GBPS;
	case 16: return IBV_RATE_40_GBPS;
	case 24: return IBV_RATE_60_GBPS;
	case 32: return IBV_RATE_80_GBPS;
	case 48: return IBV_RATE_120_GBPS;
	case 11: return IBV_RATE_28_GBPS;
	case 20: return IBV_RATE_50_GBPS;
	case 160: return IBV_RATE_400_GBPS;
	case 240: return IBV_RATE_600_GBPS;
	default: return IBV_RATE_MAX;
	}
}

int ibv_rate_to_mbps(enum ibv_rate rate)
{
	switch (rate) {
	case IBV_RATE_2_5_GBPS: return 2500;
	case IBV_RATE_5_GBPS:   return 5000;
	case IBV_RATE_10_GBPS:  return 10000;
	case IBV_RATE_20_GBPS:  return 20000;
	case IBV_RATE_30_GBPS:  return 30000;
	case IBV_RATE_40_GBPS:  return 40000;
	case IBV_RATE_60_GBPS:  return 60000;
	case IBV_RATE_80_GBPS:  return 80000;
	case IBV_RATE_120_GBPS: return 120000;
	case IBV_RATE_14_GBPS:  return 14062;
	case IBV_RATE_56_GBPS:  return 56250;
	case IBV_RATE_112_GBPS: return 112500;
	case IBV_RATE_168_GBPS: return 168750;
	case IBV_RATE_25_GBPS:  return 25781;
	case IBV_RATE_100_GBPS: return 103125;
	case IBV_RATE_200_GBPS: return 206250;
	case IBV_RATE_300_GBPS: return 309375;
	case IBV_RATE_28_GBPS:  return 28125;
	case IBV_RATE_50_GBPS:  return 53125;
	case IBV_RATE_400_GBPS: return 425000;
	case IBV_RATE_600_GBPS: return 637500;
	default:               return -1;
	}
}

enum ibv_rate mbps_to_ibv_rate(int mbps)
{
	switch (mbps) {
	case 2500:   return IBV_RATE_2_5_GBPS;
	case 5000:   return IBV_RATE_5_GBPS;
	case 10000:  return IBV_RATE_10_GBPS;
	case 20000:  return IBV_RATE_20_GBPS;
	case 30000:  return IBV_RATE_30_GBPS;
	case 40000:  return IBV_RATE_40_GBPS;
	case 60000:  return IBV_RATE_60_GBPS;
	case 80000:  return IBV_RATE_80_GBPS;
	case 120000: return IBV_RATE_120_GBPS;
	case 14062:  return IBV_RATE_14_GBPS;
	case 56250:  return IBV_RATE_56_GBPS;
	case 112500: return IBV_RATE_112_GBPS;
	case 168750: return IBV_RATE_168_GBPS;
	case 25781:  return IBV_RATE_25_GBPS;
	case 103125: return IBV_RATE_100_GBPS;
	case 206250: return IBV_RATE_200_GBPS;
	case 309375: return IBV_RATE_300_GBPS;
	case 28125:  return IBV_RATE_28_GBPS;
	case 53125:  return IBV_RATE_50_GBPS;
	case 425000: return IBV_RATE_400_GBPS;
	case 637500: return IBV_RATE_600_GBPS;
	default:     return IBV_RATE_MAX;
	}
}

int __ibv_query_device(struct ibv_context *context,
		       struct ibv_device_attr *device_attr)
{
	return context->ops.query_device(context, device_attr);
}
default_symver(__ibv_query_device, ibv_query_device);

int __ibv_query_port(struct ibv_context *context, uint8_t port_num,
		     struct ibv_port_attr *port_attr)
{
	return context->ops.query_port(context, port_num, port_attr);
}
default_symver(__ibv_query_port, ibv_query_port);

int __ibv_query_gid(struct ibv_context *context, uint8_t port_num,
		    int index, union ibv_gid *gid)
{
	char name[24];
	char attr[41];
	uint16_t val;
	int i;

	snprintf(name, sizeof name, "ports/%d/gids/%d", port_num, index);

	if (ibv_read_sysfs_file(context->device->ibdev_path, name,
				attr, sizeof attr) < 0)
		return -1;

	for (i = 0; i < 8; ++i) {
		if (sscanf(attr + i * 5, "%hx", &val) != 1)
			return -1;
		gid->raw[i * 2    ] = val >> 8;
		gid->raw[i * 2 + 1] = val & 0xff;
	}

	return 0;
}
default_symver(__ibv_query_gid, ibv_query_gid);

int __ibv_query_pkey(struct ibv_context *context, uint8_t port_num,
		     int index, uint16_t *pkey)
{
	char name[24];
	char attr[8];
	uint16_t val;

	snprintf(name, sizeof name, "ports/%d/pkeys/%d", port_num, index);

	if (ibv_read_sysfs_file(context->device->ibdev_path, name,
				attr, sizeof attr) < 0)
		return -1;

	if (sscanf(attr, "%hx", &val) != 1)
		return -1;

	*pkey = htons(val);
	return 0;
}
default_symver(__ibv_query_pkey, ibv_query_pkey);

struct ibv_pd *__ibv_alloc_pd(struct ibv_context *context)
{
	struct ibv_pd *pd;

	pd = context->ops.alloc_pd(context);
	if (pd)
		pd->context = context;
#ifdef USE_MPRUD
  mprud_init_ctx();
#endif
	return pd;
}
default_symver(__ibv_alloc_pd, ibv_alloc_pd);

int __ibv_dealloc_pd(struct ibv_pd *pd)
{
  // MPRUD by mingman 
#ifdef USE_MPRUD
  mprud_destroy_resources();
#endif 
	return pd->context->ops.dealloc_pd(pd);
}
default_symver(__ibv_dealloc_pd, ibv_dealloc_pd);


struct ibv_mr *__ibv_reg_shared_mr(struct ibv_exp_reg_shared_mr_in *in)
{
	fprintf(stderr, "ibv_exp_reg_shared_mr is not supported\n");

	errno = ENOSYS;
	return NULL;
}

static int is_shared_mr(uint64_t access_flags)
{
	/* We should check whether IBV_EXP_ACCESS_SHARED_MR_USER_READ or
	 * other shared bits were turned on.
	 */
	return !!(access_flags & (IBV_EXP_ACCESS_SHARED_MR_USER_READ |
				IBV_EXP_ACCESS_SHARED_MR_USER_WRITE |
				IBV_EXP_ACCESS_SHARED_MR_GROUP_READ |
				IBV_EXP_ACCESS_SHARED_MR_GROUP_WRITE |
				IBV_EXP_ACCESS_SHARED_MR_OTHER_READ |
				IBV_EXP_ACCESS_SHARED_MR_OTHER_WRITE));
}

struct ibv_mr *__ibv_common_reg_mr(struct ibv_exp_reg_mr_in *in,
				   struct verbs_context_exp *context_exp)
{
	struct ibv_mr *mr;
	int is_contig;
	int is_odp;
	int is_pa;
	int is_dm;
  
  // MPRUD passes here 
	if (is_shared_mr(in->exp_access)) {
		fprintf(stderr, "shared_mr flags are not supported\n");
		errno = EOPNOTSUPP;
		return NULL;
	}

  // MPRUD: If IBV_EXP_ACCESS_ALLOCATE_MR flag is set,
  // just set address & length only.
  // then it will retrieve buffer. (no malloc)
	if ((in->exp_access & IBV_EXP_ACCESS_ALLOCATE_MR) && in->addr != NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((in->comp_mask & IBV_EXP_REG_MR_DM) && !in->dm) {
		errno = EINVAL;
		return NULL;
	}

	is_dm = !!(in->comp_mask & IBV_EXP_REG_MR_DM);

	is_contig = !!((in->exp_access & IBV_EXP_ACCESS_ALLOCATE_MR) ||
		     ((in->comp_mask & IBV_EXP_REG_MR_CREATE_FLAGS) &&
		     (in->create_flags & IBV_EXP_REG_MR_CREATE_CONTIG)));

	is_odp = !!(in->exp_access & IBV_EXP_ACCESS_ON_DEMAND);
	is_pa = !!(in->exp_access & IBV_EXP_ACCESS_PHYSICAL_ADDR);
	/*
	 * Fork support for contig or dm is handled by the provider.
	 * For ODP no special code is needed.
	 * Physical MR is not in the process address space, and therefore it is
	 * not affected by fork.
	 */
	if (!is_odp && !is_contig && !is_pa && !is_dm) {
		if (ibv_dontfork_range(in->addr, in->length))
			return NULL;
	}
	if (context_exp){
    // MPRUD passes here
		mr = context_exp->drv_exp_reg_mr(in);
  }
	else{
		mr = in->pd->context->ops.reg_mr(in->pd, in->addr, in->length,
						 in->exp_access);
  }
	if (mr) {
    // MPRUD passes here
		mr->context = in->pd->context;
		mr->pd      = in->pd;
		if (in->exp_access & IBV_EXP_ACCESS_ALLOCATE_MR)
			;
		/* In case that internal allocator was used
		     addr already set internally
		*/
		else if (!(in->exp_access & IBV_EXP_ACCESS_RELAXED))
			mr->addr    = in->addr;
		if (!(in->exp_access & IBV_EXP_ACCESS_RELAXED))
			mr->length  = in->length;
	} else if (!is_odp && !is_contig && !is_pa) {
		ibv_dofork_range(in->addr, in->length);
	}

	return mr;
}

struct ibv_mr *__ibv_exp_reg_mr(struct ibv_exp_reg_mr_in *in)
{
	struct verbs_context_exp *ctx = verbs_get_exp_ctx(in->pd->context);
  if (!ctx->drv_exp_reg_mr) {
		errno = ENOSYS;
		return NULL;
	}
  //MPRUD by mingman~
  /**
   *  If buffer address & size are passed, Verbs API will automatically
   *  allocate memory to buffer.
   *  Since Perftest app sets 'IBV_EXP_ACCESS_ALLOCATE_MR' Flag.
   */
#ifdef USE_MPRUD
  if (mprud_init_ctx()){
    printf("MPRUD init_ctx failed.\n");
    return NULL;
  }
#ifdef MG_DEBUG_MODE
  printf("original buffer size : %d\n", in->length);
#endif
  // buf = [Original Buf] + [Sub Buf] + [Last Msg Buf] + [Send Buf] + [Recv Buf]
  in->length += MPRUD_GRH_SIZE + (MPRUD_GRH_SIZE + MPRUD_DEFAULT_MTU) * (MPRUD_NUM_PATH + 1) + MPRUD_SEND_BUF_SIZE + MPRUD_RECV_BUF_SIZE;
  struct ibv_mr *mr = __ibv_common_reg_mr(in, ctx);
  mprud_set_outer_buffer(mr->addr, mr->length);
#ifdef MG_DEBUG_MODE
  printf("buffer size : %d\n", mr->length);
#endif
  
  return mr;
#else
  return __ibv_common_reg_mr(in, ctx);
#endif
  //~MPRUD by mingman
}

struct ibv_mr *__ibv_reg_mr(struct ibv_pd *pd, void *addr,
			    size_t length, int access)
{
	struct ibv_exp_reg_mr_in in;
  
  // Edit here for buffer setting change
	memset(&in, 0, sizeof(in));
	in.pd = pd;
	in.addr = addr;
	in.length = length;
  in.exp_access = access;

	return __ibv_common_reg_mr(&in, NULL);
}
default_symver(__ibv_reg_mr, ibv_reg_mr);

int __ibv_rereg_mr(struct ibv_mr *mr, int flags,
		   struct ibv_pd *pd, void *addr,
		   size_t length, int access)
{
	int dofork_onfail = 0;
	int err;
	void *old_addr;
	size_t old_len;

	if (flags & ~IBV_REREG_MR_FLAGS_SUPPORTED) {
		errno = EINVAL;
		return IBV_REREG_MR_ERR_INPUT;
	}

	if ((flags & IBV_REREG_MR_CHANGE_TRANSLATION) &&
	    (!length || !addr)) {
		errno = EINVAL;
		return IBV_REREG_MR_ERR_INPUT;
	}

	if (access && !(flags & IBV_REREG_MR_CHANGE_ACCESS)) {
		errno = EINVAL;
		return IBV_REREG_MR_ERR_INPUT;
	}

	if (!mr->context->ops.rereg_mr) {
		errno = ENOSYS;
		return IBV_REREG_MR_ERR_INPUT;
	}

	if (flags & IBV_REREG_MR_CHANGE_TRANSLATION) {
		err = ibv_dontfork_range(addr, length);
		if (err)
			return IBV_REREG_MR_ERR_DONT_FORK_NEW;
		dofork_onfail = 1;
	}

	old_addr = mr->addr;
	old_len = mr->length;
	err = mr->context->ops.rereg_mr(mr, flags, pd, addr, length, access);
	if (!err) {
		if (flags & IBV_REREG_MR_CHANGE_PD)
			mr->pd = pd;
		if (flags & IBV_REREG_MR_CHANGE_TRANSLATION) {
			mr->addr    = addr;
			mr->length  = length;
			err = ibv_dofork_range(old_addr, old_len);
			if (err)
				return IBV_REREG_MR_ERR_DO_FORK_OLD;
		}
	} else {
		err = IBV_REREG_MR_ERR_CMD;
		if (dofork_onfail) {
			if (ibv_dofork_range(addr, length))
				err = IBV_REREG_MR_ERR_CMD_AND_DO_FORK_NEW;
		}
	}

	return err;
}
default_symver(__ibv_rereg_mr, ibv_rereg_mr);

int __ibv_dereg_mr(struct ibv_mr *mr)
{
	int ret;
	struct verbs_context_exp *vctx;
	struct ibv_exp_dereg_out out;
	void *addr	= mr->addr;
	size_t length	= mr->length;

	memset(&out, 0, sizeof(out));
	out.need_dofork = 1;

	vctx = verbs_get_exp_ctx_op(mr->context, drv_exp_dereg_mr);
	if (vctx)
		ret = vctx->drv_exp_dereg_mr(mr, &out);
	else
		ret = mr->context->ops.dereg_mr(mr);

	if (!ret) {
		if (out.need_dofork)
			ibv_dofork_range(addr, length);
	}

	return ret;
}
default_symver(__ibv_dereg_mr, ibv_dereg_mr);

static struct ibv_comp_channel *ibv_create_comp_channel_v2(struct ibv_context *context)
{
	struct ibv_abi_compat_v2 *t = context->abi_compat;
	static int warned;

	if (!pthread_mutex_trylock(&t->in_use))
		return &t->channel;

	if (!warned) {
		fprintf(stderr, PFX "Warning: kernel's ABI version %d limits capacity.\n"
			"    Only one completion channel can be created per context.\n",
			abi_ver);
		++warned;
	}

	return NULL;
}

struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context)
{
	struct ibv_comp_channel            *channel;
	struct ibv_create_comp_channel      cmd;
	struct ibv_create_comp_channel_resp resp;

	if (abi_ver <= 2)
		return ibv_create_comp_channel_v2(context);

	channel = malloc(sizeof *channel);
	if (!channel)
		return NULL;

	IBV_INIT_CMD_RESP(&cmd, sizeof cmd, CREATE_COMP_CHANNEL, &resp, sizeof resp);
	if (write(context->cmd_fd, &cmd, sizeof cmd) != sizeof cmd) {
		free(channel);
		return NULL;
	}

	VALGRIND_MAKE_MEM_DEFINED(&resp, sizeof resp);

	channel->context = context;
	channel->fd      = resp.fd;
	channel->refcnt  = 0;

	return channel;
}

static int ibv_destroy_comp_channel_v2(struct ibv_comp_channel *channel)
{
	struct ibv_abi_compat_v2 *t = (struct ibv_abi_compat_v2 *) channel;
	pthread_mutex_unlock(&t->in_use);
	return 0;
}

int ibv_destroy_comp_channel(struct ibv_comp_channel *channel)
{
	struct ibv_context *context;
	int ret;

	context = channel->context;
	pthread_mutex_lock(&context->mutex);

	if (channel->refcnt) {
		ret = EBUSY;
		goto out;
	}

	if (abi_ver <= 2) {
		ret = ibv_destroy_comp_channel_v2(channel);
		goto out;
	}

	close(channel->fd);
	free(channel);
	ret = 0;

out:
	pthread_mutex_unlock(&context->mutex);

	return ret;
}

struct ibv_cq *__ibv_create_cq(struct ibv_context *context, int cqe, void *cq_context,
			       struct ibv_comp_channel *channel, int comp_vector)
{
	struct ibv_cq *cq;

	pthread_mutex_lock(&context->mutex);
  
  //MPRUD by mingman
#ifdef USE_MPRUD
  cqe = 1023;
#endif
  // change cqe size here
  //cqe = 1023;

	cq = context->ops.create_cq(context, cqe, channel, comp_vector);

	if (cq) {
		cq->context    	     	   = context;
		cq->channel		   = channel;
		if (channel)
			++channel->refcnt;
		cq->cq_context 	     	   = cq_context;
		cq->comp_events_completed  = 0;
		cq->async_events_completed = 0;
		pthread_mutex_init(&cq->mutex, NULL);
		pthread_cond_init(&cq->cond, NULL);
	}

	pthread_mutex_unlock(&context->mutex);

	return cq;
}
default_symver(__ibv_create_cq, ibv_create_cq);

int __ibv_resize_cq(struct ibv_cq *cq, int cqe)
{
	if (!cq->context->ops.resize_cq)
		return ENOSYS;

	return cq->context->ops.resize_cq(cq, cqe);
}
default_symver(__ibv_resize_cq, ibv_resize_cq);

int __ibv_destroy_cq(struct ibv_cq *cq)
{
	struct ibv_comp_channel *channel = cq->channel;
	int ret;

	if (channel)
		pthread_mutex_lock(&channel->context->mutex);

	ret = cq->context->ops.destroy_cq(cq);

	if (channel) {
		if (!ret)
			--channel->refcnt;
		pthread_mutex_unlock(&channel->context->mutex);
	}

	return ret;
}
default_symver(__ibv_destroy_cq, ibv_destroy_cq);

int __ibv_get_cq_event(struct ibv_comp_channel *channel,
		       struct ibv_cq **cq, void **cq_context)
{
	struct ibv_comp_event ev;

	if (read(channel->fd, &ev, sizeof ev) != sizeof ev)
		return -1;

	*cq         = (struct ibv_cq *) (uintptr_t) ev.cq_handle;
	*cq_context = (*cq)->cq_context;

	if ((*cq)->context->ops.cq_event)
		(*cq)->context->ops.cq_event(*cq);

	return 0;
}
default_symver(__ibv_get_cq_event, ibv_get_cq_event);

void __ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
{
	pthread_mutex_lock(&cq->mutex);
	cq->comp_events_completed += nevents;
	pthread_cond_signal(&cq->cond);
	pthread_mutex_unlock(&cq->mutex);
}
default_symver(__ibv_ack_cq_events, ibv_ack_cq_events);

struct ibv_srq *__ibv_create_srq(struct ibv_pd *pd,
				 struct ibv_srq_init_attr *srq_init_attr)
{
	struct ibv_srq *srq;

	if (!pd->context->ops.create_srq)
		return NULL;

	srq = pd->context->ops.create_srq(pd, srq_init_attr);
	if (srq) {
		srq->context          = pd->context;
		srq->srq_context      = srq_init_attr->srq_context;
		srq->pd               = pd;
		srq->events_completed = 0;
		pthread_mutex_init(&srq->mutex, NULL);
		pthread_cond_init(&srq->cond, NULL);
	}

	return srq;
}
default_symver(__ibv_create_srq, ibv_create_srq);

int __ibv_modify_srq(struct ibv_srq *srq,
		     struct ibv_srq_attr *srq_attr,
		     int srq_attr_mask)
{
	return srq->context->ops.modify_srq(srq, srq_attr, srq_attr_mask);
}
default_symver(__ibv_modify_srq, ibv_modify_srq);

int __ibv_query_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr)
{
	return srq->context->ops.query_srq(srq, srq_attr);
}
default_symver(__ibv_query_srq, ibv_query_srq);

int __ibv_destroy_srq(struct ibv_srq *srq)
{
	return srq->context->ops.destroy_srq(srq);
}
default_symver(__ibv_destroy_srq, ibv_destroy_srq);

struct ibv_qp *__ibv_create_qp(struct ibv_pd *pd,
			       struct ibv_qp_init_attr *qp_init_attr)
{
	struct ibv_qp *qp = pd->context->ops.create_qp(pd, qp_init_attr);
 
	if (qp) {
		qp->context    	     = pd->context;
		qp->qp_context 	     = qp_init_attr->qp_context;
		qp->pd         	     = pd;
		qp->send_cq    	     = qp_init_attr->send_cq;
		qp->recv_cq    	     = qp_init_attr->recv_cq;
		qp->srq        	     = qp_init_attr->srq;
		qp->qp_type          = qp_init_attr->qp_type;
		qp->state	     = IBV_QPS_RESET;
		qp->events_completed = 0;
		pthread_mutex_init(&qp->mutex, NULL);
		pthread_cond_init(&qp->cond, NULL);
	}
  //MPRUD by mingman~
#ifdef USE_MPRUD
  if (mprud_create_ah_list(pd, qp_init_attr)){
    printf("MPRUD create_ah_list failed.\n");
    return NULL;
  }
  if (mprud_create_inner_qps(pd, qp_init_attr)){
    printf("MPRUD create_inner_qps failed.\n");
    return NULL;
  }
#endif
  //~MPRUD by mingman

#ifdef USE_MPRUD
#ifdef MG_DEBUG_MODE
  // TEMP
  if (1){
       struct ibv_qp_attr attr;
         struct ibv_qp_init_attr init_attr;
         ibv_query_qp(qp,&attr, 0, &init_attr);

         printf("max send: %u |  max recv: %u | send_cqe: %u | recv_cqe: %u\n", init_attr.cap.max_send_wr, init_attr     .cap.max_recv_wr, qp->send_cq->cqe, qp->recv_cq->cqe);
         printf("Outer QP --> qkey: %u qp_num: %u  dlid: %d  dest_qp_num: %u\n", attr.qkey, qp->qp_num, attr.ah_attr.dlid, attr.dest_qp_num);
  // Set recv&send qp size
/*  mprud_set_send_size(qp_init_attr->cap.max_send_wr);
  mprud_set_recv_size(qp_init_attr->cap.max_recv_wr);
  mprud_set_cq_size(qp_init_attr->cap.max_recv_wr);
  */
  /*mprud_set_send_size(512);
  mprud_set_recv_size(512);
  mprud_set_cq_size(qp->recv_cq->cqe);
  */
       }
#endif
#endif

	return qp;
}
default_symver(__ibv_create_qp, ibv_create_qp);

int __ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		   int attr_mask,
		   struct ibv_qp_init_attr *init_attr)
{
	int ret;

	ret = qp->context->ops.query_qp(qp, attr, attr_mask, init_attr);
	if (ret)
		return ret;

	if (attr_mask & IBV_QP_STATE)
		qp->state = attr->qp_state;

	return 0;
}
default_symver(__ibv_query_qp, ibv_query_qp);

int __ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		    int attr_mask)
{
	int ret;

	ret = qp->context->ops.modify_qp(qp, attr, attr_mask);
	if (ret)
		return ret;

	if (attr_mask & IBV_QP_STATE)
		qp->state = attr->qp_state;

#ifdef USE_MPRUD
  ret = mprud_modify_qp(qp, attr, attr_mask);
  if (ret)
    return ret;
  // Get remote QPN when RC
  if (qp->qp_type != IBV_QPT_UD && attr->qp_state == IBV_QPS_RTR){
    mprud_set_dest_qp_num(attr->dest_qp_num);
  }
#endif

	return 0;
}
default_symver(__ibv_modify_qp, ibv_modify_qp);

int __ibv_destroy_qp(struct ibv_qp *qp)
{
	return qp->context->ops.destroy_qp(qp);
}
default_symver(__ibv_destroy_qp, ibv_destroy_qp);

static inline int ipv6_addr_v4mapped(const struct in6_addr *a)
{
	return ((a->s6_addr32[0] | a->s6_addr32[1]) |
		(a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0UL ||
		/* IPv4 encoded multicast addresses */
	       (a->s6_addr32[0]  == htonl(0xff0e0000) &&
		((a->s6_addr32[1] |
		  (a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0UL));
}


struct peer_address {
	void *address;
	uint32_t size;
};

static inline int create_peer_from_gid(int family, void *raw_gid,
				       struct peer_address *peer_address)
{
	switch (family) {
	case AF_INET:
		peer_address->address = raw_gid + 12;
		peer_address->size = 4;
		break;
	case AF_INET6:
		peer_address->address = raw_gid;
		peer_address->size = 16;
		break;
	default:
		return -1;
	}

	return 0;
}

#define NEIGH_GET_DEFAULT_TIMEOUT_MS 3000
static struct ibv_ah *__ibv_create_ah_nl(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
	struct ibv_ah *ah = NULL;
#ifndef NRESOLVE_NEIGH
	int err;
	int dst_family;
	int src_family;
	int oif;
	struct get_neigh_handler neigh_handler;
	union ibv_gid sgid;
	struct ibv_exp_ah_attr attr_ex;
	char ethernet_ll[ETHERNET_LL_SIZE];
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(pd->context,
							      drv_exp_ibv_create_ah);
	struct peer_address src;
	struct peer_address dst;

	if (!vctx) {
#endif
		ah = pd->context->ops.create_ah(pd, attr);
#ifndef NRESOLVE_NEIGH
		return ah;
	}

	memset(&attr_ex, 0, sizeof(attr_ex));

	memcpy(&attr_ex, attr, sizeof(*attr));
	memset((void *)&attr_ex + sizeof(*attr), 0,
	       sizeof(attr_ex) - sizeof(*attr));

	err = ibv_query_gid(pd->context, attr->port_num,
			    attr->grh.sgid_index, &sgid);

	if (err) {
		fprintf(stderr, PFX "ibv_create_ah failed to query sgid.\n");
		return NULL;
	}

	if (neigh_init_resources(&neigh_handler, NEIGH_GET_DEFAULT_TIMEOUT_MS))
		return NULL;

	dst_family = ipv6_addr_v4mapped((struct in6_addr *)attr->grh.dgid.raw) ?
			AF_INET : AF_INET6;
	src_family = ipv6_addr_v4mapped((struct in6_addr *)sgid.raw) ?
			AF_INET : AF_INET6;

	if (create_peer_from_gid(dst_family, attr->grh.dgid.raw, &dst)) {
		fprintf(stderr, PFX "ibv_create_ah failed to create dst "
			"peer\n");
		goto free_resources;
	}
	if (create_peer_from_gid(src_family, &sgid.raw, &src)) {
		fprintf(stderr, PFX "ibv_create_ah failed to create src "
			"peer\n");
		goto free_resources;
	}
	if (neigh_set_dst(&neigh_handler, dst_family, dst.address,
			  dst.size)) {
		fprintf(stderr, PFX "ibv_create_ah failed to create dst "
			"addr\n");
		goto free_resources;
	}

	if (neigh_set_src(&neigh_handler, src_family, src.address,
			  src.size)) {
		fprintf(stderr, PFX "ibv_create_ah failed to create src "
			"addr\n");
		goto free_resources;
	}

	oif = neigh_get_oif_from_src(&neigh_handler);

	if (oif > 0) {
		neigh_set_oif(&neigh_handler, oif);
	} else {
		fprintf(stderr, PFX "ibv_create_ah failed to get output IF\n");
		goto free_resources;
	}


	/* blocking call */
	if (process_get_neigh(&neigh_handler)) {
		fprintf(stderr, PFX "Neigh resolution process failed\n");
		goto free_resources;
	}

	attr_ex.vid = neigh_get_vlan_id_from_dev(&neigh_handler);

	if (attr_ex.vid <= 0xfff) {
		neigh_set_vlan_id(&neigh_handler, attr_ex.vid);
		attr_ex.comp_mask |= IBV_EXP_AH_ATTR_VID;
	}
	/* We are using only ethernet here */
	attr_ex.ll_address.len = neigh_get_ll(&neigh_handler, ethernet_ll,
					      sizeof(ethernet_ll));

	if (attr_ex.ll_address.len <= 0)
		goto free_resources;

	attr_ex.comp_mask |= IBV_EXP_AH_ATTR_LL;
	attr_ex.ll_address.type = LL_ADDRESS_ETH;
	attr_ex.ll_address.address = ethernet_ll;

	ah = vctx->drv_exp_ibv_create_ah(pd, &attr_ex);

free_resources:
	neigh_free_resources(&neigh_handler);

#endif
	return ah;
}

struct ibv_ah *__ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
	struct ibv_exp_port_attr port_attr;
	struct ibv_exp_ah_attr attr_ex = {};
	struct ibv_ah *ah;
	int err;
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(pd->context,
							      drv_exp_ibv_create_kah);

	port_attr.comp_mask = IBV_EXP_QUERY_PORT_ATTR_MASK1;
	port_attr.mask1 = IBV_EXP_QUERY_PORT_LINK_LAYER;
	err = ibv_exp_query_port(pd->context, attr->port_num, &port_attr);

	if (err) {
		fprintf(stderr, PFX "ibv_create_ah failed to query port.\n");
		return NULL;
	}

	if ((port_attr.link_layer == IBV_LINK_LAYER_ETHERNET) &&
	    !attr->is_global) {
		fprintf(stderr, PFX "GRH is mandatory For RoCE address handle\n");
		return NULL;
	}

	if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
		ah = pd->context->ops.create_ah(pd, attr);
	} else {
		if (!vctx) {
			ah = __ibv_create_ah_nl(pd, attr);
		} else {
			memcpy(&attr_ex, attr, sizeof(*attr));
			ah = vctx->drv_exp_ibv_create_kah(pd, &attr_ex);
			if (!ah)
				ah = __ibv_create_ah_nl(pd, attr);
		}
	}

	if (ah) {
		ah->context = pd->context;
		ah->pd      = pd;
	}
	return ah;
}
default_symver(__ibv_create_ah, ibv_create_ah);

static int ibv_find_gid_index(struct ibv_context *context, uint8_t port_num,
			      union ibv_gid *gid, uint32_t gid_type)
{
	struct ibv_exp_gid_attr gid_attr;
	union ibv_gid sgid;
	int i = 0, ret;

	gid_attr.comp_mask = IBV_EXP_QUERY_GID_ATTR_TYPE;

	do {
		ret = ibv_query_gid(context, port_num, i, &sgid);
		if (!ret)
			ret = ibv_exp_query_gid_attr(context, port_num, i,
						     &gid_attr);
		i++;
	} while (!ret && (memcmp(&sgid, gid, sizeof *gid) || (gid_type != gid_attr.type)));

	return ret ? ret : i - 1;
}

static inline void map_ipv4_addr_to_ipv6(__be32 ipv4, struct in6_addr *ipv6)
{
	__be32 tmp = 0;

	memcpy(&ipv6->s6_addr32[0], &tmp, 4);
	memcpy(&ipv6->s6_addr32[1], &tmp, 4);
	tmp = htonl(0x0000FFFF);
	memcpy(&ipv6->s6_addr32[2], &tmp, 4);
	memcpy(&ipv6->s6_addr32[3], &ipv4, 4);
}

static inline uint16_t ipv4_calc_hdr_csum(uint16_t *data, unsigned int num_hwords)
{
	unsigned int i = 0;
	uint32_t sum = 0;

	for (i = 0; i < num_hwords; i++)
		sum += *(data++);

	sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

static inline int get_grh_header_version(struct ibv_grh *grh)
{
	int ip6h_version = (ntohl(grh->version_tclass_flow) >> 28) & 0xf;
	struct iphdr *ip4h = (struct iphdr *)((void *)grh + 20);
	struct iphdr ip4h_checked;

	if (ip6h_version != 6) {
		if (ip4h->version == 4)
			return 4;
		errno = EPROTONOSUPPORT;
		return -1;
	}
	/* version may be 6 or 4 */
	if (ip4h->ihl != 5) /* IPv4 header length must be 5 for RoCE v2. */
		return 6;
	/*
	* Verify checksum.
	* We can't write on scattered buffers so we have to copy to temp
	* buffer.
	*/
	memcpy(&ip4h_checked, ip4h, sizeof(ip4h_checked));
	/* Need to set the checksum field (check) to 0 before re-calculating
	 * the checksum.
	 */
	ip4h_checked.check = 0;
	ip4h_checked.check = ipv4_calc_hdr_csum((uint16_t *)&ip4h_checked, 10);
	/* if IPv4 header checksum is OK, believe it */
	if (ip4h->check == ip4h_checked.check)
		return 4;
	return 6;
}

static inline void set_ah_attr_generic_fields(struct ibv_ah_attr *ah_attr,
					      struct ibv_wc *wc,
					      struct ibv_grh *grh,
					      uint8_t port_num)
{
	uint32_t flow_class;

	flow_class = ntohl(grh->version_tclass_flow);
	ah_attr->grh.flow_label = flow_class & 0xFFFFF;
	ah_attr->dlid = wc->slid;
	ah_attr->sl = wc->sl;
	ah_attr->src_path_bits = wc->dlid_path_bits;
	ah_attr->port_num = port_num;
}

static inline int set_ah_attr_by_ipv4(struct ibv_context *context,
				      struct ibv_ah_attr *ah_attr,
				      struct iphdr *ip4h, uint8_t port_num)
{
	union ibv_gid sgid;
	int ret;

	/* No point searching multicast GIDs in GID table */
	if (IN_CLASSD(ntohl(ip4h->daddr))) {
		errno = EINVAL;
		return -1;
	}

	map_ipv4_addr_to_ipv6(ip4h->daddr, (struct in6_addr *)&sgid);
	ret = ibv_find_gid_index(context, port_num, &sgid,
				 IBV_EXP_ROCE_V2_GID_TYPE);
	if (ret < 0)
		return ret;
	map_ipv4_addr_to_ipv6(ip4h->saddr,
			      (struct in6_addr *)&ah_attr->grh.dgid);
	ah_attr->grh.sgid_index = (uint8_t) ret;
	ah_attr->grh.hop_limit = ip4h->ttl;
	ah_attr->grh.traffic_class = ip4h->tos;

	return 0;
}

#define IB_NEXT_HDR    0x1b
static inline int set_ah_attr_by_ipv6(struct ibv_context *context,
				  struct ibv_ah_attr *ah_attr,
				  struct ibv_grh *grh, uint8_t port_num)
{
	uint32_t flow_class;
	uint32_t sgid_type;
	int ret;

	/* No point searching multicast GIDs in GID table */
	if (grh->dgid.raw[0] == 0xFF) {
		errno = EINVAL;
		return -1;
	}

	ah_attr->grh.dgid = grh->sgid;
	if (grh->next_hdr == IPPROTO_UDP) {
		sgid_type = IBV_EXP_ROCE_V2_GID_TYPE;
	} else if (grh->next_hdr == IB_NEXT_HDR) {
		sgid_type = IBV_EXP_IB_ROCE_V1_GID_TYPE;
	} else {
		errno = EPROTONOSUPPORT;
		return -1;
	}

	ret = ibv_find_gid_index(context, port_num, &grh->dgid,
				 sgid_type);
	if (ret < 0)
		return ret;

	ah_attr->grh.sgid_index = (uint8_t) ret;
	flow_class = ntohl(grh->version_tclass_flow);
	ah_attr->grh.hop_limit = grh->hop_limit;
	ah_attr->grh.traffic_class = (flow_class >> 20) & 0xFF;

	return 0;
}

#define CLASS_D_ADDR	(0xeUL << 28)
#define CLASS_D_MASK	(0xfUL << 28)
int ibv_init_ah_from_wc(struct ibv_context *context, uint8_t port_num,
			struct ibv_wc *wc, struct ibv_grh *grh,
			struct ibv_ah_attr *ah_attr)
{
	int version;
	int ret = 0;
	struct iphdr *iph;

	memset(ah_attr, 0, sizeof *ah_attr);
	set_ah_attr_generic_fields(ah_attr, wc, grh, port_num);

	if (wc->wc_flags & IBV_WC_GRH) {
		ah_attr->is_global = 1;
		version = get_grh_header_version(grh);

		if (version == 4) {
			iph = (struct iphdr *)((void *)grh + 20);
			if (((ntohl)(iph->daddr) & CLASS_D_MASK) == CLASS_D_ADDR)
				return EINVAL;
			ret = set_ah_attr_by_ipv4(context, ah_attr,
						  iph,
						  port_num);
		}
		else if (version == 6) {
			if (grh->dgid.raw[0] == 0xFF)
				return EINVAL;
			ret = set_ah_attr_by_ipv6(context, ah_attr, grh,
						  port_num);
		}
		else
			ret = -1;
	}

	return ret;
}

struct ibv_ah *ibv_create_ah_from_wc(struct ibv_pd *pd, struct ibv_wc *wc,
				     struct ibv_grh *grh, uint8_t port_num)
{
	struct ibv_ah_attr ah_attr;
	int ret;

	ret = ibv_init_ah_from_wc(pd->context, port_num, wc, grh, &ah_attr);
	if (ret)
		return NULL;

	return ibv_create_ah(pd, &ah_attr);
}

int __ibv_destroy_ah(struct ibv_ah *ah)
{
	return ah->context->ops.destroy_ah(ah);
}
default_symver(__ibv_destroy_ah, ibv_destroy_ah);

int __ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	return qp->context->ops.attach_mcast(qp, gid, lid);
}
default_symver(__ibv_attach_mcast, ibv_attach_mcast);

int __ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	return qp->context->ops.detach_mcast(qp, gid, lid);
}
default_symver(__ibv_detach_mcast, ibv_detach_mcast);


/* XRC compatability layer */
struct ibv_xrc_domain *ibv_open_xrc_domain(struct ibv_context *context,
					   int fd, int oflag)
{

	struct ibv_xrcd *ibv_xrcd;
	struct ibv_xrcd_init_attr xrcd_init_attr;

	memset(&xrcd_init_attr, 0, sizeof(xrcd_init_attr));

	xrcd_init_attr.fd = fd;
	xrcd_init_attr.oflags = oflag;

	xrcd_init_attr.comp_mask = IBV_XRCD_INIT_ATTR_FD |
					IBV_XRCD_INIT_ATTR_OFLAGS;

	ibv_xrcd = ibv_open_xrcd(context, &xrcd_init_attr);
	if (!ibv_xrcd)
		return NULL;

	/* Caller should relate this returned pointer as an opaque, internally will be used
	  * as ibv_xrcd pointer.
	*/
	return (struct ibv_xrc_domain *)ibv_xrcd;

}


struct ibv_srq *ibv_create_xrc_srq(struct ibv_pd *pd,
				   struct ibv_xrc_domain *xrc_domain,
				   struct ibv_cq *xrc_cq,
				   struct ibv_srq_init_attr *srq_init_attr)
{

	struct ibv_srq_init_attr_ex ibv_srq_init_attr_ex;
	struct ibv_srq_legacy *ibv_srq_legacy;
	struct ibv_srq *ibv_srq;
	uint32_t		xrc_srq_num;
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(pd->context, drv_exp_set_legacy_xrc);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	memset(&ibv_srq_init_attr_ex, 0, sizeof ibv_srq_init_attr_ex);

	ibv_srq_init_attr_ex.xrcd = (struct ibv_xrcd *)xrc_domain;
	ibv_srq_init_attr_ex.comp_mask = IBV_SRQ_INIT_ATTR_XRCD |
				IBV_SRQ_INIT_ATTR_TYPE |
				IBV_SRQ_INIT_ATTR_CQ | IBV_SRQ_INIT_ATTR_PD;

	ibv_srq_init_attr_ex.cq = xrc_cq;
	ibv_srq_init_attr_ex.pd = pd;
	ibv_srq_init_attr_ex.srq_type = IBV_SRQT_XRC;

	ibv_srq_init_attr_ex.attr.max_sge = srq_init_attr->attr.max_sge;
	ibv_srq_init_attr_ex.attr.max_wr = srq_init_attr->attr.max_wr;
	ibv_srq_init_attr_ex.attr.srq_limit = srq_init_attr->attr.srq_limit;
	ibv_srq_init_attr_ex.srq_context = srq_init_attr->srq_context;

	ibv_srq = ibv_create_srq_ex(pd->context, &ibv_srq_init_attr_ex);
	if (!ibv_srq)
		return NULL;

	/* handle value LEGACY_XRC_SRQ_HANDLE should be reserved, in case got it
	  * allocating other one, than free it to in order to get a new handle.
	*/
	if (ibv_srq->handle == LEGACY_XRC_SRQ_HANDLE) {

		struct ibv_srq *ibv_srq_tmp = ibv_srq;
		int ret;

		ibv_srq = ibv_create_srq_ex(pd->context, &ibv_srq_init_attr_ex);
		/* now  destroying previous one */
		ret = ibv_destroy_srq(ibv_srq_tmp);
		if (ret) {
			fprintf(stderr, PFX "ibv_create_xrc_srq, fail to destroy intermediate srq\n");
			return NULL;
		}

		if (!ibv_srq)
			return NULL;

		/* still get this value - set an error */
		if (ibv_srq->handle == LEGACY_XRC_SRQ_HANDLE) {
			ret = ibv_destroy_srq(ibv_srq);
			if (ret)
				fprintf(stderr, PFX "ibv_create_xrc_srq, fail to destroy intermediate srq\n");
			errno = EAGAIN;
			return NULL;
		}
	}

	ibv_srq_legacy = calloc(1, sizeof(*ibv_srq_legacy));
	if (!ibv_srq_legacy) {
		errno = ENOMEM;
		goto err;
	}

	if (ibv_get_srq_num(ibv_srq, &xrc_srq_num))
		goto err_free;

	ibv_srq_legacy->ibv_srq = ibv_srq;
	ibv_srq_legacy->xrc_srq_num = xrc_srq_num;

	/* setting the bin compat fields */
	ibv_srq_legacy->xrc_srq_num_bin_compat = xrc_srq_num;
	ibv_srq_legacy->xrc_domain_bin_compat = xrc_domain;
	ibv_srq_legacy->xrc_cq_bin_compat = xrc_cq;
	ibv_srq_legacy->context          = pd->context;
	ibv_srq_legacy->srq_context      = srq_init_attr->srq_context;
	ibv_srq_legacy->pd               = pd;
	/* Set an indication that this is a legacy structure.
	  * In all cases that we have this indication should use internal ibv_srq having real handle and fields.
	  *
	*/
	ibv_srq_legacy->handle	   = LEGACY_XRC_SRQ_HANDLE;
	ibv_srq_legacy->xrc_domain       = xrc_domain;
	ibv_srq_legacy->xrc_cq           = xrc_cq;
	/* mutex & cond are not set on legacy_ibv_srq, internal ones are used.
	  * We don't expect application to use them.
	  */
	ibv_srq_legacy->events_completed = 0;

	vctx->drv_exp_set_legacy_xrc(ibv_srq, ibv_srq_legacy);
	return (struct ibv_srq *)(ibv_srq_legacy);

err_free:
	free(ibv_srq_legacy);
err:
	ibv_destroy_srq(ibv_srq);
	return NULL;

}



static pthread_mutex_t xrc_tree_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *ibv_xrc_qp_tree;

static int xrc_qp_compare(const void *a, const void *b)
{

	if ((*(uint32_t *) a) < (*(uint32_t *) b))
	       return -1;
	else if ((*(uint32_t *) a) > (*(uint32_t *) b))
	       return 1;
	else
	       return 0;

}

struct ibv_qp *ibv_find_xrc_qp(uint32_t qpn)
{
	uint32_t **qpn_ptr;
	struct ibv_qp *ibv_qp = NULL;

	pthread_mutex_lock(&xrc_tree_mutex);
	qpn_ptr = tfind(&qpn, &ibv_xrc_qp_tree, xrc_qp_compare);
	if (!qpn_ptr)
		goto end;

	ibv_qp = container_of(*qpn_ptr, struct ibv_qp, qp_num);

end:
	pthread_mutex_unlock(&xrc_tree_mutex);
	return ibv_qp;
}

static int ibv_clear_xrc_qp(uint32_t qpn)
{
	uint32_t **qpn_ptr;
	int ret = 0;

	pthread_mutex_lock(&xrc_tree_mutex);
	qpn_ptr = tdelete(&qpn, &ibv_xrc_qp_tree, xrc_qp_compare);
	if (!qpn_ptr)
		ret = EINVAL;

	pthread_mutex_unlock(&xrc_tree_mutex);
	return ret;
}

static int ibv_store_xrc_qp(struct ibv_qp *qp)
{
	uint32_t **qpn_ptr;
	int ret = 0;

	if (ibv_find_xrc_qp(qp->qp_num)) {
		/* set an error in case qpn alreday exists, not expected to happen */
		fprintf(stderr, PFX "ibv_store_xrc_qp failed, qpn=%u is already stored\n",
				qp->qp_num);
		return EEXIST;
	}

	pthread_mutex_lock(&xrc_tree_mutex);
	qpn_ptr = tsearch(&qp->qp_num, &ibv_xrc_qp_tree, xrc_qp_compare);
	if (!qpn_ptr)
		ret = EINVAL;

	pthread_mutex_unlock(&xrc_tree_mutex);
	return ret;

}

int ibv_close_xrc_domain(struct ibv_xrc_domain *d)
{
	struct ibv_xrcd *ibv_xrcd = (struct ibv_xrcd *)d;
	return ibv_close_xrcd(ibv_xrcd);
}

int ibv_create_xrc_rcv_qp(struct ibv_qp_init_attr *init_attr,
			  uint32_t *xrc_rcv_qpn)
{
	struct ibv_xrcd *ibv_xrcd;
	struct ibv_qp_init_attr_ex qp_init_attr_ex;
	struct ibv_qp *ibv_qp;
	int ret;

	if (!init_attr || !(init_attr->xrc_domain))
		return EINVAL;

	ibv_xrcd = (struct ibv_xrcd *) init_attr->xrc_domain;
	memset(&qp_init_attr_ex, 0, sizeof(qp_init_attr_ex));
	qp_init_attr_ex.qp_type = IBV_QPT_XRC_RECV;
	qp_init_attr_ex.comp_mask = IBV_QP_INIT_ATTR_XRCD;
	qp_init_attr_ex.xrcd = ibv_xrcd;

	ibv_qp = ibv_create_qp_ex(ibv_xrcd->context, &qp_init_attr_ex);
	if (!ibv_qp)
		return errno;

	/* We should return xrc_rcv_qpn and manage the handle  */
	*xrc_rcv_qpn = ibv_qp->qp_num;
	ret = ibv_store_xrc_qp(ibv_qp);
	if (ret) {
		int err;
		err = ibv_destroy_qp(ibv_qp);
		if (err)
			fprintf(stderr, PFX "ibv_create_xrc_rcv_qp, ibv_destroy_qp failed, err=%d\n", err);
		return ret;
	}

	return 0;
}


int ibv_modify_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			  uint32_t xrc_qp_num,
			  struct ibv_qp_attr *attr, int attr_mask)
{
	struct ibv_qp *qp;

	qp = ibv_find_xrc_qp(xrc_qp_num);
	if (!qp)
		return EINVAL;

	/* no use of xrc doamin */
	return ibv_modify_qp(qp, attr, attr_mask);

}

int ibv_query_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain, uint32_t xrc_qp_num,
			 struct ibv_qp_attr *attr, int attr_mask,
			 struct ibv_qp_init_attr *init_attr)
{
	struct ibv_qp *qp;

	qp = ibv_find_xrc_qp(xrc_qp_num);
	if (!qp)
		return EINVAL;

	/* no use of xrc doamin */
	return ibv_query_qp(qp, attr, attr_mask, init_attr);

}
int ibv_reg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain, uint32_t xrc_qp_num)
{

	struct ibv_qp *qp;
	struct ibv_qp_open_attr attr;
	struct ibv_xrcd *ibv_xrcd = (struct ibv_xrcd *)xrc_domain;
	int ret;

	memset(&attr, '\0', sizeof(attr));

	attr.qp_num = xrc_qp_num;
	attr.qp_type = IBV_QPT_XRC_RECV;
	attr.xrcd = ibv_xrcd;
	attr.comp_mask = IBV_QP_OPEN_ATTR_XRCD | IBV_QP_OPEN_ATTR_NUM |
			IBV_QP_OPEN_ATTR_TYPE;

	qp = ibv_open_qp(ibv_xrcd->context, &attr);
	if (!qp)
		return errno;
	/* xrc_qp_num should be equal to qp->qp_num - same kernel qp.
	  * This API expects to be called from other process comparing the creator one
	  * No mapping between same qpn to more that 1 ibv_qp pointer.
	*/
	ret = ibv_store_xrc_qp(qp);
	if (ret) {
		int err;
		err = ibv_destroy_qp(qp);
		if (err)
			fprintf(stderr, PFX "ibv_reg_xrc_rcv_qp, ibv_destroy_qp failed, err=%d\n", err);

		return ret;
	}

	return 0;

}

int ibv_unreg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			 uint32_t xrc_qp_num)
{

	struct ibv_qp *qp;
	int ret;

	qp = ibv_find_xrc_qp(xrc_qp_num);
	if (!qp)
		return EINVAL;

	ret = ibv_clear_xrc_qp(xrc_qp_num);
	if (ret) {
		fprintf(stderr, PFX "ibv_unreg_xrc_rcv_qp, fail via clear, qpn=%u, err=%d\n",
			xrc_qp_num, ret);
		return ret;
	}

	return ibv_destroy_qp(qp);

}

