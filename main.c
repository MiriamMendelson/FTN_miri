
#include "test_ftn.h"


bool parse_spesific_arg(char * str_arg, char * str_true_val, char * str_false_val, bool * out_val)
{
	if (0 == strcmp(str_arg, str_true_val))
	{
		*out_val = true;
	}
	else if (0 == strcmp(str_arg, str_false_val))
	{
		*out_val = false;
	}
	else
	{
		return false;
	}
	
	return true;
}

bool hex2int(char *hex, uint64_t * out_val) {
    uint64_t val = 0;
	if (!hex) return false;
	if (hex[0] != 0 && hex[1] != 0 && hex[2] != 0)
	{
		if (hex[0] == '0' && hex[1] == 'x')
		{
			hex+=2;
		}
	}
	
    while (*hex) {
        // get current character then increment
        uint8_t byte = *hex++; 
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;   
		else return false;
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        val = (val << 4) | (byte & 0xF);
    }
	
	*out_val = val;
    return true;
}

bool convert_str_to_ip(char *str_ip, FTN_IPV4_ADDR * out_ip)
{
	int ret_val = 0;
	struct sockaddr_in sa = {0};

	// store this IP address in sa:
	ret_val = inet_pton(AF_INET, str_ip, &(sa.sin_addr));
	if (ret_val != 1)
	{
		printf("insert valid IPv4!\n");
		return false;
	}
	
	out_ip->ip_addr[0] = sa.sin_addr.s_addr & 0xff;
	out_ip->ip_addr[1] = ((sa.sin_addr.s_addr) >> 8) & 0xff;
	out_ip->ip_addr[2] = ((sa.sin_addr.s_addr) >> 16) & 0xff;
	out_ip->ip_addr[3] = ((sa.sin_addr.s_addr) >> 24) & 0xff;
	
	return true;
}

char * FIRST_PARAM_PRINT_HELP = "*.exe <srv/cli>";
char * CLIENT_PARAM_LIST_PRINT_HELP = "*.exe <cli> <server_ip> <server_port>\nall in hex";
char * SRV_PARAM_LIST_PRINT_HELP = "*.exe <srv> <srv_port> <num of clients> <test number>\nall in hex";

int main(int argc, char **argv)
{
	bool parse_success = true;
	FTN_IPV4_ADDR srv_ip = {0};
	uint64_t srv_port = 0;
	uint64_t num_of_clients = 0;
	uint64_t test_number = 0;
	bool is_srv = false;
	
	if (argc < 2)
	{
		printf("%s\n", FIRST_PARAM_PRINT_HELP);
		return 1;
	}
	
	parse_success &= parse_spesific_arg(argv[1], "srv", "cli", &is_srv);
	if (!parse_success)
	{
		printf("%s\n", FIRST_PARAM_PRINT_HELP);
		return 1;
	}
	
	if (!is_srv)
	{
		//run as client
		printf("set as client!\n");
		if (argc != 4)
		{
			printf("%s\n", CLIENT_PARAM_LIST_PRINT_HELP);
			return 1;
		}
		parse_success &= convert_str_to_ip(argv[2], &srv_ip);
		parse_success &= hex2int(argv[3], &srv_port);
		
		if (!parse_success)
		{
			printf("%s\n", CLIENT_PARAM_LIST_PRINT_HELP);
			return 1;
		}
		
		return start_app_client(srv_ip, srv_port);
	}

	//run as server
	printf("set as server!\n");
	if (argc != 5)
	{
		printf("%s\n", SRV_PARAM_LIST_PRINT_HELP);
		return 1;
	}
		
	parse_success &= hex2int(argv[2], &srv_port);
	parse_success &= hex2int(argv[3], &num_of_clients);
	parse_success &= hex2int(argv[4], &test_number);
	
	if (!parse_success)
	{
		printf("%s\n", SRV_PARAM_LIST_PRINT_HELP);
		return 1;
	}
	
	return start_app_srv(srv_port, num_of_clients, test_number);
}