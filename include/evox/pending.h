// What is not rebuilt yet. Each one is a stub in src/pending.cpp that logs its name and original
// address and returns a neutral result; replace them as the real code is rebuilt. The dashboard
// classes that are still stubs are declared in include/evox/dash.h.
#pragma once
#include <xtl.h>

// main
bool DebugMonitorPresent();        // 0x218D0 (main.cpp)
void ResetNicRegisters();          // (main.cpp)

// leaving the dashboard (launch.cpp)
void LaunchXbe(const char *path);   // 0x232A0 record the XBE to launch
void SetTrainer(const char *trainer); // 0x23420
void PrepareForExit();             // 0x24140 network down before halting or launching
void IgrFinish();                  // 0x23450 write the launch data, quick reboot

void InitHardDisk();               // 0x23820 "Main : Init Hdd!!"
BYTE ChameleonRead(int reg);       // 0x1F300 0xAA from reg 0: a Chameleon is fitted
void ChameleonSetLed(int on);      // 0x1F3C0
void IgrInstall();                 // 0x23BA0

class CTelnetServer { // 0x1220 bytes, created by main and never deleted
public:
    CTelnetServer(); // 0x17390
};

// the dashboard
HRESULT InitDevices23D70();               // 0x23D70 in Create
HRESULT InitDevices23F70();               // 0x23F70 in Create
int KeyboardPoll();                       // 0x23E10 each frame
const BYTE *KeyboardGetKeystroke();       // 0x24030 each frame: {vkey, flags} or NULL
void ChameleonSetColour(int colour);      // 0x1F400
void ChameleonSetFlash(int on);           // 0x1F380
void Func20F20();                         // 0x20F20 state 15
void Func20460();                         // 0x20460 state 5
void ClockTick();                         // 0x12390 SNTP clock, after each frame
void Tick1F610();                         // 0x1F610 after each frame
void Tick1FE70();                         // 0x1FE70 after each frame
void Tick21820();                         // 0x21820 after each frame
