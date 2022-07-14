# MPRUD: Multi-Path RDMA using Unreliable Datagram

This program is to utilize RDMA UD for multipath implementation.

Unlike RC QP, UD QP does not store all the context information of destination.
Instead, it needs to specify AH(address handle) in WR every time it posts requests.


MPRUD has some advantages:
- enjoys full benefits of using multipath such as robustness & load balancing
- shows compatible performance (near 40G) <-> RC flow
- requires only a small amount of cache on RNIC (Scalability) <-> RC multipath

## Design
- When a message is posted, MPRUD will divide the target memory area into the number of QP (or paths) it employs.
Each QP will take responsibility of each portion of memory so that the packet sequence of each portion at least can be in order.
- To increase performance, MPRUD use some reverse tricks. Unlike the conventional order of transmitting packet,
  MPRUD post in reversed order for avoiding use of buffer. This scheme basically splits messages since only message size under MTU is allowed in RDMA UD.
  Additionally, when RNIC transmits UD packet, it automatically adds 40 bytes of GRH header which includes information of how this packet can traverse across to the remote node. By processing in reverse order, MPRUD can receive the entire data without any help of buffer in the middle.
 

## Pre-requisite
MPRUD works in a quite refined environment.
- Need to assign several ip interfaces to RNIC port in advance
- Need to create list of AHs by specifying GIDs (in libibverbs-41mlnx1/src/mprud.c)


## Note
- For simplicity, MPRUD does not take into account all the different sizes of message. (Actually it requires fine-grained, well-defined dividing logic in order to support all message sizes.) Recommend to test message sizes of multiples of 4096.
- For link failure recovery test, MPRUD assumes only the last QP failure once. Thus, the recovery algorithm deals with only the last path failure for now. If some general purpose use is required, some logic should be changed such as active_num, MPRUD_NUM_PATH, etc.

