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
    printf("the coping func is onnn!\n");
    bool ret_val = (memcpy((buff->msgs[buff->tail]).msg, new_msg, len) != NULL);
    if (ret_val)
    {
        (buff->msgs[buff->tail]).len = len;
        printf("im done coping %ld byts\n", len);
        // buff->msgs[buff->tail].len = len;
        buff->msgs[buff->tail].seq_num = ++seq_counter;
    }
    return ret_val;
}

bool extract(ring_buffer *buff, uint32_t *out_index)
{
    printf("staeting extarct!\n");
    if (buff->head == -1)
    {
        printf("\nQueue is Empty");
        return false;
    }

    *out_index = buff->head;
    //memcpy(out_msg, (buff->msgs)[buff->head].msg, (buff->msgs)[buff->head].len);

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

bool peek(ring_buffer *buff, msg *out_msg)
{
    if (buff->head == -1)
    {
        printf("\nQueue is Empty");
        return false;
    }

    memcpy(out_msg, (buff->msgs)[buff->head].msg, (buff->msgs)[buff->head].len);

    return true;
}

bool empty(ring_buffer *buff)
{
    return ((buff->head == -1) && (buff->tail == -1));
}

// uint64_t extract_first(ring_buffer *buff, uint64_t num_of_rings, msg *out_msg)
// {
//     uint64_t oldest_msg_seq_num = 0;
//     uint64_t oldest_msg_id = 0;
//     msg temp;
//     for (uint64_t i = 0; i < num_of_rings; i++)
//     {
//         if (peek(&(buff[i]), &temp))
//             if (temp.seq_num <= oldest_msg_seq_num)
//             {
//                 oldest_msg_seq_num = temp.seq_num;
//                 oldest_msg_id = i;
//             }
//     }
//     if (0 == oldest_msg_id) // oldest message not found - no messages in the rings
//         return 0;
//     if (0 == extract(&(buff[oldest_msg_id]), out_msg))
//         return 0;
//     return oldest_msg_id;
// }