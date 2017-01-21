
#include "wrappers.h"
#include "klogger.h"
#include <Ntstrsafe.h>

// should be at nonpaged pool
static PKTIMER test_timer = NULL;
static PRKDPC   timer_dpc = NULL;

// there is timeout every  5 seconds
// this value must be greater than 1 second (because of KeSetTimerEx())
// see below 
#define TIMER_PERIOD 5

int alloc_memory_for_timer_and_dpc() {
	test_timer = my_alloc(sizeof(KTIMER));
	if (NULL == test_timer) {
		PRINT("ERROR[]: cannot allocate memory for test_timer\n");
		return -1;
	}

	timer_dpc = my_alloc(sizeof(KDPC));
	if (NULL == timer_dpc) {
		my_free(test_timer);
		PRINT("ERROR[]: cannot allocate memory for timer_dpc\n");
		test_timer = NULL;
		return -1;
	}
	return 0;
}

KDEFERRED_ROUTINE timer_dpc_routine;

// DPC level
VOID timer_dpc_routine(
	_In_     struct _KDPC *Dpc,
	_In_opt_ PVOID        DeferredContext,
	_In_opt_ PVOID        SystemArgument1,
	_In_opt_ PVOID        SystemArgument2
	)
{
	klogger_t *klogger = (klogger_t *)DeferredContext;
	//char str[64];
	//NTSTATUS status;
	KIRQL cur_irql;
	static int i = 0;

	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);
	UNREFERENCED_PARAMETER(Dpc);

	//memset(str, '\0', 64);
	if ((i++ % 2) == 0) {
		// at HIGH_LEVEL
		KeRaiseIrql(HIGH_LEVEL, &cur_irql);

		if (add_to_rbuf(klogger, "HIGH_LEVEL\r\n") < 0) {
			PRINT("[timer_dpc_routine] cannot add_to_rbuf");
		}

		// restores the IRQL on the current processor to its original value
		KeLowerIrql(cur_irql);
	} else {
		// at DISPATCH_LEVEL
		if (add_to_rbuf(klogger, "DISPATCH\r\n") < 0) {
			PRINT("[timer_dpc_routine] cannot add_to_rbuf");
		}
	}
#if 0
	// PASSIVE_LEVEL
	status = RtlStringCbPrintfA(
		str,
		64,
		"CurThread=%x\r\n",
		KeGetCurrentThread()
		);
	PRINT("%s\n", str);
	if (add_to_rbuf(klogger, str) < 0) {
		PRINT("[timer_dpc_routine] cannot add_to_rbuf");
	}
#endif
}

// should be called at <= DISPATCH_LEVEL
int init_auto_test(klogger_t *klogger) {
	LARGE_INTEGER DueTime;

	PRINT("[init_auto_test] starts...\n");
	if (alloc_memory_for_timer_and_dpc() < 0)
		return -1;

	// 2. init timer
	// KeInitializeTimer  can only initialize a notification timer
	// Use KeInitializeTimerEx to initialize a notification timer or a synchronization timer
	KeInitializeTimer(test_timer);

	// 3. init dpc
	KeInitializeDpc(
		timer_dpc,
		timer_dpc_routine,		// dpc routine
		(PVOID)klogger			// context argument for @timer_dpc_routine
		);

	// 4.
	DueTime.QuadPart = -10000000LL;	// 1 seconds
	KeSetTimerEx(
		test_timer,
		DueTime,
		(TIMER_PERIOD - 1) * 1000,	// If the value of this parameter is zero, the timer is a nonperiodic timer that does not automatically re-queue itself.
		timer_dpc
		);

	PRINT("init_auto_test: success\n");

	return 0;
}

void deinit_auto_test() {
	PRINT("deinit_auto_test: starts\n");
	if (NULL == timer_dpc || NULL == test_timer) {
		PRINT("[deinit_auto_test] auto_test init failed before\n");
		PRINT("[deinit_auto_test] so in this function @timer_dpc or @test_timer are NULLs\n");
	}

	// Use the KeFlushQueuedDpcs routine to block until the DPC executes
	KeFlushQueuedDpcs();

	// If the timer object is currently in the system timer queue, 
	// it is removed from the queue. 
	// If a DPC object is associated with the timer, it too is canceled
	KeCancelTimer(test_timer);

	my_free(test_timer);
	my_free(timer_dpc);
	PRINT("deinit_auto_test: success\n");
}
