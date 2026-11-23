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
void AtaCommand(AtaRegisters *) { PENDING("AtaCommand", 0x23530); }
void SmartEnabled237C0() { PENDING("SmartEnabled237C0", 0x237C0); }
void Func235F0() { PENDING("Func235F0", 0x235F0); }
BYTE ChameleonRead(int) { PENDING("ChameleonRead", 0x1F300); return 0; }
void ChameleonSetLed(int) { PENDING("ChameleonSetLed", 0x1F3C0); }
void IgrInstall() { PENDING("IgrInstall", 0x23BA0); }
CTelnetServer::CTelnetServer() { PENDING("CTelnetServer::CTelnetServer", 0x17390); }

// the dashboard
HRESULT InitDevices23D70() { PENDING("InitDevices23D70", 0x23D70); return S_OK; }
HRESULT InitDevices23F70() { PENDING("InitDevices23F70", 0x23F70); return S_OK; }
int KeyboardPoll() { PENDING_ONCE("KeyboardPoll", 0x23E10); return 0; }
const BYTE *KeyboardGetKeystroke() { PENDING_ONCE("KeyboardGetKeystroke", 0x24030); return nullptr; }
void ChameleonSetColour(int) { PENDING("ChameleonSetColour", 0x1F400); }
void ChameleonSetFlash(int) { PENDING("ChameleonSetFlash", 0x1F380); }
void Func20F20() { PENDING("Func20F20", 0x20F20); }
void Func20460() { PENDING("Func20460", 0x20460); }
void ClockTick() { PENDING_ONCE("ClockTick", 0x12390); }
void Tick1F610() { PENDING_ONCE("Tick1F610", 0x1F610); }
void Tick1FE70() { PENDING_ONCE("Tick1FE70", 0x1FE70); }
void Tick21820() { PENDING_ONCE("Tick21820", 0x21820); }

// stub classes from dash.h
CDebugServer::CDebugServer() { memset(this, 0, sizeof *this); PENDING("CDebugServer::CDebugServer", 0x22FA0); }
CFtpServer::CFtpServer() { memset(this, 0, sizeof *this); PENDING("CFtpServer::CFtpServer", 0x14D90); }

void CEvoxDash::SaveTrainerOptions() { PENDING("CEvoxDash::SaveTrainerOptions", 0x1B380); }
void CEvoxDash::SaveTrainerFile() { PENDING("CEvoxDash::SaveTrainerFile", 0x195E0); }
void CEvoxDash::ScanBios(const char *dir) { Log("pending: CEvoxDash::ScanBios (0x19480) %s\n", dir); }
void CEvoxDash::ScanSkins(const char *dir) { Log("pending: CEvoxDash::ScanSkins (0x1D440) %s\n", dir); }
void CEvoxDash::ScanTrainers(const char *dir) { Log("pending: CEvoxDash::ScanTrainers (0x19750) %s\n", dir); }
void CEvoxDash::PrepareMenuLine(char *) { PENDING("CEvoxDash::PrepareMenuLine", 0x199A0); }
void CEvoxDash::ParseMenuSection(CMenu *) { PENDING("CEvoxDash::ParseMenuSection", 0x19B20); }
void CEvoxDash::OpenFileView(const char *) { PENDING("CEvoxDash::OpenFileView", 0x1B130); }
void CEvoxDash::OpenItem() { PENDING("CEvoxDash::OpenItem", 0x1CAC0); }
void CEvoxDash::DrawPass187B0() { PENDING_ONCE("CEvoxDash::DrawPass187B0", 0x187B0); }
void CEvoxDash::DrawPass18D10() { PENDING_ONCE("CEvoxDash::DrawPass18D10", 0x18D10); }
void CEvoxDash::DrawMessage() { PENDING_ONCE("CEvoxDash::DrawMessage", 0x18E30); }
