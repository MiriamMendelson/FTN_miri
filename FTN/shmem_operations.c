#include "./shmem_interface.h"

FTN_RET_VAL create_shmem(uint64_t shmem_id, int64_t *out_shmem_fd, void **out_shmem_adrr)
{
	char file_name[ID_LENGTH] = {0};

	sprintf(file_name, "%ld", shmem_id);

	*out_shmem_fd = shm_open(file_name, O_RDWR | O_CREAT, 0644);

	if (*out_shmem_fd < 0)
	{
		printf("shm_open error\n");
		return FTN_ERROR_SHMEM_FAILURE;
	}
	if (ftruncate(*out_shmem_fd, MEM_SIZE) < 0)
	{
		printf("truncate file failed\n");
		return FTN_ERROR_SHMEM_FAILURE;
	}
	*out_shmem_adrr = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *out_shmem_fd, 0);
	
	if (*out_shmem_adrr == MAP_FAILED)
	{
		printf("Shared memory map failed\n");
		return FTN_ERROR_SHMEM_FAILURE;
	}
	return FTN_ERROR_SUCCESS;
}

FTN_RET_VAL try_open_exist_shmem(uint64_t id, void **out_shmem_adrr)
{
	char file_name[ID_LENGTH] = {0};
	sprintf(file_name, "%ld", id);

	int fd = shm_open(file_name, O_RDWR | O_EXCL | O_TRUNC, 0644);
	if (fd < 0)
	{
		printf("shm_open neighbor not found\n");
		return FTN_ERROR_SUCCESS;
	}
	*out_shmem_adrr = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (*out_shmem_adrr == MAP_FAILED)
	{
		printf("Shared memory map failed\n");
		return FTN_ERROR_SHMEM_FAILURE;
	}
	return FTN_ERROR_SUCCESS;
}