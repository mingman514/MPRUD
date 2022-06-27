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
#define USE_REVERSE_POST

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
