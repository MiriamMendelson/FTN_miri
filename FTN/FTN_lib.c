#include "socket_operations.c"
#include "ring_buffer.c"

#define min(a, b) ((a < b) ? (a) : (b))
#define verify_succ(x) (x)//!= 0 ? exit(FTN_ERROR_MUTEX_ERROR) : 1

ring_buffer ring_buffs[MAX_NUMBER_OF_CLIENTS] = {};
END_POINT CLIENTS[MAX_NUMBER_OF_CLIENTS] = {};

unsigned int sockaddr_len = 16;
pthread_t rcv_handler_thread;
bool run_mood = false;
pthread_mutex_t lock;
char buffer[MAX_DATA_BUFFER_LEN];


// #pragma  logger_things
void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

#define LOGGER_SIZE (0x22222)
msg_log logg_rcv[LOGGER_SIZE] = {0};
uint64_t lgr_rcv_count = 0;

msg_log logg_snt[LOGGER_SIZE] = {0};
uint64_t lgr_snt_count = 0;

bool print_log(msg_log* logg, int len){
	UNUSED_PARAM(len);
	for(int i = 0; i < len; i++){
		printf("%d src: %ld ------------------------------\n", i, logg[i].src);
		DumpHex(logg[i].msg, logg[i].len);
		printf(" ---------------------------------\n");
	}
	return true;
}

void add_pkt_log(bool is_send, void *data_buffer, uint64_t len, uint64_t src, uint64_t dest)
{
	if (lgr_snt_count >= LOGGER_SIZE || lgr_rcv_count >= LOGGER_SIZE)
	{
		printf("###############################\n");
		exit(100);
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
		printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		exit(100);
	}

	memcpy(log_entry->msg, data_buffer, len);
	log_entry->len = len;
	log_entry->target = dest;
	log_entry->src = src;
}

// #pragma endregion


void *manage_insertion_rings(void *param)
{
	UNUSED_PARAM(param);
	run_mood = true; // thread stated- we're good to go!
	struct sockaddr_in from = {0};
	uint64_t actual_len = 0;//REMM! turn back to int64_t
	char msg_to_insert[MAX_DATA_BUFFER_LEN] = {0};
	uint64_t seq_counter = 0;

	while (1)
	{
		actual_len = recvfrom(SOCKFD, &(msg_to_insert[0]), MAX_DATA_BUFFER_LEN, NO_FLAGS, (struct sockaddr *)&from, &sockaddr_len);
		if (0 == actual_len)
		{
			return (void *)FTN_ERROR_NETWORK_FAILURE;
		}

		verify_succ(pthread_mutex_lock(&lock)); // wait on lock before referencing shared DS
		for (uint64_t i = SERVER_ADDRESS_ID; i < g_num_of_cli; i++)
		{
			if (is_same_addr(&from, &(CLIENTS[i])))
			{
				while(RB_is_full(&ring_buffs[i])){
					verify_succ(pthread_mutex_unlock(&lock)); //releasing & owning mutex while checking DS
					verify_succ(pthread_mutex_lock(&lock));
					}
				if (true != RB_insert(&ring_buffs[i], &msg_to_insert[0], seq_counter++,  actual_len))
				{
					return (void *)FTN_ERROR_DATA_SENT_IS_TOO_LONG;
				}
				// memcpy(&(logg_rcv[lgr_rcv_count].msg), &msg_to_insert[0], actual_len);
				// logg_rcv[lgr_rcv_count].len = actual_len;
				// logg_rcv[lgr_rcv_count].src = i;
				// logg_rcv[lgr_rcv_count++].target = CLIENTS[GET_PRIVATE_ID].id;

				add_pkt_log(false, &msg_to_insert[0], actual_len, i, CLIENTS[GET_PRIVATE_ID].id);
				verify_succ(pthread_mutex_unlock(&lock)); // release mutex
				break;
			}
		}
	}
	return NULL;
}
bool store_endpoint(int index, struct sockaddr_in *end_point)
{
	printf("store_endpoint()\n");
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
	char data_received[sizeof(CONN_REQ)];
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
		printf("------waiting for cli------\n");
		if (-1 == recvfrom(SOCKFD, &data_received, sizeof(CONN_REQ), NO_FLAGS, (struct sockaddr *)&cli_addr, &sockaddr_len))
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
		if (0 == strcmp(data_received, CONN_REQ))
		{
			if (true != store_endpoint(x, &cli_addr))
			{
				return FTN_ERROR_UNKNONE;
			}
		}
	}
	printf("*******all cli's are connected********\n");

	for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_cli; x++) // send EP-details arr to all conncted cli's 
	{
		CLIENTS[GET_PRIVATE_ID].id = x; // store the ID in 0 index (cli will extract it from there)

		if (!init_addr(&cli_addr, CLIENTS[x].port, CLIENTS[x].ip.ip_addr))
		{
			return FTN_ERROR_UNKNONE;
		}
		if (sendto(SOCKFD, CLIENTS, CLI_ARR_SIZE(g_num_of_cli),
				   NO_FLAGS, (struct sockaddr *)&cli_addr, sockaddr_len) == -1)
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
	printf("server is saying: cli arr size: %ld (1 sper place in 0 index), and they all have RB's. my privat id is: %ld\n", g_num_of_cli, CLIENTS[GET_PRIVATE_ID].id);

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
	uint32_t serv_ip;
	memcpy(&serv_ip, server_ip.ip_addr, sizeof(uint32_t));
	struct sockaddr_in server = {AF_INET, htons(server_port), {serv_ip}, {0}};
	struct sockaddr_in client = {};
	int num_of_received_bytes;

	SOCKFD = create_socket(GET_RAND_PORT, &client);
	if (0 == SOCKFD)
	{
		return FTN_ERROR_SOCKET_OPERATION;
	}

	printf("clints port: %d\n servers port: %d\n servers ip: %d\n", client.sin_port, server.sin_port, server.sin_addr.s_addr);

	if (sendto(SOCKFD, CONN_REQ, sizeof(CONN_REQ), NO_FLAGS, (struct sockaddr *)&server, sizeof(server)) == -1)
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

	store_endpoint(SERVER_ADDRESS_ID, &server); // store server details in index 1

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
	// printf("<<< FTN_recv is called source_address_id: %lx, my id: %d, is aync: %d.\n",source_address_id, CLIENTS[GET_PRIVATE_ID].id, async);
	if (!run_mood) //no srv.cli was created yet...
	{
		printf("run_mood is off!\n");
		return FTN_ERROR_NO_CONNECT;
	}

	uint64_t out_index = 0;
	uint64_t cli_index = 0;
	bool ret_val = true;
	// printf("<<< FTN_resv waits on lock :(\n");
	// printf("<<< FTN_resv owns lock :)\n");

	if (source_address_id != BROADCAST_ADDRESS_ID) // specific rcv
	{
		verify_succ(pthread_mutex_lock(&lock)); // wait on lock before referncing shared DS
		if (RB_empty(&ring_buffs[source_address_id]) && async) // no blocking
		{
			pthread_mutex_unlock(&lock);
			*out_pkt_len = 0;
			opt_out_source_address_id = NULL;
			return FTN_ERROR_SUCCESS;
		}
		while (RB_empty(&ring_buffs[source_address_id])) // blocking is on
		{
			verify_succ(pthread_mutex_unlock(&lock)); //releasing & owning mutex while checking DS
			verify_succ(pthread_mutex_lock(&lock));
		}
		ret_val = RB_extract(&ring_buffs[source_address_id], &out_index); //done waiting- getting the new msg
		if (ret_val)
		{
			*out_pkt_len = min(data_buffer_size, ring_buffs[source_address_id].msgs[out_index].len);
			memcpy(data_buffer, ring_buffs[source_address_id].msgs[out_index].msg, *out_pkt_len);
		}
		verify_succ(pthread_mutex_unlock(&lock));
	}
	else // get any- return oldest message
	{
		ret_val = false;
		do{
			verify_succ(pthread_mutex_lock(&lock)); //releasing & owning mutex while checking DS
			// printf("<<< FTN_recv any case. clis'd count: %ld, a'd count: %ld\n",ring_buffs[2].count, ring_buffs[1].count);

			ret_val = RB_extract_first(ring_buffs, g_num_of_cli, &cli_index); // find out which cli has earliest msg

			if (ret_val) // where there any msgs at all?
			{
				ret_val = RB_extract(&ring_buffs[cli_index], &out_index);

				if (ret_val)
				{
					*out_pkt_len = min(data_buffer_size, ring_buffs[cli_index].msgs[out_index].len);
					memcpy(data_buffer, ring_buffs[cli_index].msgs[out_index].msg, *out_pkt_len);
				}
			}
			verify_succ(pthread_mutex_unlock(&lock)); //releasing & owning mutex while checking DS
		}while(!ret_val && !async);

		if(!ret_val){//there were NO msgs...
			out_pkt_len = 0;
			opt_out_source_address_id = NULL;
		}
	}
	// printf("<<< FTN_resv have copyeid data!! lets go of lock...\n");

	if (NULL != opt_out_source_address_id)
	{
		(*opt_out_source_address_id) = source_address_id == BROADCAST_ADDRESS_ID ? cli_index : source_address_id;
	}

	return FTN_ERROR_SUCCESS;
}
FTN_RET_VAL FTN_send(void *data_buffer, uint64_t data_buffer_size, uint64_t dest_address_id)
{
	// printf("FTN_send: data_buffer: %p. data_buffer_size: %ld. dest_address_id: %ld \n",data_buffer, data_buffer_size, dest_address_id);
	struct sockaddr_in dest_client;
	uint64_t x;
			
	// TODO if(dest is in same com) use shared memory
	if (dest_address_id > MAX_NUMBER_OF_CLIENTS)
	{
		return FTN_ERROR_ARGUMENTS_ERROR;
	}
	if (dest_address_id == BROADCAST_ADDRESS_ID)
	{
		for (x = SERVER_ADDRESS_ID; x < g_num_of_cli; x++)
		{
			
			if(x == CLIENTS[GET_PRIVATE_ID].id) // dont send to yourself!
			{
				continue;
			}
			
			if (!init_addr(&dest_client, CLIENTS[x].port, CLIENTS[x].ip.ip_addr))
			{
				return FTN_ERROR_UNKNONE;
			}
			
			// printf("i've NOT send pckg to IP: %d.%d.%d.%d, PORT: %ld. on size %ld.\n", CLIENTS[x].ip.ip_addr[0],CLIENTS[x].ip.ip_addr[1],CLIENTS[x].ip.ip_addr[2],CLIENTS[x].ip.ip_addr[3], CLIENTS[x].port, data_buffer_size);
			if (-1 == sendto(SOCKFD, data_buffer, data_buffer_size, NO_FLAGS, (struct sockaddr *)&dest_client, sizeof(dest_client)))
			{
				return FTN_ERROR_NETWORK_FAILURE;
			}			
			// DumpHex(data_buffer, data_buffer_size);
		}
	}

	else{// specific sending
		if (!init_addr(&dest_client, CLIENTS[dest_address_id].port, CLIENTS[dest_address_id].ip.ip_addr))
		{
			return FTN_ERROR_UNKNONE;
		}
		if (sendto(SOCKFD, data_buffer, data_buffer_size, NO_FLAGS, (struct sockaddr *)&dest_client, sizeof(dest_client)) == -1)
		{
			return FTN_ERROR_NETWORK_FAILURE;
		}
	}
		// if(CLIENTS[GET_PRIVATE_ID].id == 2){
		// 		printf("====================\n");
		// 		DumpHex(data_buffer, data_buffer_size);
		// 	}
	add_pkt_log(true, data_buffer, data_buffer_size, CLIENTS[GET_PRIVATE_ID].id, x);

	return FTN_ERROR_SUCCESS;
}