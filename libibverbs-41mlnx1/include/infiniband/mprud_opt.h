/*MPRUD by mingman*/

#ifndef MPRUD_OPT_H
#define MPRUD_OPT_H

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

#define USE_MPRUD
//#define MG_DEBUG_MODE
#define MPRUD_CHUNK_MODE
//#define debugpath

#define MPRUD_NUM_PATH 4
#define MPRUD_DEFAULT_PORT 1
/**
 * MPRUD_BUF_SPLIT_NUM must be larger than max send/recv
 * queue size (send_size, recv_size). Otherwise, the buffer
 * will be overlapped before the data is processed.
 **/
#define MPRUD_GRH_SIZE 40
#define MPRUD_DEFAULT_MTU 4096

#define MPRUD_SEND_BUF_OFFSET (MPRUD_DEFAULT_MTU)
#define MPRUD_RECV_BUF_OFFSET (MPRUD_GRH_SIZE + MPRUD_DEFAULT_MTU)

// Buffer
#define MPRUD_SEND_BUF_SIZE 12
#define MPRUD_RECV_BUF_SIZE (12 + MPRUD_GRH_SIZE)

#define MPRUD_TABLE_LEN 4000
#define MPRUD_MONITOR_CYCLE 10    // us
#define MPRUD_REPORT_RECV_CYCLE 100    // us
#define MPRUD_TIMEOUT_THRESHOLD 30    // us
#define MPRUD_QPS_DEAD 0
#define MPRUD_QPS_ACTIVE 1
#define MPRUD_QPS_ERROR 2

#define MPRUD_POLL_BATCH 16

#define SUCCESS (0)
#define FAILURE (1)
#define MP_SERVER (0)
#define MP_CLIENT (1)


#define MG_DEBUG 0
#define MG_DEBUG_BUFFER 0
#define MG_DEBUG_POLL 0
#define MG_DEBUG_AH 0

#endif
