// Stubs for what is not rebuilt yet (include/evox/pending.h, the stub classes in
// include/evox/dash.h). They log and return a neutral result, so the XBE runs end to end while
// the real code is rebuilt. Once-per-frame stubs log only their first call.
#include <string.h>
#include "evox/dash.h"
#include "evox/log.h"
#include "evox/pending.h"

#define PENDING(name, addr) Log("pending: %s (0x%X)\n", name, addr)
#define PENDING_ONCE(name, addr)                                                                   \
    do {                                                                                           \
        static bool logged;                                                                        \
        if (!logged) {                                                                             \
            logged = true;                                                                         \
            Log("pending: %s (0x%X) (logged once)\n", name, addr);                                 \
        }                                                                                          \
    } while (0)

// main
static BYTE s_NoIdentify[0x200]; // until IdentifyDrive is rebuilt: an all-zero IDENTIFY block
BYTE *g_DriveIdentify = s_NoIdentify;
void IdentifyDrive() { PENDING("IdentifyDrive", 0x236E0); }
bool AtaCommand(AtaRegisters *) { PENDING("AtaCommand", 0x23530); return false; }
void SmartEnabled237C0() { PENDING("SmartEnabled237C0", 0x237C0); }
void Func235F0() { PENDING("Func235F0", 0x235F0); }

// the dashboard
void Func20F20() { PENDING("Func20F20", 0x20F20); }

void CEvoxDash::OpenItem() { PENDING("CEvoxDash::OpenItem", 0x1CAC0); }
void CEvoxDash::DrawPass187B0() { PENDING_ONCE("CEvoxDash::DrawPass187B0", 0x187B0); }
void CEvoxDash::DrawMessage() { PENDING_ONCE("CEvoxDash::DrawMessage", 0x18E30); }
