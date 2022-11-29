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
