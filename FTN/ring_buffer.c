#include "./ring_buffer.h"

bool init_ring_buffer(ring_buffer *ring_buff)
{
    ring_buff->head = 0;
    ring_buff->tail = 0;
    ring_buff->count = 0;
    ring_buff->head_in_prograss = 0;
    ring_buff->tail_in_prograss = 0;
    ring_buff->len_in_prograss = 0;
    return true;
}

bool init_ringbuffer_arr(ring_buffer *ring_buff_arr, uint64_t len)
{
    uint64_t x = 0;
    for (x = SERVER_ADDRESS_ID; x < len; x++)
    {
        if (!init_ring_buffer(&ring_buff_arr[x]))
        {
            return false;
        }
    }
    return true;
}

bool RB_insert(ring_buffer *ring_buff, msg *new_msg)
{
    int64_t index_to_write = 0;
    uint64_t actual_count = __atomic_add_fetch(&ring_buff->len_in_prograss, 1, __ATOMIC_SEQ_CST);
    if (actual_count > RING_BUFFER_SIZE)
    {
        actual_count = __atomic_fetch_sub(&ring_buff->len_in_prograss, 1, __ATOMIC_SEQ_CST);
        return 0;
    }
    index_to_write = __atomic_fetch_add(&(ring_buff->tail_in_prograss), 1, __ATOMIC_SEQ_CST);
    memcpy(&(ring_buff->msgs[(index_to_write % RING_BUFFER_SIZE)]), new_msg, sizeof(msg));
    while ((ring_buff->tail) % RING_BUFFER_SIZE != (index_to_write % RING_BUFFER_SIZE))
        ;
    __atomic_fetch_add(&(ring_buff->tail), 1, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&(ring_buff->count), 1, __ATOMIC_SEQ_CST);
    return 1;
}
bool RB_extract(ring_buffer *ring_buff, msg *out_msg)
{
    int64_t index_to_read = 0;
    int actual_count = __atomic_sub_fetch(&ring_buff->count, 1, __ATOMIC_SEQ_CST);
    if (actual_count < 0)
    {
        __atomic_add_fetch(&ring_buff->count, 1, __ATOMIC_SEQ_CST);
        return 0;
    }
    index_to_read = __atomic_fetch_add(&(ring_buff->head_in_prograss), 1, __ATOMIC_SEQ_CST);
    memcpy(out_msg, &ring_buff->msgs[(index_to_read % RING_BUFFER_SIZE)], sizeof(msg));
    while ((ring_buff->head) % RING_BUFFER_SIZE != (index_to_read % RING_BUFFER_SIZE))
    {
    }
    __atomic_fetch_add(&(ring_buff->head), 1, __ATOMIC_SEQ_CST);
    __atomic_sub_fetch(&(ring_buff->len_in_prograss), 1, __ATOMIC_SEQ_CST);
    return 1;
}

/* LESS protected version of insert/extract
bool RB_insert(ring_buffer *ring_buff, msg *new_msg)
{
    if (!ring_buff || !new_msg)
    {
        return false;
    }
    if (RB_is_full(ring_buff))
    {
        printf("Queue is Full, couldnt write\n");
        return false;
    }

    memcpy(&ring_buff->msgs[ring_buff->tail], new_msg, sizeof(msg));
    if (CLIENTS[GET_PRIVATE_ID].id == 1){
        printf("count: %ld <------------------: head: %ld, tailg got insert: %ld\n", ring_buff->count + 1, ring_buff->head, ring_buff->tail);
        DumpHex((ring_buff->msgs[ring_buff->tail]).msg, (ring_buff->msgs[ring_buff->tail]).len); // <<====
        printf(" <----------------------------------\n");
    }
    ring_buff->tail++;

    if (ring_buff->tail == RING_BUFFER_SIZE) // end of circular buffer?
    {
        ring_buff->tail = 0; // turn around
    }

    ring_buff->count++;

    return true;
}

bool RB_extract(ring_buffer *ring_buff, msg *out_msg)
{
    if (RB_empty(ring_buff))
    {
        printf("RB_extract- Queue is Empty\n");
        return false;
    }
    memcpy(out_msg, &ring_buff->msgs[ring_buff->head], sizeof(msg));

    printf("%ld ------------------>: head of extract: %ld, tail: %ld\n", e++, ring_buff->head, ring_buff->tail);
    DumpHex((ring_buff->msgs[ring_buff->head]).msg, (ring_buff->msgs[ring_buff->head]).len); // <<====
    printf(" ---------------------------------->\n");

    ring_buff->head++;

    printf("thread couses: RB_extract: tail- %ld, head- %ld, len of stored data: %ld, seq_num: %ld, total in this cli: %ld\n", ring_buff->tail, ring_buff->head, ring_buff->msgs[ring_buff->tail].len, ring_buff->msgs[ring_buff->tail].seq_num, ring_buff->count + 1);
    if (ring_buff->head == RING_BUFFER_SIZE) // end of circular buffer?
    {
        printf("***********\n");
        ring_buff->head = 0; // turn around
    }

    ring_buff->count--;
    return true;
}
*/
bool RB_peek(ring_buffer *ring_buff, uint32_t *out_index)
{
    if (RB_empty(ring_buff))
    {
        // printf("RB_peek- Queue is Empty\n");
        return false;
    }

    *out_index = ring_buff->head % RING_BUFFER_SIZE;

    return true;
}

bool RB_empty(ring_buffer *ring_buff)
{
    return ring_buff->count == 0;
}

bool RB_extract_first(ring_buffer *ring_buff, uint64_t num_of_rings, msg *out_oldest_msg, uint64_t *opt_out_source_address_id)
{
    uint64_t i = 0;
    uint32_t curr_i_head = 0;
    uint64_t cli_with_oldest_msg = 0;
    uint64_t oldest_msg_seq_num = 0xFFFFFFFFFFFFFFFF;

    for (i = SERVER_ADDRESS_ID; i < num_of_rings; i++)
    {
        if (RB_peek(&(ring_buff[i]), &curr_i_head))
        {
            if (ring_buff[i].msgs[curr_i_head].seq_num < oldest_msg_seq_num)
            {
                oldest_msg_seq_num = ring_buff[i].msgs[curr_i_head].seq_num;
                cli_with_oldest_msg = i;
            }
        }
    }
    if (0 == cli_with_oldest_msg)
    {
        return false; // oldest message not found - no messages in the rings
    }
    RB_extract(&ring_buff[cli_with_oldest_msg], out_oldest_msg);
    *opt_out_source_address_id = cli_with_oldest_msg;
    return true;
}
bool RB_is_full(ring_buffer *ring_buff)
{
    return ring_buff->count == RING_BUFFER_SIZE - 1;
}