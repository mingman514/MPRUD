/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/mprud_opt.h>

#define MIN(x,y) (((x) > (y)) ? (y) : (x))
struct mp_window {
  int split_num;
  struct {
    struct ibv_qp *ibqp;
    struct ibv_send_wr *send_wr;
    struct ibv_recv_wr *recv_wr;
    struct ibv_send_wr **bad_wr;
  }post_ctx;
};

struct path_manager {
  struct ibv_qp *report_qp;
  char *ack_base_addr;   // base address for ACK space
  int is_active[MPRUD_NUM_PATH];     // 0 = inactive, 1 = active
  uint32_t qp_comp[MPRUD_NUM_PATH];  // completion num for each inner QP
  uint32_t qp_comp_tot[MPRUD_NUM_PATH];  // total completion num
  uint32_t comp_needed[MPRUD_NUM_PATH][MPRUD_WINDOW_SIZE];  // This stores the number of completions needed from each QP to actually poll one real completion.
};

struct mprud_context {
  struct {
    uint64_t pkt;
    uint64_t ack;
  } posted_scnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  } posted_rcnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  } polled_scnt;
  struct {
    uint64_t pkt;
    uint64_t ack;
  } polled_rcnt;
  int recv_size;
  int send_size;
  int split_num;
  int last_size;
  char *outer_buf;
  struct ibv_qp *inner_qps[MPRUD_NUM_PATH];
  struct ibv_ah *ah_list[MPRUD_NUM_PATH];
  int post_turn;   // Used to count inner recv turn
  int poll_turn;
  struct path_manager mp_manager;
  struct {
    int is_full;
    int head;   // indicates the oldest window element
    int cur;    // indicates the next window to store ctx
    struct mp_window mp_win[MPRUD_WINDOW_SIZE];
  } win;
  uint32_t dest_qp_num;
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
char *mprud_get_outer_buffer();
void mprud_set_outer_buffer(void* ptr, int buf_size);
//void mprud_set_recv_size(int size);
//void mprud_set_send_size(int size);
//void mprud_set_cq_size(int size);
void mprud_set_dest_qp_num(uint32_t qp_num);
int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc, int skip_outer_poll);

void mprud_print_inner_buffer();
int mprud_destroy_resources();
#endif
