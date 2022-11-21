#include "./socket_operations.c"
#include "ring_buffer.c"

#define min(a, b) (a > b ? a : b)

unsigned int SOCKADDR_LEN = (8);
ring_buffer ring_buffs[MAX_NUMBER_OF_CLIENTS];

pthread_t rcv_handler_thread;
pthread_mutex_t lock;
bool first_call = true;

pthread_mutex_t recv_thread_start;

bool is_same_addr(struct sockaddr_in *src, END_POINT *ep)
{
	bool res;
	char str[INET_ADDRSTRLEN];
	res = (src->sin_port == ep->port);
	inet_ntop(AF_INET, &(((struct sockaddr_in *)src)->sin_addr),
			  str, INET_ADDRSTRLEN);
	res &= (strcmp(ep->ip_addr, str) == 0);

	return res;
}
void *recv_and_insert_from_OS_to_ring(void *param)
{
	UNUSED_PARAM(param);
	while (1)
	{
		struct sockaddr_in from;
		char msg_to_insert[MAX_DATA_BUFFER_LEN];
		uint64_t actual_len;
		actual_len = recvfrom(SOCKFD, &(msg_to_insert[0]), MAX_DATA_BUFFER_LEN, 0, (struct sockaddr *)&from, &SOCKADDR_LEN);
		if (!first_call)
		{
			pthread_mutex_lock(&lock);
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
	CLIENTS[index].port = end_point->sin_port;
	CLIENTS[index].id = index;

	char str_ip[INET_ADDRSTRLEN];
	if (inet_ntop(AF_INET, &(((struct sockaddr_in *)end_point)->sin_addr), str_ip, INET_ADDRSTRLEN) == NULL)
		return false;

	if (strcpy(CLIENTS[index].ip_addr, str_ip) == NULL)
	{
		return false;
	}
	printf("stored ip:%s, port: %ld, index:%d\n", CLIENTS[index].ip_addr, CLIENTS[index].port, index);
	return true;
}
void get_my_ip()
{
	char host[256];
	char *IP;
	struct hostent *host_entry;
	gethostname(host, sizeof(host));  // find the host name
	host_entry = gethostbyname(host); // find host information
	IP = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
	// Convert into IP string
	printf("Current Host Name: %s\n", host);
	printf("Host IP: %s", IP);
}
bool thread_start()
{
	bool ret_val = true;
	ret_val &= (pthread_mutex_lock(&lock) == 0);
	ret_val &= (pthread_create(&rcv_handler_thread, NULL,
							   recv_and_insert_from_OS_to_ring, NULL) == 0);
	return ret_val;
}

FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network)
{
	uint32_t x = 0;
	struct sockaddr_in *server;
	struct sockaddr_in from;
	unsigned int addrlen = sizeof(from);
	char data_received[sizeof(CONN_REQ)];
	g_num_of_cli = num_of_nodes_in_network + SERVER_ADDRESS_ID;
	SOCKFD = create_socket(server_port, &server);

	if (num_of_nodes_in_network > MAX_NUMBER_OF_CLIENTS)
	{
		printf("num of clients is too big .\n updata to %d", MAX_NUMBER_OF_CLIENTS);
		g_num_of_cli = MAX_NUMBER_OF_CLIENTS;
	}

	printf("--------waiting for clients---------\n");
	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		recvfrom(SOCKFD, &data_received, sizeof(CONN_REQ), 0, (struct sockaddr *)&from, &addrlen);
		if (0 == strcmp(data_received, CONN_REQ))
		{
			store_endpoint(x, &from);
		}
	}
	printf("********got all of the clients*********\n");

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		CLIENTS[GET_PRIVATE_ID].id = x;

		struct sockaddr_in cli_addr = {AF_INET, CLIENTS[x].port, {inet_addr(CLIENTS[x].ip_addr)}, {0}};
		if (sendto(SOCKFD, CLIENTS, CLI_ARR_SIZE(g_num_of_cli),
				   0, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) == -1)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	CLIENTS[GET_PRIVATE_ID].id = SERVER_ADDRESS_ID; // set our dear serves ID back to original

	if (!init_rb_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}
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
	char server_ip_str[INET_ADDRSTRLEN];
	int num_of_received_bytes;

	if (0 == convert_ip_to_str(server_ip, server_ip_str))
	{
		printf("couldn convert ip addr");
		return FTN_ERROR_ARGUMENTS_ERROR;
	}

	struct sockaddr_in server = declare_server(server_ip_str, server_port);
	struct sockaddr_in *client;

	SOCKFD = create_socket(GET_RAND_PORT, &client);

	printf("clints port: %d\n", client->sin_port);
	printf("servers port: %d\n", server.sin_port);
	printf("servers ip: %d\n", server.sin_addr.s_addr);

	if (sendto(SOCKFD, CONN_REQ, sizeof(CONN_REQ), 0, (struct sockaddr *)&server, sizeof(server)) == -1)
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

	if (!init_rb_arr(ring_buffs, g_num_of_cli))
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
	pthread_mutex_lock(&lock);
	uint32_t out_index;
	uint32_t cli_index;
	bool ret_val;
	if (0 != source_address_id)
	{
		if (empty(&ring_buffs[source_address_id]) && async)
		{
			*out_pkt_len = 0;
			UNUSED_PARAM(*opt_out_source_address_id);
			pthread_mutex_unlock(&lock);
			return FTN_ERROR_SUCCESS;
		}
		while (empty(&ring_buffs[source_address_id]))
		{
			pthread_mutex_unlock(&lock);
			pthread_mutex_lock(&lock);
		}
		ret_val = extract(&ring_buffs[source_address_id], &out_index);
		if (ret_val)
		{
			*out_pkt_len = min(data_buffer_size, ring_buffs[source_address_id].msgs[out_index].len);
			ret_val = (memcpy(data_buffer, ring_buffs[source_address_id].msgs[out_index].msg, *out_pkt_len) != NULL);
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
				ret_val = (memcpy(data_buffer, ring_buffs[cli_index].msgs[out_index].msg, *out_pkt_len) != NULL);
			}
		}
	}
	pthread_mutex_unlock(&lock);

	if (NULL != opt_out_source_address_id)
	{
		(*opt_out_source_address_id) = source_address_id == 0 ? cli_index : source_address_id;
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

	if (dest_address_id == 0) // broadcast
	{
		for (x = FIRST_CLIENT_ADDRESS_ID; x <= g_num_of_cli; x++)
		{
			dest_client = (struct sockaddr_in){AF_INET, CLIENTS[x].port, {inet_addr(CLIENTS[x].ip_addr)}, {0}};

			if (sendto(SOCKFD, data_buffer, data_buffer_size, 0, (struct sockaddr *)&dest_client, sizeof(dest_client)) == -1)
			{
				return FTN_ERROR_NETWORK_FAILURE;
			}
		}
		return FTN_ERROR_SUCCESS;
	}

	// TODO move to outer func
	dest_client = (struct sockaddr_in){AF_INET, CLIENTS[dest_address_id].port, {inet_addr(CLIENTS[dest_address_id].ip_addr)}, {0}};
	if (sendto(SOCKFD, data_buffer, data_buffer_size, 0, (struct sockaddr *)&dest_client, sizeof(dest_client)) == -1)
	{
		printf("sendto ho nooooo\n");
		return FTN_ERROR_NETWORK_FAILURE;
	}
	return FTN_ERROR_SUCCESS;
}