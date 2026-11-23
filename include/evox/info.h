// The live information: the per-frame tasks after each Render (clock, info texts, DVD tray,
// memory units) and the texts they keep for the skin variables (include/evox/skinvars.h) and
// the settings screen's information rows.
#pragma once
#include <xtl.h>

void ClockTick();  // 0x12390 set the clock once from an SNTP server (src/clock.cpp)
void InfoTick();   // 0x1F610 the info texts and $Time$
void TrayTick();   // 0x1FE70 the DVD tray: $CD$ and the disc type
void MuTick();     // 0x21820 mount and unmount memory units

const WCHAR *FreeSpaceText(const char *root); // 0x1F200
void ReadBiosVersion();                       // 0x1F020 $BIOSVer$ from the flash's MD5
int SmartTemperature();                       // 0x234F0 the drive's temperature, or -1 (hdd.cpp)

extern WCHAR g_InfoNetType[0x40];        // 0x47CD20
extern WCHAR g_InfoLineStatus[0x40];     // 0x57D3E8
extern WCHAR g_InfoMac[0x40];            // 0x47C120
extern WCHAR g_InfoSecurity[0x40];       // 0x590948
extern WCHAR g_InfoHardware[0x40];       // 0x47C4A0
extern WCHAR g_InfoChameleonRaw[0x20];   // 0x57D3A0
extern WCHAR g_InfoChameleonSw1[0x20];   // 0x47C3E0
extern WCHAR g_InfoChameleonSw2[0x20];   // 0x47C290
extern WCHAR g_InfoChameleonMode[0x20];  // 0x47C358
extern WCHAR g_InfoChameleonBank[0x20];  // 0x57D360
extern WCHAR g_ProgressText[0x200];      // 0x47CDE0 cleared by InfoTick's first run
extern char g_BiosMd5Text[0x48];         // 0x47C2D0 "0x" and the flash's MD5 in hex
extern char g_MuDrive[8];                // 0x57F5D8 memory unit drive letters (0: not mounted)
extern BYTE g_SmartData[0x200];          // 0x590548 SMART attributes (read by SmartEnabled237C0)
extern bool g_SmartOn;                   // 0x58FB00 SMART is enabled on the drive (hdd.cpp)
extern BYTE g_HddTempId;                 // 0x95CD0 the SMART attribute with the temperature
