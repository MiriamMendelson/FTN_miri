#include "socket_operations.c"
#include "ring_buffer.c"
#include "shmem_operations.c"

#define min(a, b) ((a < b) ? (a) : (b))
#define verify_succ(x) ((x != 0) ? exit(FTN_ERROR_MUTEX_ERROR) : 1)

ring_buffer ring_buffs[MAX_NUMBER_OF_CLIENTS] = {};
END_POINT CLIENTS[MAX_NUMBER_OF_CLIENTS] = {};

unsigned int sockaddr_len = 16;
pthread_t rcv_handler_thread;
pthread_t shmem_handler_thread;
bool run_mood = false;
bool shmem_thread_on = false;
pthread_mutex_t lock;
uint64_t seq_counter = 0;
uint64_t g_my_id = 0;
uint64_t g_num_of_cli = 0;
uint64_t g_sockfd = 0;
// will be used in shmem
int64_t g_fd_shmem = 0;
void *g_shmem_adrr = NULL;
ring_buffer *g_shmem_buffer = NULL;

/* 
LOG uses
void DumpHex(const void *data, size_t size)
{
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i)
	{
		printf("%02X ", ((unsigned char *)data)[i]);
		if (((unsigned char *)data)[i] >= ' ' && ((unsigned char *)data)[i] <= '~')
		{
			ascii[i % 16] = ((unsigned char *)data)[i];
		}
		else
		{
			ascii[i % 16] = '.';
		}
		if ((i + 1) % 8 == 0 || i + 1 == size)
		{
			printf(" ");
			if ((i + 1) % 16 == 0)
			{
				printf("|  %s \n", ascii);
			}
			else if (i + 1 == size)
			{
				ascii[(i + 1) % 16] = '\0';
				if ((i + 1) % 16 <= 8)
				{
					printf(" ");
				}
				for (j = (i + 1) % 16; j < 16; ++j)
				{
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}
#define LOGGER_SIZE (0xffff)
msg_log logg_rcv[LOGGER_SIZE] = {0};
uint64_t lgr_rcv_count = 0;
msg_log logg_snt[LOGGER_SIZE] = {0};
uint64_t lgr_snt_count = 0;
bool print_log(msg_log *logg, uint64_t len)
{
	UNUSED_PARAM(len);
	for (uint64_t i = 1; i < len; i++)
	{
		printf("inedx: %ld src: %ld des: %ld len: %ld\n", i, logg[i].src, logg[i].target, logg[i].len);
		DumpHex(logg[i].msg, logg[i].len);
		printf(" ------------------------------------\n");
	}
	return true;
}
void add_pkt_log(bool is_send, void *data_buffer, uint64_t len, uint64_t src, uint64_t dest)
{
	if (lgr_snt_count >= LOGGER_SIZE || lgr_rcv_count >= LOGGER_SIZE)
	{
		// printf("lgr_snt_count: %ld, lgr_rcv_count: %ld.LOGGER-------LEAK HEAR\n", lgr_snt_count, lgr_rcv_count);
		return;
	}
	uint64_t *ptr_pos = NULL;
	uint64_t cur_pos = 0;
	msg_log *log_entry = NULL;

	if (is_send)
	{
		ptr_pos = &lgr_snt_count;
		log_entry = logg_snt;
	}
	else
	{
		ptr_pos = &lgr_rcv_count;
		log_entry = logg_rcv;
	}
	cur_pos = __atomic_fetch_add(ptr_pos, 1, __ATOMIC_SEQ_CST);

	log_entry += cur_pos;
	if (len >= MAX_DATA_BUFFER_LEN)
	{
		printf("U SEND A HUGEEEEEE DATA\n");
		exit(100);
	}

	memcpy(log_entry->msg, data_buffer, len);
	log_entry->len = len;
	log_entry->target = dest;
	log_entry->src = src;
}
*/

void *manage_insertion_rings(void *param)
{
	UNUSED_PARAM(param);
	uint64_t i = 0;
	run_mood = true; // thread stated- we're good to go!
	int64_t ret_val = 0;
	uint64_t actual_len = 0; // REMM! turn back to int64_t
	struct sockaddr_in from = {0};
	uint64_t seq = 0;
	msg new_msg = {0};

	while (1)
	{
		ret_val = recvfrom(g_sockfd, &new_msg.msg, MAX_DATA_BUFFER_LEN, NO_FLAGS, (struct sockaddr *)&from, &sockaddr_len);
		if (0 >= ret_val)
		{
			return (void *)FTN_ERROR_NETWORK_FAILURE;
		}
		if (ret_val > PACKET_MAX_SIZE)
		{
			printf("data is too big\n");
			return (void *)FTN_ERROR_DATA_SENT_IS_TOO_LONG;
		}
		actual_len = ret_val;

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
				seq = __atomic_fetch_add(&seq_counter, 1, __ATOMIC_SEQ_CST);
				new_msg.len = actual_len;
				new_msg.src = i;
				new_msg.seq_num = seq;
				if (true != RB_insert(&ring_buffs[i], &new_msg))
				{
					return (void *)FTN_ERROR_DATA_SENT_IS_TOO_LONG;
				}
				verify_succ(pthread_mutex_unlock(&lock)); // release mutex
				break;
			}
		}
	}
	return NULL;
}
void *manage_shmem_insertion_rings(void *param)
{
	UNUSED_PARAM(param);
	shmem_thread_on = true;
	msg new_msg = {0};

	while (1)
	{
		while (RB_empty(g_shmem_buffer))
			;

		if (!RB_extract(g_shmem_buffer, &new_msg))
		{
			printf("thread couldn't extract data\n");
		}
		new_msg.seq_num = __atomic_fetch_add(&seq_counter, 1, __ATOMIC_SEQ_CST);

		while (RB_is_full(&ring_buffs[new_msg.src]))
			;

		if (!RB_insert(&ring_buffs[new_msg.src], &new_msg))
		{
			printf("thread couldn't insert data\n");
		}
		// add_pkt_log(true, new_msg.msg, new_msg.len, new_msg.src, g_my_id);
	}
}
FTN_RET_VAL send_pkt_via_shm(void *data_buffer, uint64_t data_buffer_size, uint64_t dest)

{
	msg new_msg = {0};
	new_msg.src = g_my_id;
	new_msg.len = data_buffer_size;
	memcpy(new_msg.msg, data_buffer, data_buffer_size);

	// while (RB_is_full((ring_buffer *)CLIENTS[dest].shmem_addr))
	// 	;
	while (!RB_insert((ring_buffer *)CLIENTS[dest].shmem_addr, &new_msg))
	{
		// printf("couldn't sent data using shmem...\n");
	}

	return FTN_ERROR_SUCCESS;
}
bool copy_data_to_cli(void *data_buffer, uint64_t data_buffer_size, msg *msg, uint64_t *out_pkt_len)
{
	if (data_buffer_size < msg->len)
	{
		printf("provided data buffer is too small.\n");
		return false;
	}
	*out_pkt_len = msg->len;
	memcpy(data_buffer, msg->msg, *out_pkt_len);
	return true;
}
bool find_local_end_points()
{
	uint64_t i = 0;
	for (i = SERVER_ADDRESS_ID; i < g_num_of_cli; i++)
	{
		if (i == g_my_id)
		{
			continue;
		}
		if (try_open_exist_shmem(i, &CLIENTS[i].shmem_addr) == FTN_ERROR_SHMEM_FAILURE)
		{
			printf("strange mapping error accured\n");
		}
	}
	return true;
}
bool store_endpoint(int index, struct sockaddr_in *end_point)
{
	if (index > MAX_NUMBER_OF_CLIENTS)
	{
		printf("index out of range\n");
		return false;
	}
	CLIENTS[index].id = index;
	CLIENTS[index].port = end_point->sin_port;
	CLIENTS[index].shmem_addr = NULL;
	memcpy(&CLIENTS[index].ip.ip_addr, &(end_point->sin_addr.s_addr), sizeof(uint32_t));

	return true;
}
bool start_threads()
{
	bool ret_val = true;
	verify_succ(pthread_mutex_init(&lock, NULL)); // init mutex- before it gets lock in the thread func

	ret_val &= (pthread_create(&rcv_handler_thread, NULL,
							   manage_insertion_rings, NULL) == 0);
	ret_val &= (pthread_create(&shmem_handler_thread, NULL, manage_shmem_insertion_rings, NULL) == 0);
	return ret_val;
}
FTN_RET_VAL FTN_server_init(uint64_t server_port, uint64_t num_of_nodes_in_network)
{
	g_my_id = SERVER_ADDRESS_ID;
	uint32_t x = 0;
	struct sockaddr_in server = {};
	struct sockaddr_in cli_addr = {};
	char conn_buff[sizeof(CONN_REQ_MSG)];
	char shmm_buff[sizeof(CREATED_SHMEM_MSG)];

	g_num_of_cli = num_of_nodes_in_network + SERVER_ADDRESS_ID; // global num of cli + (1): ignoring the 0's place in the cli arr

	g_sockfd = create_socket(server_port, &server);
	if (0 == g_sockfd)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	if (num_of_nodes_in_network > MAX_NUMBER_OF_CLIENTS)
	{
		return FTN_ERROR_ARGUMENTS_ERROR;
	}

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		if (-1 == recvfrom(g_sockfd, &conn_buff, sizeof(CONN_REQ_MSG), NO_FLAGS, (struct sockaddr *)&cli_addr, &sockaddr_len))
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
		if (0 == strcmp(conn_buff, CONN_REQ_MSG))
		{
			if (true != store_endpoint(x, &cli_addr))
			{
				return FTN_ERROR_UNKNONE;
			}
		}
	}

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++) // send EP-details-arr to all conncted cli's
	{
		CLIENTS[GET_PRIVATE_ID].id = x; // store the ID in 0 index (cli will extract it from there)

		if (send_pkt_to_cli(CLIENTS, CLI_ARR_SIZE(g_num_of_cli), x) == FTN_ERROR_NETWORK_FAILURE)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}

	//-----> open our own shmm
	if (create_shmem(SERVER_ADDRESS_ID, &g_fd_shmem, &g_shmem_adrr) == FTN_ERROR_SHMEM_FAILURE)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	g_shmem_buffer = (ring_buffer *)g_shmem_adrr;

	init_ring_buffer(g_shmem_buffer);
	// printf("created shmem succesfully! in addr: %p, FD: %ld\n", g_shmem_adrr, g_fd_shmem);

	//-----> make sure all cli's created shmem :-D
	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		if (recv(g_sockfd, shmm_buff, sizeof(CREATED_SHMEM_MSG), 0) == -1)
		{
			printf("recv faild\n");
			x--;
			continue;
		}
		if (strcmp(shmm_buff, CREATED_SHMEM_MSG) != 0)
		{
			printf("CREATED_SHMEM error");
			return FTN_ERROR_SHMEM_FAILURE;
		}
	}

	//-----> tell everyone: "all cli are shmemed- u can now check who of them is your neighbor"
	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++)
	{
		if (send_pkt_to_cli(ALL_SHMEM_CONNCETED_MSG, sizeof(ALL_SHMEM_CONNCETED_MSG), x) == FTN_ERROR_NETWORK_FAILURE)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	if (!find_local_end_points())
	{
		return FTN_ERROR_SHMEM_FAILURE;
	}

	if (!start_threads())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}
	return FTN_ERROR_SUCCESS;
}
FTN_RET_VAL FTN_client_init(FTN_IPV4_ADDR server_ip, uint64_t server_port, uint64_t *out_my_address)
{
	struct sockaddr_in client = {0};
	int num_of_received_bytes = 0;
	char all_shmmed_buff[sizeof(ALL_SHMEM_CONNCETED_MSG)];

	g_sockfd = create_socket(GET_RAND_PORT, &client);
	if (0 == g_sockfd)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	memcpy(&CLIENTS[SERVER_ADDRESS_ID].ip.ip_addr, &server_ip, sizeof(uint32_t));
	CLIENTS[SERVER_ADDRESS_ID].port = htons(server_port);
	if (send_pkt_to_cli(CONN_REQ_MSG, sizeof(CONN_REQ_MSG), SERVER_ADDRESS_ID) == FTN_ERROR_NETWORK_FAILURE)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}

	num_of_received_bytes = recv(g_sockfd, &CLIENTS, CLI_ARR_SIZE(MAX_NUMBER_OF_CLIENTS), 0);
	if (num_of_received_bytes <= 0)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	g_num_of_cli = num_of_received_bytes / sizeof(END_POINT); // calculat the actual num of conn cli
	g_my_id = CLIENTS[GET_PRIVATE_ID].id;
	*out_my_address = g_my_id; // extract my ID from 0 index element

	memcpy(&CLIENTS[SERVER_ADDRESS_ID].ip.ip_addr, &server_ip, sizeof(uint32_t)); // store server details in index 1
	CLIENTS[SERVER_ADDRESS_ID].port = htons(server_port);

	//------> creat uniqe shmem
	if (create_shmem(*out_my_address, &g_fd_shmem, &g_shmem_adrr) == FTN_ERROR_SHMEM_FAILURE)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	g_shmem_buffer = (ring_buffer *)g_shmem_adrr;
	init_ring_buffer(g_shmem_buffer);
	// printf("created shmem succesfully! in addr: %p, FD: %ld\n", g_shmem_adrr, g_fd_shmem);

	//------> tell srv i have created shmem
	if (send_pkt_to_cli(CREATED_SHMEM_MSG, sizeof(CREATED_SHMEM_MSG), SERVER_ADDRESS_ID) == FTN_ERROR_NETWORK_FAILURE)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	//------> wait till all cli's are shmemed
	if (recv(g_sockfd, all_shmmed_buff, sizeof(ALL_SHMEM_CONNCETED_MSG), NO_FLAGS) <= 0)
	{
		return FTN_ERROR_NETWORK_FAILURE;
	}
	if (strcmp(all_shmmed_buff, ALL_SHMEM_CONNCETED_MSG) != 0)
	{
		return FTN_ERROR_SHMEM_FAILURE;
	}

	if (!find_local_end_points())
	{
		return FTN_ERROR_SHMEM_FAILURE;
	}

	if (!init_ringbuffer_arr(ring_buffs, g_num_of_cli))
	{
		printf("round buffs init error\n");
		return FTN_ERROR_UNKNONE;
	}

	if (!start_threads())
	{
		printf("thread start error\n");
		return FTN_ERROR_UNKNONE;
	}

	printf("FTN_client_init is finished! ip %d.%d.%d.%d port %lx\n", server_ip.ip_addr[0], server_ip.ip_addr[1], server_ip.ip_addr[2], server_ip.ip_addr[3], server_port);
	// sleep(1);
	return FTN_ERROR_SUCCESS;
}
FTN_RET_VAL FTN_recv(void *data_buffer, uint64_t data_buffer_size, uint64_t *out_pkt_len,
					 uint64_t source_address_id, uint64_t *opt_out_source_address_id, bool async)
{
	msg extract_msg = {0};
	bool ret_val = false;

	if (!run_mood || !shmem_thread_on) // no srv.cli was created yet+ the threads aren't RUNNING...
	{
		printf("run_mood is off\n");
		return FTN_ERROR_NO_CONNECT;
	}
	if (source_address_id != BROADCAST_ADDRESS_ID) // specific rcv
	{
		if (RB_empty(&ring_buffs[source_address_id]) && async) // no msgs & no blocking
		{
			pthread_mutex_unlock(&lock);
			*out_pkt_len = 0;
			opt_out_source_address_id = NULL;
			return FTN_ERROR_SUCCESS;
		}
		while (RB_empty(&ring_buffs[source_address_id]))
			; // blocking is on

		ret_val = RB_extract(&ring_buffs[source_address_id], &extract_msg); // done waiting- getting the new msg
		if (ret_val)
		{
			copy_data_to_cli(data_buffer, data_buffer_size, &extract_msg, out_pkt_len);
			*opt_out_source_address_id = source_address_id;
		}
	}
	else // get any- return oldest message
	{
		do
		{
			ret_val = RB_extract_first(ring_buffs, g_num_of_cli, &extract_msg, opt_out_source_address_id); // find out which cli has earliest msg
			if (ret_val)
			{
				ret_val &= copy_data_to_cli(data_buffer, data_buffer_size, &extract_msg, out_pkt_len);
			}
		} while (!ret_val && !async);
		if (!ret_val) // there were NO msgs AND there is NO patient to wait...
		{
			out_pkt_len = 0;
			opt_out_source_address_id = NULL;
		}
	}
	// add_pkt_log(false, extract_msg.msg, extract_msg.len, extract_msg.src, g_my_id);
	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL FTN_send(void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id)
{
	uint64_t x = 0;
	if (dest_address_id > MAX_NUMBER_OF_CLIENTS)
	{
		printf("cli index out of range\n");
		return FTN_ERROR_ARGUMENTS_ERROR;
	}
	if (dest_address_id == BROADCAST_ADDRESS_ID)
	{
		for (x = SERVER_ADDRESS_ID; x < g_num_of_cli; x++)
		{
			if (x == g_my_id) // dont send to yourself!
			{
				continue;
			}
			if (CLIENTS[x].shmem_addr != NULL) // send by shmem
			{
				send_pkt_via_shm(data_buffer, data_buffer_size, x);
			}
			else if (send_pkt_to_cli(data_buffer, data_buffer_size, x) == FTN_ERROR_NETWORK_FAILURE) // send using UDP
			{
				return FTN_ERROR_NETWORK_FAILURE;
			}
		}
	}
	else // specific sending
	{
		if (CLIENTS[dest_address_id].shmem_addr != NULL) // send by shmem
		{
			send_pkt_via_shm(data_buffer, data_buffer_size, dest_address_id);
		}
		else if (send_pkt_to_cli(data_buffer, data_buffer_size, dest_address_id) == FTN_ERROR_NETWORK_FAILURE)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
	return FTN_ERROR_SUCCESS;
}