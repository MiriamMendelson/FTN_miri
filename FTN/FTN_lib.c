#include "socket_operations.c"
#include "ring_buffer.c"

#define min(a, b) ((a < b) ? (a) : (b))
#define verify_succ(x) ((x != 0) ? exit(FTN_ERROR_MUTEX_ERROR) : 1)

ring_buffer ring_buffs[MAX_NUMBER_OF_CLIENTS] = {};
END_POINT CLIENTS[MAX_NUMBER_OF_CLIENTS] = {};

unsigned int sockaddr_len = 16;
pthread_t rcv_handler_thread;
bool run_mood = false;
pthread_mutex_t lock;
// will be used in shmem
int64_t g_fd_shmem = 0;
void *g_shmem_adrr = NULL;

void *manage_insertion_rings(void *param)
{
	UNUSED_PARAM(param);
	uint64_t i = 0;
	run_mood = true;		 // thread stated- we're good to go!
	uint64_t actual_len = 0; // REMM! turn back to int64_t
	uint64_t seq_counter = 0;
	struct sockaddr_in from = {0};
	char msg_to_insert[MAX_DATA_BUFFER_LEN] = {0};

	while (1)
	{
		actual_len = recvfrom(SOCKFD, &(msg_to_insert[0]), MAX_DATA_BUFFER_LEN, NO_FLAGS, (struct sockaddr *)&from, &sockaddr_len);
		if (0 == actual_len)
		{
			return (void *)FTN_ERROR_NETWORK_FAILURE;
		}
		verify_succ(pthread_mutex_lock(&lock)); // wait on lock before referencing shared DS
		for (i = SERVER_ADDRESS_ID; i < g_num_of_cli; i++)
		{
			if (is_same_addr(&from, &(CLIENTS[i])))
			{
				while (RB_is_full(&ring_buffs[i]))
				{
					verify_succ(pthread_mutex_unlock(&lock)); // releasing & owning mutex while checking DS
					verify_succ(pthread_mutex_lock(&lock));
				}

				if (true != RB_insert(&ring_buffs[i], &msg_to_insert[0], seq_counter++, actual_len))
				{
					return (void *)FTN_ERROR_DATA_SENT_IS_TOO_LONG;
				}
				verify_succ(pthread_mutex_unlock(&lock));
				break;
			}
		}
	}
	return NULL;
}
bool copy_data_to_cli(void *data_buffer, uint64_t data_buffer_size, const void *restrict src, uint64_t src_len, uint64_t *out_pkt_len)
{
	*out_pkt_len = min(data_buffer_size, src_len);
	memcpy(data_buffer, src, *out_pkt_len);
	return true;
}
bool store_endpoint(int index, struct sockaddr_in *end_point)
{
	if (index > MAX_NUMBER_OF_CLIENTS)
	{
		return false;
	}
	CLIENTS[index].id = index;
	CLIENTS[index].port = end_point->sin_port;
	memcpy(&CLIENTS[index].ip.ip_addr, &(end_point->sin_addr.s_addr), sizeof(uint32_t));

	return true;
}
bool thread_start()
{
	verify_succ(pthread_mutex_init(&lock, NULL)); // init mutex- before it gets lock in the thread func

	return (0 == pthread_create(&rcv_handler_thread, NULL,
								manage_insertion_rings, NULL));
}
FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network)
{
	uint32_t x = 0;
	struct sockaddr_in server = {};
	struct sockaddr_in cli_addr = {};
	char conn_buff[sizeof(CONN_REQ)];

	g_num_of_cli = num_of_nodes_in_network + SERVER_ADDRESS_ID; // global num of cli + (1): ignoring the 0's place in the cli arr

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
		if (-1 == recvfrom(SOCKFD, &conn_buff, sizeof(CONN_REQ), NO_FLAGS, (struct sockaddr *)&cli_addr, &sockaddr_len))
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
		if (0 == strcmp(conn_buff, CONN_REQ))
		{
			if (true != store_endpoint(x, &cli_addr))
			{
				return FTN_ERROR_UNKNONE;
			}
		}
	}

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++) // send EP-details arr to all conncted cli's
	{
		CLIENTS[GET_PRIVATE_ID].id = x; // store the ID in 0 index (cli will extract it from there)

		if (send_pkt_to_cli(CLIENTS, CLI_ARR_SIZE(g_num_of_cli), x) == FTN_ERROR_NETWORK_FAILURE)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	CLIENTS[GET_PRIVATE_ID].id = SERVER_ADDRESS_ID; // set serves ID back to 1

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}

	if (!thread_start())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}

	return FTN_ERROR_SUCCESS;
}
FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t *out_my_address)
{
	struct sockaddr_in client = {};
	int num_of_received_bytes;

	SOCKFD = create_socket(GET_RAND_PORT, &client);
	if (0 == SOCKFD)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	memcpy(&CLIENTS[SERVER_ADDRESS_ID].ip.ip_addr, &server_ip, sizeof(uint32_t));
	CLIENTS[SERVER_ADDRESS_ID].port = htons(server_port);
	if (send_pkt_to_cli(CONN_REQ, sizeof(CONN_REQ), SERVER_ADDRESS_ID) == FTN_ERROR_NETWORK_FAILURE)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}

	num_of_received_bytes = recv(SOCKFD, &CLIENTS, CLI_ARR_SIZE(MAX_NUMBER_OF_CLIENTS), 0);
	if (num_of_received_bytes == -1)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	g_num_of_cli = num_of_received_bytes / sizeof(END_POINT); // calculat the actual num of conn cli

	*out_my_address = CLIENTS[GET_PRIVATE_ID].id; // extract my ID from 0 index element

	memcpy(&CLIENTS[SERVER_ADDRESS_ID].ip.ip_addr, &server_ip, sizeof(uint32_t));
	CLIENTS[SERVER_ADDRESS_ID].port = htons(server_port); // store server details in index 1

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}

	if (!thread_start())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}

	// TODO - if ID didnt arrive retry.
	printf("FTN_client_init is finished! ip %d.%d.%d.%d port %lx\n", server_ip.ip_addr[0], server_ip.ip_addr[1], server_ip.ip_addr[2], server_ip.ip_addr[3], server_port);
	return FTN_ERROR_SUCCESS;
}
FTN_RET_VAL FTN_recv(void *data_buffer, uint64_t data_buffer_size, uint64_t *out_pkt_len,
					 uint64_t source_address_id, uint64_t *opt_out_source_address_id, bool async)
{
	uint64_t out_index = 0;
	uint64_t cli_index = 0;
	bool ret_val = false;

	if (!run_mood) // no srv.cli was created yet...
	{
		printf("run_mood is off!\n");
		return FTN_ERROR_NO_CONNECT;
	}

	if (source_address_id != BROADCAST_ADDRESS_ID) // specific rcv
	{
		verify_succ(pthread_mutex_lock(&lock));				   // wait on lock before referncing shared DS
		if (RB_empty(&ring_buffs[source_address_id]) && async) // no msgs & no blocking
		{
			pthread_mutex_unlock(&lock);
			*out_pkt_len = 0;
			opt_out_source_address_id = NULL;
			return FTN_ERROR_SUCCESS;
		}
		while (RB_empty(&ring_buffs[source_address_id])) // blocking is on
		{
			verify_succ(pthread_mutex_unlock(&lock)); // releasing & owning mutex while checking DS
			verify_succ(pthread_mutex_lock(&lock));
		}
		ret_val = RB_extract(&ring_buffs[source_address_id], &out_index); // done waiting- getting the new msg
		if (ret_val)
		{
			copy_data_to_cli(data_buffer, data_buffer_size, ring_buffs[source_address_id].msgs[out_index].msg, ring_buffs[source_address_id].msgs[out_index].len, out_pkt_len);
		}
		verify_succ(pthread_mutex_unlock(&lock));
	}
	else // get any- return oldest message
	{
		do
		{
			verify_succ(pthread_mutex_lock(&lock)); // releasing & owning mutex while checking DS

			ret_val = RB_extract_first(ring_buffs, g_num_of_cli, &cli_index); // find out which cli has earliest msg

			if (ret_val) // where there any msgs at all?
			{
				ret_val = RB_extract(&ring_buffs[cli_index], &out_index);
				if (ret_val)
				{
					copy_data_to_cli(data_buffer, data_buffer_size, ring_buffs[cli_index].msgs[out_index].msg, ring_buffs[cli_index].msgs[out_index].len, out_pkt_len);
				}
			}
			verify_succ(pthread_mutex_unlock(&lock)); // releasing & owning mutex while checking DS
		} while (!ret_val && !async);

		if (!ret_val) // there were NO msgs...
		{
			out_pkt_len = 0;
			opt_out_source_address_id = NULL;
		}
	}

	if (NULL != opt_out_source_address_id)
	{
		(*opt_out_source_address_id) = source_address_id == BROADCAST_ADDRESS_ID ? cli_index : source_address_id;
	}

	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL FTN_send(void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id)
{
	uint64_t x = 0;

	if (dest_address_id > MAX_NUMBER_OF_CLIENTS)
	{
		return FTN_ERROR_ARGUMENTS_ERROR;
	}

	if (dest_address_id == BROADCAST_ADDRESS_ID)
	{
		for (x = SERVER_ADDRESS_ID; x < g_num_of_cli; x++)
		{
			if (x == CLIENTS[GET_PRIVATE_ID].id) // dont send to yourself!
			{
				continue;
			}
			if (FTN_ERROR_NETWORK_FAILURE == send_pkt_to_cli(data_buffer, data_buffer_size, x))
			{
				return FTN_ERROR_NETWORK_FAILURE;
			}
		}
	}
	else // specific sending
	{
		if (send_pkt_to_cli(data_buffer, data_buffer_size, dest_address_id) == FTN_ERROR_NETWORK_FAILURE)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	return FTN_ERROR_SUCCESS;
}