// What is not rebuilt yet. Each one is a stub in src/pending.cpp that logs its name and original
// address and returns a neutral result; replace them as the real code is rebuilt. The dashboard
// classes that are still stubs are declared in include/evox/dash.h.
#pragma once
#include <xtl.h>
#include "evox/chameleon.h"

// main
bool DebugMonitorPresent();        // 0x218D0 (main.cpp)
void ResetNicRegisters();          // (main.cpp)

// leaving the dashboard (launch.cpp)
void LaunchXbe(const char *path);   // 0x232A0 record the XBE to launch
void SetTrainer(const char *trainer); // 0x23420
void PrepareForExit();             // 0x24140 network down before halting or launching
void IgrFinish();                  // 0x23450 write the launch data, quick reboot
void IgrInstall();                 // 0x23BA0 the in-game reset (igr.cpp)

void InitHardDisk();               // 0x23820 drive letters, SMART (hdd.cpp)
bool ReadDiskSector(DWORD sector, void *buf); // 0x23060 (hdd.cpp)

// the drive (InitHardDisk)
struct AtaRegisters {              // 16 bytes
    BYTE features, sectorCount, lbaLow, lbaMid, lbaHigh, device, command, unk07;
    BYTE unk08[8];
};
extern BYTE *g_DriveIdentify;      // 0x58FB08 the drive's IDENTIFY data (512 bytes)
void IdentifyDrive();              // 0x236E0
void AtaCommand(AtaRegisters *r);  // 0x23530
void SmartEnabled237C0();          // 0x237C0
void Func235F0();                  // 0x235F0 end of InitHardDisk

class CTelnetServer { // 0x1220 bytes, created by main and never deleted
public:
    CTelnetServer(); // 0x17390
};

// the dashboard
HRESULT InitDevices23D70();               // 0x23D70 in Create
HRESULT InitDevices23F70();               // 0x23F70 in Create
int KeyboardPoll();                       // 0x23E10 each frame
const BYTE *KeyboardGetKeystroke();       // 0x24030 each frame: {vkey, flags} or NULL
void Func20F20();                         // 0x20F20 state 15
void StartBackup();                       // 0x20460 state 5: the backup thread (backup.cpp)
