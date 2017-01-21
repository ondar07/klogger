/*
* Ring Buffer routine
*/


// TODO: in kernel space remove the following line!
//#include "stdafx.h"

#include "wrappers.h"

#ifndef KERNEL
// USERSPACE
#include <assert.h>			// for debugging
#else
// KERNEL
#endif

#include <memory.h>
#include "ring_buffer.h"

#ifndef KERNEL
	#define DEBUG
#endif

#pragma warning (disable : 4189)
#pragma warning (disable : 4127)

#define KB 1024
#define MB (KB * 1024)
#define BUF_SIZE (1 * MB)

#define FREE_PTR  0
#define DIRTY_PTR 1

#define BUF_FILLING_THRESHOLD ( rbuf->bufsize / 2 )

#define GET_VALUE_ATOMICALLY(pval) InterlockedOr(pval, 0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * AUXILIARY FUNCTIONS * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int init_writers_count_parameter(rbuf_t *rbuf);
static void deinit_writers_count_parameter(rbuf_t *rbuf);
static int init_blocks(rbuf_t *rbuf);
static void deinit_blocks(rbuf_t *rbuf);
static int init_locks(rbuf_t *rbuf);
static void deinit_locks(rbuf_t *rbuf);



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * MAIN FUNCTIONS* * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


static int set_ring_buffer_info(rbuf_t *rbuf, rbsize_t size) {
	rbuf->free = rbuf->dirty = rbuf->start;
	rbuf->end = (PVOID)((char *)rbuf->start + size - 1);
	rbuf->bufsize = size;

	return 0;
}

rbuf_t *init_ring_buffer(rbsize_t size) {
	rbuf_t *rbuf = NULL;

	PRINT("[init_ring_buffer]: init_ring_buffer starts...\n");
	rbuf = (rbuf_t *)my_alloc(sizeof(rbuf_t));
	if (NULL == rbuf) {
		PRINT("ERROR [init_ring_buffer]: rbuf == NULL\n");
		return NULL;
	}

	// 2.
	rbuf->start = (PVOID)my_alloc(sizeof(TCHAR) * size);
	if (NULL == rbuf->start) {
		PRINT("ERROR [init_ring_buffer]: rbuf == NULL\n");
		goto free_rbuf;
	}

	// 3. 
	if (set_ring_buffer_info(rbuf, size) < 0) {
		PRINT("ERROR [init_ring_buffer]: cannot set_ring_buffer_info\n");
		goto free_buffer;
	}

	// 4. init blocks
	if (init_blocks(rbuf) < 0) {
		PRINT("ERROR [init_block]: cannot init blocks\n");
		goto free_blocks;
	}

	// 5. init writers_count
	if (init_writers_count_parameter(rbuf) < 0) {
		PRINT("ERROR [init_ring_buffer]: can not init writers_count parameter\n");
		goto free_blocks;
	}

	// 6. create synchronization primitives for dirty and free pointers
	if (init_locks(rbuf) < 0) {
		PRINT("ERROR [init_ring_buffer]: can not init locks\n");
		goto free_blocks;
	}

	// 7. fill buf with zero bytes
	memset(rbuf->start, 0, sizeof(TCHAR) * size);

	PRINT("[init_ring_buffer]: init_ring_buffer is successful...\n");
	return rbuf;

free_blocks:
	deinit_blocks(rbuf);

free_buffer:
	my_free(rbuf->start);
free_rbuf:
	my_free(rbuf);

	return NULL;
}

static int init_locks(rbuf_t *rbuf) {
#ifdef KERNEL
	// TODO
	UNREFERENCED_PARAMETER(rbuf);
	return 0;
#else
	if (NULL == (rbuf->free_ptr_lock = create_lock()))
		goto error;
	if (NULL == (rbuf->dirty_ptr_lock = create_lock())) {
		destroy_lock(rbuf->free_ptr_lock);
		goto error;
	}
	return 0;
error:
	return -1;
#endif

}

static void deinit_locks(rbuf_t *rbuf) {
	destroy_lock(rbuf->free_ptr_lock);
	destroy_lock(rbuf->dirty_ptr_lock);
}

static int init_writers_count_parameter(rbuf_t *rbuf) {

	// TODO: USERSPACE
	// The variable pointed to by the 'writers_count' parameter must be aligned on a 32 - bit boundary
	// otherwise, this function will behave unpredictably on multiprocessor x86 systems and any non-x86 systems
	// See _aligned_malloc.
#if 0
	if ( (rbuf->writers_count = alloc_volatile_ptr(sizeof(rbuf->writers_count))) == NULL ) {
		PRINT("ERROR [init_writers_count_parameter]: writers_count == NULL");
		return -1;
	}
#endif
	rbuf->writers_count = 0;

	return 0;
}

static void deinit_writers_count_parameter(rbuf_t *rbuf) {
	UNREFERENCED_PARAMETER(rbuf);

#if 0
	if ((!rbuf) || (!rbuf->writers_count)) {
		PRINT("ERROR [deinit_writers_count_parameter]: critical error");
		return;
	}
	free_volatile_ptr(rbuf->writers_count);
#endif
}

/*
 * @buf_size -- size of buffer IN BYTES
 */
static void set_block_info(block_t *block, INT block_index, PVOID buf_start, rbsize_t buf_size) {
	rbsize_t block_size;	// in bytes

	block_size = buf_size / BLOCKS_NUM;
	block->size = block_size;
	block->start = (PVOID)((char *)buf_start + block_index * block_size);
	block->end = (PVOID)((char *)block->start + block_size - 1);
	// TODO: check (ref is volatile var)
	// interlocked... (?)
	block->ref = 0;
}

static int init_blocks(rbuf_t *rbuf) {
	INT i;

	rbuf->blocks = (block_t *)my_alloc(BLOCKS_NUM * sizeof(block_t));
	if (NULL == rbuf->blocks) {
		PRINT("ERROR [init_blocks]: rbuf->blocks == NULL\n");
		return -1;
	}
	for (i = 0; i < BLOCKS_NUM; i++) {
#if 0
		// 1. allocate memory for ref field of each block
		rbuf->blocks[i].ref = (LONG volatile *)alloc_volatile_ptr(sizeof(LONG volatile));
		if (NULL == rbuf->blocks[i].ref) {
			PRINT("ERROR [init_blocks]: blocks[i].ref == NULL", i);
			return -1;
		}
#endif
		// 2. fill each block with its information
		set_block_info(&rbuf->blocks[i], i, rbuf->start, rbuf->bufsize);
	}
	return 0;
}


static void deinit_blocks(rbuf_t *rbuf) {
	int i;

	if (NULL == rbuf->blocks)
		return;

	for (i = 0; i < BLOCKS_NUM; i++) {
#if 0
		if (NULL == rbuf->blocks[i].ref)
			continue;
#endif	
		// this function will be called 1) by init function (when something goes wrong)
		// or 2) by deinit_ring_buffer, which calls this function after waiting until all writers finish work with buffer (blocks)
#ifdef DEBUG
		assert(GET_VALUE_ATOMICALLY(&rbuf->blocks[i].ref) == 0);
#endif

#if 0
		free_volatile_ptr(rbuf->blocks[i].ref);
#endif
	}

	my_free(rbuf->blocks);
}

void deinit_ring_buffer(rbuf_t *rbuf) {
	if (!rbuf)
		return;

	PRINT("[deinit_ring_buffer]: starts...\n");
	if (NULL != rbuf->start) {

		// 1. wait until all writers finish work with buffer
		while (GET_VALUE_ATOMICALLY(&rbuf->writers_count) != 0)
			;

		// 2. 
		deinit_blocks(rbuf);

		// 3. now we can free buffer safely
		PRINT("INFO [deinit_ring_buffer]: free rbuf->start\n");
		my_free(rbuf->start);
	}
#if 0
	PRINT("INFO [deinit_ring_buffer]: free rbuf->writers_count");
	deinit_writers_count_parameter(rbuf);
#endif

	// 4.
	deinit_locks(rbuf);

	PRINT("INFO [deinit_ring_buffer]: free rbuf\n");
	my_free(rbuf);
}

static __inline size_t get_str_size_in_bytes(char *str) {
#if 0
	return _tcslen(str) * sizeof(TCHAR);
#endif

	return strlen(str) * sizeof(char);
}


/*
 * Get rbuf->free or rbuf->dirty at the moment
 * by locking and unlocking functions for this pointers
 */
static PVOID get_ptr_atomically(rbuf_t *rbuf, int ptr_type) {
	PVOID ptr;
	SYNCR_PRIM lock;

#ifdef DEBUG
	assert(rbuf);
#endif
	lock = (FREE_PTR == ptr_type) ? rbuf->free_ptr_lock : rbuf->dirty_ptr_lock;
	acquire_lock(lock);
	ptr = (ptr_type == FREE_PTR) ? rbuf->free : rbuf->dirty;
	release_lock(lock);

	return ptr;
}

/*
 * Set @new_ptr_value to rbuf->free or rbuf->dirty
 * by locking and unlocking functions for this pointers
 *
 * return:
 *		old value of this ptr
 */
static PVOID set_ptr_atomically(rbuf_t *rbuf, int ptr_type, PVOID new_ptr_value) {
	PVOID old_ptr_value;
	SYNCR_PRIM lock = (FREE_PTR == ptr_type) ? rbuf->free_ptr_lock : rbuf->dirty_ptr_lock;

	acquire_lock(lock);
	old_ptr_value = (FREE_PTR == ptr_type) ? rbuf->free : rbuf->dirty;
	if (FREE_PTR == ptr_type)
		rbuf->free = new_ptr_value;
	else if (DIRTY_PTR == ptr_type)
		rbuf->dirty = new_ptr_value;
	release_lock(lock);

	return old_ptr_value;
}

static size_t rbuf_free_mem_count(rbuf_t *rbuf, PVOID cur_free, PVOID cur_dirty) {
	size_t free_bytes_count = 0;

	if (cur_free == cur_dirty) {
		// it means, that buffer is free completely
		free_bytes_count = rbuf->bufsize;
		goto ret;
	}

	if (cur_free > cur_dirty) {
		free_bytes_count += ((char *)rbuf->end - (char *)cur_free + 1);
		free_bytes_count += ((char *)cur_dirty - (char *)rbuf->start);
	}
	else if (cur_free < cur_dirty) {
		free_bytes_count += ((char *)cur_dirty - (char *)cur_free);
	}

ret:
	return free_bytes_count;
}

/*
 * return:
 *		if we can write @str_size bytes into buffer
 *			 0
 *     else
 *			-1
 */
static int can_write_into_buffer(rbuf_t *rbuf, PVOID free, PVOID dirty, size_t str_size) {
	return (rbuf_free_mem_count(rbuf, free, dirty) > str_size) ? 0 : -1;
}

/*
 * Allocates a memory place in buffer for string
 * which length is @str_size_in_bytes
 *
 * @free -- the first byte in buffer to be filled by string
 */
static PVOID allocate_str_place(rbuf_t *rbuf, PVOID free, size_t str_size_in_bytes) {
	free = (PVOID)((char *)free + str_size_in_bytes);
	if (free > rbuf->end) {
		free = (PVOID)((char *)free - rbuf->bufsize);
	}
	return free;
}

/*
 * Get the last byte of allocated memory
 *
 * @new_free_ptr -- pointer to the first byte of memory
 *                  which is placed after allocated memory (this function is called after allocate_str_place)
 */
static __inline PVOID get_last_byte_of_alloc_mem(rbuf_t *rbuf, PVOID new_free_ptr) {
	return (new_free_ptr == rbuf->start) ? rbuf->end : (PVOID)((char *)new_free_ptr - 1);
}

static __inline int get_block_index(rbuf_t *rbuf, PVOID addr) {
	int block_index;

	if (addr < rbuf->start || addr > rbuf->end) {
		PRINT("ERROR [get_block_index]: addr=%x is not correct!\n", (LONG) addr);
		return -1;
	}
	// TODO: this implementations is correct, but it's possible to improve it
	for (block_index = 0; block_index < BLOCKS_NUM; block_index++) {
		if (addr >= rbuf->blocks[block_index].start && addr <= rbuf->blocks[block_index].end)
			return block_index;
	}
	// 
	PRINT("ERROR [get_block_index]: cannot define block_index for addr=%x\n", (LONG)addr);
	return -1;
}

/*
 * the next block for last block (which index == (BLOCKS_NUM - 1) ) is 0 block
 */
static __inline int get_next_block_index(int blocks_count, int cur_block_index) {
	return (cur_block_index != blocks_count - 1) ? (cur_block_index + 1) : 0;
}

/*
 * the previous block for 0 block is the last block, which index is equal to (BLOCKS_NUM - 1)
 */
static __inline int get_prev_block_index(int blocks_count, int cur_block_index) {
	return (cur_block_index != 0) ? (cur_block_index - 1) : (blocks_count - 1);
}

#define INCR_OPERATION 0
#define DECR_OPERATION 1

/*
 *
 * @alloc_mem_start -- the first byte of allocated mem for string
 * @alloc_mem_end   -- the last byte of allocated mem
 */
static void blocks_refs_operation(rbuf_t *rbuf, PVOID alloc_mem_start, PVOID alloc_mem_end, int oper_type) {
	int end_block_index = get_block_index(rbuf, alloc_mem_end);
	int cur_block_index = get_block_index(rbuf, alloc_mem_start);
	BOOLEAN should_works_with_all_blocks = FALSE;

#ifdef DEBUG
	assert(end_block_index != -1);
	assert(cur_block_index != -1);
#endif

	if (cur_block_index == end_block_index &&
		alloc_mem_end < alloc_mem_start && 
		BLOCKS_NUM > 1)
	{
		should_works_with_all_blocks = TRUE;
	}

	while (1) {
		if (INCR_OPERATION == oper_type)
			InterlockedIncrement(&rbuf->blocks[cur_block_index].ref);
		else if (DECR_OPERATION == oper_type)
			InterlockedDecrement(&rbuf->blocks[cur_block_index].ref);

		if (cur_block_index == end_block_index) {
			if (should_works_with_all_blocks) {
				goto next;
			}
			break;
		}
next:
		cur_block_index = get_next_block_index(BLOCKS_NUM, cur_block_index);
		if (should_works_with_all_blocks) {
			should_works_with_all_blocks = FALSE;
			end_block_index = get_prev_block_index(BLOCKS_NUM, end_block_index);
		}
	}
}

static void copy_string_to_buf(rbuf_t *rbuf, PVOID alloc_mem_start, PVOID alloc_mem_end, char *str) {
	LONG bytes_count;

	if (alloc_mem_start <= alloc_mem_end) {
		// alloc_mem_start == alloc_mem_end if and only if strlen(str) == 1 (1 symbol)

		// there is NOT wrapping
		bytes_count = (LONG)alloc_mem_end - (LONG)alloc_mem_start + 1;
#ifdef DEBUG
		assert(bytes_count > 0);
#endif
		memcpy(alloc_mem_start, str, bytes_count);
		return;
	}

	// there is WRAPPING
	// 1. 
	bytes_count = (LONG)rbuf->end - (LONG)alloc_mem_start + 1;
#ifdef DEBUG
	assert(bytes_count > 0);
#endif
	memcpy(alloc_mem_start, (PVOID)str, bytes_count);

	// 2.
	str = (char *)str + bytes_count;
	bytes_count = (LONG)alloc_mem_end - (LONG)rbuf->start + 1;
#ifdef DEBUG
	assert(bytes_count > 0);
#endif
	memcpy(rbuf->start, (PVOID)str, bytes_count);
}

int write_to_rbuf(rbuf_t *rbuf, char *str) {
	int status = 0;
	PVOID free, dirty, alloc_mem_end;
	size_t str_size_in_bytes;

	if (!str || (*str == '\0')) {
		PRINT("ERROR [write_to_rbuf]: str == NULL\n");
		return -1;
	}

	str_size_in_bytes = get_str_size_in_bytes(str);

	if (str_size_in_bytes > rbuf->bufsize) {
		PRINT("ERROR: Too long string\n");
		return -1;
	}
	
	/* * * * * * * * * * * * * * * * */
	// 1. lock free_ptr
	acquire_lock(rbuf->free_ptr_lock);
	free = rbuf->free;
	
	// we should locate here memory barrier
	// because writers acquires free_ptr_lock and after that dirty_ptr_lock
	// so if processor here acquire dirty_ptr_lock earlier
	// there may be deadlock situation
#ifdef KERNEL
	KeMemoryBarrier();
#else
	MemoryBarrier();
#endif

	dirty = get_ptr_atomically(rbuf, DIRTY_PTR);

	// 2. 
	if (can_write_into_buffer(rbuf, free, dirty, str_size_in_bytes) < 0) {
		PRINT("Cannot write string=\"%s\" into buffer\n", str);
		release_lock(rbuf->free_ptr_lock);
		return -1;
	}

	// 3. allocate memory
	rbuf->free = allocate_str_place(rbuf, free, str_size_in_bytes);
	// now rbuf->free points to beginning of memory which is placed after allocated memory
	alloc_mem_end = get_last_byte_of_alloc_mem(rbuf, rbuf->free);

	// 4. increment refs of blocks for writing
	blocks_refs_operation(rbuf, free, alloc_mem_end, INCR_OPERATION);

	// 5. unlock free_ptr
	release_lock(rbuf->free_ptr_lock);

	/* * * * * * * * * * * * * * * * */

	// 6.
	InterlockedIncrement(&rbuf->writers_count);

	// 7.
	copy_string_to_buf(rbuf, free, alloc_mem_end, str);
	
	// 8.
	blocks_refs_operation(rbuf, free, alloc_mem_end, DECR_OPERATION);

	// 9.
	InterlockedDecrement(&rbuf->writers_count);

	return status;
}

/* * * * * * * * * * * * */
/* * * * * * * * * * * * */
/* * * * * * * * * * * * */

static __inline int is_ptr_in_this_block(block_t *block, PVOID ptr) {
	return (block->start <= ptr && ptr <= block->end) ? 1 : 0;
}

static PVOID define_end_addr_for_flush(rbuf_t *rbuf, PVOID cur_free_ptr, PVOID cur_dirty_ptr, int cur_dirty_ptr_block_index) {
	PVOID cur_block_end, end_addr = NULL;

	if (cur_free_ptr == rbuf->blocks[cur_dirty_ptr_block_index].start) {
		// it means that current value of @rbuf->free points to 
		// the first byte of cur_block_index block.
		// Flush is already DONE 
		return NULL;
	}

	if (is_ptr_in_this_block(&rbuf->blocks[cur_dirty_ptr_block_index], cur_free_ptr) && cur_free_ptr > cur_dirty_ptr) {
		// 
		end_addr = (PVOID)((char *)cur_free_ptr - sizeof(char));
	}
	else {
		// cur_free_ptr < cur_dirty_ptr || cur_free_ptr is NOT replaced in cur_dirty_ptr_block
		// Note that, cur_free_ptr != cur_dirty_ptr (see can_write_into_buffer() function)
		//            we DON'T write to rbuf, if after writing free == dirty
		
		// maybe situation when cur_free_ptr and cur_dirty_ptr are replaced at the same block
		// but cur_free_ptr < cur_dirty_ptr. It means, that buffer is almost completely filled
		// so we choose cur_block_end as end_addr for flush
		cur_block_end = rbuf->blocks[cur_dirty_ptr_block_index].end;
		end_addr = cur_block_end;
	}
	
	return end_addr;
}

/*
 * The function writes all bytes (are replaced >= @start and <= @end)
 * into @file file
 */
static void write_to_file(HANDLE file, char *start, char *end) {
	LONG str_length;
	NTSTATUS ret;
	IO_STATUS_BLOCK IoStatusBlock;

	if (!file)
		return;

	str_length = (LONG)end - (LONG)start + sizeof(char);

#ifdef KERNEL
	// TODO
	ret = ZwWriteFile(file,
		NULL,	// event (optional)
		NULL,
		NULL,
		&IoStatusBlock,
		(PVOID)start,
		str_length,
		NULL,	// byte offset (in a file)
		NULL);	// key
	if (STATUS_SUCCESS  != ret) {
		PRINT("ERROR [write_to_file]: ZwWriteFile");
	}

#else
	// USERSPACE
	ret = WriteFile(file,
			(LPCVOID)start,
			str_length,
			NULL,
			NULL);
	if (FALSE == ret) {
		PRINT("ERROR [write_to_file]: LastError = %ld", GetLastError());
	}

#endif
}

void flush(rbuf_t *rbuf, HANDLE file) {
	PVOID cur_free_ptr, cur_dirty_ptr;

	// 1. get current value of rbuf->free ptr
	cur_free_ptr = get_ptr_atomically(rbuf, FREE_PTR);

	// we should locate here memory barrier
	// because writers acquires free_ptr_lock and after that dirty_ptr_lock
	// so if processor here acquire dirty_ptr_lock earlier
	// there may be deadlock situation
#ifdef KERNEL
	// TODO
	// KeMemoryBarrier();
#else
	MemoryBarrier();
#endif

	// 2. 
	cur_dirty_ptr = get_ptr_atomically(rbuf, DIRTY_PTR);

	while (cur_dirty_ptr != cur_free_ptr) {
		PVOID end_addr;
		int cur_dirty_ptr_block_index;

		// 1. get block index, where current vaue of @rbuf->dirty is replaced
		cur_dirty_ptr_block_index = get_block_index(rbuf, cur_dirty_ptr);

		// 2. check ref value of this block
		if (GET_VALUE_ATOMICALLY(&rbuf->blocks[cur_dirty_ptr_block_index].ref) > 0) {
			// it means that
			// this block is used by writers yet
			// TODO: break work or wait here ?
			//break;
			PRINT("[flush thread] block %d is used\n", cur_dirty_ptr_block_index);
			;
		}

		// 3. 
		end_addr = define_end_addr_for_flush(rbuf, cur_free_ptr, cur_dirty_ptr, cur_dirty_ptr_block_index);
		if (!end_addr) {
			// it means that flush is DONE
			// maybe this code is NOT possible
			break;
		}

		// 4.
		write_to_file(file, cur_dirty_ptr, end_addr);
		
		// 5. 
		if (end_addr == rbuf->blocks[cur_dirty_ptr_block_index].end) {
			// 
			int next_block_index = get_next_block_index(BLOCKS_NUM, cur_dirty_ptr_block_index);
			cur_dirty_ptr = rbuf->blocks[next_block_index].start;
		}
		else {
			cur_dirty_ptr = cur_free_ptr;
		}

		// 6.
		set_ptr_atomically(rbuf, DIRTY_PTR, cur_dirty_ptr);
	}
}


BOOLEAN should_flush(rbuf_t *rbuf) {
	size_t free_mem;			// in BYTES
	PVOID cur_free, cur_dirty;

	cur_free = get_ptr_atomically(rbuf, FREE_PTR);
	cur_dirty = get_ptr_atomically(rbuf, DIRTY_PTR);
	free_mem = rbuf_free_mem_count(rbuf, cur_free, cur_dirty);
	return ( free_mem < BUF_FILLING_THRESHOLD ) ? TRUE : FALSE;
}