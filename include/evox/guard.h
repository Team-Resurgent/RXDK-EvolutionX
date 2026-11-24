// Crash guards: EvoX's catch (...) blocks. The original's compiler let catch (...) catch
// structured exceptions too (an access violation, say), so these blocks keep the dashboard
// running after a fault in the code they guard: the catch releases the evox.ini lock, logs, and
// the function carries on after the block. RXDK's clang drops __try/__except on its target, and
// its C++ exceptions never see a fault, so the guard registers an exception frame of its own on
// fs:[0]; on a fault its handler unwinds the frames above it and longjmps back to the guard.
//
//     if (!CrashGuard([&] { ...the try block... }))
//         ...the catch block...;
//
// A return inside the lambda leaves only the guarded block; the callers carry the result out.
#pragma once
#include <setjmp.h>

struct CrashGuardFrame {
    CrashGuardFrame *next; // EXCEPTION_REGISTRATION_RECORD: the next frame and the handler
    void *handler;
    jmp_buf resume;
};
void CrashGuardPush(CrashGuardFrame *frame);
void CrashGuardPop(CrashGuardFrame *frame);

// run body; false if it faulted (it then stopped at the fault)
template <class F> bool CrashGuard(F &&body)
{
    CrashGuardFrame frame;
    if (setjmp(frame.resume)) {
        CrashGuardPop(&frame);
        return false;
    }
    CrashGuardPush(&frame);
    body();
    CrashGuardPop(&frame);
    return true;
}
