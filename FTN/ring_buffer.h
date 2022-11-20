
#ifndef __FTN_RING_BUFFER_H__
#define __FTN_RING_BUFFER_H__

#include "./FTN_interface.h"
#define SIZE (100)

typedef struct msg
{
    char msg[MAX_DATA_BUFFER_LEN];
    uint64_t len;
    uint64_t seq_num;
} msg;

typedef struct ring_buffer
{
    msg msgs[SIZE];
    int64_t head;
    int64_t tail;  
} ring_buffer;

bool init_ring_buffer(ring_buffer *buff);
bool init_rb_arr(ring_buffer *buff, uint64_t len);
bool empty(ring_buffer *buff);
bool insert(ring_buffer *buff, char *new_msg,uint64_t len);
bool extract(ring_buffer *buff, uint32_t *out_index);
bool peek(ring_buffer *buff, msg *out_msg);
// uint64_t extract_first(ring_buffer *buff, uint64_t num_of_rings, msg *out_msg);
#endif