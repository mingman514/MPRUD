/*MPRUD by mingman*/

#include <infiniband/mprud.h>
#include <arpa/inet.h>
#include <stdlib.h>



// Global variable initialization
struct mprud_context mpctx;

// Cleaning target later
int last_size = 0;
int split_num = 1;   // number of splitted requests
char *inner_buf = NULL;
char *outer_buf = NULL;
struct ibv_ah* ah_list[MPRUD_NUM_PATH];


static uint32_t ibuf_idx = 0; // inner buffer idx
static uint32_t obuf_idx = 0; // outer buffer idx 

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

  // Prepare enough queue space
  mpctx.send_size = 128;
  mpctx.recv_size = 512;

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
  
  iqp_init_attr.cap.max_send_wr = mpctx.send_size;
  iqp_init_attr.cap.max_recv_wr = mpctx.recv_size;

  /* automatically allocate memory of cq & qp in verbs */

  for (int i = 0; i < MPRUD_NUM_PATH; i++){
    struct ibv_cq *send_cq;
    send_cq = pd->context->ops.create_cq(pd->context, mpctx.send_size * 2, NULL, 0);
    // Need recv_cq for each inner QPs
    struct ibv_cq *recv_cq;
    recv_cq = pd->context->ops.create_cq(pd->context, mpctx.recv_size * 2, NULL, 0);

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

inline int mprud_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
  struct ibv_ah **ah_list = mprud_get_ah_list();
  if (!ah_list[mpctx.post_turn]){
    printf("[qp.c/mlx5_post_send] AH is NULL!!\n");
    return FAILURE;
  }

  uint32_t size = wr->sg_list->length;
  int i, err;
  //last_size = size % MPRUD_DEFAULT_MTU;
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
  int ne;
  char* sbuf = mprud_get_inner_buffer();

  uint64_t *posted_cnt = &mpctx.posted_scnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_scnt.pkt;

#ifdef MG_DEBUG_MODE
  printf("[DEBUG] sbuf: %p\tsg_list->addr: %lx\n", sbuf, wr->sg_list->addr);//, (uint64_t)sbuf - wr->sg_list->addr);
  printf("\n----------------------------------------\n");
  printf("Split into %d post_sends. Last msg: %d bytes.\n", split_num, last_size);
  printf("recv_size=%d | send_size=%d for each inner qp\n", mpctx.recv_size, mpctx.send_size);

#endif

  struct ibv_wc *wc = NULL;
  ALLOCATE(wc, struct ibv_wc, MPRUD_POLL_BATCH);

  /**
   * Work Request Setting
   */
  //wr->send_flags = 0;
  wr->send_flags = IBV_SEND_SIGNALED;

  // Edit sg_list values
  struct ibv_send_wr *tmp_wr;
  ALLOCATE(tmp_wr, struct ibv_send_wr, 1);
  memcpy(tmp_wr, wr, sizeof(struct ibv_send_wr));

  struct ibv_sge *tmp_sge;
  ALLOCATE(tmp_sge, struct ibv_sge, 1);
  memcpy(tmp_sge, wr->sg_list, sizeof(struct ibv_sge));

  tmp_sge->addr = (uint64_t) sbuf;
#ifdef MG_DEBUG_MODE
  printf("tmp_sge->addr: %lx  %s\n",tmp_sge->addr, (char*)tmp_sge->addr);
#endif

  /**
   * Message Splitting
   */
  char *cur;
//  int max_outstd = MIN(mpctx.send_size * MPRUD_NUM_PATH, MPRUD_BUF_SPLIT_NUM);  // MIN(inner queue capacity, buffer capacity)
  int max_outstd = 128;
  for (i = 0; i < split_num; i++, ibuf_idx++){
    ibuf_idx = ibuf_idx % MPRUD_BUF_SPLIT_NUM;
    cur = sbuf + ibuf_idx * MPRUD_SEND_BUF_OFFSET;  // MPRUD_HEADER_SIZE + MPRUD_DEFAULT_MTU
    /* initialize buffer */
    memset(cur, 0, MPRUD_SEND_BUF_OFFSET);

    // 1. write header
    struct mprud_header mh = {
      .sid = 128,
      .msg_sqn = mpctx.msg_sqn, // global variable
      .pkt_sqn = i,
    };
    memcpy(cur, &mh, MPRUD_HEADER_SIZE);

    // 2. copy data
    memcpy(cur + MPRUD_HEADER_SIZE, (char*)wr->sg_list->addr + i*MPRUD_DEFAULT_MTU, (i == split_num-1 && last_size ? last_size : MPRUD_DEFAULT_MTU))     ;
#ifdef MG_DEBUG_MODE
    printf("[MPRUD] header in buffer: sid=%u msg_sqn=%u pkt_sqn=%u\n", *(uint32_t*)cur, *((uint32_t*)cur + 1), *((uint32_t*)cur + 2));
    printf("[MPRUD] data in buffer: %s\n", cur+MPRUD_HEADER_SIZE);
#endif

    // 3. replace wr
    tmp_sge->addr = (uint64_t) cur;
    tmp_sge->length = MPRUD_SEND_BUF_OFFSET;

    tmp_wr->sg_list = tmp_sge;

    // 4. set ah & remote_qpn
    tmp_wr->wr.ud.ah = ah_list[mpctx.post_turn];
    tmp_wr->wr.ud.remote_qpn = wr->wr.ud.remote_qpn + (mpctx.post_turn + 1);


#ifdef MG_DEBUG_MODE
    printf("mpctx.post_turn= %d\n", mpctx.post_turn);
#endif

    // set WR SIGNALED FLAG
    //if (i == split_num-1)
    //tmp_wr->send_flags = IBV_SEND_SIGNALED;

    /* QP selection */

    // 5. post_send
    err = ibqp->context->ops.post_send(mpctx.inner_qps[mpctx.post_turn], tmp_wr, bad_wr, 1); // original post send
    mpctx.post_turn = (mpctx.post_turn + 1) % MPRUD_NUM_PATH;
    *posted_cnt += 1;

#ifdef MG_DEBUG_MODE
    printf("posted: %lu   polled: %lu  diff: %lu\n", *posted_cnt, *polled_cnt, *posted_cnt-*polled_cnt);
#endif
    if (err){
      printf("ERROR while splited post_send!\n");
      return err;
    }
    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd) {
      if (mprud_poll_cq(mpctx.inner_qps[0]->send_cq, MPRUD_POLL_BATCH, wc, 1)) // inner poll & update count included
        return FAILURE; //skip outer poll
     /* if (ne > 0){
        *polled_cnt += ne;
#ifdef MG_DEBUG_MODE
        printf("  ---> [INNER POLL] %d\n\ttotal posted: %lu  total polled: %lu  send_size: %d\n", ne, *posted_cnt, *polled_cnt, mpctx.send_size);
#endif
        for (int i = 0; i < ne; i++) {
          if (wc[i].status != IBV_WC_SUCCESS) {
            fprintf(stderr, "[MPRUD] Poll send CQ error status=%u qp %d\n", wc[i].status,(int)wc[i].wr_id);
          }
        }
      } else if (ne < 0){
        fprintf(stderr, "[MPRUD] IPS poll CQ failed %d\n", ne);
        free(wc);
        return FAILURE;
      }*/
    }
  }
  mpctx.msg_sqn++;

  return SUCCESS;
}

inline int mprud_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
  uint32_t size_with_grh = wr->sg_list->length;
  uint32_t size = size_with_grh - MPRUD_GRH_SIZE;
  int i, err;
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
  int ne;
  char* rbuf = mprud_get_inner_buffer();

  uint64_t *posted_cnt = &mpctx.posted_rcnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_rcnt.pkt;

#ifdef MG_DEBUG_MODE
  printf("[DEBUG] rbuf: %p\tsg_list->addr: %lx\n", rbuf, wr->sg_list->addr);//, (uint64_t)rbuf - wr->sg_list->addr);
  printf("\n----------------------------------------\n");
  printf("Split into %d post_recvs. Last msg: %d bytes.\n", split_num, last_size);

#endif

  struct ibv_wc *wc = NULL;
  ALLOCATE(wc, struct ibv_wc, MPRUD_POLL_BATCH);


  // Edit sg_list values
  struct ibv_recv_wr *tmp_wr;
  ALLOCATE(tmp_wr, struct ibv_recv_wr, 1);
  memcpy(tmp_wr, wr, sizeof(struct ibv_recv_wr));

  struct ibv_sge *tmp_sge;
  ALLOCATE(tmp_sge, struct ibv_sge, 1);
  memcpy(tmp_sge, wr->sg_list, sizeof(struct ibv_sge));

  tmp_sge->addr = (uint64_t) rbuf;
#ifdef MG_DEBUG_MODE
  printf("Data to changed buf ---> %lx\n", tmp_sge->addr);
#endif

  /**
   * Message Splitting
   */
  char *cur;
//  int max_outstd = MIN(mpctx.recv_size * MPRUD_NUM_PATH, MPRUD_BUF_SPLIT_NUM);  // MIN(inner queue capacity, buffer capacity)
  int max_outstd = 512;

  for (i = 0; i < split_num; i++, ibuf_idx++){
    // 1. receive request
    ibuf_idx = ibuf_idx % MPRUD_BUF_SPLIT_NUM;
    cur = rbuf + ibuf_idx * MPRUD_RECV_BUF_OFFSET; // MPRUD_GRH_SIZE + MPRUD_HEADER_SIZE + MPRUD_DEFAULT_MTU
    /* initialize buffer */
    memset(cur, 0, MPRUD_RECV_BUF_OFFSET);

    tmp_sge->addr = (uint64_t)cur;
    tmp_sge->length = MPRUD_RECV_BUF_OFFSET;

#ifdef MG_DEBUG_MODE
    printf("[MPRUD] Msg #%d) addr: %lx\tlength: %u\n", i, tmp_sge->addr, tmp_sge->length);
#endif
    tmp_wr->sg_list = tmp_sge;

    // 2. post_recv
    //err = mlx5_post_recv2(ibqp, tmp_wr, bad_wr);
    err = ibqp->context->ops.post_recv(mpctx.inner_qps[mpctx.post_turn], tmp_wr, bad_wr, 1);  // original post recv

    mpctx.post_turn = (mpctx.post_turn + 1) % MPRUD_NUM_PATH;
    *posted_cnt += 1;
#ifdef MG_DEBUG_MODE
    printf(">> posted! [%lu] (turn=%d)\n", *posted_cnt, mpctx.post_turn);
#endif
    if (err){
      printf("ERROR while splited post_recv! #%d\n", err);
      return err;
    }
    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd){
      if (mprud_poll_cq(mpctx.inner_qps[0]->recv_cq, MPRUD_POLL_BATCH, wc, 1))  // inner poll & update count included
        return FAILURE; // last arg 1 => skip outer poll

      //ne = mlx5_poll_cq_1(ibqp->recv_cq, MPRUD_POLL_BATCH, wc, 1);
     /* if (ne > 0){
        mpctx.polled_scnt.pkt += ne;
#ifdef MG_DEBUG_MODE
        printf("  ---> [INNER POLL] %d\n\ttotal posted: %lu  total polled: %lu\n", ne, *posted_cnt, *polled_cnt);
#endif
        for (int i = 0; i < ne; i++) {
          if (wc[i].status != IBV_WC_SUCCESS) {
            fprintf(stderr, "[MPRUD] Poll send CQ error status=%u qp %d\n",
                wc[i].status,(int)wc[i].wr_id);
          }
        }
        // Check buffer
#ifdef MG_DEBUG_MODE
        printf("\n---------- PRINT BUFFER [pre-inner polling] -----------\n");
        mprud_print_inner_buffer();
#endif

      } else if (ne < 0){
        fprintf(stderr, "[MPRUD] IPR poll CQ failed: %d\n", ne);
        free(wc);
        return FAILURE;
      }*/
    }
  }
  return SUCCESS;

}

static inline void mprud_syn_buffer(int outer_poll_num)
{
  char* inner_buf = mprud_get_inner_buffer();
  char* outer_buf = mprud_get_outer_buffer();
  char* cur;

  for (int i = 0; i < outer_poll_num; i++){
    for (int j = 0; j < split_num; j++, obuf_idx++){
      obuf_idx = obuf_idx % MPRUD_BUF_SPLIT_NUM;
      cur = inner_buf + obuf_idx * MPRUD_RECV_BUF_OFFSET;

      memcpy(outer_buf + (j * MPRUD_DEFAULT_MTU), cur + MPRUD_GRH_SIZE + MPRUD_HEADER_SIZE,(j == split_num - 1 ? last_size: MPRUD_DEFAULT_MTU));
#ifdef MG_DEBUG_MODE
      printf("obuf_idx: %d  inner_cur: %p   outer_cur: %p\n", obuf_idx, cur, outer_buf + (j * MPRUD_DEFAULT_MTU));
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

    struct ibv_cq *cq = (Iam == MP_SERVER ? mpctx.inner_qps[mpctx.poll_turn]->recv_cq : mpctx.inner_qps[mpctx.poll_turn]->send_cq);
#ifdef USE_MPRUD
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc, 1); // Go to original poll_cq
#else
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc);
#endif


    if (now_polled > 0){
#ifdef MG_DEBUG_MODE
      printf("[Inner Poll] %d (qp #%d)\n", now_polled, mpctx.poll_turn);
      //mprud_print_inner_buffer();
#endif
      *polled_cnt += now_polled;
      // Move to next cq only when one polling was successful
      //if (Iam == MP_SERVER)
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

int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc, int skip_outer_poll)
{
  uint64_t *posted_cnt, *polled_cnt;
  int Iam = -1;

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
    //printf("Both pkt cnt is 0. Meaning it's done?\n");
    return 0;
  }
#endif
printf("mprud_poll_cq - Iam: %d  skip: %d\n", Iam, skip_outer_poll);
  //************************
  // OUTER POLLING
  //************************
  if (!skip_outer_poll){
    int outer_poll_num = mprud_outer_poll(ne, wc, posted_cnt, polled_cnt, Iam); 
    if (outer_poll_num)
      return outer_poll_num;
  }

  //************************
  // INNER POLLING
  //************************
  if (mprud_inner_poll(posted_cnt, polled_cnt, Iam))
    return -1;
  
  return 0;
}

/*
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
*/
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

int mprud_destroy_resources()
{
  if (mprud_destroy_inner_qps())
    goto clean_err;
  if (mprud_destroy_ah_list())
    goto clean_err;

  return SUCCESS;
  
clean_err:
  printf("Error while cleaning.\n");
  return FAILURE;
}

int mprud_destroy_inner_qps()
{
  int i;

  for (i=0; i<MPRUD_NUM_PATH; i++){
    if (mpctx.inner_qps[i]){
      if (ibv_destroy_qp(mpctx.inner_qps[i]))
        return FAILURE;
    }
  }
  printf("Destoryed inner qps\n", i);
  return SUCCESS;
}

int mprud_destroy_ah_list()
{
  int i;

  for (i=0; i<MPRUD_NUM_PATH; i++){
    if (ah_list[i]){
      if (ibv_destroy_ah(ah_list[i]))
        return FAILURE;
    }
  }
  printf("Destoryed AH list\n", i);
  return SUCCESS;
}

