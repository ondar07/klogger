
#ifndef _WRAPPERS_
#define _WRAPPERS_

#define KERNEL

// #define KERNEL
#ifdef KERNEL
#include <Wdm.h>
#else
// TODO: USERSPACE include files
#include <tchar.h>
#include <windows.h>
#endif

#ifdef KERNEL
#define PRINT(msg, ...) DbgPrint(msg, __VA_ARGS__)
#else
// USERSPACE PRINT
#define PRINT(msg, ...) _tprintf(TEXT(msg), __VA_ARGS__)
#endif

#ifndef KERNEL
#define DEBUG
#endif

#ifdef KERNEL
typedef PKTIMER KLOGGER_TIMER;
#else
typedef HANDLE KLOGGER_TIMER;
#endif

static PVOID my_alloc(size_t mem_size) {
	PVOID mem;

#ifdef KERNEL
	/*
	* This function allocates only NonPagedPool memory
	* (so this memory is available at any IRQL)
	*
	* WARNING:
	*     Callers of ExAllocatePoolWithTag must be executing at IRQL <= DISPATCH_LEVEL
	*/
	mem = ExAllocatePoolWithTag(
		_In_ NonPagedPool,			// driver might access this mem while it is running at IRQL > APC_LEVEL
		_In_ mem_size,
		_In_ 'Tag1'					// TODO: should use a unique pool tag to help debuggers and verifiers identify the code path
		);
#else
	// USERSPACE
	mem = malloc(mem_size);
#endif

	return mem;
}

// call this function at IRQL <= DISPATCH_LEVEL
static void my_free(PVOID mem) {
#ifdef KERNEL
	ExFreePoolWithTag(
		_In_ mem,
		_In_ 'Tag1'
		);
#else
	// USERSPACE
	free(mem);
#endif
}

#if 0
static PVOID alloc_volatile_ptr(size_t size) {
	PVOID ptr = NULL;

#ifdef KERNEL
	// TODO: check here!
	ptr = my_alloc(size);
#else
	// more info: read remarks of InterlockedIncrement function
	// TODO: check here!
	ptr = _aligned_malloc(size, 32);
	// check: ptr % 32 == 0 ????

#endif
	return ptr;
}

static void free_volatile_ptr(VOID volatile *ptr) {

	if (!ptr)
		return;
	// TODO: check this!
	my_free(ptr);

}
#endif


/* * * * * * * * * * * * * */
/* * * SET EVENT WRAPPER * */
/* * * * * * * * * * * * * */
#ifdef KERNEL
/*
* WARNING:
*		If Wait is set to FALSE, the caller can be running at IRQL <= DISPATCH_LEVEL.
*		Otherwise, callers of KeSetEvent must be running at IRQL <= APC_LEVEL and in a nonarbitrary thread context
*/
static void my_set_event(_Inout_ PRKEVENT event) {
	KeSetEvent(
		event,
		0,
		FALSE
		);
}
#else
// USERSPACE
static void my_set_event(_In_ HANDLE hEvent) {
	SetEvent(hEvent);
}
#endif // KERNEL

/* * * * * * * * * * * * * * * * * */
/* WAIT FOR SINGLE OBJECT WRAPPER  */
/* * * * * * * * * * * * * * * * * */
#ifdef KERNEL
/*
* WARNING:
*		Callers of KeWaitForSingleObject must be running at IRQL <= DISPATCH_LEVEL
*		However, if Timeout = NULL or *Timeout != 0, the caller must be running at IRQL <= APC_LEVEL and in a nonarbitrary thread context.
*/
static NTSTATUS my_wait_for_single_object(_In_ PVOID object, _In_opt_ PLARGE_INTEGER  Timeout)
{
	return KeWaitForSingleObject(
		object,
		Executive,
		KernelMode,
		FALSE,			// not alertable
		Timeout
		);
}
#else // USERSPACE
static NTSTATUS my_wait_for_single_object() {
	// TODO
	;
}
#endif // KERNEL


static KLOGGER_TIMER create_timer() {
	KLOGGER_TIMER timer = NULL;
#ifdef KERNEL
	// TODO

#else
	// USERSPACE
	timer = CreateWaitableTimer(
		NULL,		// security attributes 
		TRUE,		// manual-reset notification timer (FALSE -- the timer is a synchronization timer)
		NULL);		// timer without name
#endif
	return timer;
}

/*
 * Set @timer timer for 10 seconds
 * if success, return 0. Otherwise, -1.
 */
static int set_timer(KLOGGER_TIMER timer) {
	LARGE_INTEGER due_time;
	int status;

	UNREFERENCED_PARAMETER(status);
	if (NULL == timer)
		return -1;

	due_time.QuadPart = -100000000LL;
#ifdef KERNEL
	KeSetTimer(
		timer,
		due_time,
		NULL			// DPC (optional)
	);
#else
	// USERSPACE
	status = SetWaitableTimer(timer, &due_time, 
		0,		// NOT periodic. The timer is signaled once.
		NULL, 
		NULL, 
		0);
	return (0 != status) ? 0 : -1;
#endif
}

static void destroy_timer(KLOGGER_TIMER timer) {
#ifdef KERNEL
	KeCancelTimer(timer);
#else
	if (!CancelWaitableTimer(timer))
		PRINT("ERROR [flush_thread_routine]: can not cancel waitable timer");
	CloseHandle(timer);
#endif
}

/* * * * * * * * * */
/* LOCK OPERATIONS */
/* * * * * * * * * */

#ifdef KERNEL
typedef HANDLE SYNCR_PRIM;
#else
typedef HANDLE SYNCR_PRIM;
#endif

static SYNCR_PRIM create_lock() {
	SYNCR_PRIM lock;

#ifdef KERNEL
	// TODO:
	
#else
	// USERSPACE
	lock = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
#endif

	return lock;
}

#ifdef KERNEL
// TODO:
#define acquire_lock(lock)	;
#else
#define acquire_lock(lock)							\
	do {											\
		DWORD dwWaitResult;							\
		dwWaitResult = WaitForSingleObject(			\
			lock,       /* handle to mutex */		\
			INFINITE);  /* no time-out interval */	\
		switch (dwWaitResult) {						\
		case WAIT_OBJECT_0:							\
			break;									\
		case WAIT_ABANDONED:						\
			PRINT("ERROR: WAIT ABANDONED\n");		\
		}											\
	} while (0);
#endif

#ifdef KERNEL
// TODO:
#define release_lock(lock)	;
#else
#define release_lock(lock)											\
	do {															\
		if (!ReleaseMutex(lock))									\
			PRINT("ERROR [release_lock]: can not release lock\n");	\
		} while (0);
#endif

static void destroy_lock(SYNCR_PRIM lock) {
#ifdef KERNEL
	// TODO
	UNREFERENCED_PARAMETER(lock);
	;
#else
	if (!lock)
		return;
	CloseHandle(lock);
#endif
}

#endif  // _WRAPPERS_