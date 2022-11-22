#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "test_ftn.h"

uint64_t g_my_client_id = INVALID_ADDRESS_ID;
uint64_t g_num_of_clients = (~0ULL);
uint64_t g_test_num = (~0ULL);

#define TEST_RET_SUCCESS (0)
#define TEST_RET_ERROR_PARAM_CHK (-2)
#define TEST_RET_ERROR_API_ERROR (-3)
#define TEST_RET_ERROR_SANITI_FAIL (-4)
#define TEST_RET_ERROR_TIMEOUT (-5)
#define TEST_RET_ERROR_INTERNAL_ERROR (-6)
#define TEST_RET_ERROR_MEMORY_ERROR (-7)

#define MONITOR_SLEEP_TIME_IN_MS (5000)

uint64_t g_num_of_send_pkts = 0;
uint64_t g_num_of_recv_pkts = 0;

bool g_is_test_end = false;

int create_monitor_thread(volatile uint64_t * ptr_to_watch, char * string_to_print);

uint32_t rand_range(uint32_t start, uint32_t end)
{
	return (rand() % (end - start)) + start;
}

uint32_t endiness_convert_32(uint32_t val)
{
	uint32_t swapped;
	swapped = ((val>>24)&0xff); // move byte 3 to byte 0
	swapped |= ((val<<8)&0xff0000); // move byte 1 to byte 2
	swapped |= ((val>>8)&0xff00); // move byte 2 to byte 1
	swapped |= ((val<<24)&0xff000000); // byte 0 to byte 3

	return swapped;
}

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
	struct timespec ts;
	int res;

	if (msec < 0)
	{
		errno = EINVAL;
		return -1;
	}

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;

	do {
		res = nanosleep(&ts, &ts);
	} while (res && errno == EINTR);

	return res;
}

uint32_t hash(void *buffer, uint64_t len)
{
	uint32_t hash = 5381;
	int c;
	uint64_t x = 0;
	uint8_t *ptr = buffer;
	
	for (x = 0; x < len; ++x)
	{
		c = ptr[x];
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	if (hash & 1)
	{
		hash ^= endiness_convert_32(hash);
	}

	return hash;
}

void build_pkt_data(char * addr, uint64_t data_buffer_len, uint64_t iteration_number)
{
	uint32_t sum_val = 0;
	uint32_t x = 0;
	uint32_t val_to_expected = 0;
	
	val_to_expected = iteration_number;

	for (x = 0; x < data_buffer_len; x+=sizeof(uint32_t))
	{
		val_to_expected = hash(&val_to_expected, sizeof(val_to_expected));
		*((uint32_t *)&(addr[x])) = val_to_expected;
		
		sum_val += val_to_expected;
	}
	
	*((uint32_t *)&(addr[0])) = sum_val;
}

int recv_exacly_len(void * data_buffer, uint64_t data_buffer_size, uint64_t expected_src_address_id)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	uint64_t get_pkt_len = 0;
	uint64_t pkt_source_address_id = 0;
	
	ret_val = FTN_recv(data_buffer, data_buffer_size, &get_pkt_len, expected_src_address_id, &pkt_source_address_id, false);
	if (!FTN_SUCCESS(ret_val))
	{
		return TEST_RET_ERROR_API_ERROR;
	}
	
	if (get_pkt_len != data_buffer_size)
	{
		printf("wrong data buffer len!\n");
		return TEST_RET_ERROR_SANITI_FAIL;
	}
	
	if (pkt_source_address_id != expected_src_address_id)
	{
		printf("got pkt from the wrong source!\n");
		return TEST_RET_ERROR_SANITI_FAIL;
	}
	
	return TEST_RET_SUCCESS;
}

typedef struct msg_init_exchange
{
	uint64_t num_of_clients;
	uint64_t test_num;
} msg_init_exchange;

int exchage_num_of_clients_to_other_app()
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	int test_ret_val = FTN_ERROR_UNKNONE;
	msg_init_exchange msg_to_send = {0};
	msg_init_exchange msg_tmp = {0};
	uint64_t x = 0;
	
	if (g_my_client_id == SERVER_ADDRESS_ID) //is server
	{
		msg_to_send.num_of_clients = g_num_of_clients;
		msg_to_send.test_num = g_test_num;
		
		for (x = FIRST_CLIENT_ADDRESS_ID; x < (g_num_of_clients + SERVER_ADDRESS_ID); ++x)
		{
			ret_val = FTN_send(&msg_to_send, sizeof(msg_to_send), x);
			if (!FTN_SUCCESS(ret_val))
			{
				printf("init send exchange error!\n");
				return TEST_RET_ERROR_API_ERROR;
			}
			
			test_ret_val = recv_exacly_len(&msg_tmp, sizeof(msg_tmp), x);
			if (test_ret_val != TEST_RET_SUCCESS)
			{
				return test_ret_val;
			}
			
			if (0 != memcmp(&msg_tmp, &msg_to_send, sizeof(msg_to_send)))
			{
				printf("init read back error!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
		}
	}
	else//is client
	{
		test_ret_val = recv_exacly_len(&msg_to_send, sizeof(msg_to_send), SERVER_ADDRESS_ID);
		if (test_ret_val != TEST_RET_SUCCESS)
		{
			return test_ret_val;
		}
		
		ret_val = FTN_send(&msg_to_send, sizeof(msg_to_send), SERVER_ADDRESS_ID);
		if (!FTN_SUCCESS(ret_val))
		{
			printf("init send exchange error!\n");
			return TEST_RET_ERROR_API_ERROR;
		}
		
		g_num_of_clients = msg_to_send.num_of_clients;
		g_test_num = msg_to_send.test_num;
	}
	
	return TEST_RET_SUCCESS;
}

int update_test_global_vars()
{
	int test_ret_val = -1;
	
	test_ret_val = exchage_num_of_clients_to_other_app();
	if (test_ret_val != TEST_RET_SUCCESS)
	{
		return test_ret_val;
	}
	
	printf("my_client_id %lx\n", g_my_client_id);
	printf("num_of_clients %lx\n", g_num_of_clients);
	printf("test_num %lx\n", g_test_num);
	
	return TEST_RET_SUCCESS;
}

#define RING_TEST_ORDER_LEN (MAX_NUMBER_OF_CLIENTS * 2)
#define RING_TEST_RUN_LEN (0x10000000)
#define RING_TEST_DATA_BUFFER_LEN (0x10)

uint64_t g_arr_all_nodes_state[MAX_NUMBER_OF_CLIENTS + 1] = {0};

void ring_test_generate_pkt_data(void * data_buf, uint64_t seed)
{
	build_pkt_data(data_buf, RING_TEST_DATA_BUFFER_LEN, seed);
}

int run_ring_test_loops(uint64_t * arr_pkg_order)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	int test_ret_val = -1;
	uint64_t iter = 0;
	uint64_t last_pos = 0;
	uint64_t cur_pos = 0;
	uint64_t next_pos = 0;
	uint64_t x = 0;
	
	uint64_t expected_pkt_data_buffer_seed = 0;
	char expected_pkt_data_buffer_data[RING_TEST_DATA_BUFFER_LEN] = {};
	char pkt_data_buffer[RING_TEST_DATA_BUFFER_LEN] = {};
	
	bool is_this_first_pkt = false;
	
	is_this_first_pkt = arr_pkg_order[0] == g_my_client_id;
	
	for (iter = 0; iter < RING_TEST_RUN_LEN; ++iter)
	{
		for (x = 0; x < RING_TEST_ORDER_LEN; ++x)
		{
			last_pos = cur_pos;
			cur_pos = next_pos;
			next_pos += 1;
			next_pos = next_pos % RING_TEST_ORDER_LEN;
			
			if (arr_pkg_order[cur_pos] == g_my_client_id)
			{
				break;
			}
		}
		
		for (x = 0; x < RING_TEST_ORDER_LEN; ++x)
		{
			if (arr_pkg_order[next_pos] != g_my_client_id)
			{
				break;
			}
			next_pos += 1;
			next_pos = next_pos % RING_TEST_ORDER_LEN;
		}
		
		if (!is_this_first_pkt)
		{
			expected_pkt_data_buffer_seed = g_arr_all_nodes_state[arr_pkg_order[last_pos]]++;
			ring_test_generate_pkt_data(&expected_pkt_data_buffer_data, expected_pkt_data_buffer_seed);
			
			test_ret_val = recv_exacly_len(&pkt_data_buffer, sizeof(pkt_data_buffer), arr_pkg_order[last_pos]);
			if (test_ret_val != TEST_RET_SUCCESS)
			{
				return test_ret_val;
			}
		
			if (0 != memcmp(pkt_data_buffer, expected_pkt_data_buffer_data, sizeof(pkt_data_buffer)))
			{
				printf("pkt integrity error!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
		}
		
		is_this_first_pkt = false;
			
		expected_pkt_data_buffer_seed = g_arr_all_nodes_state[arr_pkg_order[next_pos]]++;
		ring_test_generate_pkt_data(&pkt_data_buffer, expected_pkt_data_buffer_seed);
		ret_val = FTN_send(&pkt_data_buffer, sizeof(pkt_data_buffer), arr_pkg_order[next_pos]);
		if (!FTN_SUCCESS(ret_val))
		{
			printf("cant send ring!\n");
			return TEST_RET_ERROR_API_ERROR;
		}
		
		g_num_of_send_pkts++;
	}
	
	return TEST_RET_SUCCESS;
}

//uint64_t g_start_test_arr_all_nodes_state_for_send[MAX_NUMBER_OF_CLIENTS] = {0};
uint64_t g_star_test_arr_all_nodes_state_for_recv[MAX_NUMBER_OF_CLIENTS + 1] = {0};
uint64_t g_star_test_all_nodes_send_seed = 0;

#define STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE (0x100)
#define STAR_TEST_LOOP_STRESS_TEST_MIN_PKT_SIZE (0x10)
#define STAR_TEST_LOOP_STRESS_TEST_NUM_OF_ITERS_BEFORE_UPDATE_SEED (0x50)

#define STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX ("inc")
#define STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN (sizeof(STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX) - 1)
#define STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_SIZE (STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN + sizeof(uint64_t))

void build_star_test_buffer_for_recv(char * addr, uint64_t data_buffer_len, uint64_t cli_id)
{
	uint64_t seed = 0;
	uint64_t expected_pkt_data_buffer_seed = 0;
	
	expected_pkt_data_buffer_seed = g_star_test_arr_all_nodes_state_for_recv[cli_id];
	
	seed += expected_pkt_data_buffer_seed ^ 0xaaaaaaaa55555555ULL;
	seed *= data_buffer_len;
	
	build_pkt_data(addr, data_buffer_len, seed);
}

void build_star_test_buffer_for_send(char * addr, uint64_t data_buffer_len)
{
	uint64_t seed = 0;
	uint64_t expected_pkt_data_buffer_seed = 0;
	
	expected_pkt_data_buffer_seed = g_star_test_all_nodes_send_seed;
	
	seed += expected_pkt_data_buffer_seed ^ 0xaaaaaaaa55555555ULL;
	seed *= data_buffer_len;
	
	build_pkt_data(addr, data_buffer_len, seed);
}

void inc_client_id_node_state(uint64_t cli_id)
{
	g_star_test_arr_all_nodes_state_for_recv[cli_id]++;
}

int star_test_loop_stress_test_sender(void)
{
	char data_buffer[STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE] = {0};
	uint64_t pkt_size = 0;
	uint64_t cli_id = 0;
	uint64_t iter = 0;
	uint64_t x = 0;
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	
	while(1)
	{
		for (iter = 0; iter < STAR_TEST_LOOP_STRESS_TEST_NUM_OF_ITERS_BEFORE_UPDATE_SEED; ++iter)
		{
			for (x = 0; x < g_num_of_clients; ++x)
			{
				cli_id = x + SERVER_ADDRESS_ID;
				if (cli_id == g_my_client_id)
				{
					continue;
				}
				
				pkt_size = rand_range(STAR_TEST_LOOP_STRESS_TEST_MIN_PKT_SIZE, STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE);
				build_star_test_buffer_for_send(data_buffer, pkt_size);
				
				ret_val = FTN_send(data_buffer, pkt_size, cli_id);
				if (!FTN_SUCCESS(ret_val))
				{
					printf("send return error %x!\n", ret_val);
					return TEST_RET_ERROR_API_ERROR;
				}
				g_num_of_send_pkts++;
			}
		}
		
		g_star_test_all_nodes_send_seed++;
		memcpy(data_buffer, STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX, STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN);
		memcpy(data_buffer + STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN, &g_star_test_all_nodes_send_seed, sizeof(g_star_test_all_nodes_send_seed));
		
		ret_val = FTN_send(data_buffer, STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_SIZE, BROADCAST_ADDRESS_ID);
		if (!FTN_SUCCESS(ret_val))
		{
			printf("send all return error %x!\n", ret_val);
			return TEST_RET_ERROR_API_ERROR;
		}
		g_num_of_send_pkts++;
	}
	
	return TEST_RET_ERROR_INTERNAL_ERROR;
}

int star_test_loop_stress_test_recver(void)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	uint64_t get_pkt_len = 0;
	uint64_t pkt_source_address_id = 0;
	char data_buffer[STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE] = {0};
	char data_buffer_tmp[STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE] = {0};
	
	uint64_t tmp_val = 0;
	
	while(1)
	{	
		ret_val = FTN_recv(data_buffer, sizeof(data_buffer), &get_pkt_len, BROADCAST_ADDRESS_ID, &pkt_source_address_id, false);
		if (!FTN_SUCCESS(ret_val))
		{
			return TEST_RET_ERROR_API_ERROR;
		}
		
		g_num_of_recv_pkts++;
		
		if (get_pkt_len == STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_SIZE)
		{
			if (0 != memcmp(data_buffer, STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX, STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN))
			{
				printf("magic sanity error!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
			tmp_val = *((uint64_t *)(data_buffer + STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_PREFFIX_LEN));
			
			inc_client_id_node_state(pkt_source_address_id);
			if (g_star_test_arr_all_nodes_state_for_recv[pkt_source_address_id] != tmp_val)
			{
				printf("magic sanity error on seed!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
			
			continue;
		}
		else
		{
			if ((get_pkt_len < STAR_TEST_LOOP_STRESS_TEST_MIN_PKT_SIZE) || (get_pkt_len >= STAR_TEST_LOOP_STRESS_TEST_MAX_PKT_SIZE))
			{
				printf("pkt sanity error on size!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
			
			build_star_test_buffer_for_recv(data_buffer_tmp, get_pkt_len, pkt_source_address_id);
			if (0 != memcmp(data_buffer_tmp, data_buffer, get_pkt_len))
			{
				printf("pkt sanity error on data!\n");
				return TEST_RET_ERROR_SANITI_FAIL;
			}
		}
		
	}
	
	return TEST_RET_ERROR_INTERNAL_ERROR;
}	

void * star_test_loop_stress_test_sender_thread_warper(void * arg)
{
	int ret_val = -1;
	
	UNUSED_PARAM(arg);
	
	ret_val = star_test_loop_stress_test_recver();
	printf("send thread shuld not exit. ret val %x\n", ret_val);
	exit(TEST_RET_ERROR_INTERNAL_ERROR);
}

int star_test_with_stress_test(void)
{
	int ret_val = -1;
	pthread_t send_thread_id = {0};
	
	if (STAR_TEST_LOOP_STRESS_TEST_MAGIC_PKT_SIZE >= STAR_TEST_LOOP_STRESS_TEST_MIN_PKT_SIZE)
	{
		return TEST_RET_ERROR_INTERNAL_ERROR;
	}

	ret_val = create_monitor_thread(&g_num_of_recv_pkts, "num_of_recv");
	if (ret_val != 0)
	{
		return ret_val;
	}
	
	ret_val = create_monitor_thread(&g_num_of_send_pkts, "num_of_send");
	if (ret_val != 0)
	{
		return ret_val;
	}
	
	ret_val = pthread_create(&send_thread_id, NULL, &star_test_loop_stress_test_sender_thread_warper, NULL);
	if (ret_val != 0)
	{
		return TEST_RET_ERROR_INTERNAL_ERROR;
	}
	
	return star_test_loop_stress_test_recver();
}

int run_ring_test()
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	int test_ret_val = FTN_ERROR_UNKNONE;
	uint64_t x = 0;
	uint64_t y = 0;
	uint64_t z = 0;
	uint64_t tmp = 0;
	uint64_t arr_pkg_order[RING_TEST_ORDER_LEN] = {};
	
	test_ret_val = create_monitor_thread(&g_num_of_send_pkts, "iteration_per_second");
	if (test_ret_val != 0)
	{
		return test_ret_val;
	}
	
	if (g_my_client_id == SERVER_ADDRESS_ID) //is server
	{
		for (x = 0; x < RING_TEST_ORDER_LEN; ++x)
		{
			arr_pkg_order[x] = x % g_num_of_clients + 1;
		}
		
		for (x = 0; x < (RING_TEST_ORDER_LEN * 0x20); ++x)
		{
			y = rand_range(0, RING_TEST_ORDER_LEN);
			z = rand_range(0, RING_TEST_ORDER_LEN);
			
			tmp = arr_pkg_order[z];
			arr_pkg_order[z] = arr_pkg_order[y];
			arr_pkg_order[y] = tmp;
		}
		
		ret_val = FTN_send(&arr_pkg_order, sizeof(arr_pkg_order), BROADCAST_ADDRESS_ID);
		if (!FTN_SUCCESS(ret_val))
		{
			printf("cant send order!\n");
			return TEST_RET_ERROR_API_ERROR;
		}
	}
	else
	{
		test_ret_val = recv_exacly_len(arr_pkg_order, sizeof(arr_pkg_order), SERVER_ADDRESS_ID);
		if (test_ret_val != TEST_RET_SUCCESS)
		{
			return test_ret_val;
		}
	}
	
	printf("order is: ");
	for (x = 0; x < RING_TEST_ORDER_LEN; ++x)
	{
		printf("%lx ", arr_pkg_order[x]);
	}
	printf("\n");
	
	return run_ring_test_loops(arr_pkg_order);
}

typedef struct minitor_thread_params
{
	volatile uint64_t * pkt_count_ptr;
	char * string_counter;
	pthread_t monitor_thread_id;
} minitor_thread_params;

void * monitor_thread(void * arg)
{
	uint64_t cur_num_of_pkt = 0;
	uint64_t last_num_of_pkt = 0;
	uint64_t pkt_diff = 0;
	uint64_t pkt_per_sec = 0;
	
	minitor_thread_params * monitor_arg = arg;
	
	volatile uint64_t * pkt_send_ptr = monitor_arg->pkt_count_ptr;
	char * string_to_print = monitor_arg->string_counter;
	
	msleep(MONITOR_SLEEP_TIME_IN_MS);
	
	while(!g_is_test_end)
	{
		cur_num_of_pkt = *pkt_send_ptr;
		
		if (cur_num_of_pkt == last_num_of_pkt)
		{
			printf("probably there is deadlock!\n");
			exit(TEST_RET_ERROR_TIMEOUT);
		}
		
		pkt_diff = cur_num_of_pkt - last_num_of_pkt;
		pkt_per_sec = pkt_diff / (MONITOR_SLEEP_TIME_IN_MS / 1000);
		printf("address_id 0x%lx %s 0x%lx\n", g_my_client_id, string_to_print, pkt_per_sec);
		
		last_num_of_pkt = cur_num_of_pkt;
		msleep(MONITOR_SLEEP_TIME_IN_MS);
	}
	
	return NULL;
} 

int create_monitor_thread(volatile uint64_t * ptr_to_watch, char * string_to_print)
{
	minitor_thread_params * param = NULL;
	int ret_val = -1;
	
	param = malloc(sizeof(*param));
	if (NULL == param)
	{
		return TEST_RET_ERROR_MEMORY_ERROR;
	}
	
	memset(param, 0x0, sizeof(*param));
	
	param->pkt_count_ptr = ptr_to_watch;
	param->string_counter = string_to_print;
	
	ret_val = pthread_create(&(param->monitor_thread_id), NULL, &monitor_thread, param);
	if (ret_val != 0)
	{
		return TEST_RET_ERROR_INTERNAL_ERROR;
	}
	
	return TEST_RET_SUCCESS;
}

int run_test()
{
	int ret_val = -1;
	
	ret_val = update_test_global_vars();
	if (ret_val != TEST_RET_SUCCESS)
	{
		return ret_val;
	}
	
	switch (g_test_num)
	{
		case 0:
			printf("exchnage done!\n");
			printf("success!\n");
			g_is_test_end = true;
			return 0;
			break;
		
		case 1:
			ret_val = run_ring_test();
			if (ret_val != TEST_RET_SUCCESS)
			{
				printf("test return error! %x\n", ret_val);
			}
			g_is_test_end = true;
			return ret_val;
			break;
		case 2:
		
			ret_val = star_test_with_stress_test();
			if (ret_val != TEST_RET_SUCCESS)
			{
				printf("test return error! %x\n", ret_val);
			}
			g_is_test_end = true;
			return ret_val;
		default:
			printf("test num %lx is not supported\n", g_test_num);
			return TEST_RET_ERROR_PARAM_CHK;
	}
}

int start_app_srv(uint64_t srv_port, uint64_t num_of_clients, uint64_t test_number)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;

	srand(time(0));
	rand();
	
	if ((num_of_clients < FIRST_CLIENT_ADDRESS_ID) || (num_of_clients >= MAX_NUMBER_OF_CLIENTS))
	{
		printf("wrong number of nodes!\n");
		return TEST_RET_ERROR_PARAM_CHK;
	}

	ret_val = FTN_server_init(srv_port, num_of_clients);
	if (!FTN_SUCCESS(ret_val))
	{
		printf("server init error!\n");
		return TEST_RET_ERROR_API_ERROR;
	}
	
	g_my_client_id = SERVER_ADDRESS_ID;
	g_num_of_clients = num_of_clients;
	g_test_num = test_number;
	
	printf("run test %lx\n", test_number);
	return run_test();
}

int start_app_client(FTN_IPV4_ADDR srv_ip, uint64_t srv_port)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	
	srand(time(0));
	rand();
	
	ret_val = FTN_client_init(srv_ip, srv_port, &g_my_client_id);
	if (!FTN_SUCCESS(ret_val))
	{
		printf("client init error!\n");
		return TEST_RET_ERROR_API_ERROR;
	}
	
	return run_test();
}
