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


int start_app_srv(uint64_t srv_port, uint64_t num_of_clients, uint64_t test_number)
{
	FTN_server_init(srv_port, num_of_clients);
	
	printf("%lx", test_number);
	return 0;
}

int start_app_client(FTN_IPV4_ADDR srv_ip, uint64_t srv_port)
{
	uint64_t my_client_number = 0;
	
	FTN_client_init(srv_ip, srv_port, &my_client_number);
	
	return 0;
}
