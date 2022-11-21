#include "./socket_interface.h"

// uint64_t init_socket(FTN_IPV4_ADDR ip, uint64_t port)
// {
//     int64_t sockfd;

//     struct sockaddr_in new_socket = {AF_INET, port, {inet_aton("63.161.169.137", &new_socket.sin_addr)}, {0}};

//     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (sockfd == -1)
//     {
//         perror("socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     if (bind(sockfd, (struct sockaddr *)&new_socket, sizeof(new_socket)) == -1)
//     {
//         perror("bind");
//         exit(1);
//     }
//     return 1;
// }
int create_socket(uint64_t port, struct sockaddr_in **out_addr)
{
    int32_t sockfd;
    struct sockaddr_in temp = {AF_INET, htons(port), {INADDR_ANY}, {0}};
    socklen_t len = sizeof(temp);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == -1)
    {
        perror("socket");
        exit(-1);
    }

    *out_addr = &temp; // blumi the ONE-AND-ONLY did that!!! (★‿★)

    if (bind(sockfd, (struct sockaddr *)*out_addr, len) == -1)
    {
        perror("bind");
        exit(1);
    }

    if (getsockname(sockfd, (struct sockaddr *)*out_addr, &len) == -1)
    {
        perror("getsockname");
        exit(1);
    }

    printf("binded port %d\n", (*out_addr)->sin_port);
    return sockfd;
}

struct sockaddr_in declare_server(uint32_t server_ip, uint64_t server_port)
{
    printf("servers ip: %d\n", server_ip);

    struct sockaddr_in server = {AF_INET, htons(server_port), {server_ip}, {0}};
    return server;
}

bool init_addr(struct sockaddr_in *addr, int port, uint8_t *ip_addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = port;
    return (NULL != memcpy(&addr->sin_addr, ip_addr, sizeof(uint32_t)));
}
