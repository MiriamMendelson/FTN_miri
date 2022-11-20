#ifndef __FTN_LIB_INTERFACE_H__
#define __FTN_LIB_INTERFACE_H__

#include "FTN_common.h"


/*
 *  FTN - fast transport network
 *  library interface file
 */

// FTN consts
#define MAX_DATA_BUFFER_LEN (0x1000)
#define MAX_NUMBER_OF_CLIENTS (0x10)
// our definitions
#define CONN_REQ ("get id")
#define GET_RAND_PORT (0)
#define GET_PRIVATE_ID (0)
#define CLI_ARR_SIZE(n) (sizeof(END_POINT) * n)
#define PACKET_MAX_SIZE (1024)
#define RING_BUFF_MAX_SIZE (300)
uint64_t g_num_of_cli;
uint64_t SOCKFD;
/*
 * the address id 0 is reserved for broadcast
 * server id is 1
 * valid client id is 2 - n (n is the number of connected clients)
 * so num_of_nodes_in_network is the number of all clients including the server, and the last valid address
 */
#define BROADCAST_ADDRESS_ID (0)
#define SERVER_ADDRESS_ID (1)
#define FIRST_CLIENT_ADDRESS_ID (2)
#define INVALID_ADDRESS_ID (~0ULL)

typedef struct FTN_IPV4_ADDR
{
	uint8_t ip_addr[4];
} FTN_IPV4_ADDR;

typedef struct END_POINT
{
	uint8_t id;
	char ip_addr[INET_ADDRSTRLEN];
	uint64_t port;
} END_POINT;

END_POINT CLIENTS[MAX_NUMBER_OF_CLIENTS];

/*
 *  FTN_server_init
 *  Init FTN infrastructure as a server. must be called before any other API
 *  params:
 *  	server_port - server listening port number
 *  	num_of_nodes_in_network - (num of clients expected to connect + 1 for the server)
 *
 *	this function shuld block and not return until all clients will be connected and infrastructure is up and running!
 *	on success return FTN_ERROR_SUCCESS
 */
FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network);

/*
 *  FTN_client_init
 *  Init FTN infrastructure as a client. must be called before any other API
 *  params:
 *  	server_str_ip - ip of the server app
 *  	server_port - server listening port to connect to
 *  	out_my_address - A pointer to the variable that receives the address id of my current client
 *
 *	this function shuld block and not return until all other clients will be connected and infrastructure is up and running!
 *	on success return FTN_ERROR_SUCCESS
 */
FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t *out_my_address);

/*
 *  FTN_recv
 *  recv pkt from network
 *  params:
 *		data_buffer - A pointer to the buffer that receives the data read from network
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
 *		2. if async == FALSE - and there is no pkts wating in the Q, this funtion shuld block and wait to recv pkts.
 *		   if set to TRUE - this function shuld'n block, return success and set *out_pkt_len = 0
 *
 *	on success return FTN_ERROR_SUCCESS
 */
FTN_RET_VAL FTN_recv(void *data_buffer, uint64_t data_buffer_size, uint64_t *out_pkt_len, uint64_t source_address_id, uint64_t *opt_out_source_address_id, bool async);

/*
 *  FTN_send
 *  send pkt to network
 *  params:
 *		data_buffer - A pointer to the buffer containing the data to be send to other application
 *		data_buffer_size - The number of bytes to be send (in bytes)
 *  	dest_address_id - address id of the target app to send to
						if set to 0 - this is broadcast
						if set to other number: send to this client
 *
 *	NOTES:
 *		1. this function block only if there is no room to queue this msg
 *
 *  on success return FTN_ERROR_SUCCESS
 */
FTN_RET_VAL FTN_send(void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id);

#endif