#include "test_ftn.h"

uint64_t g_my_client_id = INVALID_ADDRESS_ID;
uint64_t g_num_of_clients = (~0ULL);
uint64_t g_test_num = (~0ULL);

#define TEST_RET_SUCCESS (0)
#define TEST_RET_ERROR_PARAM_CHK (-2)
#define TEST_RET_ERROR_API_ERROR (-3)
#define TEST_RET_ERROR_SANITI_FAIL (-4)

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
		return TEST_RET_ERROR_SANITI_FAIL;
	}
	
	if (pkt_source_address_id != expected_src_address_id)
	{
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
		
		for (x = FIRST_CLIENT_ADDRESS_ID; x < g_num_of_clients; ++x)
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
			if (msg_tmp != msg_to_send)
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
		
		ret_val = FTN_send(&msg_to_send, sizeof(msg_to_send), x);
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
	int ret_val = -1;
	
	ret_val = exchage_num_of_clients_to_other_app();
	if (test_ret_val != TEST_RET_SUCCESS)
	{
		return ret_val;
	}
	
	printf("my_client_id %lx\n", g_my_client_id);
	printf("num_of_clients %lx\n", g_num_of_clients);
	printf("test_num %lx\n", g_test_num);
	
	return TEST_RET_SUCCESS;
}

uint32_t rand_range(uint32_t start, uint32_t end)
{
	return (rand() % (end - start)) + start;
}

#define RING_TEST_ORDER_LEN (MAX_NUMBER_OF_CLIENTS * 2)
#define RING_TEST_RUN_LEN (0x10000000)

int run_ring_test_loops(uint64_t * arr_pkg_order)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	uint64_t iter = 0;
	uint64_t last_pos = 0;
	uint64_t cur_pos = 0;
	uint64_t next_pos = 1;
	
	uint64_t pkt_data_buffer = 0;
	
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
		
		test_ret_val = recv_exacly_len(&pkt_data_buffer, sizeof(pkt_data_buffer), arr_pkg_order[last_pos]);
		if (test_ret_val != TEST_RET_SUCCESS)
		{
			return test_ret_val;
		}
		
		if (last_pos * iter != pkt_data_buffer)
		{
			printf("pkt integrity error!\n");
			return TEST_RET_ERROR_SANITI_FAIL;
		}
		
		pkt_data_buffer = next_pos * iter;
		ret_val = FTN_send(&arr_pkg_order, sizeof(arr_pkg_order), arr_pkg_order[next_pos]);
		if (!FTN_SUCCESS(ret_val))
		{
			printf("cant send ring!\n");
			return TEST_RET_ERROR_API_ERROR;
		}
	}
}

int run_ring_test()
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;
	int test_ret_val = FTN_ERROR_UNKNONE;
	uint64_t x = 0;
	uint64_t y = 0;
	uint64_t z = 0;
	uint64_t tmp = 0;
	uint64_t arr_pkg_order[RING_TEST_ORDER_LEN] = {0};
	
	if (g_my_client_id == SERVER_ADDRESS_ID) //is server
	{
		for (x = 0; x < RING_TEST_ORDER_LEN; ++x)
		{
			arr_pkg_order[x] = x % num_of_clients + 1;
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
	
	
	
	return 0;
}

int run_test()
{
	int ret_val = -1;
	ret_val = update_test_global_vars();
	if (ret_val != TEST_RET_SUCCESS)
	{
		return ret_val;
	}
	
	switch (test_id)
	{
		case 0:
			printf("exchnage done!\n");
			printf("success!\n");
			return 0;
			break;
		case 1:
			return run_ring_test();
			break;
		default:
			printf("test num %lx is not supported\n", test_id);
			return 2;
	}
}

int start_app_srv(uint64_t srv_port, uint64_t num_of_clients, uint64_t test_number)
{
	FTN_RET_VAL ret_val = FTN_ERROR_UNKNONE;

	srand(time(0));
	rand();
	
	ret_val = FTN_server_init(srv_port, num_of_clients);
	if (!FTN_SUCCESS(ret_val))
	{
		printf("server init error!\n");
		return 2;
	}
	
	g_my_client_id = SERVER_ADDRESS_ID;
	g_num_of_clients = num_of_clients;
	g_test_num = test_number;
	
	printf("%lx", test_number);
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
		return 2;
	}
	
	return run_test();
}
