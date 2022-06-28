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
//struct ibv_ah* ah_list[MPRUD_NUM_PATH];

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
  return mpctx.ah_list;
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

inline int mprud_modify_report_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr_)
{
  struct ibv_qp_attr attr;

/*  memset(&attr, 0, sizeof(attr));

  attr.qp_state        = IBV_QPS_INIT;
  attr.pkey_index      = 0;
  attr.port_num        = my_port;
  attr.qkey            = 0x22222222;*/

  if (qp->context->ops.modify_qp(qp, attr_,
        IBV_QP_STATE      |
        IBV_QP_PKEY_INDEX |
        IBV_QP_PORT       |
        IBV_QP_QKEY)) {
    fprintf(stderr, "Failed to modify QP to INIT\n");
    return FAILURE;
  }

  memset(&attr, 0, sizeof(attr));

  attr.qp_state		= IBV_QPS_RTR;

  if (qp->context->ops.modify_qp(qp, &attr, IBV_QP_STATE)) {
    fprintf(stderr, "Failed to modify QP to RTR\n");
    return FAILURE;
  }

  memset(&attr, 0, sizeof(attr));

  attr.qp_state	    = IBV_QPS_RTS;
  attr.sq_psn	    = 0;

  if (qp->context->ops.modify_qp(qp, &attr,
        IBV_QP_STATE |
        IBV_QP_SQ_PSN)) {
    fprintf(stderr, "Failed to modify QP to RTS\n");
    return FAILURE;
  }

  return SUCCESS;
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
  }

  // Create QP for reporting
  struct ibv_cq *send_cq;
  send_cq = pd->context->ops.create_cq(pd->context, mpctx.send_size * 2, NULL, 0);
  // Need recv_cq for each inner QPs
  struct ibv_cq *recv_cq;
  recv_cq = pd->context->ops.create_cq(pd->context, mpctx.recv_size * 2, NULL, 0);

  iqp_init_attr.send_cq = send_cq;
  iqp_init_attr.recv_cq = recv_cq;
  iqp_init_attr.qp_type = IBV_QPT_UD;

  mpctx.mp_manager.report_qp = pd->context->ops.create_qp(pd, &iqp_init_attr);   


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
    // Modify State here  
    ret = mprud_modify_report_qp(mpctx.mp_manager.report_qp, &iqp_attr);
    if (ret){
      printf("Failed to modify Report QP state\n");
      return FAILURE;
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
#ifdef MG_DEBUG_MODE
    for(int i =0; i<MPRUD_NUM_PATH; i++){
      struct ibv_qp_attr attr;
      struct ibv_qp_init_attr init_attr;
      ibv_query_qp(mpctx.inner_qps[i],&attr, 0, &init_attr);
      printf("[%d] qkey: %u qp_num: %u  dlid: %d  dest_qp_num: %u\n", i, attr.qkey, mpctx.inner_qps[i]->qp_num, attr.ah_attr.dlid, attr.dest_qp_num);
    } 
    printf("Report QP NUM: %d\n", mpctx.mp_manager.report_qp->qp_num);
#endif
    
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
    mpctx.ah_list[i] = ibv_create_ah(pd, ah_attr);
    if (!ah_attr){
      printf("Unable to create address handler for UD QP\n");
      return FAILURE;
    }
  }
  printf("MPRUD created ah_list.\n");
  return SUCCESS;
}

void mprud_set_outer_buffer(void* ptr, int buf_size)
{
#ifdef MG_DEBUG_MODE
  printf("Set Outer Buffer to [%p]\n", ptr);
#endif

  //outer_buf = ptr;
  mpctx.outer_buf = ptr;
  // ack buf address
  mpctx.mp_manager.ack_base_addr = ptr + buf_size - (sizeof(uint32_t) * MPRUD_NUM_PATH) - MPRUD_GRH_SIZE;
}

static inline int mprud_window_store_ctx(struct ibv_qp *ibqp, struct ib_send_wr *send_wr, struct ib_recv_wr *recv_wr, struct ib_send_wr **bad_wr, int split_num)
{
  if (mpctx.win.is_full){
    printf("MP Window is full. Cannot store more ctx.\n");
    return FAILURE;
  }

  int* cur = &mpctx.win.cur;

  mpctx.win.mp_win[*cur].post_ctx.ibqp   = ibqp;
  mpctx.win.mp_win[*cur].post_ctx.send_wr = send_wr;
  mpctx.win.mp_win[*cur].post_ctx.recv_wr = recv_wr;
  mpctx.win.mp_win[*cur].post_ctx.bad_wr = bad_wr;
  mpctx.win.mp_win[*cur].split_num = split_num; 
  

  // calculate comp_needed here
  struct path_manager *manager = &mpctx.mp_manager;
  
  int q = split_num / MPRUD_NUM_PATH; // quotient
  int r = split_num - MPRUD_NUM_PATH * q; // remainder
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    manager->comp_needed[i][*cur] = q;
  }
  int post_turn = mpctx.post_turn;
  for (int i=0; i< r; i++){
    manager->comp_needed[post_turn++][*cur] += 1;

    post_turn = (post_turn == MPRUD_NUM_PATH ? 0 : post_turn);
  }
#ifdef MG_DEBUG_MODE
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    printf("[#%d] ", i);
    for (int j=0; j<MPRUD_WINDOW_SIZE; j++){
      printf("%d ", manager->comp_needed[i][j]);
    }
    printf("\n");
  }
#endif
  *cur = (*cur + 1) % MPRUD_WINDOW_SIZE;
  if (*cur == mpctx.win.head){
    // window 'cur' reached 'head'
    mpctx.win.is_full = 1;
  }

  return SUCCESS;
}


static void mprud_write_ack_buf()
{
  char *base = mpctx.mp_manager.ack_base_addr;

  for (int i=0; i<MPRUD_NUM_PATH; i++){
    memcpy(base + i*4, &mpctx.mp_manager.qp_comp_tot[i], sizeof(uint32_t));
  }
#ifdef MG_DEBUG_MODE 
  printf("Write ACK data to buf(%p)\n", base);
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    printf("%u  \n", *(base + i * 4));
  }
#endif
}

static inline int mprud_post_send_ack()
{
  struct ibv_qp *ack_qp = mpctx.mp_manager.report_qp;
  struct ibv_sge sg;
  struct ibv_send_wr wr;
  struct ibv_send_wr *bad_wr;

  struct mp_window *mp_win = &mpctx.win.mp_win[mpctx.win.head];

  int path_num = rand() % MPRUD_NUM_PATH;
  memset(&sg, 0, sizeof(sg));
  sg.addr	  = mpctx.mp_manager.ack_base_addr;// + mpctx.post_turn * 4;
  sg.length = sizeof(uint32_t) * MPRUD_NUM_PATH;
  sg.lkey	  = mp_win->post_ctx.recv_wr->sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.ud.ah   = mpctx.ah_list[path_num]; // USE Random path 
  wr.wr.ud.remote_qkey = 0x22222222;
  /* RC QP stores dest_qp_num, while UD QP does not.
   * Therefore, must use RC QP.
   * (Also possible for UD if app delivers this data.) */
  wr.wr.ud.remote_qpn = mpctx.dest_qp_num + (MPRUD_NUM_PATH + 1);

#ifdef MG_DEBUG_MODE
  printf("ack addr: %p  length: %d\n", sg.addr, sg.length);
  printf("ACK data use path_num=%d & remote_qpn=%d\n", path_num, wr.wr.ud.remote_qpn);
#endif

  int err = mp_win->post_ctx.ibqp->context->ops.post_send(ack_qp, &wr, &bad_wr, 1);
  if (err)
    return err;
#ifdef MG_DEBUG_MODE
  printf("#%d post send done.\n", mpctx.posted_scnt.ack);
#endif
  mpctx.posted_scnt.ack++;
  return SUCCESS;
}

static inline int mprud_post_recv_ack()
{
  struct ibv_qp *ack_qp = mpctx.mp_manager.report_qp;
  struct ibv_sge sg;
  struct ibv_recv_wr wr;
  struct ibv_recv_wr *bad_wr;

  struct mp_window *mp_win = &mpctx.win.mp_win[mpctx.win.head];

  memset(&sg, 0, sizeof(sg));
  sg.addr	  = mpctx.mp_manager.ack_base_addr;// + mpctx.post_turn * 4 * MPRUD_NUM_PATH;
  sg.length = sizeof(uint32_t) * MPRUD_NUM_PATH + MPRUD_GRH_SIZE;
  sg.lkey	  = mp_win->post_ctx.send_wr->sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;

  printf("ack addr: %p  length: %d\n", sg.addr, sg.length);

    
 // if (ibqp->context->ops.post_recv(ack_qp, &wr, &bad_wr, 1))
  if (mp_win->post_ctx.ibqp->context->ops.post_recv(ack_qp, &wr, &bad_wr, 1))
    return FAILURE;

#ifdef MG_DEBUG_MODE
  printf("#%d post recv done.\n", mpctx.posted_rcnt.ack);
#endif
  mpctx.posted_rcnt.ack++;
  return SUCCESS;
}

static inline int mprud_poll_ack(struct ibv_cq *cq, int is_client)
{
  struct ibv_wc wc;
  int ne;

  ne = cq->context->ops.poll_cq(cq, 1, &wc, 1); // Go to original poll_cq
  if (ne > 0) {


    if (is_client){
      mpctx.polled_rcnt.ack++;
      // when success, move the head
      mpctx.win.head = (mpctx.win.head + 1) % MPRUD_WINDOW_SIZE;
      mpctx.win.is_full = 0; // mark it is available

      //mprud_update_path();

    } else {
      mpctx.polled_scnt.ack++;

    }

    // check error
    if (wc.status != IBV_WC_SUCCESS) {
      fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", 
          ibv_wc_status_str(wc.status),
          wc.status, (int)wc.wr_id);
      return FAILURE;
    }

  }

  else if (ne < 0) {
    fprintf(stderr, "ibv_poll_cq() failed\n");
    return FAILURE;
  }


  return SUCCESS;
}

inline int mprud_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{

  struct ibv_ah **ah_list = mprud_get_ah_list();
  uint32_t size = wr->sg_list->length;
  int i, err;
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
  int ne;

  uint64_t *posted_cnt = &mpctx.posted_scnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_scnt.pkt;

  // store posting ctx
  mprud_window_store_ctx(ibqp, wr, NULL, bad_wr, split_num);

  err = mprud_post_recv_ack(ibqp, wr);
  if (err){
    printf("ERROR while ACK post_recv!\n");
    return err;
  }

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
#ifdef USE_REVERSE_POST
  char *last_addr = mpctx.outer_buf + (split_num-1) * MPRUD_DEFAULT_MTU;
  for (i = 0; i < split_num; i++){
    tmp_sge->addr = last_addr - i * MPRUD_DEFAULT_MTU;
#ifdef MG_DEBUG_MODE
    printf("last_addr: %p   target addr: %p\n", last_addr, tmp_sge->addr);
#endif
#else
  for (i = 0; i < split_num; i++){
    tmp_sge->addr = wr->sg_list->addr + i * MPRUD_DEFAULT_MTU;
#endif
    tmp_sge->length = MPRUD_SEND_BUF_OFFSET;
    tmp_wr->sg_list = tmp_sge;
    
    // set ah & remote_qpn
    tmp_wr->wr.ud.ah = mpctx.ah_list[mpctx.post_turn];
    tmp_wr->wr.ud.remote_qkey = 0x22222222;
    /*
    if (ibqp->qp_type == IBV_QPT_UD)
      tmp_wr->wr.ud.remote_qpn = wr->wr.ud.remote_qpn + (mpctx.post_turn + 1);
    else*/
    tmp_wr->wr.ud.remote_qpn = mpctx.dest_qp_num + (mpctx.post_turn + 1);


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
    /*
    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd) {
      if (mprud_poll_cq(mpctx.inner_qps[0]->send_cq, MPRUD_POLL_BATCH, wc, 1)) // inner poll & update count included
        return FAILURE; //skip outer poll
    }
    */
  }

  // poll ack here
  do {
    err = mprud_poll_ack(mpctx.mp_manager.report_qp->recv_cq, MP_CLIENT);
    if (err){
      printf("ERROR while polling ack.\n");
      return err;
    }
  } while (mpctx.win.is_full);

  return SUCCESS;
}

inline int mprud_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{
//  uint32_t size_with_grh = wr->sg_list->length;
//  uint32_t size = size_with_grh - MPRUD_GRH_SIZE;
  uint32_t size = wr->sg_list->length;
  int i, err;
  last_size = size % MPRUD_DEFAULT_MTU ? size % MPRUD_DEFAULT_MTU : MPRUD_DEFAULT_MTU;
  split_num = (last_size < MPRUD_DEFAULT_MTU ? size/MPRUD_DEFAULT_MTU + 1 : size/MPRUD_DEFAULT_MTU);
  int ne;

  uint64_t *posted_cnt = &mpctx.posted_rcnt.pkt;
  uint64_t *polled_cnt = &mpctx.polled_rcnt.pkt;

#ifdef MG_DEBUG_MODE
  printf("\n----------------------------------------\n");
  printf("Split into %d post_recvs. Last msg: %d bytes.\n", split_num, last_size);
#endif
  // store posting ctx
  mprud_window_store_ctx(ibqp, NULL, wr, bad_wr, split_num);

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
#ifdef USE_REVERSE_POST
  char *last_addr = mpctx.outer_buf + (split_num-1) * MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE;
  for (i = 0; i < split_num; i++){
    // Use size of default MTU to intetionally overwrite GRH part
    tmp_sge->addr = (last_addr - i * MPRUD_DEFAULT_MTU) - MPRUD_GRH_SIZE;
#ifdef MG_DEBUG_MODE
    printf("last_addr: %p   target addr: %p\n", last_addr, tmp_sge->addr);
#endif
#else
  for (i = 0; i < split_num; i++){
    tmp_sge->addr = mpctx.outer_buf + i * MPRUD_RECV_BUF_OFFSET;
#endif
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
    if (err){
      printf("ERROR while splited post_recv! #%d\n", err);
      return err;
    }

/*    // polling
    while (*posted_cnt - *polled_cnt >= max_outstd){
      if (mprud_poll_cq(mpctx.inner_qps[0]->recv_cq, MPRUD_POLL_BATCH, wc, 1))  // inner poll & update count included
        return FAILURE; // last arg 1 => skip outer poll
    }
    */
  }

  // if window is full, poll the cq first
  while (mpctx.win.is_full){
    //printf("Window is full. Go polling first...\n");
    mprud_poll_cq(mpctx.inner_qps[0]->recv_cq, MPRUD_POLL_BATCH, wc, 1);
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
      *polled_cnt += now_polled;


      // Check if one full message completed
      if (Iam == MP_SERVER){
        struct path_manager *manager = &mpctx.mp_manager;
        manager->qp_comp[mpctx.poll_turn] += now_polled;

#ifdef MG_DEBUG_MODE
        for (int i=0; i<MPRUD_NUM_PATH; i++){
          printf("QP#%d (%d) ",i, manager->qp_comp[i]);
        }
        printf("\n");
#endif

        // mp_window's head
        int comp_flag = 1;
#ifdef MG_DEBUG_MODE
        printf("  cur comp\tneeded comp\n");
#endif
        for (int i=0; i<MPRUD_NUM_PATH; i++){
          uint32_t comp = manager->qp_comp[i];
#ifdef MG_DEBUG_MODE
          printf("[#%d]\t%d|\t%d\n",i, comp, manager->comp_needed[i][mpctx.win.head]);
#endif
          if (comp < manager->comp_needed[i][mpctx.win.head]){
            comp_flag = 0;
            printf("Need completion at least from QP#%d\n", i);
            break;
          }
        }
        // Can make one completion!
        if (comp_flag){
          // update comp arrays
          for (int i=0; i<MPRUD_NUM_PATH; i++){
            manager->qp_comp_tot[i] += manager->qp_comp[i];
            manager->qp_comp[i] -= manager->comp_needed[i][mpctx.win.head];
          }
          // write ACK data
          mprud_write_ack_buf();
          // post send ACK
          int err = mprud_post_send_ack();
          if (err){
            printf("ERROR while ACK post_send! %d\n", err);
            return err;
          }

          // when success, move the head
          mpctx.win.head = (mpctx.win.head + 1) % MPRUD_WINDOW_SIZE;
          mpctx.win.is_full = 0; // mark it is available

          // polling the post_send ACK

          err = mprud_poll_ack(manager->report_qp->send_cq, MP_SERVER);
          if (err){
            printf("ERROR while polling ack. %d\n", err);
            return err;
          }
        }
      } 



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
  // ACK polls separately?
  /* else if (mpctx.posted_rcnt.ack > 0){
    
    Iam = MP_CLIENT;
    posted_cnt = &mpctx.posted_rcnt.ack;
    polled_cnt = &mpctx.polled_rcnt.ack;
  } else if (mpctx.posted_scnt.ack > 0){

    Iam = MP_SERVER;
    posted_cnt = &mpctx.posted_scnt.ack;
    polled_cnt = &mpctx.polled_scnt.ack;
  }*/
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
void mprud_set_dest_qp_num(uint32_t qp_num)
{
  mpctx.dest_qp_num = qp_num;
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
  ibv_destroy_qp(mpctx.mp_manager.report_qp);
  printf("Destoryed inner qps\n");
  return SUCCESS;
}

int mprud_destroy_ah_list()
{
  int i;

  for (i=0; i<MPRUD_NUM_PATH; i++){
    if (mpctx.ah_list[i]){
      if (ibv_destroy_ah(mpctx.ah_list[i]))
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


