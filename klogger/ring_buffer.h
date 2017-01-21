
#ifndef _RING_BUFFER_
#define _RING_BUFFER_

#include "wrappers.h"

#ifdef KERNEL
#include <Wdm.h>
#else
// USERSPACE
#include <stdio.h>
#include <tchar.h>
//#include <windows.h>
#endif

typedef unsigned long rbsize_t;

// 
#define BLOCKS_NUM 16

typedef struct _block {
	PVOID start;		// address of block's first byte
	PVOID end;			// address of block's last byte
	rbsize_t size;		// in bytes
	LONG volatile ref;	// ref count
} block_t;

typedef struct _ring_buffer {
	PVOID start;		// address of buffer's first byte
	PVOID end;			// address of buffer's last byte
	rbsize_t bufsize;		// in bytes

	PVOID free;
	SYNCR_PRIM free_ptr_lock;


	PVOID dirty;
	SYNCR_PRIM dirty_ptr_lock;

	// array of blocks
	block_t *blocks;

	LONG volatile writers_count;	// how many writers that uses buffer at the moment

	// WARNING:
	// if add something here, you should init it in set_ring_buffer_info() function (see ring_buffer.c)

} rbuf_t;

/*
 * This function creates a new ring buffer
 * by allocating @buf_size bytes
 *
 * @buf_size -- size of buffer IN BYTES
 *
 * return:
 *     if it's created successfully
 *          a pointer to new ring buffer
 *     else
 *          NULL
 */
rbuf_t *init_ring_buffer(rbsize_t buf_size);

/*
 * The function deinits @rb ring buffer
 * It includes flushing of remaining content of @rb
 * and free memory of @rb allocated before
 */
void deinit_ring_buffer(rbuf_t *rbuf);

/*
 * The function writes @info string into @rb ring buffer
 *
 * @rb    --
 * @str  --
 *
 */
int write_to_rbuf(rbuf_t *rb, char *str);
void flush(rbuf_t *rbuf, HANDLE file);

/*
 * The function returns TRUE, if free memory of buffer remains < half of bufsize
 * It means that we should flush buffer
 */
BOOLEAN should_flush(rbuf_t *rbuf);

#endif  // _RING_BUFFER_
