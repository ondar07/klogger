/*
 * This file is for driver routine 
 * (it will be meaninguful, if KERNEL macro is defined)
 *
 */
#include "klogger.h"
#include "wrappers.h"

#ifdef KERNEL

#include "ntddk.h"

// BUF_SIZE 1 Kb
#define BUF_SIZE 1024

#define STDCALL __stdcall

#define AUTO_TEST

// global variable
static klogger_t *klogger = NULL;

#ifdef AUTO_TEST
extern void deinit_auto_test();
extern int init_auto_test(klogger_t *klogger);
#endif

/*
 *  Undocument function 
 */
NTSTATUS STDCALL ZwCreateEvent(
	OUT PHANDLE  EventHandle,
	IN ACCESS_MASK  DesiredAccess,
	IN POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
	IN EVENT_TYPE  EventType,
	IN BOOLEAN  InitialState);

/*
 *	forward declarations of functions
 */
NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pusRegPath);

/*
 *	desired sections for functions
 */
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);

	if (!klogger) {
		PRINT("[DriverUnload]: klogger is NULL (it was not initialized before)\n");
		return STATUS_SUCCESS;
	}

	// The KeFlushQueuedDpcs routine returns 
	// after all queued DPCs on all processors have executed
	KeFlushQueuedDpcs();

	// 2.
#ifdef AUTO_TEST
	deinit_auto_test();
#endif

	// 3.
	if (deinit_klogger(klogger) < 0) {
		PRINT("[DriverUnload]: Problems with driver unloading\n");
		PRINT("[DriverUnload]: deinit_klogger() returns a value which is < 0\n");
		// STATUS_DRIVER_FAILED_PRIOR_UNLOAD -- The driver could not be loaded
		// because a previous version of the driver is still in memory
		return STATUS_DRIVER_FAILED_PRIOR_UNLOAD;
	}
	else {
		PRINT("[DriverUnload]: success\n");
	}
	return STATUS_SUCCESS;
}

/*
 *	COMMENT:
 *		entry function, [INIT]
 *	ARGUMENTS:
 *		standard for DriverEntry
 *	RETURN VALUE:
 *		NTSTATUS
 */
NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pusRegPath)
{
	NTSTATUS Status = STATUS_SUCCESS;
#if 0
	HANDLE hEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	PVOID pEvent;
#endif

	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pusRegPath);

	//__debugbreak();

	// init klogger
	klogger = init_klogger(BUF_SIZE);
	if (NULL == klogger) {
		Status = STATUS_FAILED_DRIVER_ENTRY;
		PRINT("ERROR [DriverEntry]: init_klogger returns NULL\n");
	}
	pDriverObject->DriverUnload = DriverUnload;

#ifdef AUTO_TEST
	init_auto_test(klogger);
#endif

	PRINT("[DriverEntry]: add to rbuf : Hello, world!");
	PRINT("[DriverEntry]: success\n");
	
	return Status;
}


#endif // KERNEL