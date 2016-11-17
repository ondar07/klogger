/*
 * This file is for driver routine 
 * (it will be meaninguful, if KERNEL macro is defined)
 *
 */
#include "klogger.h"
#include "wrappers.h"

#ifdef KERNEL

#include "ntddk.h"


#define STDCALL __stdcall

// global variable
static klogger_t *klogger = NULL;

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
	PRINT("%x", pDriverObject->DriverSize);

	if (deinit_klogger(klogger) < 0) {
		PRINT("Problems with driver unloading");
		// TODO: return STATUS_ ERROR
	}
	else {
		PRINT("Driver Unload: success");
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
	NTSTATUS Status;
	HANDLE hEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	PVOID pEvent;

	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pusRegPath);

	//__debugbreak();

	// init klogger
	klogger = init_klogger();
	if (NULL == klogger) {
		// TODO
		;
	}
	pDriverObject->DriverUnload = DriverUnload;

	/*
	 *	FUTURE: 
	 *		creating device and symbolic link
	 *		initialize driver functions
	 *		making this driver unloadable
	 */
	
	RtlInitUnicodeString(
		&us,
		L"\\BaseNamedObjects\\TestEvent");

	InitializeObjectAttributes(
		&oa,
		&us,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor

      	Status = ZwCreateEvent(&hEvent,EVENT_ALL_ACCESS,&oa,
		NotificationEvent,FALSE);

	if (!NT_SUCCESS(Status)) {
		DbgPrint("Failed to create event \n");
		return Status;
	}

	Status = ObReferenceObjectByHandle(
		hEvent, 		//Handle
		EVENT_ALL_ACCESS,	//DesiredAccess
		NULL,			//ObjectType
		KernelMode,		//AccessMode
		&pEvent,		//Object
		NULL);			//HandleInformation

	if (!NT_SUCCESS(Status)) {
		ZwClose(hEvent);
		DbgPrint("Failed to reference event \n");
		return Status;
	}
		
	Status = KeWaitForSingleObject(
		pEvent,  		//Object
		Executive,		//WaitReason
		KernelMode,		//WaitMode
		FALSE,			//Alertable
		NULL);			//Timeout

	DbgPrint("Return from wait with 0x%08X \n",Status);
	ObDereferenceObject(pEvent);
	ZwClose(hEvent);
	return Status;
}


#endif // KERNEL