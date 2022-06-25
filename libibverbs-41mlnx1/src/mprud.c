/*MPRUD by mingman*/

#include <infiniband/mprud.h>
#include <arpa/inet.h>
#include <stdlib.h>

// Global variable initialization
struct mprud_context mpctx;

// Cleaning target later
//uint64_t posted_cnt = 0, polled_cnt = 0;
int recv_size = 0, send_size = 0, cq_size = 0;
int last_size = 0;
int split_num = 1;   // number of splitted requests
char *inner_buf = NULL;
char *outer_buf = NULL;
struct ibv_ah* ah_list[MPRUD_NUM_PATH];


static uint64_t cur_idx = 0; // accumulated(total) outer polled cnt

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


int mprud_init_ctx()
{
  // Add other member variables later
  memset(&mpctx, 0 , sizeof(struct mprud_context));
  mpctx.split_num = 1;

  // ....  
  return SUCCESS;
}

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

int mprud_create_inner_qps(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr)
{
  struct ibv_qp_init_attr iqp_init_attr;
  memcpy(&iqp_init_attr, qp_init_attr, sizeof(struct ibv_qp_init_attr));
  /* automatically allocate memory of cq & qp in verbs */
  // One integrated send_cq is enough
  struct ibv_cq *send_cq;
  send_cq = pd->context->ops.create_cq(pd->context, 1023, NULL, 0);

  for (int i = 0; i < MPRUD_NUM_PATH; i++){
    // Need recv_cq for each inner QPs
    struct ibv_cq *recv_cq;
    recv_cq = pd->context->ops.create_cq(pd->context, 1023, NULL, 0);

    iqp_init_attr.send_cq = send_cq;
    iqp_init_attr.recv_cq = recv_cq;
    iqp_init_attr.qp_type = IBV_QPT_UD;

    mpctx.inner_qps[i] = pd->context->ops.create_qp(pd, &iqp_init_attr);   
    //printf("######### %p %p\n", mpctx.inner_qps[i]->send_cq,mpctx.inner_qps[i]->recv_cq);
  }

  for(int i =0; i<MPRUD_NUM_PATH; i++){
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    ibv_query_qp(mpctx.inner_qps[i],&attr, 0, &init_attr);
    printf("[%d] qkey: %u qp_num: %u  dlid: %d  dest_qp_num: %u\n", i, attr.qkey, mpctx.inner_qps[i]->qp_num, attr.ah_attr.dlid, attr.dest_qp_num);
  } 

  return SUCCESS;
}

int mprud_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask)
{
  int ret;
  struct ibv_qp_attr iqp_attr;
  memcpy(&iqp_attr, attr, sizeof(struct ibv_qp_attr));

  if (attr->qp_state == IBV_QPS_INIT){

    // INIT
    for (int i = 0; i < MPRUD_NUM_PATH; i++){

        ret = qp->context->ops.modify_qp(mpctx.inner_qps[i], &iqp_attr, attr_mask);
      if (ret){
        printf("Failed to modify inner QP state to INIT\n");
        return FAILURE;
      }
    }

  } else if (attr->qp_state == IBV_QPS_RTR){

    // RTR
    for (int i = 0; i < MPRUD_NUM_PATH; i++){
//      printf("BEFORE: dest_qp_num=%d\n", iqp_attr.dest_qp_num);
//      //iqp_attr.dest_qp_num += 1;
//      printf("AFTER: dest_qp_num=%d\n", iqp_attr.dest_qp_num);

        ret = qp->context->ops.modify_qp(mpctx.inner_qps[i], &iqp_attr, attr_mask);
      if (ret){
        printf("Failed to modify inner QP state to RTR\n");
        return FAILURE;
      }
    }
    
  } else if (attr->qp_state == IBV_QPS_RTS){

    // RTS
    for (int i = 0; i < MPRUD_NUM_PATH; i++){

      ret = qp->context->ops.modify_qp(mpctx.inner_qps[i], &iqp_attr, attr_mask);
      if (ret){
        printf("Failed to modify inner QP state to RTS\n");
        return FAILURE;
      }
    }

  }
  return SUCCESS;
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
char *mprud_get_inner_buffer()
{
  return inner_buf;
} 

void mprud_set_inner_buffer(void* ptr)
{
#ifdef MG_DEBUG_MODE
  printf("Set Inner Buffer to [%p]\n", ptr);
#endif
  inner_buf = ptr;
}

char *mprud_get_outer_buffer()
{
  return outer_buf;
}

void mprud_set_outer_buffer(void* ptr)
{
#ifdef MG_DEBUG_MODE
  printf("Set Outer Buffer to [%p]\n", ptr);
#endif

  outer_buf = ptr;
}

static inline void mprud_syn_buffer(int outer_poll_num)
{
  char* inner_buf = mprud_get_inner_buffer();
  char* outer_buf = mprud_get_outer_buffer();
  char* cur;

  for (int i = 0; i < outer_poll_num; i++){
    for (int j = 0; j < split_num; j++, cur_idx++){
      cur_idx = cur_idx % MPRUD_BUF_SPLIT_NUM;
      cur = inner_buf + cur_idx * MPRUD_RECV_BUF_OFFSET;

      memcpy(outer_buf + (j * MPRUD_DEFAULT_MTU), cur + MPRUD_GRH_SIZE + MPRUD_HEADER_SIZE,(j == split_num - 1 ? last_size: MPRUD_DEFAULT_MTU));
#ifdef MG_DEBUG_MODE
      printf("cur_idx: %d  inner_cur: %p   outer_cur: %p\n", cur_idx, cur, outer_buf + (j * MPRUD_DEFAULT_MTU));
#endif
    }
  }

}

// Splitted msg polling
static inline int mprud_outer_poll(int ne, struct ibv_wc *wc, uint64_t *posted_cnt, uint64_t *polled_cnt, int Iam)
{

  // App-only polling here
  uint32_t outer_poll_num = MIN(*polled_cnt/split_num, ne);

#ifdef MG_DEBUG_MODE
  if (outer_poll_num > 0){
    printf("[Outer Poll]  posted_cnt: %llu  polled_cnt: %llu  split_num:%d\n", *posted_cnt, *polled_cnt, split_num);
    printf("\t-->Outer Poll: %u\n", outer_poll_num);
  }
#endif

  if (outer_poll_num > 0){
    for (int i=0; i<outer_poll_num; i++){
      wc[i].wr_id = 0;
      wc[i].status = IBV_WC_SUCCESS;
    }      
    *posted_cnt -= split_num * outer_poll_num;
    *polled_cnt -= split_num * outer_poll_num;

    // Copy data to outer buffer
    if (Iam == MP_SERVER)
      mprud_syn_buffer(outer_poll_num);

    return outer_poll_num;
  }
  return 0;
}

static inline int mprud_inner_poll(uint64_t *posted_cnt, uint64_t *polled_cnt, int Iam)
{
  int batch = 1;  // Try polling one WR for each inner qp

  struct ibv_wc tmp_wc[batch];
  memset(tmp_wc, 0, sizeof(struct ibv_wc) * batch);

  if (*posted_cnt > *polled_cnt){

    struct ibv_cq *cq = (Iam == MP_SERVER ? mpctx.inner_qps[mpctx.poll_turn]->recv_cq : mpctx.inner_qps[0]->send_cq);
#ifdef USE_MPRUD
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc, 1); // Go to original poll_cq
#else
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc);
#endif


    if (now_polled > 0){
#ifdef MG_DEBUG_MODE
      printf("[Inner Poll] %d\n", now_polled);
      mprud_print_inner_buffer();
#endif
      *polled_cnt += now_polled;
      // Move to next cq only when one polling was successful
      if (Iam == MP_SERVER)
        mpctx.poll_turn = (mpctx.poll_turn + 1) % MPRUD_NUM_PATH;

      for (int i = 0; i < now_polled; i++) {
        if (tmp_wc[i].status != IBV_WC_SUCCESS) {
          fprintf(stderr, "[MPRUD] Poll send CQ error status=%u qp %d\n", tmp_wc[i].status,(int)tmp_wc[i].wr_id);
        }
      }
    } else if (now_polled < 0){
      printf("ERROR: Polling result is negative!\n");
      return FAILURE;
    }
  }
  return SUCCESS;
}

int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc)
{
  uint64_t *posted_cnt, *polled_cnt;
  int Iam;

  if (mpctx.posted_rcnt.pkt > 0){ // send pkt

    Iam = MP_SERVER;
    posted_cnt = &mpctx.posted_rcnt.pkt; 
    polled_cnt = &mpctx.polled_rcnt.pkt; 

  } else if (mpctx.posted_scnt.pkt > 0){

    Iam = MP_CLIENT;
    posted_cnt = &mpctx.posted_scnt.pkt;
    polled_cnt = &mpctx.polled_scnt.pkt;

  }
#ifdef MG_DEBUG_MODE
  else {
    printf("Both pkt cnt is 0. Meaning it's done?\n");
  }
#endif

  //************************
  // OUTER POLLING
  //************************
  int outer_poll_num = mprud_outer_poll(ne, wc, posted_cnt, polled_cnt, Iam); 
  if (outer_poll_num)
    return outer_poll_num;

  //************************
  // INNER POLLING
  //************************
  if (mprud_inner_poll(posted_cnt, polled_cnt, Iam))
    return -1;
  
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

void mprud_print_inner_buffer()
{
  //int n = MPRUD_BUF_SPLIT_NUM;
  int n = 30;

  printf("\n---------- PRINT BUFFER -----------\n");
  for (int i=0; i<n; i++){
    char* cur = mprud_get_inner_buffer() + i * MPRUD_RECV_BUF_OFFSET;
    /* uint32_t* + 1 => plus sizeof(uint32_t) */
    printf("[#%d] %p |  sid: %u  msg_sqn: %u  pkt_sqn: %u\n", i+1, (uint32_t*) cur, *((uint32_t*)cur + 10), *((uint32_t*)cur+11), *((uint32_t*)cur+12));
    //*((uint32_t*)cur+40),*((uint32_t*)cur+44),*((uint32_t*)cur+48));
  }
}

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

