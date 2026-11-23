// Stubs for include/evox/pending.h: they log and return a neutral result, so the XBE runs main
// end to end while the real code is rebuilt.
#include "evox/log.h"
#include "evox/pending.h"

#define PENDING(name, addr) Log("pending: %s (0x%X)\n", name, addr)

void InitHardDisk() { PENDING("InitHardDisk", 0x23820); }
BYTE ChameleonRead(int) { PENDING("ChameleonRead", 0x1F300); return 0; }
void ChameleonSetLed(int) { PENDING("ChameleonSetLed", 0x1F3C0); }
void IgrPrepare() { PENDING("IgrPrepare", 0x24140); }
void IgrInstall() { PENDING("IgrInstall", 0x23BA0); }
void IgrFinish() { PENDING("IgrFinish", 0x23450); }

CEvoxDash::CEvoxDash() { PENDING("CEvoxDash::CEvoxDash", 0x1CFE0); }
CEvoxDash::~CEvoxDash() { PENDING("CEvoxDash::~CEvoxDash", 0x173F0); }
HRESULT CEvoxDash::Create() { PENDING("CEvoxDash::Create", 0x1D920); return E_FAIL; }
void CEvoxDash::Run() { PENDING("CEvoxDash::Run", 0x1DDB0); }
void CEvoxDash::Destroy() { PENDING("CEvoxDash::Destroy", 0x1D0E0); }

CTelnetServer::CTelnetServer() { PENDING("CTelnetServer::CTelnetServer", 0x17390); }
