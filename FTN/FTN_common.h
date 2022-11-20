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
	FTN_ERROR_INPUT_DATA_BUFFER_IS_TO_SMALL,
	FTN_ERROR_NETWORK_FAILURE,
	// TODO: add yours error codes here
} FTN_RET_VAL;


// typedef enum FTN_CLIENT_REQUEST
// {
// 	GET_ID_REQ_enum = 0

// } FTN_CLI_REQ;

#endif