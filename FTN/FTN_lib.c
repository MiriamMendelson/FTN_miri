#include "socket_operations.c"
#include "ring_buffer.c"

#define min(a, b) (a < b ? a : b)

unsigned int sockaddr_len = (8);
ring_buffer ring_buffs[MAX_NUMBER_OF_CLIENTS];
END_POINT CLIENTS[MAX_NUMBER_OF_CLIENTS];
pthread_t rcv_handler_thread;
pthread_mutex_t lock;
bool first_call = true;

pthread_mutex_t recv_thread_start;

void *recv_and_insert_from_OS_to_ring(void *param)
{
	UNUSED_PARAM(param);
	while (1)
	{
		struct sockaddr_in from;
		char msg_to_insert[MAX_DATA_BUFFER_LEN];
		int64_t actual_len;
		actual_len = recvfrom(SOCKFD, &(msg_to_insert[0]), MAX_DATA_BUFFER_LEN, NO_FLAGS, (struct sockaddr *)&from, &sockaddr_len);
		if (-1 == actual_len)
		{
			return (void *)FTN_ERROR_NETWORK_FAILURE;
		}

		if (!first_call)
		{
			pthread_mutex_lock(&lock);//!!!
		}
		else
		{
			first_call = false;
		}
		for (uint64_t i = SERVER_ADDRESS_ID; i <= g_num_of_cli; i++)
		{
			if (is_same_addr(&from, &(CLIENTS[i])))
			{
				insert(&ring_buffs[i], &msg_to_insert[0], actual_len);

				pthread_mutex_unlock(&lock);
				break;
			}
		}
	}
	return NULL;
}
bool store_endpoint(int index, struct sockaddr_in *end_point)
{
	if (index > MAX_NUMBER_OF_CLIENTS)
	{
		return false;
	}
	CLIENTS[index].port = end_point->sin_port;
	CLIENTS[index].id = index;
	memcpy(&CLIENTS[index].ip.ip_addr, &(end_point->sin_addr.s_addr), sizeof(uint32_t));

	return true;
}
bool thread_start()
{
	bool ret_val = true;

	if (pthread_mutex_lock(&lock)) // mutex own failed (mutex_lock returns 0 on succ).
	{
		return false;
	}

	ret_val &= (pthread_create(&rcv_handler_thread, NULL,
							   recv_and_insert_from_OS_to_ring, NULL) == 0);
	return ret_val;
}

FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network)
{
	uint32_t x = 0;
	struct sockaddr_in server = {};
	struct sockaddr_in cli_addr = {};
	unsigned int addrlen = sizeof(cli_addr);
	char data_received[sizeof(CONN_REQ)];
	g_num_of_cli = num_of_nodes_in_network + SERVER_ADDRESS_ID;
	SOCKFD = create_socket(server_port, &server);
	if (0 == SOCKFD)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	if (num_of_nodes_in_network > MAX_NUMBER_OF_CLIENTS)
	{
		return FTN_ERROR_ARGUMENTS_ERROR;
	}

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		printf("--------waiting for cli---------\n");
		if (-1 == recvfrom(SOCKFD, &data_received, sizeof(CONN_REQ), NO_FLAGS, (struct sockaddr *)&cli_addr, &addrlen))
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
		if (0 == strcmp(data_received, CONN_REQ))
		{
			store_endpoint(x, &cli_addr);
		}
	}
	printf("********got all of the clients*********\n");

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		CLIENTS[GET_PRIVATE_ID].id = x; // Put in the first place -the ID

		if (!init_addr(&cli_addr, CLIENTS[x].port, CLIENTS[x].ip.ip_addr))
		{
			return FTN_ERROR_UNKNONE;
		}
		if (sendto(SOCKFD, CLIENTS, CLI_ARR_SIZE(g_num_of_cli),
				   NO_FLAGS, (struct sockaddr *)&cli_addr, addrlen) == -1)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	CLIENTS[GET_PRIVATE_ID].id = SERVER_ADDRESS_ID; // set our dear serves ID back to original

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}
	printf("server is saying: num of cli: %ld and they all have RB's. my privat id is: %d\n", g_num_of_cli, CLIENTS[GET_PRIVATE_ID].id);

	if (!thread_start())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}

	printf("FTN_server_init is finished! server_port %lx.\n", server_port);

	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t *out_my_address)
{
	int num_of_received_bytes;
	uint32_t serv_ip;
	memcpy(&serv_ip, server_ip.ip_addr, sizeof(uint32_t));
	struct sockaddr_in server = {AF_INET, htons(server_port), {serv_ip}, {0}};
	struct sockaddr_in client = {};

	SOCKFD = create_socket(GET_RAND_PORT, &client);
	if (0 == SOCKFD)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	printf("clints port: %d\n", client.sin_port);
	printf("servers port: %d\n", server.sin_port);
	printf("servers ip: %d\n", server.sin_addr.s_addr);

	if (sendto(SOCKFD, CONN_REQ, sizeof(CONN_REQ), NO_FLAGS, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}

	num_of_received_bytes = recv(SOCKFD, &CLIENTS, CLI_ARR_SIZE(MAX_NUMBER_OF_CLIENTS), 0);

	if (num_of_received_bytes == -1)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}

	g_num_of_cli = num_of_received_bytes / sizeof(END_POINT);

	*out_my_address = CLIENTS[GET_PRIVATE_ID].id;

	// updating servers details
	store_endpoint(SERVER_ADDRESS_ID, &server);

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}
	printf("-----sec head %ld, tail %ld, num o cli: %ld after isialization\n", ring_buffs[2].head, ring_buffs[2].tail, g_num_of_cli);

	if (!thread_start())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}

	// TODO - if ID didnt arived retry.

	printf("FTN_client_init is finished! ip %d.%d.%d.%d port %lx\n", server_ip.ip_addr[0], server_ip.ip_addr[1], server_ip.ip_addr[2], server_ip.ip_addr[3], server_port);

	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL FTN_recv(void *data_buffer, uint64_t data_buffer_size, uint64_t *out_pkt_len,
					 uint64_t source_address_id, uint64_t *opt_out_source_address_id, bool async)
{
	// printf("<<<<< FTN_recv is called! source_address_id: %lx, my id: %d, is aync: %d.\n", source_address_id, CLIENTS[GET_PRIVATE_ID].id, async);
	pthread_mutex_lock(&lock);

	uint64_t out_index;
	uint64_t cli_index;
	bool ret_val;
	if (0 != source_address_id) // specific rcv
	{
		if (empty(&ring_buffs[source_address_id]) && async) // no blocking
		{
			pthread_mutex_unlock(&lock);
			*out_pkt_len = 0;
			opt_out_source_address_id = NULL;
			return FTN_ERROR_SUCCESS;
		}
		while (empty(&ring_buffs[source_address_id])) // blocking is on
		{
			pthread_mutex_unlock(&lock);
			pthread_mutex_lock(&lock);
		}
		ret_val = extract(&ring_buffs[source_address_id], &out_index);
		if (ret_val)
		{
			*out_pkt_len = min(data_buffer_size, ring_buffs[source_address_id].msgs[out_index].len);
			memcpy(data_buffer, ring_buffs[source_address_id].msgs[out_index].msg, *out_pkt_len);
		}
	}
	else // get any, return oldest message
	{
		ret_val = extract_first(ring_buffs, g_num_of_cli, &cli_index);
		if (ret_val)
		{
			ret_val = extract(&ring_buffs[cli_index], &out_index);
			if (ret_val)
			{
				*out_pkt_len = min(data_buffer_size, ring_buffs[cli_index].msgs[out_index].len);
				memcpy(data_buffer, ring_buffs[cli_index].msgs[out_index].msg, *out_pkt_len);
			}
		}
	}
	pthread_mutex_unlock(&lock);

	if (NULL != opt_out_source_address_id)
	{
		(*opt_out_source_address_id) = source_address_id == BROADCAST_ADDRESS_ID ? cli_index : source_address_id;
	}

	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL FTN_send(void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id)
{
	struct sockaddr_in dest_client;
	uint64_t x;
	// if(dest is in same com) use shared memory
	// else:
	if (dest_address_id > MAX_NUMBER_OF_CLIENTS)
	{
		return FTN_ERROR_ARGUMENTS_ERROR;
	}

	if (dest_address_id == BROADCAST_ADDRESS_ID) // broadcast
	{
		for (x = FIRST_CLIENT_ADDRESS_ID; x <= g_num_of_cli; x++)
		{
			if (!init_addr(&dest_client, CLIENTS[x].port, CLIENTS[x].ip.ip_addr))
			{
				return FTN_ERROR_UNKNONE;
			}
			if (sendto(SOCKFD, data_buffer, data_buffer_size, NO_FLAGS, (struct sockaddr *)&dest_client, sizeof(dest_client)) == -1)
			{
				return FTN_ERROR_NETWORK_FAILURE;
			}
		}
		return FTN_ERROR_SUCCESS;
	}

	// TODO move to outer func
	if (!init_addr(&dest_client, CLIENTS[dest_address_id].port, CLIENTS[dest_address_id].ip.ip_addr))
	{
		return FTN_ERROR_UNKNONE;
	}
	if (sendto(SOCKFD, data_buffer, data_buffer_size, NO_FLAGS, (struct sockaddr *)&dest_client, sizeof(dest_client)) == -1)
	{
		printf("sendto ho nooooo\n");
		return FTN_ERROR_NETWORK_FAILURE;
	}
	return FTN_ERROR_SUCCESS;
}