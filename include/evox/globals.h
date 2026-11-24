// Globals shared across EvoX, with their addresses in the original (.data).
// Names describe what the code does with them.
#pragma once
#include <xtl.h>

extern int g_XboxVersion;            // 0x591AAC 10 = 1.0 board, 11 = 1.1 and later (DetectXboxVersion)
extern char g_DashRoot[0x200];       // 0x590F90 directory of this XBE (device path), mapped to Q:
extern char g_ImagePath[0x81];       // 0x59647F full path of this XBE
extern bool g_RunningFromCd;         // 0x591AA1 started from \Device\CdRom0 (evox.ini read-only)
extern char g_MsDashPath[0x208];     // 0x591198 [Misc] MSDashBoard
extern int g_NetSetupDelay;          // 0x591190 [Network] SetupDelay, ms between network steps
extern bool g_HasChameleon;          // 0x47BC59 Chameleon LED controller answered 0xAA
extern BYTE g_ChameleonState;        // 0x95BB8
extern bool g_IgrEnabled;            // 0x591A8C [Misc] IGR
extern int g_TsrType;                // 0x591AA8 [Misc] TSR_Type (0: no IGR installed on exit)
extern bool g_UseItems;              // 0x94B50 [Misc] UseItems
extern BYTE g_IgrButtons;            // 0x59647C IGR combo: 0x10 Start, 0x20 Back
extern WORD g_IgrButtons2;           // 0x59647D IGR combo: analog buttons (low byte), see main
extern bool g_Fahrenheit;            // 0x57E5B1 [Misc] Fahrenheit
extern int g_ScreenSaverMinutes;     // 0x47BC50 [Misc] ScreenSaver
extern int g_ShadeLevel;             // 0x94B54 [Misc] ShadeLevel, 0..100
extern DWORD g_ScreenSaverDeadline;  // 0x3D80B0 tick count when the screen saver starts
extern int g_ChameleonLed;           // 0xCE9F8 [Misc] ChameleonLed
extern bool g_NetworkStarted;        // 0x57E5B0
extern bool g_IgrOnLaunch;           // 0x591A98 g_IgrEnabled when an XBE or the MS dashboard was launched
extern BYTE g_GameRegionOverride;    // 0x47BC58 region for the IGR when non-zero (else [Misc] GameRegion)
extern BYTE g_GameRegion;            // 0x5964FF region for the IGR
extern bool g_LoaderIgr;             // 0x596500 g_IgrOnLaunch, handed to the IGR loader (main)
extern BYTE g_IgrFlags;              // 0x59647B per launched item: 1 / 2 from its flags 0x400 / 0x800
extern BYTE g_LaunchMountsD;         // 0x95D64 1: launch as "dir;name.xbe" (D: mounted); 0 for No_D_Mount items
extern bool g_HasFDrive;             // 0x58FB01 F: is set up (InitHardDisk)
extern bool g_HasGDrive;             // 0x590538 G: is set up (InitHardDisk)
extern volatile bool g_StopNetwork;  // 0x591AA2 asks the network threads to end (launch.cpp)
extern volatile int g_NetThreads;    // 0x591AA4 network threads still running (launch.cpp)

// input: the screen saver starts g_ScreenSaverMinutes from now (EvoX writes this out each time)
inline void ResetScreenSaver() { g_ScreenSaverDeadline = GetTickCount() + g_ScreenSaverMinutes * 60000; }
