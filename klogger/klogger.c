
/*
 * Klogger routine
 */

#include "klogger.h"
#include "ring_buffer.h"
#include "wrappers.h"

#define FILE_NAME_FOR_FLUSH L"\\DosDevices\\C:\\flush.txt"

// this is singleton
static klogger_t *klogger = NULL;


static HANDLE create_file_for_flush();
static NTSTATUS close_file(HANDLE file);
static HANDLE init_flushing_thread();
static void deinit_flushing_thread();

klogger_t *init_klogger() {
	NTSTATUS status;

	// WARNING:
	//		the following initializations must be made at PASSIVE LEVEL IRQ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		// critical error!
		PRINT("ERROR [init_klogger]: critical error! MUST BE PASSIVE_LEVEL ");
		return NULL;
	}

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
	klogger->rbuf = init_ring_buffer();
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
	// TODO

	// 5. 
	klogger->exit_now = FALSE;

	// 6. init flushing thread
	klogger->flushing_thread = init_flushing_thread();
	if (NULL == klogger->flushing_thread) {
		PRINT("ERROR [init_klogger]: NULL == klogger->flushing thread");
		goto close_flush_file;
	}

	PRINT("[init_klogger]: successful...");
	return klogger;

close_flush_file:
	status = close_file(klogger->file);
	if (NT_SUCCESS(status)) {
		PRINT("ERROR [init_klogger]: close_file returns not success status");
	}
deinit_rbuf:
	deinit_ring_buffer(klogger->rbuf);
free_klogger:
	my_free(klogger);

	return NULL;

}

int deinit_klogger(klogger_t *klogger) {
	// WARNING:
	//		the following code must be made at PASSIVE LEVEL IRQ
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		// critical error!
		PRINT("ERROR [deinit_klogger]: critical error! MUST BE PASSIVE_LEVEL");
		return -1;
	}

	if (!klogger) {
		PRINT("[deinit_klogger]: klogger is NULL...");
		return 0;
	}

	// 0.
	klogger->exit_now = TRUE;

	// 1.
	deinit_flushing_thread();

	// 2. close flushing file
	if (STATUS_SUCCESS != close_file(klogger->file)) {
		PRINT("[deinit_klogger]: can NOT close flushing file. Try later.");
		return -1;
	}

	// 3.
	deinit_ring_buffer(klogger->rbuf);

	// 4.
	my_free(klogger);

	PRINT("[deinit_klogger]: success");
	return 0;
}

static void set_flush_event() {
	;
}


int add_to_rbuf(TCHAR *str) {
	int status;
	
	// 0.
	if ((!klogger) || klogger->exit_now) {
		// writers can not write anymore
		return -1;
	}

	// 1. 
	status = write_to_rbuf(klogger->rbuf, str);	// TODO: write_to_rbuf (see ring_buffer.c) !

	// 2.
	set_flush_event();

	return status;
}

// Do not try to perform any file operations at higher IRQL levels
static HANDLE create_file_for_flush() {
#ifdef KERNEL
	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;
	HANDLE   handle;
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
#else
	//  USERSPACE
#endif

	if (STATUS_SUCCESS != ntstatus) {
		return NULL;
	}
	return handle;
}

// should be called at PASSIVE_LEVEL!
static NTSTATUS close_file(HANDLE file) {
#ifdef KERNEL
	return ZwClose(file);
#else // USERSPACE
	
#endif
}

// main function of flushing thread
static void flush_thread_routine(IN PVOID context) {
	context = NULL;

	// TODO: check


}

/*
 * WARNING:
 *		This function must be called at PASSIVE_LEVEL IRQ
 *
 * IMPORTANT:
 *		This thread runs at PASSIVE_LEVEL IRQ
 */
static HANDLE init_flushing_thread() {
	HANDLE flushing_thread = NULL;

#ifdef KERNEL
	NTSTATUS status;

	status = PsCreateSystemThread(
		&flushing_thread,
		THREAD_ALL_ACCESS,
		NULL,				// object attributes
		NULL,
		NULL,
		flush_thread_routine,
		NULL
		);
	if (!NT_SUCCESS(status)) {
		PRINT("ERROR [init_flushing_thread]: cannot create flushing thread");
	}
#else // USERSPACE

#endif

	return flushing_thread;
}

static void deinit_flushing_thread() {
#ifdef KERNEL	
	my_set_event(&klogger->flush_event, 0, FALSE);
	my_wait_for_single_object(&klogger->fl_thread_exiting,
		Executive,
		KernelMode,
		FALSE,
		NULL);
	PRINT("[deinit_flushing_thread]: success");
#else // USERSPACE

#endif

}