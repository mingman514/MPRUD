/*MPRUD by mingman*/
//#define _GNU_SOURCE

#include <infiniband/mprud.h>
#include <arpa/inet.h>
#include <stdlib.h>
//#include <pthread.h>
//#include <sched.h>
//#include <unistd.h>
//#include <signal.h>
//#include <sys/shm.h>
//#include <sys/mman.h>
//#include <fcntl.h>
//#include <time.h>
//#include <stdatomic.h>

/**
 * To fix
 * - Deallocate PD Fail -> maybe due to newly created ah_list?
 *   (solved): added mprud_destroy_ah_list func right before dealloc_pd.
 */
// post & poll measure
uint64_t posted_cnt = 0, polled_cnt = 0;
int recv_size = 0, send_size = 0, cq_size = 0;
int split_num = 0;   // number of splitted requests

// temp variable to trace buffer
int tmp_num = 0;
char *mprud_buf = NULL;
static struct ibv_ah* ah_list[MPRUD_NUM_PATH];
static char REMOTE_GIDS[4][50] = {
  "0000:0000:0000:0000:0000:ffff:0a00:6503", // 10.0.101.3 spine-1
  "0000:0000:0000:0000:0000:ffff:0a00:6504", // 10.0.101.4 spine-2
  "0000:0000:0000:0000:0000:ffff:0a00:6505", // 10.0.101.5 spine-3
  "0000:0000:0000:0000:0000:ffff:0a00:6502", // 10.0.101.2 spine-4
//  "0000:0000:0000:0000:0000:ffff:0a00:c902", // 10.0.201.2 spine-1
//  "0000:0000:0000:0000:0000:ffff:0a00:c903", // 10.0.201.3 spine-2
//  "0000:0000:0000:0000:0000:ffff:0a00:c904", // 10.0.201.4 spine-3
//  "0000:0000:0000:0000:0000:ffff:0a00:c905", // 10.0.201.5 spine-4
};

struct ibv_ah** mprud_get_ah_list()
{
  return ah_list;
}

void print_gid_info(union ibv_gid mgid)
{
  printf("SUBNET: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d\nGUID  : %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d\n",
     mgid.raw[0], mgid.raw[1],
     mgid.raw[2], mgid.raw[3],
     mgid.raw[4], mgid.raw[5],
     mgid.raw[6], mgid.raw[7],
     mgid.raw[8], mgid.raw[9],
     mgid.raw[10],mgid.raw[11],
     mgid.raw[12],mgid.raw[13],
     mgid.raw[14],mgid.raw[15]);
//  printf("subnet_prefix: %ld\t interface_id: %ld\n", mgid.global.subnet_prefix, mgid.global.interface_id);
}

/**
 * Already implemented in inet_pton func from arpa/inet.h
 */
uint8_t *convert_to_raw_gid(uint8_t *gid, char* str_gid)
{
  int i;
  char temp[3];
  for (i=0; i<16; i++){
    temp[0] = str_gid[i*2];
    temp[1] = str_gid[i*2+1];
    temp[2] = '\0';
    gid[i] = strtol(temp, NULL, 16);  // hex string to integer
    //printf("gid [%d]=%d\n", i, gid[i]);
  }
  return gid;
}

int mprud_create_ah_list(struct ibv_pd *pd,
			       struct ibv_qp_init_attr *qp_init_attr)
{
  printf("mprud_create_ah_list\n");
  struct ibv_ah_attr *ah_attr;

  // is my_gid needed?
  //union ibv_gid my_gid;
  //if (ibv_query_gid(pd->context, MPRUD_DEFAULT_PORT, 7, &my_gid))
  //  return FAILURE;
  //print_gid_info(my_gid); 

  // move these to header!! or Use file I/O?
  printf("REMOTE_GIDS len = %ld  |  Number of Path to use = %d\n", sizeof(REMOTE_GIDS) / sizeof(REMOTE_GIDS[0]), MPRUD_NUM_PATH);
  if (sizeof(REMOTE_GIDS) / sizeof(REMOTE_GIDS[0]) < MPRUD_NUM_PATH){
    printf("ERROR: Specified GIDs are less than the number of paths you are trying to use!\n"); 
    exit(1);
  }

  for (int i=0; i<MPRUD_NUM_PATH; i++){    // num of path can be changed!

    uint8_t *temp_gid;
    ALLOCATE(temp_gid, uint8_t, 16);
    //convert_to_raw_gid(temp_gid, REMOTE_GIDS[i]);
    inet_pton(AF_INET6, REMOTE_GIDS[i], temp_gid);

    union ibv_gid rem_gid = {
      .global = {
        .subnet_prefix = 0,
        .interface_id = 0,
      }
    };
    memcpy(rem_gid.raw, temp_gid, sizeof(uint8_t)*16);

    print_gid_info(rem_gid);

    // ah_attr setting
    ALLOCATE(ah_attr, struct ibv_ah_attr, 1); 
    memset(ah_attr, 0, sizeof(struct ibv_ah_attr));

    ah_attr->port_num = 1;
    ah_attr->is_global = 1;
    ah_attr->sl = 3;  // service level 3
    // GRH
    ah_attr->grh.dgid = rem_gid;
    ah_attr->grh.sgid_index = 7;  // differs 
    ah_attr->grh.hop_limit = 0xFF;  // default value(64) in perftest
    ah_attr->grh.traffic_class = 106;

    // create ah_attrs for static paths
    ah_list[i] = ibv_create_ah(pd, ah_attr);
    if (!ah_attr){
      printf("Unable to create address handler for UD QP\n");
      return FAILURE;
    }
  }

  return SUCCESS;
}
char *mprud_get_buffer()
{
  return mprud_buf;
} 

void mprud_set_buffer(void* ptr)
{
  mprud_buf = ptr;
}


int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc)
{
  // App-only polling here
  uint32_t outer_poll_num = MIN(polled_cnt/split_num, ne);

  if (MG_DEBUG_POLL && outer_poll_num > 0){
    printf("[Outer Poll]  posted_cnt: %lu  polled_cnt: %lu  split_num:%d\n", posted_cnt, polled_cnt, split_num);
    printf("\t-->Outer Poll: %u\n", outer_poll_num);
  }

  if (outer_poll_num > 0){
    for (int i=0; i<outer_poll_num; i++){
      wc[i].wr_id = 0;
      wc[i].status = IBV_WC_SUCCESS;
    }      
    posted_cnt -= split_num * outer_poll_num;
    polled_cnt -= split_num * outer_poll_num;
//mprud_print_buffer();
    return outer_poll_num;
  }

  struct ibv_wc tmp_wc[MPRUD_POLL_BATCH];
  memset(tmp_wc, 0, sizeof(struct ibv_wc) * MPRUD_POLL_BATCH);
  if (posted_cnt > polled_cnt){
    uint32_t now_polled = cq->context->ops.poll_cq(cq, MPRUD_POLL_BATCH, tmp_wc, 1); // Go to original poll_cq

    tmp_num++;
    printf("\n---------- PRINT BUFFER [#%d polling] -----------\n", tmp_num);
    mprud_print_buffer();

    if (now_polled > 0){
      printf("[Inner Poll] %d\n", now_polled);
      polled_cnt += now_polled;
      for (int i = 0; i < now_polled; i++) {
        if (tmp_wc[i].status != IBV_WC_SUCCESS) {
          fprintf(stderr, "[MPRUD] Poll send CQ error status=%u qp %d\n", tmp_wc[i].status,(int)tmp_wc[i].wr_id);
        }
      }
    } else if (now_polled < 0){
      printf("ERROR: Polling result is negative!\n");
      return now_polled;
    }
  }

  return 0;
}

void mprud_set_recv_size(int size)
{
  recv_size = size;
}

void mprud_set_send_size(int size)
{
  send_size = size;
}

void mprud_set_cq_size(int size)
{
  cq_size = size;
}

void mprud_print_buffer()
{
  //int n = MPRUD_BUF_SPLIT_NUM;
  int n = 50;
  
  for (int i=0; i<n; i++){
    char* cur = mprud_get_buffer() + i * MPRUD_RECV_BUF_OFFSET;
    /* uint32_t* + 1 => plus sizeof(uint32_t) */
    printf("[#%d] %p |  sid: %u  msg_sqn: %u  pkt_sqn: %u\n", i+1, (uint32_t*) cur, *((uint32_t*)cur + 10), *((uint32_t*)cur+11), *((uint32_t*)cur+12));
        //*((uint32_t*)cur+40),*((uint32_t*)cur+44),*((uint32_t*)cur+48));
  }
}
/*
struct ibv_qp* mprud_create_qp(struct ibv_qp *ibqp, struct ibv_send_wr *wr)
{
  // only for UD QP
  if (ibqp->qp_type != IBV_QPT_UD){
    printf("QP type is not UD.\n");
    goto out;
  }

  // ibv_create_qp -> need
  // (1) pd  (2) attr
  struct ibv_qp* qp = NULL;

  struct ibv_qp_init_attr attr;
  memset(&attr, 0, sizeof(struct ibv_qp_init_attr));

  // Use the same CQs of ibqp
  attr.send_cq = ibqp->send_cq;
  attr.recv_cq = ibqp->recv_cq;
  attr.cap.max_send_wr  = user_param->tx_depth;
  attr.cap.max_send_sge = MAX_SEND_SGE;
  attr.cap.max_inline_data = user_param->inline_size;

  if (user_param->use_srq && (user_param->tst == LAT || user_param->mach     ine == SERVER || user_param->duplex == ON)) {
    attr.srq = ctx->srq;
    attr.cap.max_recv_wr  = 0;
    attr.cap.max_recv_sge = 0;
  } else {
    attr.srq = NULL;
    attr.cap.max_recv_wr  = user_param->rx_depth;
    attr.cap.max_recv_sge = MAX_RECV_SGE;
  }

  switch (user_param->connection_type) {

    case RC : attr.qp_type = IBV_QPT_RC; break;
    case UC : attr.qp_type = IBV_QPT_UC; break;
    case UD : attr.qp_type = IBV_QPT_UD; break;
    default:  fprintf(stderr, "Unknown connection type \n");
              return NULL;
  }


    qp = ibv_create_qp(ctx->pd, &attr);

  if (qp == NULL && errno == ENOMEM) {
    fprintf(stderr, "Requested QP size might be too big. Try reducing TX      depth and/or inline size.\n");
    fprintf(stderr, "Current TX depth is %d and  inline size is %d .\n",      user_param->tx_depth, user_param->inline_size);
  }

  if (user_param->inline_size > attr.cap.max_inline_data) {
    user_param->inline_size = attr.cap.max_inline_data;
    printf("  Actual inline-size(%d) > requested inline-size(%d)\n",
        attr.cap.max_inline_data, user_param->inline_size);
  }



  return qp;

out:
  return NULL;
}
*/
int mprud_destroy_ah_list()
{
  int i;

  for (i=0; i<MPRUD_NUM_PATH; i++){
    if (ah_list[i]){
      printf("destroying ah #%d\n", i);
      if (ibv_destroy_ah(ah_list[i]))
        return FAILURE;
    }
  }
  return SUCCESS;
}
/*
uint32_t CHUNK_SIZE = 4096; //4KB

int use_perf = -1;

static uint32_t tenant_id = -1;
static uint32_t global_qnum = 0;
static uint32_t global_cqnum = 0;

extern uint32_t atomic_queue_try_push_u64(void *p, uint64_t *value);

void perf_early_poll_cq();


int perf_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc)
{
  if(!use_perf)
    return cq->context->ops.poll_cq(cq, ne, wc, 1);

  printf("perf_poll_cq()\n");
  uint32_t cq_idx = kh_value(cq_hash, kh_get(cqh, cq_hash, cq->handle));

  pthread_mutex_lock(&(cq_ctx[cq_idx].lock));
  if(cq_ctx[cq_idx].early_poll_num)
  {
    int ret = cq_ctx[cq_idx].early_poll_num < ne ? cq_ctx[cq_idx].early_poll_num : ne;
    int front = 0;

    if(cq_ctx[cq_idx].wc_head + ret > cq_ctx[cq_idx].max_cqe)
    {
      front = cq_ctx[cq_idx].max_cqe - cq_ctx[cq_idx].wc_head;      
      memcpy(wc, ((struct ibv_wc*)cq_ctx[cq_idx].wc_list) + cq_ctx[cq_idx].wc_head, sizeof(struct ibv_wc) * front);
      cq_ctx[cq_idx].wc_head = (cq_ctx[cq_idx].wc_head + front) % cq_ctx[cq_idx].max_cqe;
    }
    
    memcpy(wc + front, ((struct ibv_wc*)cq_ctx[cq_idx].wc_list) + cq_ctx[cq_idx].wc_head, sizeof(struct ibv_wc) * (ret - front));
    cq_ctx[cq_idx].wc_head = (cq_ctx[cq_idx].wc_head + (ret - front)) % cq_ctx[cq_idx].max_cqe;

    cq_ctx[cq_idx].early_poll_num -= ret;
    LOG_DEBUG("Copy early polled: %d, wc_head: %d, %d %d\n", ret, cq_ctx[cq_idx].wc_head, cq_idx, cq_ctx[cq_idx].early_poll_num);
    pthread_mutex_unlock(&(cq_ctx[cq_idx].lock));
    return ret;
  }
  pthread_mutex_unlock(&(cq_ctx[cq_idx].lock));
    
  return cq->context->ops.poll_cq(cq, ne, wc, 1);
}
*/
