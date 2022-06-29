/*MPRUD by mingman*/

#include <infiniband/mprud.h>
#include <arpa/inet.h>
#include <stdlib.h>



// Global variable initialization
struct mprud_context mpctx;

// Cleaning target later
int last_size = 0;
int split_num = 1;   // number of splitted requests
/*char *inner_buf = NULL;
char *outer_buf = NULL;*/
struct ibv_ah* ah_list[MPRUD_NUM_PATH];

/*
struct buf_idx {
  uint32_t in;
  uint32_t out;
};

static struct buf_idx send_idx = {0,};
static struct buf_idx recv_idx = {0,};
*/
// y101
/*static char REMOTE_GIDS[4][50] = {
  "0000:0000:0000:0000:0000:ffff:0a00:c905", // 10.0.201.5 spine-1
  "0000:0000:0000:0000:0000:ffff:0a00:c902", // 10.0.201.2 spine-2
  "0000:0000:0000:0000:0000:ffff:0a00:c904", // 10.0.201.4 spine-3
  "0000:0000:0000:0000:0000:ffff:0a00:c903", // 10.0.201.3 spine-4
  //  "0000:0000:0000:0000:0000:ffff:0a00:6503", // 10.0.101.3 spine-1
  //  "0000:0000:0000:0000:0000:ffff:0a00:6504", // 10.0.101.4 spine-2
  //  "0000:0000:0000:0000:0000:ffff:0a00:6505", // 10.0.101.5 spine-3
  //  "0000:0000:0000:0000:0000:ffff:0a00:6502", // 10.0.101.2 spine-4
};*/

// y201
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

/*  for(int i =0; i<MPRUD_NUM_PATH; i++){
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    ibv_query_qp(mpctx.inner_qps[i],&attr, 0, &init_attr);
    printf("[%d] qkey: %u qp_num: %u  dlid: %d  dest_qp_num: %u\n", i, attr.qkey, mpctx.inner_qps[i]->qp_num, attr.ah_attr.dlid, attr.dest_qp_num);
  } 
*/

  return SUCCESS;
}

int mprud_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask)
{
  int ret;
  struct ibv_qp_attr iqp_attr;
  memcpy(&iqp_attr, attr, sizeof(struct ibv_qp_attr));
  if (attr->qp_state == IBV_QPS_INIT){

    // INIT
    iqp_attr.qkey = 0x22222222;
    iqp_attr.pkey_index = 0;
    attr_mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY;
    for (int i = 0; i < MPRUD_NUM_PATH; i++){

        ret = qp->context->ops.modify_qp(mpctx.inner_qps[i], &iqp_attr, attr_mask);
      if (ret){
        printf("Failed to modify inner QP state to INIT\n");
        return FAILURE;
      }
    }

  } else if (attr->qp_state == IBV_QPS_RTR){

    // RTR
    attr_mask = IBV_QP_STATE;
    for (int i = 0; i < MPRUD_NUM_PATH; i++){

        ret = qp->context->ops.modify_qp(mpctx.inner_qps[i], &iqp_attr, attr_mask);
      if (ret){
        printf("Failed to modify inner QP state to RTR\n");
        return FAILURE;
      }
    }
    
  } else if (attr->qp_state == IBV_QPS_RTS){

    // RTS
    attr_mask = IBV_QP_STATE | IBV_QP_SQ_PSN;
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
  printf("MPRUD created ah_list.\n");
  return SUCCESS;
}

void mprud_set_outer_buffer(void* ptr, uint32_t len)
{

  mpctx.outer_buf = ptr;
  // register other base ptr
  mpctx.buf.sub = ptr + len;
  mpctx.buf.send = mpctx.buf.sub + (MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE) * MPRUD_NUM_PATH;
  mpctx.buf.recv = mpctx.buf.send + MPRUD_SEND_BUF_SIZE;;

#ifdef MG_DEBUG_MODE
  printf("Set Outer Buffer to [%p]\n", ptr);
  printf("sub: %p  send: %p  recv: %p\n", mpctx.buf.sub, mpctx.buf.send, mpctx.buf.recv);
#endif

}

void mprud_set_dest_qp_num(uint32_t qp_num)
{
  mpctx.dest_qp_num = qp_num;
}

void mprud_store_wr(struct ibv_send_wr *swr, struct ibv_recv_wr *rwr)
{
  struct mprud_wqe *wqe = &mpctx.wqe_table.wqe[mpctx.wqe_table.next];
  wqe->id = mpctx.wqe_table.sqn;
  if (swr != NULL)
    memcpy(&wqe->swr, swr, sizeof(struct ibv_send_wr));
  if (rwr != NULL)
    memcpy(&wqe->rwr, rwr, sizeof(struct ibv_recv_wr));

//  printf("wqe->id: %lu   sqn: %u  next: %u  head: %u\n", wqe->id, mpctx.wqe_table.sqn, mpctx.wqe_table.next, mpctx.wqe_table.head);
  mpctx.wqe_table.sqn += 1;
  mpctx.wqe_table.next += 1;
  if (mpctx.wqe_table.next == MPRUD_TABLE_LEN)
    mpctx.wqe_table.next = 0;
}

inline int mprud_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
  mprud_store_wr(wr, NULL);

  struct ibv_ah **ah_list = mprud_get_ah_list();

  uint32_t size = wr->sg_list->length;
  int i, err;
#ifdef MPRUD_CHUNK_MODE
  /* Only when MPRUD_NUM_PATH less than default MTU */
  uint32_t chunk_size = size / MPRUD_NUM_PATH;
  uint32_t left = size - chunk_size * MPRUD_NUM_PATH;
  // always let the last QP handles the left size
  uint32_t iter_each = (chunk_size / MPRUD_DEFAULT_MTU);
  uint32_t last_size = chunk_size % MPRUD_DEFAULT_MTU;
  if (last_size)
    iter_each += 1;
  uint32_t iter = iter_each * MPRUD_NUM_PATH;
  if (left > 0 && left <= MPRUD_DEFAULT_MTU)
    iter += 1;
  
  printf("chunk_size: %u  left: %u iter_each: %u  iter: %u last_size: %u\n", chunk_size, left, iter_each, iter, last_size);

  uint64_t base_addr[MPRUD_NUM_PATH];
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    base_addr[i] = wr->sg_list->addr + chunk_size * (i+1);
    if (i == MPRUD_NUM_PATH - 1)
      base_addr[i] += left;
    printf("base address #%d: %p\n", i, base_addr[i]);
  }
#else
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
#endif
  int ne;

  uint64_t *posted_cnt = &mpctx.posted_scnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_scnt.pkt;

#ifdef MG_DEBUG_MODE
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

#ifdef MG_DEBUG_MODE
  printf("tmp_sge->addr: %lx length: %d\nMessage: %s\n",tmp_sge->addr, wr->sg_list->length, (char*)tmp_sge->addr);
#endif

  /**
   * Message Splitting
   */
  int max_outstd = 256;
#ifdef MPRUD_CHUNK_MODE
  int chk_turn, msg_length;
  for (i = 0; i < iter; i++){
    chk_turn = i % MPRUD_NUM_PATH;
    tmp_sge->addr = base_addr[chk_turn] - (i/MPRUD_NUM_PATH) * MPRUD_DEFAULT_MTU;

    if (i == iter-1){
      // last iter
      msg_length = left;
      tmp_sge->addr += MPRUD_DEFAULT_MTU - msg_length;
      // CAUTION: Use last QP available
      chk_turn = MPRUD_NUM_PATH - 1;
    }
    else if (i % iter_each == 0){
      // chunk last msg
      msg_length = last_size;
      tmp_sge->addr += MPRUD_DEFAULT_MTU - msg_length;

    } else {
      msg_length = MPRUD_DEFAULT_MTU;
    }
    tmp_sge->length = msg_length;

    tmp_wr->sg_list = tmp_sge;
    // 4. set ah & remote_qpn
    tmp_wr->wr.ud.ah = ah_list[chk_turn];
    tmp_wr->wr.ud.remote_qpn = mpctx.dest_qp_num + (chk_turn + 1);
    tmp_wr->wr.ud.remote_qkey = 0x22222222;

    printf("[#%d] addr: %lx   length: %d\n", i, tmp_wr->sg_list->addr, tmp_wr->sg_list->length);

    // 5. post_send
    err = ibqp->context->ops.post_send(mpctx.inner_qps[chk_turn], tmp_wr, bad_wr, 1); // original post send
    *posted_cnt += 1;
#else
#ifdef MG_DEBUG_MODE
    printf("last_addr: %p   target addr: %p\n", last_addr, tmp_sge->addr);
#endif
    for (i = 0; i < split_num; i++){
      tmp_sge->addr = wr->sg_list->addr + i * MPRUD_DEFAULT_MTU;
      tmp_sge->length = MPRUD_SEND_BUF_OFFSET;
      tmp_wr->sg_list = tmp_sge;
      // 4. set ah & remote_qpn
    tmp_wr->wr.ud.ah = ah_list[mpctx.post_turn];
    tmp_wr->wr.ud.remote_qpn = mpctx.dest_qp_num + (mpctx.post_turn + 1);
    tmp_wr->wr.ud.remote_qkey = 0x22222222;

#ifdef MG_DEBUG_MODE
    printf("mpctx.post_turn: %d  wr_id:%lu\n", mpctx.post_turn, mpctx.wqe_table.wqe[mpctx.wqe_table.next-1].id);
#endif

    // 5. post_send
    err = ibqp->context->ops.post_send(mpctx.inner_qps[mpctx.post_turn], tmp_wr, bad_wr, 1); // original post send
    mpctx.post_turn = (mpctx.post_turn + 1) % MPRUD_NUM_PATH;
    *posted_cnt += 1;

#ifdef MG_DEBUG_MODE
    printf("posted: %lu   polled: %lu  diff: %lu\n", *posted_cnt, *polled_cnt, *posted_cnt-*polled_cnt);
#endif
#endif
    if (err){
      printf("ERROR while splited post_send!\n");
      return err;
    }
    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd) {
      if (mprud_poll_cq(mpctx.inner_qps[0]->send_cq, MPRUD_POLL_BATCH, wc, 1)) // inner poll & update count included
        return FAILURE; //skip outer poll
    }
  }

  return SUCCESS;
}

inline int mprud_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
  mprud_store_wr(NULL, wr);

  int i, err;
  uint32_t size = wr->sg_list->length;
  if(ibqp->qp_type == IBV_QPT_UD)
    size -= MPRUD_GRH_SIZE;
#ifdef MPRUD_CHUNK_MODE
  /* Only when MPRUD_NUM_PATH less than default MTU */
  uint32_t chunk_size = size / MPRUD_NUM_PATH;
  uint32_t left = size - chunk_size * MPRUD_NUM_PATH;
  // always let the last QP handles the left size
  uint32_t iter_each = (chunk_size / MPRUD_DEFAULT_MTU);
  uint32_t last_size = chunk_size % MPRUD_DEFAULT_MTU;
  if (last_size)
    iter_each += 1;
  uint32_t iter = iter_each * MPRUD_NUM_PATH;
  if (left > 0 && left <= MPRUD_DEFAULT_MTU)
    iter += 1;


  printf("chunk_size: %u  left: %u iter_each: %u  iter: %u last_size: %u\n", chunk_size, left, iter_each, iter, last_size);
  uint64_t base_addr[MPRUD_NUM_PATH];
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    base_addr[i] = wr->sg_list->addr + chunk_size * (i+1) + MPRUD_GRH_SIZE;
    if (i == MPRUD_NUM_PATH - 1)
      base_addr[i] += left;

    printf("base address #%d: %p\n", i, base_addr[i]);
  }
#else
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
  int ne;
#endif

  uint64_t *posted_cnt = &mpctx.posted_rcnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_rcnt.pkt;

#ifdef MG_DEBUG_MODE
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

  /**
   * Message Splitting
   */
//  int max_outstd = MIN(mpctx.recv_size * MPRUD_NUM_PATH, MPRUD_BUF_SPLIT_NUM);  // MIN(inner queue capacity, buffer capacity)
  int max_outstd = 512;
#ifdef MPRUD_CHUNK_MODE
  int chk_turn, msg_length;
  for (i = 0; i < iter; i++){
    chk_turn = i % MPRUD_NUM_PATH;
    tmp_sge->addr = base_addr[chk_turn] - (i/MPRUD_NUM_PATH) * MPRUD_DEFAULT_MTU;

    if (i == iter-1){
        // last iter
        msg_length = left;
        tmp_sge->addr += MPRUD_DEFAULT_MTU - msg_length;
        // CAUTION: Use last QP available
        chk_turn = MPRUD_NUM_PATH - 1;
      }
      else if (i % iter_each == 0){
        // chunk last msg
        msg_length = last_size;
        tmp_sge->addr += MPRUD_DEFAULT_MTU - msg_length;

      } else {
        msg_length = MPRUD_DEFAULT_MTU;
      }
      tmp_sge->length = msg_length + MPRUD_GRH_SIZE;
      tmp_sge->addr -= MPRUD_GRH_SIZE;

      tmp_wr->sg_list = tmp_sge;
      
    printf("[#%d] addr: %lx   length: %d\n", i, tmp_wr->sg_list->addr, tmp_wr->sg_list->length);
      
#ifdef MG_DEBUG_MODE
      printf("[MPRUD] Msg #%d) addr: %lx\tlength: %u\n", i, tmp_sge->addr, tmp_sge->length);
#endif

      printf("addr: %lx   length: %d\n", tmp_wr->sg_list->addr, tmp_wr->sg_list->length);
      // 3. post_recv
      err = ibqp->context->ops.post_recv(mpctx.inner_qps[chk_turn], tmp_wr, bad_wr, 1);  // original post recv

      chk_turn = (chk_turn + 1) % MPRUD_NUM_PATH;
      *posted_cnt += 1;
#else
#ifdef MG_DEBUG_MODE
    printf("last_addr: %p   target addr: %p\n", last_addr, tmp_sge->addr);
#endif
  for (i = 0; i < split_num; i++){
    tmp_sge->addr = mpctx.outer_buf + i * MPRUD_RECV_BUF_OFFSET;
    //tmp_sge->addr = wr->sg_list->addr + i * MPRUD_RECV_BUF_OFFSET;
    tmp_sge->length = MPRUD_RECV_BUF_OFFSET;

#ifdef MG_DEBUG_MODE
    printf("[MPRUD] Msg #%d) addr: %lx\tlength: %u\n", i, tmp_sge->addr, tmp_sge->length);
#endif
    tmp_wr->sg_list = tmp_sge;

    // 3. post_recv
    err = ibqp->context->ops.post_recv(mpctx.inner_qps[mpctx.post_turn], tmp_wr, bad_wr, 1);  // original post recv

    mpctx.post_turn = (mpctx.post_turn + 1) % MPRUD_NUM_PATH;
    *posted_cnt += 1;
#ifdef MG_DEBUG_MODE
    printf(">> posted! [%lu] (turn=%d)\n", *posted_cnt, mpctx.post_turn);
#endif
#endif
    if (err){
      printf("ERROR while splited post_recv! #%d\n", err);
      return err;
    }
    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd){
      if (mprud_poll_cq(mpctx.inner_qps[0]->recv_cq, MPRUD_POLL_BATCH, wc, 1))  // inner poll & update count included
        return FAILURE; // last arg 1 => skip outer poll
    }
  }
  return SUCCESS;

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
#endif
      // check validity & reordering & data copy
      //mprud_recv_manage(now_polled);

      *polled_cnt += now_polled;
      // Move to next cq only when one polling was successful
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
    // Both pkt cnt is 0. Meaning it's done.
    return 0;
  }
#endif
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
//void mprud_print_inner_buffer()
//{
//  //int n = MPRUD_BUF_SPLIT_NUM;
//  int n = 30;
//
//  printf("\n---------- PRINT BUFFER -----------\n");
//  for (int i=0; i<n; i++){
//    char* cur = mprud_get_inner_buffer() + i * MPRUD_RECV_BUF_OFFSET;
//    /* uint32_t* + 1 => plus sizeof(uint32_t) */
//    printf("[#%d] %p |  sid: %u  msg_sqn: %u  pkt_sqn: %u\n", i+1, (uint32_t*) cur, *((uint32_t*)cur + 10), *((uint32_t*)cur+11), *((uint32_t*)cur+12));
//    //*((uint32_t*)cur+40),*((uint32_t*)cur+44),*((uint32_t*)cur+48));
//  }
//}

int mprud_destroy_inner_qps()
{
  int i;

  for (i=0; i<MPRUD_NUM_PATH; i++){
    if (mpctx.inner_qps[i]){
      if (ibv_destroy_qp(mpctx.inner_qps[i]))
        return FAILURE;
    }
  }
  printf("Destoryed inner qps\n");
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
  printf("Destoryed AH list\n");
  return SUCCESS;
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


