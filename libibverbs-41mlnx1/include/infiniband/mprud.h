/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/mprud_opt.h>

#define MIN(x,y) (((x) > (y)) ? (y) : (x))
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
  int cq_size;
  int split_num;
  int last_size;
  char *inner_buf;
  char *outer_buf;
  struct ibv_qp *inner_qps[MPRUD_NUM_PATH];
  struct ibv_ah *ah_list[MPRUD_NUM_PATH];
  int post_turn;   // Used to count inner recv turn
  int poll_turn;
};

extern struct mprud_context mpctx;

// post & poll measure
//extern uint64_t posted_cnt, polled_cnt; // total inner post/poll counts
extern uint64_t ack_posted_cnt, ack_polled_cnt;
//extern uint64_t outer_polled_cnt;
extern int split_num;  // number of splitted requests
extern int last_size; 
extern int recv_size, send_size, cq_size;

int mprud_init_ctx();
struct ibv_ah** mprud_get_ah_list();
void print_gid_info(union ibv_gid mgid);
uint8_t *convert_to_raw_gid(uint8_t *gid, char* str_gid);
int mprud_create_ah_list(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
int mprud_create_inner_qps(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr);
int mprud_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask);
char *mprud_get_inner_buffer();
void mprud_set_inner_buffer(void* ptr);
char *mprud_get_outer_buffer();
void mprud_set_outer_buffer(void* ptr);
void mprud_set_recv_size(int size);
void mprud_set_send_size(int size);
void mprud_set_cq_size(int size);
int mprud_poll_cq(struct ibv_cq *cq, uint32_t ne, struct ibv_wc *wc);

void mprud_print_inner_buffer();


int mprud_destroy_ah_list();
#endif
