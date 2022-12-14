#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef __FTN_LIB_ERROR_CODES_FILE_H__
#define __FTN_LIB_ERROR_CODES_FILE_H__

#define UNUSED_PARAM(x) ((void)(x))

#define FTN_SUCCESS(ret_val) ((ret_val) == FTN_ERROR_SUCCESS)

typedef enum FTN_RET_VAL
{
	FTN_ERROR_SUCCESS = 0,

	FTN_ERROR_UNKNONE,
	FTN_ERROR_NOT_IMPLEMENTED,
	FTN_ERROR_ARGUMENTS_ERROR,
	FTN_ERROR_INPUT_DATA_BUFFER_IS_TOO_SMALL,
	FTN_ERROR_NETWORK_FAILURE,
	FTN_ERROR_SOCKET_OPERATION,
	FTN_ERROR_DATA_SENT_IS_TOO_LONG,
	FTN_ERROR_NO_CONNECT,
	FTN_ERROR_MUTEX_ERROR,
	FTN_ERROR_SHMEM_FAILURE,
	FTN_ERROR_PROVIDED_BUFFER_IS_TOO_SMALL
	// TODO: add your error codes here
} FTN_RET_VAL;
#endif