#ifndef _KLOGGER_
#define _KLOGGER_

#include "ring_buffer.h"

typedef struct _klogger {
	rbuf_t *rbuf;
	HANDLE file;					// file for flush

	HANDLE flushing_thread;			// flushing thread
	KEVENT flush_event;
	KEVENT fl_thread_exiting;		// event will be set by flushing thread, when it finishes all flushing and is terminated
	BOOLEAN exit_now;
} klogger_t;

klogger_t *init_klogger();
int deinit_klogger();

int add_to_rbuf();

#endif  // _KLOGGER_