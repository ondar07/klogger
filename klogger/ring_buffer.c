
/*
 * Ring Buffer routine
 */

#include "ring_buffer.h"
#include "wrappers.h"
#include <memory.h>

#define KB 1024
#define MB (KB * 1024)
#define BUF_SIZE (1 * MB)

#define GET_VALUE_ATOMICALLY(pval) InterlockedOr(pval, 0)

static NTSTATUS init_writers_count_parameter(rbuf_t *rbuf);
static void deinit_writers_count_parameter(rbuf_t *rbuf);

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
		goto free_rbuf;
	}

	if (init_writers_count_parameter(rbuf) < 0) {
		PRINT("ERROR [init_ring_buffer]: cannot init writers_count parameter");
		goto free_buffer;
	}

	// fill buf with zero bytes
	memset(rbuf->start, 0, sizeof(TCHAR) * BUF_SIZE);

	PRINT("[init_ring_buffer]: init_ring_buffer is successful...");
	return rbuf;

free_buffer:
	my_free(rbuf->start);
free_rbuf:
	my_free(rbuf);

	return NULL;
}

static int init_writers_count_parameter(rbuf_t *rbuf) {
#ifdef KERNEL
	if (NULL == my_alloc(rbuf->start)) {
		PRINT("ERROR [init_writers_count_parameter]: writers_count == NULL");
		return -1;
	}
	rbuf->writers_count = 0;
#else
	// TODO: USERSPACE
	// The variable pointed to by the 'writers_count' parameter must be aligned on a 32 - bit boundary
	// otherwise, this function will behave unpredictably on multiprocessor x86 systems and any non-x86 systems
	// See _aligned_malloc.
#endif

	return 0;
}

static void deinit_writers_count_parameter(rbuf_t *rbuf) {
#ifdef KERNEL
	if ((!rbuf) || (!rbuf->writers_count)) {
		PRINT("ERROR [deinit_writers_count_parameter]: critical error");
		return;
	}
	my_free(rbuf->writers_count);
#else
	// TODO: USERSPACE
#endif
}


void deinit_ring_buffer(rbuf_t *rbuf) {
	if (!rbuf)
		return;
	if (NULL != rbuf->start) {

		// 1. wait until all writers finish work with buffer
		while( GET_VALUE_ATOMICALLY(rbuf->writers_count) != 0 )
			;

		// 2. now we can free buffer safely
		PRINT("INFO [deinit_ring_buffer]: free rbuf->start");
		my_free(rbuf->start);
	}
	PRINT("INFO [deinit_ring_buffer]: free rbuf->writers_count");
	deinit_writers_count_parameter(rbuf);
	PRINT("INFO [deinit_ring_buffer]: free rbuf");
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