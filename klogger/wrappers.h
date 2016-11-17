
#ifndef _WRAPPERS_
#define _WRAPPERS_

#define KERNEL
#ifdef KERNEL
	#include <Wdm.h>
#else
	// TODO: USERSPACE include files
#endif

#ifdef KERNEL
	#define PRINT(msg, ...) DbgPrint(msg, __VA_ARGS__)
#else
	// TODO: USERSPACE PRINT
#endif

/*
* This function allocates only NonPagedPool memory
* (so this memory is available at any IRQL)
*
* WARNING:
*     Callers of ExAllocatePoolWithTag must be executing at IRQL <= DISPATCH_LEVEL
*/
static PVOID my_alloc(size_t mem_size) {
	PVOID mem;

#ifdef KERNEL
	mem = ExAllocatePoolWithTag(
		_In_ NonPagedPool,			// driver might access this mem while it is running at IRQL > APC_LEVEL
		_In_ mem_size,
		_In_ 'Tag1'					// TODO: should use a unique pool tag to help debuggers and verifiers identify the code path
		);
#else // USERSPACE
	// mem = malloc
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
#else // USERSPACE
	
#endif
}


static void my_lock() {
	// TODO
	;
}

static void my_unlock() {
	// TODO
	;
}

/* * * * * * * * * * * * * */
/* * * SET EVENT WRAPPER * */
/* * * * * * * * * * * * * */
#ifdef KERNEL
/*
* WARNING:
*		If Wait is set to FALSE, the caller can be running at IRQL <= DISPATCH_LEVEL.
*		Otherwise, callers of KeSetEvent must be running at IRQL <= APC_LEVEL and in a nonarbitrary thread context
*/
static void my_set_event(PRKEVENT event, _In_ KPRIORITY Increment, _In_ BOOLEAN Wait) {
	KeSetEvent(
		event,
		Increment,
		Wait
		);
}
#else // USERSPACE
static void my_set_event() {
	// TODO
	;
}
#endif // KERNEL

/* * * * * * * * * * * * * * * * * */
/* WAIT FOR SINGLE OBJECT WRAPPER  */
/* * * * * * * * * * * * * * * * * */
#ifdef KERNEL
/*
* WARNING:
*		If Wait is set to FALSE, the caller can be running at IRQL <= DISPATCH_LEVEL.
*		Otherwise, callers of KeSetEvent must be running at IRQL <= APC_LEVEL and in a nonarbitrary thread context
*/
static NTSTATUS my_wait_for_single_object(
	_In_     PVOID           Object,
	_In_     KWAIT_REASON    WaitReason,
	_In_     KPROCESSOR_MODE WaitMode,
	_In_     BOOLEAN         Alertable,
	_In_opt_ PLARGE_INTEGER  Timeout
	)
{
	return KeWaitForSingleObject(
		Object,
		WaitReason,
		WaitMode,
		Alertable,
		Timeout
		);
}
#else // USERSPACE
static NTSTATUS my_wait_for_single_object() {
	// TODO
	;
}
#endif // KERNEL



#endif  // _WRAPPERS_