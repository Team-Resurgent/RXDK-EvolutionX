// The crash guard's exception frame (include/evox/guard.h).
#include <xtl.h>
#include <setjmp.h>
#include "evox/guard.h"
#include "evox/log.h"

extern "C" VOID __stdcall RtlUnwind(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord,
                                    PVOID ReturnValue);

#define UNWINDING 0x06 // EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND

// RtlUnwind(frame, 0, record, 0): unwind and unlink the frames above this one. It returns through
// NtContinue with ebx, esi and edi zeroed, so they are saved around it here (as the original's
// _global_unwind2 does).
__attribute__((naked)) static void UnwindTo(void *frame, EXCEPTION_RECORD *record)
{
    __asm__("pushl %ebp\n\t"
            "pushl %ebx\n\t"
            "pushl %esi\n\t"
            "pushl %edi\n\t"
            "pushl $0\n\t"       // ReturnValue
            "pushl 28(%esp)\n\t" // ExceptionRecord
            "pushl $0\n\t"       // TargetIp
            "pushl 32(%esp)\n\t" // TargetFrame
            "calll _RtlUnwind@16\n\t"
            "popl %edi\n\t"
            "popl %esi\n\t"
            "popl %ebx\n\t"
            "popl %ebp\n\t"
            "retl");
}

static void *ExceptionList()
{
    void *p;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(p));
    return p;
}

static void SetExceptionList(void *p) { __asm__ volatile("movl %0, %%fs:0" : : "r"(p) : "memory"); }

// the frame's handler: on a fault (not while unwinding), unwind every frame registered above
// this one, as the original's catch did, and resume at the guard
extern "C" int __cdecl CrashGuardHandler(EXCEPTION_RECORD *record, void *frame, void *, void *)
{
#ifdef EVOX_TEST_SCRIPT
    Log("guard: %08lx at %p flags %lx frame %p\n", record->ExceptionCode, record->ExceptionAddress,
        record->ExceptionFlags, frame);
#endif
    if (record->ExceptionFlags & UNWINDING)
        return 1; // ExceptionContinueSearch
    UnwindTo(frame, record);
#ifdef EVOX_TEST_SCRIPT
    Log("guard: unwound\n");
#endif
    longjmp(((CrashGuardFrame *)frame)->resume, 1);
}

void CrashGuardPush(CrashGuardFrame *frame)
{
    frame->next = (CrashGuardFrame *)ExceptionList();
    frame->handler = (void *)CrashGuardHandler;
    SetExceptionList(frame);
}

void CrashGuardPop(CrashGuardFrame *frame) { SetExceptionList(frame->next); }
