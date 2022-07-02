/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/mprud_opt.h>

#define MIN(x,y) (((x) > (y)) ? (y) : (x))

typedef enum res_code {
  MPRUD_STATUS_NORMAL,
  MPRUD_STATUS_WAIT,
  MPRUD_STATUS_WARNING,
  MPRUD_STATUS_FAIL,  // path failure
  MPRUD_STATUS_ERROR  // other erros
}ResCode;

struct qp_set {
  struct ibv_qp *qp;
  int idx; // absolute idx matched with real QPs & paths
};


struct report_msg {
  uint32_t errqpn;
  uint32_t min_wqe; // min wqe among QPs --> Start point
  uint32_t max_wqe; // max wqe among QPs --> End point
//  uint32_t completed;
};

struct path_manager {
  int active_num;
  int qps[MPRUD_NUM_PATH];    // qp state
  struct qp_set *mpqp;    // referencing
  struct ibv_qp *report_qp;

  char *send_base_addr;
  char *recv_base_addr;
  struct report_msg msg;

  struct qp_status *qp_stat;  // just reference pointer

  int start_time_flag;
  struct timeval start;
  struct timeval now;
  struct timeval focus;
  struct timeval qp_wait[MPRUD_NUM_PATH];
  uint64_t prev_qp_cnt[MPRUD_NUM_PATH]; // to check completion done or not for a time unit

  int monitor_flag;

  // perf
  int p_start_time_flag;
  struct timeval p_start;
  struct timeval p_now;  
};

struct qp_status {
  struct {
    uint64_t pkt;
    uint64_t tot_pkt;
    uint64_t ack;
  }posted_scnt;
  struct {
    uint64_t pkt;
    uint64_t tot_pkt;
    uint64_t ack;
  }posted_rcnt;
  struct {
    uint64_t pkt;
    uint64_t tot_pkt;
    uint64_t ack;
  }polled_scnt;
  struct {
    uint64_t pkt;
    uint64_t tot_pkt;
    uint64_t ack;
//    uint64_t previous; // to measure performance
  }polled_rcnt;

  uint64_t recv_msg_size;
  uint32_t wqe_idx;  // currently working wqe idx
};

struct mprud_wqe {
  int valid;
  uint64_t id;
  struct ibv_send_wr swr;
  struct ibv_recv_wr rwr;
  int iter_each[MPRUD_NUM_PATH];
  int chnk_last;
  int msg_last;
  int start_idx;
  int avg_size_per_pkt;

};

struct mprud_context {
  int recv_size;
  int send_size;
  int last_size;
  char *outer_buf;
  struct ibv_qp *inner_qps[MPRUD_NUM_PATH]; // array of QPs
  struct ibv_ah *ah_list[MPRUD_NUM_PATH];
  int post_turn;   // Used to count inner recv turn
  int poll_turn;
  uint32_t dest_qp_num;
  struct {
    int head;
    int next;
    uint64_t sqn;
    struct mprud_wqe wqe[MPRUD_TABLE_LEN];
  }wqe_table;
  struct{
    char *sub;  // for last pkt of each chunk
    char *last; // for last pkt of msg
    char *send; // send control pkt
    char* recv; // recv control pkt
  }buf;
  struct qp_status qp_stat[MPRUD_NUM_PATH+1];
  uint64_t tot_rposted;
  uint64_t tot_rpolled;
  uint64_t tot_sposted; // only pkt not ack
  uint64_t tot_spolled;

  struct path_manager mp_manager; 
  uint64_t app_tot_posted;
};

extern struct mprud_context mpctx;
extern struct path_manager mp_manager;

/* Clean this later */
// post & poll measure
extern int split_num;  // number of splitted requests
extern int last_size; 

void mprud_print_qp_status();
int mprud_init_ctx();
struct ibv_ah** mprud_get_ah_list();
void print_gid_info(union ibv_gid mgid);
uint8_t *convert_to_raw_gid(uint8_t *gid, char* str_gid);
int mprud_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr);
int mprud_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr);
int mprud_create_ah_list(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
int mprud_create_inner_qps(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
int mprud_modify_report_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr_);
int mprud_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask);
void mprud_set_dest_qp_num(uint32_t qp_num);
char *mprud_get_outer_buffer();
void mprud_set_outer_buffer(void* ptr, uint32_t len);
//void mprud_set_recv_size(int size);
//void mprud_set_send_size(int size);
//void mprud_set_cq_size(int size);
int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc, int skip_outer_poll);

void mprud_print_inner_buffer();
int mprud_destroy_resources();
#endif
