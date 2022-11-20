#include "./ring_buffer.h"

bool init_ring_buffer(ring_buffer *buff)
{
    buff->head = -1;
    buff->tail = -1;
    return true;
}

bool init_rb_arr(ring_buffer *buff, uint64_t len)
{
    for (uint32_t x = 0; x < len; x++)
    {
        if (true != init_ring_buffer(&buff[x]))
        {
            return false;
        }
    }
    return true;
}

bool insert(ring_buffer *buff, char *new_msg, uint64_t len)
{
    static uint64_t seq_counter = 0;
    if ((buff->head == 0 && buff->tail == SIZE - 1) ||
        (buff->tail == (buff->head - 1) % (SIZE - 1)))
    {
        printf("\nQueue is Full, couldnt write");
        return false;
    }

    if (buff->head == -1) /* Insert First Element */
    {
        buff->head = 0;
        buff->tail = 0;
    }
    else
    {
        buff->tail = (buff->tail + 1) % SIZE;
    }
    // printf("match has been found! the coping func is onnn! head: %ld, tail: %ld\n",buff->head,buff->tail);
    bool ret_val = (memcpy((buff->msgs[buff->tail]).msg, new_msg, len) != NULL);
    if (ret_val)
    {
        (buff->msgs[buff->tail]).len = len;
        // printf("im done coping %ld byts into %ld index\n", len, buff->tail);
        // buff->msgs[buff->tail].len = len;noneed
        buff->msgs[buff->tail].seq_num = ++seq_counter;
    }
    return ret_val;
}

bool extract(ring_buffer *buff, uint32_t *out_index)
{
    //printf("starting extarct!\n");
    if (buff->head == -1)
    {
        printf("\nQueue is Empty");
        return false;
    }

    *out_index = buff->head;

    if (buff->head == buff->tail)
    {
        buff->head = -1;
        buff->tail = -1;
    }
    else
    {
        buff->head = (buff->head + 1) % SIZE;
    }

    return true;
}

bool peek(ring_buffer *buff, uint32_t *out_index)
{
    if (buff->head == -1)
    {
        printf("\nQueue is Empty");
        return false;
    }

    *out_index = buff->head;

    return true;
}

bool empty(ring_buffer *buff)
{
    return ((buff->head == -1) && (buff->tail == -1));
}

bool extract_first(ring_buffer *buff, uint64_t num_of_rings, uint32_t *out_cli_index)
{
    uint64_t i;
    uint32_t curr_i_head;
    uint64_t oldest_msg_id = 0;
    uint64_t oldest_msg_seq_num = -1;
    for (i = SERVER_ADDRESS_ID; i < num_of_rings; i++)
    {
        if (peek(&(buff[i]), &curr_i_head))
            if (buff[i].msgs[curr_i_head].seq_num < oldest_msg_seq_num)
            {
                oldest_msg_seq_num = buff[i].msgs[curr_i_head].seq_num;
                oldest_msg_id = i;
            }
    }
    if (0 == oldest_msg_id) // oldest message not found - no messages in the rings
        return false;
    *out_cli_index = oldest_msg_id;
    return true;
}