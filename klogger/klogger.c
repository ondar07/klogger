
/*
 * Klogger routine
 */
// TODO: remove this line in kernel
//#include "stdafx.h"

#include "klogger.h"
#include "ring_buffer.h"
#include "wrappers.h"
#include <intrin.h>

#pragma warning (disable : 4127)

#pragma warning (disable : 4102)
#pragma warning (disable : 4100)

#ifdef KERNEL
#define FILE_NAME_FOR_FLUSH L"\\DosDevices\\C:\\flush.txt"
#else
#define FILE_NAME_FOR_FLUSH TEXT("flush.txt")
#endif

// this is singleton
//static klogger_t *klogger = NULL;

//#define HUH

static HANDLE create_file_for_flush();
static int close_file(HANDLE file);
static HANDLE init_flushing_thread(klogger_t *klogger);
static void deinit_flushing_thread(klogger_t *klogger);
static int init_klogger_events(klogger_t *klogger);
static void deinit_klogger_events(klogger_t *klogger);

static void set_terminate_event(klogger_t *klogger);
static void set_flush_event(klogger_t *klogger);


klogger_t *init_klogger(rbsize_t buf_size) {
	klogger_t *klogger = NULL;

#ifdef KERNEL
	// WARNING:
	//		the following initializations must be made at PASSIVE LEVEL IRQ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		// critical error!
		PRINT("ERROR [init_klogger]: critical error! THERE MUST BE PASSIVE_LEVEL ");
		return NULL;
	}
#endif

	if (klogger) {
		PRINT("ERROR [init_klogger]: this function may be called earlier...");
		return NULL;
	}

	// 1. init klogger
	klogger = (klogger_t *)my_alloc(sizeof(klogger_t));
	if (NULL == klogger) {
		PRINT("ERROR [init_klogger]: klogger == NULL \n");
		return NULL;
	}


	// 2. init rbuf
	klogger->rbuf = init_ring_buffer(buf_size);
	if (NULL == klogger->rbuf) {
		goto free_klogger;
	}


	// 3. init flushing file
	klogger->file = create_file_for_flush();
	if (NULL == klogger->file) {
		PRINT("ERROR [init_klogger]: NULL == klogger->file");
		goto deinit_rbuf;
	}

	// 4. initialize events for klogger (this code must be before initializing of flushing thread)
	if (init_klogger_events(klogger) < 0) {
		PRINT("ERROR [init_klogger]: ");
		goto close_flush_file;
	}

	// 5. 
	klogger->exit_now = FALSE;
	
	// 6. init flushing thread
#ifdef KERNEL
	if (NULL == init_flushing_thread(klogger)) {
		PRINT("ERROR [init_klogger]: NULL == klogger->flushing thread");
		goto deinit_events;
	}
#else
	klogger->flushing_thread = init_flushing_thread(klogger);
	if (NULL == klogger->flushing_thread) {
		PRINT("ERROR [init_klogger]: NULL == klogger->flushing thread");
		goto deinit_events;
	}
#endif

#ifndef HUH
	__debugbreak();
#endif

	PRINT("[init_klogger]: successful...");
	return klogger;

deinit_events:
	deinit_klogger_events(klogger);

close_flush_file:
	if (close_file(klogger->file) < 0)
		PRINT("ERROR [init_klogger]: close_file returns -1");
deinit_rbuf:
	deinit_ring_buffer(klogger->rbuf);
free_klogger:
	my_free(klogger);

	return NULL;

}

int deinit_klogger(klogger_t *klogger) {
#ifdef KERNEL
	// WARNING:
	//		the following code must be made at PASSIVE LEVEL IRQ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		// critical error!
		PRINT("ERROR [deinit_klogger]: critical error! THERE MUST BE PASSIVE_LEVEL\n");
		return -1;
	}
#endif

	if (!klogger) {
		PRINT("[deinit_klogger]: klogger is NULL...\n");
		return 0;
	}

	// 0.
	klogger->exit_now = TRUE;	// this is for new writers
	set_terminate_event(klogger);		// for flush thread

	/* THERE MUST BE BARRIER! */
	// can be called at any IRQL
	KeMemoryBarrier();
	
	// 1.
	deinit_flushing_thread(klogger);

	// 2. close flushing file
	if (close_file(klogger->file) < 0) {
		PRINT("[deinit_klogger]: can NOT close flushing file. Try later.\n");
		return -1;
	}
	
	PRINT("deinit_flushing_thread() and close_file() finish\n");
	PRINT("now deinit_ring_buffer()\n");
	__debugbreak();

	/* THERE MUST BE BARRIER! */
	// can be called at any IRQL
	KeMemoryBarrier();

	// 3.
	deinit_ring_buffer(klogger->rbuf);


	// 4.
	deinit_klogger_events(klogger);

	// 5.
	my_free(klogger);

	PRINT("[deinit_klogger]: success\n");
	return 0;
}

// can be called at any IRQL
static int init_klogger_events(klogger_t *klogger) {
#ifdef KERNEL
	// called at any IRQL level
	KeInitializeEvent(
		&klogger->flush_event,
		SynchronizationEvent,	// TODO
		FALSE);		//  initial state of the event

	KeInitializeEvent(
		&klogger->terminate_event,
		SynchronizationEvent,	// TODO
		FALSE);		//  initial state of the event
#else
	// USERSPACE

	// 1.
	klogger->flush_event = CreateEvent(
		NULL,               // default security attributes
		FALSE,              // auto-reset event object
		FALSE,              // initial state is nonsignaled
		TEXT("FlushEvent")  // object name
		);

	if (NULL == klogger->flush_event) {
		PRINT("CreateEvent FlushEvent failed (%d)\n", GetLastError());
		return -1;
	}

	// 2.
	klogger->terminate_event = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // not auto-reset (manually set)
		FALSE,              // initial state is nonsignaled
		TEXT("TerminateEvent")  // object name
		);
	if (NULL == klogger->terminate_event) {
		PRINT("CreateEvent TerminateEvent failed (%d)\n", GetLastError());

		CloseHandle(klogger->flush_event);

		return -1;
	}
#endif

	return 0;
}

static void deinit_klogger_events(klogger_t *klogger) {
#ifdef KERNEL
	UNREFERENCED_PARAMETER(klogger);
	/* NOTHING */;
	// TODO: maybe remove Events From Queue (if are there any?)
	ZwClose(&klogger->flush_event);
	ZwClose(&klogger->terminate_event);
#else
	// USERSPACE
	CloseHandle(klogger->flush_event);
	CloseHandle(klogger->terminate_event);
#endif
}

// this function is used only by userspace
// for kernel space there is __set_kevent()
static BOOLEAN _set_event(KLOGGER_EVENT eventHandle) {
#ifdef KERNEL
	UNREFERENCED_PARAMETER(eventHandle);
	/* nothing */;
#else
	// USERSPACE
	return SetEvent(eventHandle);
#endif
}


// Called at DISPATCH_LEVEL
static LONG __set_kevent(
	_In_     struct _KDPC *Dpc,
	_In_opt_ PVOID        DeferredContext,
	_In_opt_ PVOID        SystemArgument1,
	_In_opt_ PVOID        SystemArgument2
	)
{
	KLOGGER_EVENT *event = (KLOGGER_EVENT *)DeferredContext;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	// If Wait is set to FALSE, the caller can be running at IRQL <= DISPATCH_LEVEL
	return KeSetEvent(
		event,
		0,
		FALSE);
}

// Can be called at any IRQL level
static VOID _set_kevent(KLOGGER_EVENT *event, klogger_t *klogger) {
	if (event == &klogger->flush_event) {
		KeInitializeDpc(
			&klogger->flush_event_dpc,
			__set_kevent,
			event);

		// If the specified DPC object is not currently in a DPC queue, 
		// KeInsertQueueDpc queues the DPC and returns TRUE.
		// If the specified DPC object has already been queued,
		// no operation is performed except to return FALSE
		KeInsertQueueDpc(
			&klogger->flush_event_dpc,
			NULL,
			NULL);
	}
	else if (event == &klogger->terminate_event) {
		KeInitializeDpc(
			&klogger->terminate_event_dpc,
			__set_kevent,
			event);

		KeInsertQueueDpc(
			&klogger->terminate_event_dpc,
			NULL,
			NULL);
	}
}

static void set_terminate_event(klogger_t *klogger) {
#ifdef KERNEL
	_set_kevent(&klogger->terminate_event, klogger);
#else
	// USERSPACE
	if (!_set_event(klogger->terminate_event)) {
		PRINT("ERROR [set_flush_event]: cannot set terminate_event event! (%d)\n", GetLastError());
	}
#endif
}

static void set_flush_event(klogger_t *klogger) {
#ifdef KERNEL
	_set_kevent(&klogger->flush_event, klogger);
#else
	// USERSPACE
	if (!_set_event(klogger->flush_event)) {
		PRINT("ERROR [set_flush_event]: cannot set flush event! (%d)\n", GetLastError());
	}
#endif
}


int add_to_rbuf(klogger_t *klogger, char *str) {
	int status;
	
	// 0.
	if ((!klogger) || klogger->exit_now) {
		// writers can not write anymore
		return -1;
	}

	// 1. 
	status = write_to_rbuf(klogger->rbuf, str);

	// 2.
	if (should_flush(klogger->rbuf))
		set_flush_event(klogger);

	return status;
}

// Do not try to perform any file operations at higher IRQL levels
static HANDLE create_file_for_flush() {
	HANDLE   handle;

#ifdef KERNEL
	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;
	NTSTATUS ntstatus;
	IO_STATUS_BLOCK    ioStatusBlock;	// the caller can determine the cause of the failure by checking this value

	RtlInitUnicodeString(&uniName, FILE_NAME_FOR_FLUSH);  // or L"\\SystemRoot\\example.txt"
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);


	ntstatus = ZwCreateFile(&handle,		// file handle
		GENERIC_WRITE,						// DesiredAccess
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,				// file attributes
		FILE_SHARE_READ,					// SHARE READ ACCESS (!)
		FILE_OVERWRITE_IF,					// Specifies the action to perform if the file does or does not exist
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);
	if (STATUS_SUCCESS != ntstatus) {
		return NULL;
	}
#else
	//  USERSPACE
	handle = CreateFile(
		FILE_NAME_FOR_FLUSH,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL);
#endif

	return handle;
}

/*
 * Should be called at PASSIVE_LEVEL!
 * Close @file flushing file.
 * return:
 *		if success
 *			0
 *		else
 *			-1
 */
static int close_file(HANDLE file) {
#ifdef KERNEL
	// TODO: 
	return ZwClose(file);
#else 
	// USERSPACE
	return (0 == CloseHandle(file)) ? -1 : 0;
#endif
}

// TODO: if add timer, change here this value!
#define NUMBER_OF_OBJECTS_TO_BE_WAITED 2

// main function of flushing thread
static VOID flush_thread_routine(IN PVOID context) {
	klogger_t *klogger = (klogger_t *)context;
	PVOID handles[NUMBER_OF_OBJECTS_TO_BE_WAITED];
	int status;
	LARGE_INTEGER due_time;

#ifdef KERNEL
	// 0. check IRQL level for flushing thread

	PRINT("currentIrql==%d\n", KeGetCurrentIrql());

	// WARNING:
	//		the following initializations must be made at PASSIVE LEVEL IRQ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		// critical error!
		PRINT("ERROR [init_klogger]: critical error! THERE MUST BE PASSIVE_LEVEL ");
	}
#endif

#if 0
	klogger->timer = create_timer();
	if (NULL == klogger->timer) {
		PRINT("CAN'T create a timer!\n");
	}
#endif

	handles[0] = (PVOID)&klogger->terminate_event; 
	handles[1] = (PVOID)&klogger->flush_event;
	//handles[2] = (PVOID)klogger->timer;

	// wait for 10 seconds
	due_time.QuadPart = -100000000LL;

	PRINT("INFO [flush_thread_routine]: before cycle...\n");
	// 1. handle events in cycle
	while (TRUE) {
		// 0. set timer
		//set_timer(klogger->timer);

		// 1. waits flush or timer events
#ifdef KERNEL
		status = KeWaitForMultipleObjects(
			NUMBER_OF_OBJECTS_TO_BE_WAITED,	// the number of objects to be waited on
			handles,	//
			WaitAny,	// any one of the objects
			Executive,
			KernelMode,	// WaitMode 
			TRUE,		// Alertable (TODO ?)
			&due_time,	// timeout
			NULL
			);
#else
		// USERSPACE
		status = WaitForMultipleObjects(3, handles, FALSE, INFINITE);
#endif
		if (STATUS_TIMEOUT == status)
			PRINT("timeout...\n");

		// 2. get flush event, so we must flush ring buffer
		flush(klogger->rbuf, klogger->file);


#ifdef KERNEL
		// TODO!
		// how to define TerminateEvent,using KeWaitForMultipleObjects() ?
		// TODO: try "status == STATUS_WAIT_0"
		// see here: ftp://ftp.informatik.hu-berlin.de/pub3/doc/o-reilly/9780735618039/cd_contents/Samples/Chap14/polling/sys/ReadWrite.cpp
		if (status == STATUS_WAIT_0) {
			// terminate event is set
			PRINT("[flush_thread_routine]: flushing thread gets terminate event\n");
			break;
		}
#else
		// 3.
		if (status == WAIT_OBJECT_0) {
			// terminate event is set
			break;
		}
#endif
	}
	
	PRINT("[flush_thread_routine] finishes the cycle\n");
	// 2. cancel timer
	// Drivers must cancel any active timers in their Unload routines
	//destroy_timer(klogger->timer);

	// 3. thread is terminated
#ifdef KERNEL
	// 
	PsTerminateSystemThread(STATUS_SUCCESS);
#else
	// USERSPACE
	// ExitThread is the preferred method of exiting a thread in C code
	// When this function is called (either explicitly or by returning from a thread procedure), 
	// the current thread's stack is deallocated, all pending I/O initiated by the thread is canceled, and the thread terminates.
	// But Terminating a thread does not necessarily remove the thread object from the operating system. 
	// A thread object is deleted when the last handle to the thread is closed.
	// So we MUST CloseHandle for this thread in deinit_flushing_thread() function!
	ExitThread(0);
#endif
}

/*
 * WARNING:
 *		This function must be called at PASSIVE_LEVEL IRQ
 *
 * IMPORTANT:
 *		This thread runs at PASSIVE_LEVEL IRQ
 */
static HANDLE init_flushing_thread(klogger_t *klogger) {
	HANDLE flushing_thread = NULL;

#ifdef KERNEL
	NTSTATUS status;

	// The newly created system thread runs at PASSIVE_LEVEL
	status = PsCreateSystemThread(
		&flushing_thread,
		THREAD_ALL_ACCESS,
		NULL,				// object attributes
		NULL,
		NULL,
		flush_thread_routine,
		(PVOID)klogger		// a single argument that is passed to the thread when it begins execution
		);
	if (!NT_SUCCESS(status)) {
		PRINT("ERROR [init_flushing_thread]: can not create flushing thread\n");
		return NULL;
	}
	ObReferenceObjectByHandle(
		flushing_thread,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		(PVOID *)&klogger->flushing_thread,
		NULL);
	ZwClose(flushing_thread);

#ifndef HUH
	__debugbreak();
#endif
	return klogger->flushing_thread;

#else 
	// USERSPACE
	flushing_thread = CreateThread(NULL, 
		0,									// default size of stack
		flush_thread_routine,
		(LPVOID)klogger,					// argument for flush_thread_routine() function
		0,									// The thread runs immediately after creation
		NULL);								// thread ID will be not used

	return flushing_thread;
#endif
}

static void deinit_flushing_thread(klogger_t *klogger) {
#ifdef KERNEL

#ifndef HUH
	__debugbreak();
#endif

	// 1.
	// wait for klogger->flushing_thread !
	KeWaitForSingleObject(
		klogger->flushing_thread,
		Executive,
		KernelMode,
		FALSE,
		NULL
		);
	ObDereferenceObject(klogger->flushing_thread);
#ifndef HUH
	__debugbreak();
#endif

	// 3.
	// TODO: close handle ?
	//CloseHandle(klogger->flushing_thread);
#else 
	// USERSPACE
	// 2. wait for this thread
	WaitForSingleObject(klogger->flushing_thread, INFINITE);

	// 3. close handle
	CloseHandle(klogger->flushing_thread);
#endif

	PRINT("[deinit_flushing_thread]: success\n");
}