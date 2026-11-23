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
void InitHardDisk() { PENDING("InitHardDisk", 0x23820); }
BYTE ChameleonRead(int) { PENDING("ChameleonRead", 0x1F300); return 0; }
void ChameleonSetLed(int) { PENDING("ChameleonSetLed", 0x1F3C0); }
void PrepareForExit() { PENDING("PrepareForExit", 0x24140); }
void IgrInstall() { PENDING("IgrInstall", 0x23BA0); }
void IgrFinish() { PENDING("IgrFinish", 0x23450); }
CTelnetServer::CTelnetServer() { PENDING("CTelnetServer::CTelnetServer", 0x17390); }

// the dashboard
HRESULT InitDevices23D70() { PENDING("InitDevices23D70", 0x23D70); return S_OK; }
HRESULT InitDevices23F70() { PENDING("InitDevices23F70", 0x23F70); return S_OK; }
int KeyboardPoll() { PENDING_ONCE("KeyboardPoll", 0x23E10); return 0; }
const BYTE *KeyboardGetKeystroke() { PENDING_ONCE("KeyboardGetKeystroke", 0x24030); return nullptr; }
void ChameleonSetColour(int) { PENDING("ChameleonSetColour", 0x1F400); }
void ChameleonSetFlash(int) { PENDING("ChameleonSetFlash", 0x1F380); }
void LaunchXbe(const char *path) { Log("pending: LaunchXbe (0x232A0) %s\n", path); }
void SetTrainer(int) { PENDING("SetTrainer", 0x23420); }
void Func20F20() { PENDING("Func20F20", 0x20F20); }
void Func20460() { PENDING("Func20460", 0x20460); }
void ClockTick() { PENDING_ONCE("ClockTick", 0x12390); }
void Tick1F610() { PENDING_ONCE("Tick1F610", 0x1F610); }
void Tick1FE70() { PENDING_ONCE("Tick1FE70", 0x1FE70); }
void Tick21820() { PENDING_ONCE("Tick21820", 0x21820); }

const void *const g_ResourceXpr = nullptr;
void *const g_ResourceTags = nullptr;
const void *const g_FontData = nullptr;

// stub classes from dash.h
CEmbeddedResource::CEmbeddedResource() { memset(raw, 0, sizeof raw); }
CEmbeddedResource::~CEmbeddedResource() {}
HRESULT CEmbeddedResource::Create(IDirect3DDevice8 *, DWORD) { PENDING("CEmbeddedResource::Create", 0x25250); return S_OK; }
CXBPackedResource::CXBPackedResource() { memset(raw, 0, sizeof raw); }
CXBPackedResource::~CXBPackedResource() {}
HRESULT CXBPackedResource::Create(IDirect3DDevice8 *, const void *, DWORD, void *)
{
    PENDING("CXBPackedResource::Create", 0x26180);
    return S_OK;
}
CXBFont::CXBFont() { memset(raw, 0, sizeof raw); }
HRESULT CXBFont::Create(IDirect3DDevice8 *, const void *, CXBPackedResource *)
{
    PENDING("CXBFont::Create", 0x25C80);
    return S_OK;
}
CMenuItem *CMenu::GetItem(int)
{
    static char empty[1];
    static CMenuItem item = {{}, empty};
    PENDING("CMenu::GetItem", 0x24E90);
    return &item;
}
CDebugServer::CDebugServer() { memset(this, 0, sizeof *this); PENDING("CDebugServer::CDebugServer", 0x22FA0); }
CFtpServer::CFtpServer() { memset(this, 0, sizeof *this); PENDING("CFtpServer::CFtpServer", 0x14D90); }

void CEvoxDash::InitItems() { PENDING("CEvoxDash::InitItems", 0x1D680); }
void CEvoxDash::LoadSkin(const char *name) { Log("pending: CEvoxDash::LoadSkin (0x1CC80) %s\n", name); }
void CEvoxDash::ReleaseSkin() { PENDING("CEvoxDash::ReleaseSkin", 0x195E0); }
void CEvoxDash::OpenSettings() { PENDING("CEvoxDash::OpenSettings", 0x1A150); }
void CEvoxDash::OpenFileView(const char *) { PENDING("CEvoxDash::OpenFileView", 0x1B130); }
void CEvoxDash::OpenItem() { PENDING("CEvoxDash::OpenItem", 0x1CAC0); }
void CEvoxDash::DrawBackground() { PENDING_ONCE("CEvoxDash::DrawBackground", 0x18010); }
void CEvoxDash::DrawPass17CD0() { PENDING_ONCE("CEvoxDash::DrawPass17CD0", 0x17CD0); }
void CEvoxDash::DrawPass1CA50() { PENDING_ONCE("CEvoxDash::DrawPass1CA50", 0x1CA50); }
void CEvoxDash::DrawMenu() { PENDING_ONCE("CEvoxDash::DrawMenu", 0x18320); }
void CEvoxDash::DrawPass187B0() { PENDING_ONCE("CEvoxDash::DrawPass187B0", 0x187B0); }
void CEvoxDash::DrawSettings() { PENDING_ONCE("CEvoxDash::DrawSettings", 0x1B820); }
void CEvoxDash::DrawPass18D10() { PENDING_ONCE("CEvoxDash::DrawPass18D10", 0x18D10); }
void CEvoxDash::DrawMessage() { PENDING_ONCE("CEvoxDash::DrawMessage", 0x18E30); }
void CEvoxDash::DrawQuad(IDirect3DTexture8 *, float, void *) { PENDING_ONCE("CEvoxDash::DrawQuad", 0x17450); }
void CEvoxDash::DrawRect(float, float, float, float, DWORD, DWORD) { PENDING_ONCE("CEvoxDash::DrawRect", 0x18140); }
