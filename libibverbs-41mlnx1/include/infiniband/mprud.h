/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/mprud_opt.h>

#define MIN(x,y) (((x) > (y)) ? (y) : (x))

struct path_manager {
  int active_num;
  int is_active[MPRUD_NUM_PATH];
  struct ibv_qp *report_qp;
  char *send_base_addr;
  char *recv_base_addr;
};

struct qp_status {
  struct {
    uint64_t pkt;
    uint64_t ack;
  }posted_scnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  }posted_rcnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  }polled_scnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  }polled_rcnt;

  int wqe_idx;  // currently working wqe idx
};

struct mprud_wqe {
  int valid;
  uint64_t id;
  struct ibv_send_wr swr;
  struct ibv_recv_wr rwr;
  int iter_each[MPRUD_NUM_PATH];
  int chnk_last;
  int msg_last;
};

struct mprud_context {
  int recv_size;
  int send_size;
  int split_num;
  int last_size;
  char *outer_buf;
  struct ibv_qp *inner_qps[MPRUD_NUM_PATH];
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
  struct qp_status qp_stat[MPRUD_NUM_PATH];
  uint64_t tot_rposted;
  uint64_t tot_rpolled;
  uint64_t tot_sposted; // only pkt not ack
  uint64_t tot_spolled;

  struct path_manager mp_manager; 
};

extern struct mprud_context mpctx;

/* Clean this later */
// post & poll measure
extern int split_num;  // number of splitted requests
extern int last_size; 

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
