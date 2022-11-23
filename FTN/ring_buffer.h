#ifndef __FTN_RING_BUFFER_H__
#define __FTN_RING_BUFFER_H__

#include "./FTN_interface.h"

typedef struct msg
{
    char msg[MAX_DATA_BUFFER_LEN];
    uint64_t len;
    uint64_t seq_num;
} msg;

typedef struct ring_buffer
{
    msg msgs[RING_BUFFER_SIZE];
    int64_t head;
    int64_t tail;
    uint64_t count;
} ring_buffer;

bool init_ring_buffer(ring_buffer *ring_buff);
bool init_ringbuffer_arr(ring_buffer *ring_buff, uint64_t len);
bool empty(ring_buffer *ring_buff);
bool insert(ring_buffer *ring_buff, char *new_msg, uint64_t len);
bool extract(ring_buffer *ring_buff, uint64_t *out_index);
bool peek(ring_buffer *ring_buff, uint32_t *out_index);
bool extract_first(ring_buffer *ring_buff, uint64_t num_of_rings, uint64_t *out_cli_index);
#endif