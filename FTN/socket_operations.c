#include "./socket_interface.h"

bool init_addr(struct sockaddr_in *addr, int port, uint8_t *ip_addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = port;
    memcpy(&addr->sin_addr, ip_addr, sizeof(uint32_t));
    return true;
}

int create_socket(uint64_t port, struct sockaddr_in *out_addr)
{
    int32_t sockfd;
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = htons(port);
    out_addr->sin_addr.s_addr = INADDR_ANY;
    socklen_t len = sizeof(struct sockaddr_in);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == -1)
    {
        printf("socket");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)out_addr, len) == -1)
    {
        printf("bind");
        return -1;
    }

    if (getsockname(sockfd, (struct sockaddr *)out_addr, &len) == -1)
    {
        close(sockfd);
        printf("getsockname");
        return -1;
    }

    printf("binded port %d\n", (out_addr)->sin_port);
    return sockfd;
}

bool is_same_addr(struct sockaddr_in *src, END_POINT *ep)
{
    bool res;
    uint32_t ep_ip_full;

    if (ep == NULL || src == NULL)
    {
        return false;
    }

    res = (src->sin_port == ep->port);
    memcpy(&ep_ip_full, ep->ip.ip_addr, sizeof(uint32_t));
    res &= (ep_ip_full == src->sin_addr.s_addr);

    return res;
}

uint64_t miris_int = 0;
FTN_RET_VAL send_pkt_to_cli(void *data_buffer, uint64_t data_buffer_size, uint64_t sento_index)
{
    struct sockaddr_in dest_client;

    int64_t send_to_rslt = 0;
    uint64_t bytes_got_send = 0;

    if (!init_addr(&dest_client, CLIENTS[sento_index].port, CLIENTS[sento_index].ip.ip_addr))
    {
        return FTN_ERROR_UNKNONE;
    }

    while (bytes_got_send < data_buffer_size)
    {
        send_to_rslt = sendto(SOCKFD, data_buffer + bytes_got_send, data_buffer_size - bytes_got_send, NO_FLAGS,
                              (struct sockaddr *)&dest_client, sizeof(dest_client));
        if (send_to_rslt == -1)
        {
            return FTN_ERROR_NETWORK_FAILURE;
        }
        bytes_got_send += send_to_rslt;
    }
    
    // if(CLIENTS[GET_PRIVATE_ID].id == 2 && sento_index){
        // printf("%ld ------------------>\n", miris_int++);
        // DumpHex(data_buffer, data_buffer_size);
    // }

    // add_pkt_log(true, data_buffer, data_buffer_size, CLIENTS[GET_PRIVATE_ID].id, sento_index);

    return FTN_ERROR_SUCCESS;
}