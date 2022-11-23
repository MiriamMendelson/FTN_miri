#include "./ring_buffer.h"

bool init_ring_buffer(ring_buffer *ring_buff)
{
    ring_buff->head = 0;
    ring_buff->tail = 0;
    ring_buff->count = 0;
    return true;
}

bool init_ringbuffer_arr(ring_buffer *ring_buff, uint64_t len)
{
    for (uint32_t x = 0; x < len; x++)
    {
        if (true != init_ring_buffer(&ring_buff[x]))
        {
            return false;
        }
    }
    return true;
}

bool insert(ring_buffer *ring_buff, char *new_msg, uint64_t len)
{
    static uint64_t seq_counter = 0;

    if (len > MAX_DATA_BUFFER_LEN)
    {
        return FTN_ERROR_ARGUMENTS_ERROR;
    }

    if (ring_buff->count == RING_BUFFER_SIZE)
    {
        printf("\nQueue is Full, couldnt write");
        return false;
    }

    memcpy((ring_buff->msgs[ring_buff->tail]).msg, new_msg, len);

    (ring_buff->msgs[ring_buff->tail]).len = len;               //store msg len
    ring_buff->msgs[ring_buff->tail].seq_num = ++seq_counter;   //and msg identefining seq_num

    ring_buff->tail++;

    if (ring_buff->tail == RING_BUFFER_SIZE - 1)                //end of circular buffer?
    {
        ring_buff->tail = 0;                                    //turn around
    }

    ring_buff->count++;
    return true;
}

bool extract(ring_buffer *ring_buff, uint64_t *out_index)
{
    if (empty(ring_buff))
    {
        printf("\nQueue is Empty");
        return false;
    }

    *out_index = ring_buff->head;

    ring_buff->head++;

    if (ring_buff->head == RING_BUFFER_SIZE - 1)                //end of circular buffer?
    {
        ring_buff->head = 0;                                    //turn around
    }

    ring_buff->count--;
    return true;
}

bool peek(ring_buffer *ring_buff, uint32_t *out_index)
{
    if (empty(ring_buff))
    {
        printf("\nQueue is Empty");
        return false;
    }

    *out_index = ring_buff->head;

    return true;
}

bool empty(ring_buffer *ring_buff)
{
    return ring_buff->count == 0;
}

bool extract_first(ring_buffer *ring_buff, uint64_t num_of_rings, uint64_t *out_cli_index)
{
    uint64_t i;
    uint32_t curr_i_head = 0;
    uint64_t oldest_msg_id = 0;
    uint64_t oldest_msg_seq_num = -1;

    for (i = SERVER_ADDRESS_ID; i <= num_of_rings; i++)
    {
        if (peek(&(ring_buff[i]), &curr_i_head))
        {
            if (ring_buff[i].msgs[curr_i_head].seq_num < oldest_msg_seq_num)
            {
                oldest_msg_seq_num = ring_buff[i].msgs[curr_i_head].seq_num;
                oldest_msg_id = i;
            }
        }
    }
    if (0 == oldest_msg_id) // oldest message not found - no messages in the rings
        return false;
    *out_cli_index = oldest_msg_id;
    return true;
}