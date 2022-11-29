#ifndef __FTN_RING_BUFFER_H__
#define __FTN_RING_BUFFER_H__

#include "./FTN_interface.h"

// typedef struct msg
// {
//     char msg[MAX_DATA_BUFFER_LEN];
//     uint64_t len;
//     uint64_t seq_num;
// } msg;

typedef struct ring_buffer
{
    msg msgs[RING_BUFFER_SIZE];
    int64_t head;
    int64_t tail;
    uint64_t count;
} ring_buffer;



/*
 *  init_ring_buffer
 *  Init RB parans. must be called before using RB
 *  params:
 *  	ring_buff - pointer to RB.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool init_ring_buffer(ring_buffer *ring_buff);
/*
 *  init_ring_buffer_arr
 *  initialize each RB in the arr. must be called before using RB
 *  params:
 *  	ring_buff_arr - pointer to RB arr.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool init_ringbuffer_arr(ring_buffer *ring_buff_arr, uint64_t len);
/*
 *  RB_empty
 *  returns true if there is any data in the RB.
 *  params:
 *  	ring_buff_arr - A pointer to RB arr.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_empty(ring_buffer *ring_buff);
/*
 *  RB_insert
 *  insertes new data to the end of RB, and updates its sequence number.
 *  params:
 *  	ring_buff_arr - pointer to RB arr.
 *      new_msg - pointer to data.
 *      len - size of data to copy.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_insert(ring_buffer *ring_buff, char *new_msg, uint64_t seq_num, uint64_t len);
/*
 *  RB_insert
 *  extracts the head of RB.
 *  params:
 *  	ring_buff_arr - A pointer to RB arr.
 *      out_index - A pointer to the variable that receives the index of the head
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_extract(ring_buffer *ring_buff, uint64_t *out_index);
/*
 *  RB_peek
 *  returns the index of the RB head.
 *  params:
 *  	ring_buff_arr - A pointer to RB arr.
 *      out_index - A pointer to the variable that receives the index of the head.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_peek(ring_buffer *ring_buff, uint32_t *out_index);
/*
 *  RB_extract_first
 *  returns the index of the RB with the earliest sequence number.
 *  params:
 *  	ring_buff_arr - A pointer to an arr of RB's.
 *      num_of_rings - The ring_buff arr size.
 *      out_index - A pointer to the variable that receives the index of the arr that contains the oldest data.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_extract_first(ring_buffer *ring_buff, uint64_t num_of_rings, uint64_t *out_cli_index);
/*
 *  RB_is_full
 *  returns whether the reing fuffer is full or not.
 *  params:
 *  	ring_buff_arr - A pointer to an arr of RB's.
 *
 *	this function is not thread safe. for safe multithreaded usues must be call under lock.
 */
bool RB_is_full(ring_buffer *ring_buff);
#endif