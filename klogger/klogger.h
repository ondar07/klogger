#ifndef _KLOGGER_
#define _KLOGGER_

#include "ring_buffer.h"
#include <Ntddk.h>

#ifdef KERNEL
// TODO
typedef KEVENT KLOGGER_EVENT;
#else
typedef HANDLE KLOGGER_EVENT;
#endif

typedef struct _klogger {
	rbuf_t *rbuf;
	HANDLE file;					// file for flush

#ifdef KERNEL
	PKTHREAD flushing_thread;
#else
	HANDLE flushing_thread;			// flushing thread
#endif

	KLOGGER_EVENT flush_event;
	KLOGGER_EVENT terminate_event;

	BOOLEAN exit_now;

	KDPC flush_event_dpc;
	KDPC terminate_event_dpc;

	KLOGGER_TIMER timer;
} klogger_t;

klogger_t *init_klogger(rbsize_t buf_size);
int deinit_klogger(klogger_t *klogger);

int add_to_rbuf(klogger_t *klogger, char *str);

#endif  // _KLOGGER_