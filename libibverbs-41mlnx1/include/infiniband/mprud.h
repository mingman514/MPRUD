/*MPRUD by mingman*/

#ifndef MPRUD_H
#define MPRUD_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>

#define LOG_LEVEL 1

#if ((LOG_LEVEL) > 0)
  #define LOG_DEBUG(s, a...)  printf((s), ##a)
#else
  #define LOG_DEBUG(s, a...)
#endif
#define LOG_ERROR(s, a...)  printf((s), ##a)

#define ALLOCATE(var,type,size)                                     \
{ if((var = (type*)malloc(sizeof(type)*(size))) == NULL)        \
  { fprintf(stderr," Cannot Allocate\n"); exit(1);}}

#define MIN(x,y) (x > y ? y : x)

#define MPRUD_NUM_PATH 4
#define MPRUD_DEFAULT_PORT 1
#define MPRUD_HEADER_SIZE 12  // Session ID | MSG SQN | Pkt SQN
/**
 * MPRUD_BUF_SPLIT_NUM must be larger than max send/recv
 * queue size (send_size, recv_size). Otherwise, the buffer
 * will be overlapped before the data is processed.
 **/
#define MPRUD_BUF_SPLIT_NUM 1024    // Set as default QP size
#define MPRUD_GRH_SIZE 40
#define MPRUD_DEFAULT_MTU 4096
#define MPRUD_POLL_BATCH 16
#define MPRUD_SEND_BUF_OFFSET (MPRUD_HEADER_SIZE + MPRUD_DEFAULT_MTU)
#define MPRUD_RECV_BUF_OFFSET (MPRUD_GRH_SIZE + MPRUD_HEADER_SIZE + MPRUD_DEFAULT_MTU)

#define SUCCESS (0)
#define FAILURE (1)

#define MG_DEBUG 1
#define MG_DEBUG_BUFFER 0
#define MG_DEBUG_POLL 1
#define MG_DEBUG_AH 1

// post & poll measure
extern uint64_t posted_cnt, polled_cnt; // total inner post/poll counts
extern uint64_t ack_posted_cnt, ack_polled_cnt;
extern uint64_t outer_polled_cnt;
extern int split_num;   // number of splitted requests
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
