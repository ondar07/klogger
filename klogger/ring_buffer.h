
#ifndef _RING_BUFFER_
#define _RING_BUFFER_

#include <Wdm.h>


typedef struct _ring_buffer {
	PVOID start;
	ULONG buf_size;		// in bytes
} rbuf_t;

rbuf_t *init_ring_buffer();
void deinit_ring_buffer(rbuf_t *rbuf);

int write_to_rbuf(rbuf_t *rbuf, TCHAR *str);
void flush(rbuf_t *rbuf, HANDLE file);

#endif  // _RING_BUFFER_