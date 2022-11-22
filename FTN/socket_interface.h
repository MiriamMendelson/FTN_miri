#ifndef __FTN_SOCKET_INTEFACE_H__
#define __FTN_SOCKET_INTEFACE_H__

#include "./FTN_interface.h"

int create_socket(uint64_t port, struct sockaddr_in* out_addr);
bool init_addr(struct sockaddr_in *addr, int port, uint8_t *ip_addr);
bool is_same_addr(struct sockaddr_in *src, END_POINT *ep);
#endif