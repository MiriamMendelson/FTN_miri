
#include "./FTN_interface.h"

#ifndef __FTN_SOCKET_INTEFACE_H__
#define __FTN_SOCKET_INTEFACE_H__

// uint64_t init_socket(FTN_IPV4_ADDR ip, uint64_t port);

int create_socket(uint64_t port, struct sockaddr_in** out_addr);
struct sockaddr_in declare_server(char* server_ip, uint64_t server_port);
bool convert_ip_to_str(FTN_IPV4_ADDR ip, char *out_str_ip);

#endif