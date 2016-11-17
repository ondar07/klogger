
/*
 * Ring Buffer routine
 */

#include "ring_buffer.h"
#include "wrappers.h"
#include <memory.h>

#define KB 1024
#define MB (KB * 1024)
#define BUF_SIZE (1 * MB)


rbuf_t *init_ring_buffer() {
	rbuf_t *rbuf = NULL;

	PRINT("[init_ring_buffer]: init_ring_buffer starts...");
	rbuf = (rbuf_t *)my_alloc(sizeof(rbuf_t));
	if (NULL == rbuf) {
		PRINT("ERROR [init_ring_buffer]: rbuf == NULL");
		return NULL;
	}

	rbuf->start = (PVOID)my_alloc(sizeof(TCHAR) * BUF_SIZE);
	if (NULL == rbuf->start) {
		PRINT("ERROR [init_ring_buffer]: rbuf == NULL");
		my_free(rbuf);
		return NULL;
	}

	// TODO: writers_count MUST be 0!!!
	// rbuf->writers_count = 0;

	// fill buf with zero bytes
	memset(rbuf->start, 0, sizeof(TCHAR) * BUF_SIZE);

	PRINT("[init_ring_buffer]: init_ring_buffer is successful...");
	return rbuf;
}

void deinit_ring_buffer(rbuf_t *rbuf) {
	if (!rbuf)
		return;
	if (NULL != rbuf->start) {
		// TODO:
		// check if all flushing is done already

		// after that
		PRINT("[deinit_ring_buffer]: free rbuf->start");
		my_free(rbuf->start);
	}
	PRINT("[deinit_ring_buffer]: free rbuf");
	my_free(rbuf);
}

#if 0
int write_to_rbuf(rbuf_t *rbuf, TCHAR *str) {
	// TODO
	;
}

void flush(rbuf_t *rbuf, HANDLE file) {
	// TODO
	;
}
#endif