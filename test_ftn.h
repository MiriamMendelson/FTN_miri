#ifndef __FTN_TEST_APP_H__
#define __FTN_TEST_APP_H__


#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>


#include "FTN/FTN_interface.h"

int start_app_srv(uint64_t srv_port, uint64_t num_of_clients, uint64_t test_number);

int start_app_client(FTN_IPV4_ADDR srv_ip, uint64_t srv_port);


#endif