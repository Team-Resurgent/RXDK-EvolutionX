// Functions main calls that are not rebuilt yet. Each one is a stub in src/pending.cpp that logs
// its name and original address; replace them as the real code is rebuilt.
#pragma once
#include <xtl.h>

void InitHardDisk();               // 0x23820 "Main : Init Hdd!!"
BYTE ChameleonRead(int reg);       // 0x1F300 0xAA from reg 0: a Chameleon is fitted
void ChameleonSetLed(int on);      // 0x1F3C0
void IgrPrepare();                 // 0x24140 after the dashboard returns
void IgrInstall();                 // 0x23BA0
void IgrFinish();                  // 0x23450

// "OpenX_Dash": the dashboard application object on main's stack (0x1940 bytes)
class CEvoxDash {
public:
    CEvoxDash();     // 0x1CFE0
    ~CEvoxDash();    // 0x173F0
    HRESULT Create(); // 0x1D920
    void Run();       // 0x1DDB0
    void Destroy();   // 0x1D0E0
    bool m_InstallIgr = false; // +0x193C checked by main after Run
};

class CTelnetServer { // 0x1220 bytes, created and never deleted
public:
    CTelnetServer(); // 0x17390
};
