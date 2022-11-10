#include <stdio.h>
#include <stdbool.h>
#include "FTN_interface.h"

FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network)
{
	printf("FTN_server_init is called! server_port %lx num_of_expected_clients %lx\n", server_port, num_of_nodes_in_network);
	
	return FTN_ERROR_NOT_IMPLEMENTED;
}

FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t * out_my_address)
{
	(*out_my_address) = INVALID_ADDRESS_ID;
	printf("FTN_client_init is called! ip %d.%d.%d.%d port %lx\n", server_ip.ip_addr[0], server_ip.ip_addr[1], server_ip.ip_addr[2], server_ip.ip_addr[3], server_port);
	
	return FTN_ERROR_NOT_IMPLEMENTED;
}

FTN_RET_VAL FTN_recv(void * data_buffer, uint64_t data_buffer_size, uint64_t * out_pkt_len, uint64_t source_address_id, uint64_t * opt_out_source_address_id, bool async)
{
	UNUSED_PARAM(data_buffer);
	UNUSED_PARAM(data_buffer_size);
	
	(*out_pkt_len) = 0;
	if (NULL != opt_out_source_address_id)
	{
		(*opt_out_source_address_id) = INVALID_ADDRESS_ID;
	}
	
	printf("FTN_recv is called! %lx %x\n", source_address_id, async);
	
	return FTN_ERROR_NOT_IMPLEMENTED;
}

FTN_RET_VAL FTN_send(const void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id)
{
	UNUSED_PARAM(data_buffer);
	UNUSED_PARAM(data_buffer_size);
	
	printf("FTN_send is called! %lx\n", dest_address_id);
	
	return FTN_ERROR_NOT_IMPLEMENTED;
}