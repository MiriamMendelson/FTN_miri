
#include <stdint.h>
#include <stdbool.h>
#include "FTN_common.h"

#ifndef __FTN_LIB_INTERFACE_H__
#define __FTN_LIB_INTERFACE_H__

/*
 *  FTN - fast transport network
 *  library interface file
 */


/*
 *  FTN consts
 *  
 */
#define MAX_DATA_BUFFER_LEN (0x280000000)
#define MAX_NUMBER_OF_CLIENTS (0x500)

/*
 * the address id 0 is reserved for broadcast
 * server id is 1
 * valid client id is 2 - n (n is the number of connected clients)
 */
#define BROADCAST_ADDRESS_ID (0)
#define INVALID_ADDRESS_ID (~0ULL)

typedef struct FTN_IPV4_ADDR
{
	uint8_t ip_addr[4];
} FTN_IPV4_ADDR;

/*
 *  FTN_server_init
 *  Init FTN infrastructure as a server. must be called before any other API
 *  params:
 *  	server_port - server listening port number
 *  	num_of_expected_clients - num of clients expected to connect
 *
 *	this function shuld block and not return until all clients will be connected and infrastructure is up and running!
 */
FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_expected_clients);

/*
 *  FTN_client_init
 *  Init FTN infrastructure as a client. must be called before any other API
 *  params:
 *  	server_str_ip - ip of the server app
 *  	server_port - server listening port to connect to
 *  	out_my_address - get the address id to me as a client
 *
 *	this function shuld block and not return until all other clients will be connected and infrastructure is up and running!
 */
FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t * out_my_address);

/*
 *  FTN_recv
 *  recv pkt from network
 *  params:
 *      data_buffer - A pointer to the buffer that receives the data read from network
 *		data_buffer_size - The maximum number of bytes to be read (in bytes)
 *		out_pkt_len - A pointer to the variable that receives the number of bytes actually read into the data buffer (in bytes)
 *  	source_address_id - spesific source to recv from.
							if set to 0 - get ANY source.
							if set to other number: get pkts only from this spesific src
 *  	opt_out_source_address_id - A pointer to the variable that receives the source address id of the pkt. (optional - can be set to NULL)
 *  	async - if set to TRUE, this function never block!.
 *
 *	NOTES:
 *		1. if pkt is bigger then data_buffer_size, must return error. set *out_pkt_len to expected pkt size
 *      2. if async == FALSE - and there is no pkts wating in the Q, this funtion shuld block and wait to recv pkts.
 *		   if set to TRUE - this function shuld'n block, return success and set *out_pkt_len = 0
 */
FTN_RET_VAL FTN_recv(void * data_buffer, uint64_t data_buffer_size, uint64_t * out_pkt_len, uint64_t source_address_id, uint64_t * opt_out_source_address_id, bool async);

/*
 *  FTN_send
 *  send pkt to network
 *  params:
 *      data_buffer - A pointer to the buffer containing the data to be send to other application
 *		data_buffer_size - The number of bytes to be send (in bytes)
 *  	dest_address_id - address id of the target app to send to
							if set to 0 - this is broadcast
							if set to other number: send to this client
 *
 *	NOTES:
 *		1. this function block only is there is no room to queue this msg
 */
FTN_RET_VAL FTN_send(const void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id);

#endif