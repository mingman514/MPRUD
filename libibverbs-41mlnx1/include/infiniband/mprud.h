/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/mprud_opt.h>

#define MIN(x,y) (((x) > (y)) ? (y) : (x))
// post & poll measure
extern uint64_t posted_cnt, polled_cnt; // total inner post/poll counts
extern uint64_t ack_posted_cnt, ack_polled_cnt;
extern uint64_t outer_polled_cnt;
extern int split_num, last_size;   // number of splitted requests
extern int recv_size, send_size, cq_size;

struct ibv_ah** mprud_get_ah_list();
void print_gid_info(union ibv_gid mgid);
uint8_t *convert_to_raw_gid(uint8_t *gid, char* str_gid);
int mprud_create_ah_list(struct ibv_pd *pd,
			       struct ibv_qp_init_attr *qp_init_attr);
//struct ibv_qp* mprud_create_qp(struct ibv_qp *ibqp, struct ibv_send_wr *wr);
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
