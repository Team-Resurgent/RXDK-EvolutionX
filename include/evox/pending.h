// What is not rebuilt yet. Each one is a stub in src/pending.cpp that logs its name and original
// address and returns a neutral result; replace them as the real code is rebuilt. The dashboard
// classes that are still stubs are declared in include/evox/dash.h.
#pragma once
#include <xtl.h>

// main
void InitHardDisk();               // 0x23820 "Main : Init Hdd!!"
BYTE ChameleonRead(int reg);       // 0x1F300 0xAA from reg 0: a Chameleon is fitted
void ChameleonSetLed(int on);      // 0x1F3C0
void PrepareForExit();             // 0x24140 before halting or the IGR steps
void IgrInstall();                 // 0x23BA0
void IgrFinish();                  // 0x23450

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
void LaunchXbe(const char *path);         // 0x232A0
void SetTrainer(int trainer);             // 0x23420
void Func20F20();                         // 0x20F20 state 15
void Func20460();                         // 0x20460 state 5
void ClockTick();                         // 0x12390 SNTP clock, after each frame
void Tick1F610();                         // 0x1F610 after each frame
void Tick1FE70();                         // 0x1FE70 after each frame
void Tick21820();                         // 0x21820 after each frame

// data embedded in the original, not extracted yet (NULL for now)
extern const void *const g_ResourceXpr;   // 0xCF240 XPR with 4 resources
extern void *const g_ResourceTags;        // 0x94D70 their names
extern const void *const g_FontData;      // 0x33D240
