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

#define MONITOR_SLEEP_TIME_IN_MS (5000)

uint64_t g_num_of_send_pkts = 0;

bool g_is_test_end = false;

pthread_t monitor_thread_id = {0};

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

uint64_t g_arr_all_nodes_state[MAX_NUMBER_OF_CLIENTS] = {0};

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

int run_ring_test()
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	int test_ret_val = FTN_ERROR_UNKNONE;
	uint64_t x = 0;
	uint64_t y = 0;
	uint64_t z = 0;
	uint64_t tmp = 0;
	uint64_t arr_pkg_order[RING_TEST_ORDER_LEN] = {};
	
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

void * monitor_thread(void * arg)
{
	uint64_t cur_num_of_pkt = 0;
	uint64_t last_num_of_pkt = 0;
	uint64_t pkt_diff = 0;
	uint64_t pkt_per_sec = 0;
	
	UNUSED_PARAM(arg);
	
	msleep(MONITOR_SLEEP_TIME_IN_MS);
	
	while(!g_is_test_end)
	{
		cur_num_of_pkt = g_num_of_send_pkts;
		
		if (cur_num_of_pkt == last_num_of_pkt)
		{
			printf("probably there is deadlock!\n");
			exit(TEST_RET_ERROR_TIMEOUT);
		}
		
		pkt_diff = cur_num_of_pkt - last_num_of_pkt;
		pkt_per_sec = pkt_diff / (MONITOR_SLEEP_TIME_IN_MS / 1000);
		printf("address_id 0x%lx iteration_per_second 0x%lx\n", g_my_client_id, pkt_per_sec);
		
		last_num_of_pkt = cur_num_of_pkt;
		msleep(MONITOR_SLEEP_TIME_IN_MS);
	}
	
	return NULL;
}

int run_test()
{
	int ret_val = -1;
	ret_val = update_test_global_vars();
	if (ret_val != TEST_RET_SUCCESS)
	{
		return ret_val;
	}
	
	pthread_create(&monitor_thread_id, NULL, &monitor_thread, NULL);
	
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
