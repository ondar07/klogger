
#ifndef _RING_BUFFER_
#define _RING_BUFFER_

#define KERNEL
#ifdef KERNEL
	#include <Wdm.h>
#else

#endif

// 
#define BLOCKS_NUM 32

// TODO struct block

typedef struct _ring_buffer {
	PVOID start;		// buffer start
	PVOID end;			// buffer end
	ULONG buf_size;		// in bytes

	PVOID free;
	// TODO: free_spinlock
	
	PVOID dirty;
	
	// TODO: array of blocks

	LONG volatile *writers_count;	// how many writers that uses buffer at the moment

} rbuf_t;

rbuf_t *init_ring_buffer();
void deinit_ring_buffer(rbuf_t *rbuf);

int write_to_rbuf(rbuf_t *rbuf, TCHAR *str);
void flush(rbuf_t *rbuf, HANDLE file);

#endif  // _RING_BUFFER_