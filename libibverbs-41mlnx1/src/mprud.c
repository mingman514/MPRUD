/*MPRUD by mingman*/

#include <infiniband/mprud.h>
#include <arpa/inet.h>
#include <stdlib.h>



// Global variable initialization
struct mprud_context mpctx;
struct path_manager mp_manager;

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

  // Prepare enough queue space
  mpctx.send_size = 128;
  mpctx.recv_size = 512;

  // initialize path manager
  mp_manager.active_num = MPRUD_NUM_PATH;
  for (int i=0; i<MPRUD_NUM_PATH; i++)
    mp_manager.qps[i] = 1;

  mp_manager.qp_stat = &mpctx.qp_stat;
  mp_manager.monitor_flag = 1;
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

static inline float mprud_get_us(struct timeval start, struct timeval end)
{
 return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec); 
}

void mprud_perf_per_qp()
{
  if (!mp_manager.p_start_time_flag){
    mp_manager.p_start_time_flag = 1;
    gettimeofday(&mp_manager.p_start, NULL);
    return ;
  }

  gettimeofday(&mp_manager.p_now, NULL);
  float usec = mprud_get_us(mp_manager.p_start, mp_manager.p_now);
  if (usec < MPRUD_PRINT_PERF_CYCLE)
    return ;
 
  uint64_t tot_bytes = 0;
  for(int i=0; i<MPRUD_NUM_PATH; i++){
    uint64_t bytes = (double)mpctx.qp_stat[i].recv_msg_size;
    tot_bytes += bytes;
    printf("[QP#%d] Total: %lld B   Avg BW: %.2f Gb/s\n",i,bytes, (bytes/(double)(1000*1000*1000/8))/(usec/(1000*1000)));
    mpctx.qp_stat[i].recv_msg_size = 0; 
  }
  printf("  ==>[ALL QP] Total: %lld B   Avg BW: %.2f Gb/s\n\n",tot_bytes, (tot_bytes/(double)(1000*1000*1000/8))/(usec/(1000*1000)));

  // update time
  gettimeofday(&mp_manager.p_start, NULL);

/*    
//-------------------------------------------------
      printf("POSTED\n");
      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.qp_stat[i].posted_rcnt.pkt);

      }
      printf("\nPOLLED\n");

      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.qp_stat[i].polled_rcnt.pkt);
      }
      printf("\nNeeded\n");

      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.wqe_table.wqe[mpctx.wqe_table.head].iter_each[i]);
      }
      printf("\n----------------------------\n");
//-------------------------------------------------
*/
}

void mprud_update_mpqp()
{
  printf("-------- UPDATE MPQP ---------\n");
  printf("Active Num: %d\n", mp_manager.active_num);
  if (mp_manager.mpqp != NULL)
    free(mp_manager.mpqp);

  mp_manager.mpqp = (struct qp_set*) malloc(mp_manager.active_num * sizeof(struct qp_set));

  int idx=0;
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    if (mp_manager.qps[i] == MPRUD_QPS_ACTIVE){
      mp_manager.mpqp[idx].qp = mpctx.inner_qps[i];
      mp_manager.mpqp[idx].idx = i;
      idx++;
#ifdef debugpath
    printf("MPQP #%d (mpqp) --> %d (innerqps)\n", idx, i);
#endif
    }
  }

  if (idx != mp_manager.active_num){
    printf("ERROR: mp_manager.mpqp should be equal to active num\n");
  }
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

  mp_manager.report_qp = pd->context->ops.create_qp(pd, &iqp_init_attr);

  mprud_update_mpqp();

  return SUCCESS;
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

  attr.qp_state   = IBV_QPS_RTR;

  if (qp->context->ops.modify_qp(qp, &attr, IBV_QP_STATE)) {
    fprintf(stderr, "Failed to modify QP to RTR\n");
    return FAILURE;
  }

  memset(&attr, 0, sizeof(attr));

  attr.qp_state     = IBV_QPS_RTS;
  attr.sq_psn     = 0;

  if (qp->context->ops.modify_qp(qp, &attr,
        IBV_QP_STATE |
        IBV_QP_SQ_PSN)) {
    fprintf(stderr, "Failed to modify QP to RTS\n");
    return FAILURE;
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
    ret = mprud_modify_report_qp(mp_manager.report_qp, &iqp_attr);
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
      printf("[%d] qkey: %u qp_num: %u  dlid: %d  dest_qp_num: %u\n", i, attr.qkey, mpctx.inner_qps[i    ]->qp_num, attr.ah_attr.dlid, attr.dest_qp_num);
    }
    printf("Report QP NUM: %d\n", mp_manager.report_qp->qp_num);
#endif
  }
  return SUCCESS;
}

int mprud_create_ah_list(struct ibv_pd *pd,
    struct ibv_qp_init_attr *qp_init_attr)
{
  struct ibv_ah_attr ah_attr;

  printf("REMOTE_GIDS len = %ld  |  Number of Path to use = %d\n", sizeof(REMOTE_GIDS) / sizeof(REMOTE_GIDS[0]), MPRUD_NUM_PATH);
  if (sizeof(REMOTE_GIDS) / sizeof(REMOTE_GIDS[0]) < MPRUD_NUM_PATH){
    printf("ERROR: Specified GIDs are less than the number of paths you are trying to use!\n"); 
    exit(1);
  }

  for (int i=0; i<MPRUD_NUM_PATH; i++){    // num of path can be changed!

    uint8_t temp_gid[16];
    //convert_to_raw_gid(temp_gid, REMOTE_GIDS[i]);
    inet_pton(AF_INET6, REMOTE_GIDS[i], &temp_gid);

    union ibv_gid rem_gid = {
      .global = {
        .subnet_prefix = 0,
        .interface_id = 0,
      }
    };
    memcpy(rem_gid.raw, &temp_gid, sizeof(uint8_t)*16);

    print_gid_info(rem_gid);

    // ah_attr setting
    memset(&ah_attr, 0, sizeof(struct ibv_ah_attr));

    ah_attr.port_num = 1;
    ah_attr.is_global = 1;
    ah_attr.sl = 3;  // service level 3
    // GRH
    ah_attr.grh.dgid = rem_gid;
    ah_attr.grh.sgid_index = 7;  // differs 
    ah_attr.grh.hop_limit = 0xFF;  // default value(64) in perftest
    ah_attr.grh.traffic_class = 106;

    // create ah_attrs for static paths
    mpctx.ah_list[i] = ibv_create_ah(pd, &ah_attr);
    if (!&ah_attr){
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
  mpctx.buf.recv = ptr + len - MPRUD_RECV_BUF_SIZE;
  mpctx.buf.send = mpctx.buf.recv - MPRUD_SEND_BUF_SIZE;
  mpctx.buf.last = mpctx.buf.send - (MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE);
  mpctx.buf.sub = mpctx.buf.last - (MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE) * MPRUD_NUM_PATH;

  mp_manager.send_base_addr = mpctx.buf.send;
  mp_manager.recv_base_addr = mpctx.buf.recv;
  
#ifdef MG_DEBUG_MODE
  printf("Set Outer Buffer to [%p]\n", ptr);
  printf("sub: %p  send: %p  recv: %p\n", mpctx.buf.sub, mpctx.buf.send, mpctx.buf.recv);
#endif

}

void mprud_set_dest_qp_num(uint32_t qp_num)
{
  mpctx.dest_qp_num = qp_num;
}


void mprud_print_qp_status()
{
  struct path_manager *manager = &mp_manager;
  printf("------ QP STATE ------\n");
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    printf("[%02d] active    : %d\n", i, manager->qps[i]);
    printf("     send post : %d\n", manager->qp_stat->posted_scnt.pkt);
    printf("     recv post : %d\n", manager->qp_stat->posted_rcnt.pkt);
    printf("     wqe index : %d\n", manager->qp_stat->wqe_idx);
  }
}


int mprud_post_recv_ack(struct ibv_qp *rep_qp)
{
  struct ibv_sge sg;
  struct ibv_recv_wr wr;
  struct ibv_recv_wr *bad_wr;

  memset(&sg, 0, sizeof(sg));
  sg.addr   = mp_manager.recv_base_addr;
  sg.length = sizeof(uint32_t) + MPRUD_GRH_SIZE;
  sg.lkey   = mpctx.wqe_table.wqe[mpctx.wqe_table.head].rwr.sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
    
  if (rep_qp->context->ops.post_recv(rep_qp, &wr, &bad_wr, 1))
    return FAILURE;

#ifdef MG_DEBUG_MODE
  printf("#%d post recv done.\n", mpctx.qp_stat[MPRUD_NUM_PATH].posted_rcnt.ack);
#endif

  return SUCCESS;
}

int mprud_post_send_ack(struct ibv_qp *rep_qp)
{
  struct ibv_sge sg;
  struct ibv_send_wr wr;
  struct ibv_send_wr *bad_wr;

  uint32_t ack = 1;

  int path_num = rand() % mp_manager.active_num;
  //int path_num = 0 ;
  memset(&sg, 0, sizeof(sg));
  sg.addr   = mp_manager.send_base_addr;// + mpctx.post_turn * 4;
  sg.length = sizeof(uint32_t);
  sg.lkey   = mpctx.wqe_table.wqe[mpctx.wqe_table.head].swr.sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.ud.ah   = mpctx.ah_list[mp_manager.mpqp[path_num].idx]; // USE Random path 
  wr.wr.ud.remote_qkey = 0x22222222;
  wr.wr.ud.remote_qpn = mpctx.dest_qp_num + (MPRUD_NUM_PATH + 1);

  int err = rep_qp->context->ops.post_send(rep_qp, &wr, &bad_wr, 1);
  if (err)
    return err;
#ifdef debugpath
  printf("#%d report post_send done.\n", mp_manager.qp_stat[MPRUD_NUM_PATH].posted_scnt.ack);
#endif

  return SUCCESS;
}

int mprud_post_recv_report(struct ibv_qp *rep_qp)
{
  struct ibv_sge sg;
  struct ibv_recv_wr wr;
  struct ibv_recv_wr *bad_wr;

  memset(&sg, 0, sizeof(sg));
  sg.addr   = mp_manager.recv_base_addr;
  sg.length = sizeof(struct report_msg) + MPRUD_GRH_SIZE;
  sg.lkey   = mpctx.wqe_table.wqe[mpctx.wqe_table.head].swr.sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
    
  if (rep_qp->context->ops.post_recv(rep_qp, &wr, &bad_wr, 1))
    return FAILURE;

#ifdef MG_DEBUG_MODE
  printf("#%d post recv done.\n", mpctx.qp_stat[MPRUD_NUM_PATH].posted_rcnt.ack);
#endif
  mpctx.qp_stat[MPRUD_NUM_PATH].posted_rcnt.ack++;
  return SUCCESS;
}

ResCode mprud_wait_report_msg()
{
  int err, ne;
  struct ibv_qp *rep_qp = mp_manager.report_qp;
  struct path_manager *manager = &mp_manager;
  if (!manager->start_time_flag){
    manager->start_time_flag = 1;
    gettimeofday(&manager->start, NULL);
    // post_recv in advance
    err = mprud_post_recv_report(rep_qp);
    if (err){
      printf("Error while post_recv report.\n");
      return MPRUD_STATUS_ERROR;
    }

    return MPRUD_STATUS_WAIT;
  }

  gettimeofday(&manager->now, NULL);
  float usec = mprud_get_us(manager->start, manager->now);
  if (usec < MPRUD_REPORT_RECV_CYCLE){
    return MPRUD_STATUS_WAIT;
  }

  // poll msg here
  ne = mprud_poll_report(rep_qp->recv_cq, 0); 
  if (ne > 0){
    printf("GOT report msg!\n");
    // Got report msg!
    mp_manager.qp_stat[MPRUD_NUM_PATH].polled_rcnt.ack += ne;

    memcpy(&mp_manager.msg, mp_manager.recv_base_addr + MPRUD_GRH_SIZE, sizeof(struct report_msg));


    // New post_recv for report
    err = mprud_post_recv_report(rep_qp);

    // Go to recovery routine
    return MPRUD_STATUS_FAIL;
  }
  else if (ne < 0) {
    printf("Error: poll result is negative value.\n");
    return MPRUD_STATUS_ERROR;
  }

  // update time
  memcpy(&manager->start, &manager->now, sizeof(struct timeval));

  return MPRUD_STATUS_NORMAL;
}

ResCode mprud_monitor_path()
{
  struct path_manager *manager = &mp_manager;

  // For the first time initiaization
  if (!manager->start_time_flag){
    manager->start_time_flag = 1;
    gettimeofday(&manager->start, NULL);
    
    for (int i=0; i<MPRUD_NUM_PATH; i++){
      if (manager->qps[i] == MPRUD_QPS_ACTIVE){
        manager->prev_qp_cnt[i] = manager->qp_stat[i].polled_rcnt.tot_pkt;
        memcpy(&manager->qp_wait[i], &manager->start, sizeof(struct timeval));
      }
    }
    return MPRUD_STATUS_NORMAL;
  }

  // Monitor flag
  if (!mp_manager.monitor_flag ){
    int ne = mprud_poll_report(mp_manager.report_qp->recv_cq, 0);
    if (ne > 0){
      printf("GOT report msg!\n");
      mp_manager.monitor_flag = 1;
    }
    else
      return MPRUD_STATUS_WAIT;
  }

  // Monitor cycle
  gettimeofday(&manager->now, NULL);
  float usec = mprud_get_us(manager->start, manager->now);
  if (usec < MPRUD_MONITOR_CYCLE){

    return MPRUD_STATUS_WAIT;
  }


  /*
#ifdef debugpath
  printf("%.2f us elapsed.\n", usec);
#endif
*/
  for (int i=0; i<MPRUD_NUM_PATH; i++){

    /* Only for active QPs */ 
    if (manager->qps[i] != MPRUD_QPS_ACTIVE)
      continue;

    /* Timeout */
    // write time when poll cnt is updated
    if (manager->prev_qp_cnt[i] < manager->qp_stat[i].polled_rcnt.tot_pkt){
      memcpy(&manager->qp_wait[i], &manager->now, sizeof(struct timeval));
    } else {
      // check if this QP reached threshold
      if (mprud_get_us(manager->qp_wait[i], manager->now) > MPRUD_TIMEOUT_THRESHOLD){
        // Go to Error state
        manager->qps[i] = MPRUD_QPS_ERROR;
#ifdef debugpath
        printf("QP#%d -- prev:%llu  now:%llu\n",i, manager->prev_qp_cnt[i], manager->qp_stat[i].polled_rcnt.tot_pkt);
        printf("QP#%d -- TIMEOUT! (%.2f > %d)\n", i, mprud_get_us(manager->qp_wait[i], manager->now), MPRUD_TIMEOUT_THRESHOLD);
#endif
      }
    }
    // update cnts
    manager->prev_qp_cnt[i] = manager->qp_stat[i].polled_rcnt.tot_pkt;
  } 

  // update time
  memcpy(&manager->start, &manager->now, sizeof(struct timeval));

  // Find Error QP
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    if (manager->qps[i] == MPRUD_QPS_ERROR) 
      return MPRUD_STATUS_FAIL;
  }

  return MPRUD_STATUS_NORMAL;
}

int mprud_post_resend(uint64_t id, struct ibv_send_wr *wr)
{
  struct ibv_recv_wr *bad_wr;

  uint32_t size = wr->sg_list->length;
  int i, ert, ne;

  /* Only when MPRUD_NUM_PATH less than default MTU */
  //int active_num = mp_manager.active_num;
  int active_num = MPRUD_NUM_PATH;  // simulate the full-path situation
  uint32_t chunk_size = size / active_num;
  uint32_t msg_last = size - chunk_size * active_num;
  uint32_t iter_each = (chunk_size / MPRUD_DEFAULT_MTU);
  uint32_t chnk_last = chunk_size % MPRUD_DEFAULT_MTU;
  if (chnk_last)
    iter_each += 1;
  else
    chnk_last = MPRUD_DEFAULT_MTU;

  //uint32_t iter = iter_each * MPRUD_NUM_PATH;
  uint32_t iter = iter_each * active_num;
  if (msg_last > 0 && msg_last <= MPRUD_DEFAULT_MTU)
    iter += 1;

  //mprud_store_wr(wr, NULL, iter_each, chnk_last, msg_last);
#ifdef MG_DEBUG_MODE
  printf("chunk_size: %u  msg_last: %u iter_each: %u  iter: %u chnk_last: %u\n", chunk_size, msg_last, iter_each, iter, chnk_last);
#endif
  uint64_t base_addr[active_num];
  for (int i=0; i<active_num; i++){
    base_addr[i] = wr->sg_list->addr + chunk_size * (i + 1);
  }

#ifdef MG_DEBUG_MODE
  printf("\n----------------------------------------\n");
  printf("recv_size=%d | send_size=%d for each inner qp\n", mpctx.recv_size, mpctx.send_size);
#endif

  struct ibv_wc wc[MPRUD_POLL_BATCH];

  /**
   * Work Request Setting
   */
  //wr->send_flags = 0;
  wr->send_flags = IBV_SEND_SIGNALED;

  // Edit sg_list values
  struct ibv_send_wr tmp_wr;
  memcpy(&tmp_wr, wr, sizeof(struct ibv_send_wr));

  struct ibv_sge tmp_sge;
  memcpy(&tmp_sge, wr->sg_list, sizeof(struct ibv_sge));

#ifdef MG_DEBUG_MODE
  printf("tmp_sge.addr: %lx length: %d\nMessage: %s\n",tmp_sge.addr, wr->sg_list->length, (char*)tmp_sge.addr);
#endif

  /**
   * Message Splitting
   */
  int max_outstd = 256;
  int msg_length;
  int mem_chnk_num = active_num;
  int m_turn = mpctx.wqe_table.wqe[id].start_idx;
  int path_idx = mp_manager.mpqp[rand() % mp_manager.active_num].idx; // path number to use for recovery
#ifdef debugpath
  // numbers are based on m_turn (inner_qps)
  printf("Err QP: %d   --->  Recovery QP: %d\n", mp_manager.msg.errqpn, path_idx);
#endif

  for (i = 0; i < iter; i++){
    //int q_turn = mp_manager.mpqp[m_turn].idx;
    if (mp_manager.msg.errqpn != m_turn){
      m_turn = (m_turn + 1) % mem_chnk_num;
      continue;
    }

    tmp_sge.addr = base_addr[m_turn] - (i/mem_chnk_num + 1) * MPRUD_DEFAULT_MTU;

    // LAST MSG NOT EXIST
    if (!msg_last){

      //if (iter - i <= MPRUD_NUM_PATH){
      if (iter - i <= mem_chnk_num){

        // last iter
        msg_length = chnk_last;
        tmp_sge.addr += MPRUD_DEFAULT_MTU - msg_length;

      } else {

        msg_length = MPRUD_DEFAULT_MTU;

      }
    } else {

      // LAST MSG EXIST
      if (i == iter-1){

        msg_length = msg_last;
        tmp_sge.addr = base_addr[mem_chnk_num-1];

      //} else if (iter-i-1 <= MPRUD_NUM_PATH) {
      } else if (iter-i-1 <= mem_chnk_num) {

        // last iter
        msg_length = chnk_last;
        tmp_sge.addr += MPRUD_DEFAULT_MTU - msg_length;

      } else {
        
        msg_length = MPRUD_DEFAULT_MTU;

      }
    }

    tmp_sge.length = msg_length;
    tmp_wr.sg_list = &tmp_sge;

    // 4. set ah & remote_qpn
    tmp_wr.wr.ud.ah = mpctx.ah_list[path_idx]; // USE Random path 
    tmp_wr.wr.ud.remote_qpn = mpctx.dest_qp_num + (m_turn + 1);
    tmp_wr.wr.ud.remote_qkey = 0x22222222;
#ifdef MG_DEBUG_MODE
    printf("RESEND: [#%d] addr: %lx   length: %d\n", m_turn, tmp_wr.sg_list->addr, tmp_wr.sg_list->length);
#endif
    // 5. post_send
    int err = mp_manager.report_qp->context->ops.post_send(mpctx.inner_qps[path_idx], &tmp_wr, bad_wr, 1); // original post send

//    int q_turn = mp_manager.mpqp[mpctx.post_turn].idx;
//    mpctx.qp_stat[q_turn].posted_scnt.pkt += 1;
//    mpctx.qp_stat[q_turn].posted_scnt.tot_pkt += 1;
//    mpctx.tot_sposted += 1;
    m_turn = (m_turn + 1) % mem_chnk_num;
    if (err){
      printf("ERROR while splited post_send!\n");
      return err;
    }
    // polling
    do {
      ne = mp_manager.report_qp->context->ops.poll_cq(mp_manager.mpqp[m_turn].qp->send_cq, 1, &wc, 1); // Go to original poll_cq
    } while (ne == 0);

    }
  return SUCCESS;
}

int mprud_post_send_report(struct ibv_qp *rep_qp, struct report_msg *msg)
{
  struct ibv_sge sg;
  struct ibv_send_wr wr;
  struct ibv_send_wr *bad_wr;

  // copy msg
  memcpy(mp_manager.send_base_addr, msg, sizeof(struct report_msg));

  int path_num = rand() % mp_manager.active_num;
  //int path_num = 0 ;
  memset(&sg, 0, sizeof(sg));
  sg.addr   = mp_manager.send_base_addr;// + mpctx.post_turn * 4;
  sg.length = sizeof(struct report_msg);
  sg.lkey   = mpctx.wqe_table.wqe[msg->max_wqe].rwr.sg_list->lkey;

  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.ud.ah   = mpctx.ah_list[mp_manager.mpqp[path_num].idx]; // USE Random path 
  wr.wr.ud.remote_qkey = 0x22222222;
  wr.wr.ud.remote_qpn = mpctx.dest_qp_num + (MPRUD_NUM_PATH + 1);

#ifdef debugpath
  printf("Report addr: %p  length: %d\n", sg.addr, sg.length);
  printf("Report data use path_num=%d & remote_qpn=%d\n", path_num, wr.wr.ud.remote_qpn);
#endif

  int err = rep_qp->context->ops.post_send(rep_qp, &wr, &bad_wr, 1);
  if (err)
    return err;
#ifdef debugpath
  printf("#%d report post_send done.\n", mp_manager.qp_stat[MPRUD_NUM_PATH].posted_scnt.ack);
#endif
  mp_manager.qp_stat[MPRUD_NUM_PATH].posted_scnt.ack++;
  

  return SUCCESS;
}

int mprud_poll_report(struct ibv_cq *cq, int is_blocking)
{
  struct ibv_wc wc;
  int ne;

  if (is_blocking){
    do {
      ne = cq->context->ops.poll_cq(cq, 1, &wc, 1); // Go to original poll_cq
    } while (ne == 0);
    if (ne > 0) {

      // check error
      if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n", 
            ibv_wc_status_str(wc.status),
            wc.status, (int)wc.wr_id);
      }
    }
    else if (ne < 0) {
      fprintf(stderr, "ibv_poll_cq() failed\n");
    }


  } else {
    // client does not wait for polling 
    ne = cq->context->ops.poll_cq(cq, 1, &wc, 1); // Go to original poll_cq
  }

  return ne;
} 

/**
 * We assume only ONE link failure.
 */
int mprud_recovery_routine_client()
{
#ifdef debugpath
  printf("############ got in recovery routin client\n"); 
  //mprud_print_qp_status();
#endif
  
#ifdef debugpath
    printf("<<<Report Message>>>\n");
    printf("errqpn: %u\n", mp_manager.msg.errqpn);
    printf("min_wqe: %u\n", mp_manager.msg.min_wqe);
    printf("max_wqe: %u\n", mp_manager.msg.max_wqe);
//    printf("comp: %u\n", mp_manager.msg.completed);
#endif


  // recover by new post_send
  int i = mp_manager.msg.min_wqe;
  while (i <= mp_manager.msg.max_wqe){

    mprud_post_resend(i, &mpctx.wqe_table.wqe[i].swr);
    i++;
  }

  // ACK to Server
  mprud_post_send_ack(mp_manager.report_qp);
  mprud_poll_report(mp_manager.report_qp->send_cq, 1);

  // update MPQP
  mp_manager.active_num -= 1;
  mp_manager.qps[mp_manager.msg.errqpn] = MPRUD_QPS_DEAD;
  mprud_update_mpqp();

/*
  uint32_t errqpn = mp_manager.msg.errqpn;

  int i=1;
  while (errqpn > 0){
    if (errqpn % 2 == 0)

    mp_manager.active_num
    errqpn = errqpn >> 1;
  }
  */

  return SUCCESS;
}

int mprud_recovery_routine_server()
{ 
  int err;

  /* 1. Make report msg */
  struct report_msg msg = {0,};
  // Find inactive QPs
  for (int i=0; i<MPRUD_NUM_PATH; i++){
    if (mp_manager.qps[i] == MPRUD_QPS_ERROR){

//------------------------------------------------------------
//  THIS SHOULD BE UNCOMMENTED AFTER TESTING DONE
      // change num of active QPs
      mp_manager.active_num -= 1;

//------------------------------------------------------------

      // Now take this error QP as dead QP
      mp_manager.qps[i] = MPRUD_QPS_DEAD;

//      msg.errqpn += (1 << i);
      msg.errqpn = i;
    }
  }

  if (mp_manager.active_num <= 0)
    printf("WARNING: active path num is negative.\n");
  if (mp_manager.active_num == 0){
    printf("Active paths = 0\n");
    return FAILURE;
  }

  uint32_t min=-1, max=0, cur;
  for (int i=0; i<MPRUD_NUM_PATH; i++){

    cur = mpctx.qp_stat[i].wqe_idx;
#ifdef debugpath
    printf("QP#%d -- wqe#%u\n", i, cur);
#endif
    if (cur < min) min = cur;
    if (cur > max) max = cur;
  }
  msg.min_wqe = min;
  msg.max_wqe = max;
//  msg.completed = mpctx.qp_stat[min].polled_rcnt.pkt;

#ifdef debugpath
  printf("error QPs: %u\n", msg.errqpn);
  printf("min: %u  max: %u\n", msg.min_wqe, msg.max_wqe);
#endif

  /* 2. Transmit msg */
  struct ibv_qp *rep_qp = mp_manager.report_qp;
  err = mprud_post_send_report(rep_qp, &msg);
  if (err){
    printf("Error while post_send report: %d\n", err);
    return FAILURE;
  }
 
  int ne = mprud_poll_report(rep_qp->send_cq, 1); 
  mp_manager.qp_stat[MPRUD_NUM_PATH].polled_scnt.ack += ne;

  // TMP
  // current status
//  for (int i=0; i<MPRUD_NUM_PATH; i++){
//    printf("QP#%d poll: %u  need: %u\n",i, mpctx.qp_stat[i].polled_rcnt.pkt, mpctx.wqe_table.wqe[min].iter_each[i]);
//  }

  // Flag initialize --> no monitor while waiting ack
  *(uint32_t*)(mpctx.buf.recv + MPRUD_GRH_SIZE) = 1;

  // SERVER does not need to do anything here?

  /* 3. Prepare post_recv for recovery pkts */ 
  // change WQE table
//  err = mprud_change_wqe_table(&msg);

  // post_recv from min_wqe

  // let original polling system work

  // rescheduling needs to be done automatically when posting WR by active_num

  // Post recv for ACK
  mprud_post_recv_ack(mp_manager.report_qp);  

  // stop monitoring because already detected failure
  mp_manager.monitor_flag = 0;

  return SUCCESS;
}

void mprud_store_wr(struct ibv_send_wr *swr, struct ibv_recv_wr *rwr, int iter, int iter_each, int chnk_last, int msg_last)
{
  struct mprud_wqe *wqe = &mpctx.wqe_table.wqe[mpctx.wqe_table.next];
  wqe->id = mpctx.wqe_table.sqn;
  if (swr != NULL){
    memcpy(&wqe->swr, swr, sizeof(struct ibv_send_wr));
    wqe->swr.sg_list = (struct ibv_sge*) malloc(sizeof(struct ibv_sge));
    memcpy(wqe->swr.sg_list, swr->sg_list, sizeof(struct ibv_sge));
  }
  if (rwr != NULL){
    memcpy(&wqe->rwr, rwr, sizeof(struct ibv_recv_wr));
    wqe->rwr.sg_list = (struct ibv_sge*) malloc(sizeof(struct ibv_sge));
    memcpy(wqe->rwr.sg_list, rwr->sg_list, sizeof(struct ibv_sge));

    wqe->avg_size_per_pkt = rwr->sg_list->length / (msg_last? iter-1:iter);
//    printf("Msg per pkt = %d (origin len: %d)\n", wqe->avg_size_per_pkt, rwr->sg_list->length);
  }

  wqe->chnk_last = chnk_last;
  wqe->msg_last = msg_last;
  wqe->valid = 1;
  wqe->start_idx = mpctx.post_turn;

  memset(wqe->iter_each, 0, MPRUD_NUM_PATH * sizeof(int));

  int m_turn, q_turn; // m_turn = memory turn, q_turn = qp turn
  //for (int i=0; i<MPRUD_NUM_PATH; i++){
  for (int i=0; i<mp_manager.active_num; i++){
    //turn = (mpctx.post_turn + i) % MPRUD_NUM_PATH;
    m_turn = (mpctx.post_turn + i) % mp_manager.active_num;
    q_turn = mp_manager.mpqp[m_turn].idx;

    wqe->iter_each[q_turn] = iter_each;

    if (i == 0 && msg_last > 0)
      wqe->iter_each[q_turn] += 1;
  }

    //printf("wqe->id: %lu   sqn: %u  next: %u  head: %u valid: %d\n", wqe->id, mpctx.wqe_table.sqn, mpctx.wqe_table.next, mpctx.wqe_table.head, wqe->valid);
  mpctx.wqe_table.sqn += 1;
  mpctx.wqe_table.next += 1;
  if (mpctx.wqe_table.next == MPRUD_TABLE_LEN)
    mpctx.wqe_table.next = 0;
}


inline int mprud_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr)
{
  //mpctx.app_tot_posted++;
  //printf("mprud app_tot_posted: %d\n", mpctx.app_tot_posted);
  uint32_t size = wr->sg_list->length;
  int i, err;

  /* Only when MPRUD_NUM_PATH less than default MTU */
  //uint32_t chunk_size = size / MPRUD_NUM_PATH;
  int active_num = mp_manager.active_num;
  uint32_t chunk_size = size / active_num;
  //uint32_t msg_last = size - chunk_size * MPRUD_NUM_PATH;
  uint32_t msg_last = size - chunk_size * active_num;
  // always let the last QP handles the left size
  uint32_t iter_each = (chunk_size / MPRUD_DEFAULT_MTU);
  uint32_t chnk_last = chunk_size % MPRUD_DEFAULT_MTU;
  if (chnk_last)
    iter_each += 1;
  else
    chnk_last = MPRUD_DEFAULT_MTU;

  //uint32_t iter = iter_each * MPRUD_NUM_PATH;
  uint32_t iter = iter_each * active_num;
  if (msg_last > 0 && msg_last <= MPRUD_DEFAULT_MTU)
    iter += 1;

  mprud_store_wr(wr, NULL, iter, iter_each, chnk_last, msg_last);
#ifdef MG_DEBUG_MODE
  printf("chunk_size: %u  msg_last: %u iter_each: %u  iter: %u chnk_last: %u\n", chunk_size, msg_last, iter_each, iter, chnk_last);
#endif
  //uint64_t base_addr[MPRUD_NUM_PATH];
  uint64_t base_addr[active_num];
  //for (int i=0; i<MPRUD_NUM_PATH; i++){
  for (int i=0; i<active_num; i++){
    base_addr[i] = wr->sg_list->addr + chunk_size * (i + 1);
    //printf("base address #%d: %p\n", i, base_addr[i]);
  }

#ifdef MG_DEBUG_MODE
  printf("\n----------------------------------------\n");
  printf("recv_size=%d | send_size=%d for each inner qp\n", mpctx.recv_size, mpctx.send_size);
#endif

  struct ibv_wc wc[MPRUD_POLL_BATCH];

  /**
   * Work Request Setting
   */
  //wr->send_flags = 0;
  wr->send_flags = IBV_SEND_SIGNALED;

  // Edit sg_list values
  struct ibv_send_wr tmp_wr;
  memcpy(&tmp_wr, wr, sizeof(struct ibv_send_wr));

  struct ibv_sge tmp_sge;
  memcpy(&tmp_sge, wr->sg_list, sizeof(struct ibv_sge));

#ifdef MG_DEBUG_MODE
  printf("tmp_sge.addr: %lx length: %d\nMessage: %s\n", tmp_sge.addr, wr->sg_list->length, (char*)tmp_sge.addr);
#endif

  /**
   * Message Splitting
   */
  int max_outstd = 256;
  int msg_length;
  int mem_chnk_num = active_num;
  mpctx.post_turn %= active_num;
  for (i = 0; i < iter; i++){
#ifdef SIMULATE_LINK_FAILURE
  if (mpctx.post_turn == 0){
    mpctx.post_turn = (mpctx.post_turn + 1) % mem_chnk_num; 
    continue;
  }
#endif
    tmp_sge.addr = base_addr[mpctx.post_turn] - (i/mem_chnk_num + 1) * MPRUD_DEFAULT_MTU;

    // LAST MSG NOT EXIST
    if (!msg_last){

      if (iter - i <= mem_chnk_num){

        // last iter
        msg_length = chnk_last;
        tmp_sge.addr += MPRUD_DEFAULT_MTU - msg_length;

      } else {

        msg_length = MPRUD_DEFAULT_MTU;

      }
    } else {

      // LAST MSG EXIST
      if (i == iter-1){

        msg_length = msg_last;
        tmp_sge.addr = base_addr[mem_chnk_num-1];

      } else if (iter-i-1 <= mem_chnk_num) {

        // last iter
        msg_length = chnk_last;
        tmp_sge.addr += MPRUD_DEFAULT_MTU - msg_length;

      } else {
        
        msg_length = MPRUD_DEFAULT_MTU;

      }
    }

    tmp_sge.length = msg_length;
    tmp_wr.sg_list = &tmp_sge;

    // 4. set ah & remote_qpn
    tmp_wr.wr.ud.ah = mpctx.ah_list[mpctx.post_turn];
    tmp_wr.wr.ud.remote_qpn = mpctx.dest_qp_num + (mpctx.post_turn + 1);
    tmp_wr.wr.ud.remote_qkey = 0x22222222;
#ifdef MG_DEBUG_MODE
    printf("[#%d] addr: %lx   length: %d\n", mpctx.post_turn, tmp_wr.sg_list->addr, tmp_wr.sg_list->length);
#endif
    // 5. post_send
    err = ibqp->context->ops.post_send(mp_manager.mpqp[mpctx.post_turn].qp, &tmp_wr, bad_wr, 1); // original post send

    int q_turn = mp_manager.mpqp[mpctx.post_turn].idx;
    mpctx.qp_stat[q_turn].posted_scnt.pkt += 1;
    mpctx.qp_stat[q_turn].posted_scnt.tot_pkt += 1;
    mpctx.tot_sposted += 1;
    mpctx.post_turn = (mpctx.post_turn + 1) % mem_chnk_num;
    if (err){
      printf("ERROR while splited post_send!\n");
      return err;
    }
    // polling

/*    for(int i=0; i<MPRUD_NUM_PATH; i++){
      printf("#%d (%d/%d) ",i, mpctx.qp_stat[i].polled_scnt.pkt, mpctx.qp_stat[i].posted_scnt.tot_pkt);
    }
    printf("\n");
*/
    while (mpctx.tot_sposted - mpctx.tot_spolled >= max_outstd){
      if (mprud_poll_cq(mpctx.inner_qps[0]->send_cq, MPRUD_POLL_BATCH, wc, 1)) // inner poll & update count included
        return FAILURE; //skip outer poll
    }

  }

  return SUCCESS;
}

inline int mprud_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr)
{

  int i, err;
  uint32_t size = wr->sg_list->length;
  if(ibqp->qp_type == IBV_QPT_UD)
    size -= MPRUD_GRH_SIZE;

  /* Only when MPRUD_NUM_PATH less than default MTU */
  int active_num = mp_manager.active_num;
  uint32_t chunk_size = size / active_num;
  uint32_t msg_last = size - chunk_size * active_num;
  uint32_t iter_each = (chunk_size / MPRUD_DEFAULT_MTU);
  uint32_t chnk_last = chunk_size % MPRUD_DEFAULT_MTU;
  if (chnk_last)
    iter_each += 1;
  else
    chnk_last = MPRUD_DEFAULT_MTU;

  uint32_t iter = iter_each * active_num;
  if (msg_last > 0 && msg_last <= MPRUD_DEFAULT_MTU)
    iter += 1;

  mprud_store_wr(NULL, wr, iter, iter_each, chnk_last, msg_last);
  

#ifdef MG_DEBUG_MODE
  printf("chunk_size: %u  msg_last: %u iter_each: %u  iter: %u chnk_last: %u\n", chunk_size, msg_last, iter_each, iter, chnk_last);
#endif
  uint64_t base_addr[active_num];
  for (int i=0; i<active_num; i++){
    base_addr[i] = wr->sg_list->addr + chunk_size * (i + 1) + MPRUD_GRH_SIZE;
    //printf("addr: %x  chunk_size: %d   grh: %d   base address #%d: %p\n",wr->sg_list->addr, chunk_size, MPRUD_GRH_SIZE, i, base_addr[i]);
  }

  struct ibv_wc wc[MPRUD_POLL_BATCH];

  // Edit sg_list values
  struct ibv_recv_wr tmp_wr;
  memcpy(&tmp_wr, wr, sizeof(struct ibv_recv_wr));

  struct ibv_sge tmp_sge;
  memcpy(&tmp_sge, wr->sg_list, sizeof(struct ibv_sge));

  /**
   * Message Splitting
   */
  int max_outstd = 512;
  int msg_length;
  int mem_chnk_num = active_num;
  mpctx.post_turn %= active_num;
  for (i = 0; i < iter; i++){
    tmp_sge.addr = base_addr[mpctx.post_turn] - (i/mem_chnk_num + 1) * MPRUD_DEFAULT_MTU;

    // LAST MSG NOT EXIST
    if (!msg_last){

      if (iter - i <= mem_chnk_num){   

        // chunk last msg
        msg_length = chnk_last;
        tmp_sge.addr = mpctx.buf.sub + (MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE) * mpctx.post_turn;
      } else {

        msg_length = MPRUD_DEFAULT_MTU;

      }
    } else {

      // LAST MSG EXIST
      if (i == iter-1){

        // last msg
        msg_length = msg_last;
        tmp_sge.addr = mpctx.buf.last; 

      } else if (iter-i-1 <= mem_chnk_num) {

        // chunk last msg
        msg_length = chnk_last;
        tmp_sge.addr = mpctx.buf.sub + (MPRUD_DEFAULT_MTU + MPRUD_GRH_SIZE) * mpctx.post_turn;

      } else {

        msg_length = MPRUD_DEFAULT_MTU;

      }
    }

    tmp_sge.length = msg_length + MPRUD_GRH_SIZE;
    tmp_sge.addr -= MPRUD_GRH_SIZE;

    tmp_wr.sg_list = &tmp_sge;


#ifdef MG_DEBUG_MODE
    printf("[#%d] addr: %lx   length: %d\n", mpctx.post_turn, tmp_wr.sg_list->addr, tmp_wr.sg_list->length);
#endif

    // 3. post_recv
    err = ibqp->context->ops.post_recv(mp_manager.mpqp[mpctx.post_turn].qp, &tmp_wr, bad_wr, 1);  // original post recv

    int q_turn = mp_manager.mpqp[mpctx.post_turn].idx;
    mpctx.qp_stat[q_turn].posted_rcnt.pkt += 1;
    mpctx.tot_rposted += 1;
    mpctx.post_turn = (mpctx.post_turn + 1) % mem_chnk_num;
    if (err){
      printf("ERROR while splited post_recv! #%d\n", err);
      return err;
    }
    // polling
    while (mpctx.tot_rposted - mpctx.tot_rpolled >= max_outstd){
      if (mprud_poll_cq(mpctx.inner_qps[0]->recv_cq, MPRUD_POLL_BATCH, wc, 1))  
        return FAILURE; // last arg 1 => skip outer poll
    }

  }
  return SUCCESS;

}

static inline uint32_t mprud_get_outer_poll(int Iam)
{
  struct mprud_wqe *wqe = &mpctx.wqe_table.wqe[mpctx.wqe_table.head];
  int comp_flag = 1;
  int active_num = mp_manager.active_num;
  
  int q_turn;
  if (Iam == MP_SERVER){
    for (int i=0; i<active_num; i++){
      q_turn = mp_manager.mpqp[i].idx;
      if (mpctx.qp_stat[q_turn].polled_rcnt.pkt < wqe->iter_each[q_turn]){
        comp_flag = 0;
        break;
      }
    }

    // update polled cnt
    if (comp_flag){
      for (int i=0; i<active_num; i++){
        q_turn = mp_manager.mpqp[i].idx;
        mpctx.qp_stat[q_turn].polled_rcnt.pkt -= wqe->iter_each[q_turn];
      }
    }
  } else {

    for (int i=0; i<active_num; i++){
      q_turn = mp_manager.mpqp[i].idx;
#ifdef SIMULATE_LINK_FAILURE
    if (q_turn == 0)
      continue;
#endif
      if (mpctx.qp_stat[q_turn].polled_scnt.pkt < wqe->iter_each[q_turn]){
        comp_flag = 0;
        break;
      }
    }

    // update polled cnt
    if (comp_flag){

      for (int i=0; i<active_num; i++){
        q_turn = mp_manager.mpqp[i].idx;

        mpctx.qp_stat[q_turn].polled_scnt.pkt -= wqe->iter_each[q_turn];
      }
/*
      printf("POSTED\n");
      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.qp_stat[i].posted_scnt.pkt);

      }
      printf("\nPOLLED\n");

      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.qp_stat[i].polled_scnt.pkt);
      }
      printf("\nNeeded\n");

      for(int i=0; i<MPRUD_NUM_PATH; i++){
        printf("%d  ", mpctx.wqe_table.wqe[mpctx.wqe_table.head].iter_each[i]);
      }
      printf("\n----------------------------\n");
*/
    }
  }

  if (comp_flag){
    // update head
    //memset(&mpctx.wqe_table.wqe[mpctx.wqe_table.head], 0, sizeof(struct mprud_wqe));
    mpctx.wqe_table.head += 1;
    if (mpctx.wqe_table.head == MPRUD_TABLE_LEN)
      mpctx.wqe_table.head = 0;

    return 1;
  }

  return 0;
}

// Splitted msg polling
static inline int mprud_outer_poll(int ne, struct ibv_wc *wc, int Iam)
{

  // App-only polling here
  uint32_t outer_poll_num = mprud_get_outer_poll(Iam);

#ifdef MG_DEBUG_MODE
  if (outer_poll_num > 0){
    printf("\t-->Outer Poll: %u\n", outer_poll_num);
  }
#endif

  if (outer_poll_num > 0){
    for (int i=0; i<outer_poll_num; i++){
      wc[i].wr_id = 0;
      wc[i].status = IBV_WC_SUCCESS;
    }      

    return outer_poll_num;
  }
  return 0;
}

static inline int mprud_inner_poll(int Iam)
{
  int batch = 1;  // Try polling one WR for each inner qp

  struct ibv_wc tmp_wc[batch];
  memset(tmp_wc, 0, sizeof(struct ibv_wc) * batch);

  int poll_turn = mpctx.poll_turn;
  int q_turn = mp_manager.mpqp[poll_turn].idx;
  struct qp_status *qp_stat = &mpctx.qp_stat[q_turn];
  int active_num = mp_manager.active_num;

  uint64_t *tot_posted, *tot_polled;

  tot_posted = (Iam == MP_SERVER ? &mpctx.tot_rposted : &mpctx.tot_sposted);
  tot_polled = (Iam == MP_SERVER ? &mpctx.tot_rpolled : &mpctx.tot_spolled);

  if (*tot_posted - *tot_polled > 0){

    struct ibv_cq *cq = (Iam == MP_SERVER ? mp_manager.mpqp[poll_turn].qp->recv_cq : mp_manager.mpqp[poll_turn].qp->send_cq);
#ifdef USE_MPRUD
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc, 1); // Go to original poll_cq
#else
    uint32_t now_polled = cq->context->ops.poll_cq(cq, batch, tmp_wc);
#endif
    mpctx.poll_turn = (mpctx.poll_turn + 1) % active_num;

    if (now_polled > 0){
#ifdef MG_DEBUG_MODE
      printf("[Inner Poll] %d (qp #%d)\n", now_polled, poll_turn);
#endif

      if (Iam == MP_SERVER){
        qp_stat->polled_rcnt.pkt += now_polled;
        qp_stat->polled_rcnt.tot_pkt += now_polled;
        
       // update recv msg size
       qp_stat->recv_msg_size += mpctx.wqe_table.wqe[qp_stat->wqe_idx].avg_size_per_pkt;

      } else {
        qp_stat->polled_scnt.pkt += now_polled;
        qp_stat->polled_scnt.tot_pkt += now_polled;
      }
      
      *tot_polled += now_polled;

      // if this turn is to use sub buffer
      if (Iam == MP_SERVER && qp_stat->polled_rcnt.pkt == mpctx.wqe_table.wqe[qp_stat->wqe_idx].iter_each[poll_turn]){

#ifdef MG_DEBUG_MODE
       // printf("Data in sub buffer: %s\n", mpctx.buf.sub + poll_turn * (MPRUD_GRH_SIZE + MPRUD_DEFAULT_MTU) + MPRUD_GRH_SIZE);
#endif
        // get address for sub buffer copy
        struct mprud_wqe *wqe = &mpctx.wqe_table.wqe[qp_stat->wqe_idx];
        uint64_t addr = wqe->rwr.sg_list->addr;
        uint32_t length = wqe->rwr.sg_list->length;
        int size = wqe->chnk_last;

        for (int i=0; i<active_num; i++){
          uint64_t cur_addr = addr + (length / active_num) * i;

          // copy the msg_last if exist
          if (i == active_num - 1 && wqe->msg_last > 0){
            memcpy(cur_addr, mpctx.buf.last + MPRUD_GRH_SIZE, wqe->msg_last);

          } else {
            memcpy(cur_addr, mpctx.buf.recv + MPRUD_GRH_SIZE + (MPRUD_GRH_SIZE + MPRUD_DEFAULT_MTU)*i, size);
          }
        }
        qp_stat->wqe_idx += 1;
        if (qp_stat->wqe_idx == MPRUD_TABLE_LEN)
          qp_stat->wqe_idx = 0;
      }

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
  int Iam = -1;
  if (mpctx.tot_rposted > 0){ // send pkt
    Iam = MP_SERVER;

  } else if (mpctx.tot_sposted > 0){
    Iam = MP_CLIENT;
  }
#ifdef MG_DEBUG_MODE
  else {
    // Both pkt cnt is 0. Meaning it's done.
    return 0;
  }
#endif
  if (mpctx.wqe_table.wqe[mpctx.wqe_table.head].valid == 0){
    printf("No more work requests. Stop polling.\n");
    printf("head: %d   next: %d\n", mpctx.wqe_table.head, mpctx.wqe_table.next);
    return 0;
  }

  //************************
  // OUTER POLLING
  //************************
  if (!skip_outer_poll){
    int outer_poll_num = mprud_outer_poll(ne, wc, Iam); 
    if (outer_poll_num){
//      printf("mpctx.app.tot_polled = %lu\n", mpctx.app_tot_posted);
      return outer_poll_num;
    }
  }

  //************************
  // INNER POLLING
  //************************
  if (mprud_inner_poll(Iam))
    return -1;

  //************************
  // PATH MANAGE
  //************************
/*
  ResCode code;
  int err;

  if (Iam == MP_SERVER)
    code = mprud_monitor_path();
  else if (Iam == MP_CLIENT)
    code = mprud_wait_report_msg();


  switch (code){
    case MPRUD_STATUS_NORMAL:

    case MPRUD_STATUS_WAIT:
      // not reached monitoring period
      break;
    case MPRUD_STATUS_WARNING:
    case MPRUD_STATUS_ERROR:
      printf("Error Occured in CLIENT.\n");
      return -1;

    case MPRUD_STATUS_FAIL: 
#ifdef MG_DEBUG_MODE
      printf("Path failure detected. Start recovery routine.\n");
#endif

      if (Iam == MP_SERVER){
        err = mprud_recovery_routine_server();
      } else if (Iam == MP_CLIENT){
        err = mprud_recovery_routine_client();
      }
      if(err){
        printf("Failed to recover path.\n");
        return -1;
      }
      break;

  };
 */ 

#ifdef PRINT_PERF_PER_QP
    mprud_perf_per_qp();
#endif
  return 0;
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
  ibv_destroy_qp(mp_manager.report_qp);
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


//#ifdef MG_DEBUG_MODE
//      printf("POSTED\n");
//      for(int i=0; i<MPRUD_NUM_PATH; i++){
//        printf("%d  ", mpctx.qp_stat[i].posted_scnt.pkt);
//
//      }
//      printf("\nPOLLED\n");
//
//      for(int i=0; i<MPRUD_NUM_PATH; i++){
//        printf("%d  ", mpctx.qp_stat[i].polled_scnt.pkt);
//      }
//      printf("\nNeeded\n");
//
//      for(int i=0; i<MPRUD_NUM_PATH; i++){
//        printf("%d  ", mpctx.wqe_table.wqe[mpctx.wqe_table.head].iter_each[i]);
//      }
//      printf("\n----------------------------\n");
//
//#endif
