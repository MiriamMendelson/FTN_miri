#ifndef __FTN_SHMEM_INTERFACE_H__
#define __FTN_SHMEM_INTERFACE_H__

#include "./FTN_interface.h"
FTN_RET_VAL create_shmem(uint64_t shmem_id, int64_t *out_shmem_fd, void **out_shmem_adrr);
FTN_RET_VAL try_open_exist_shmem(uint64_t id, void **out_shmem_adrr);
#endif