// Globals shared across EvoX, with their addresses in the original (.data).
// Names describe what the code does with them; "Unknown" ones are not understood yet.
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
extern bool g_Unknown591A98;         // 0x591A98
extern BYTE g_GameRegionOverride;    // 0x47BC58 region for the IGR when non-zero (else [Misc] GameRegion)
extern BYTE g_GameRegion;            // 0x5964FF region for the IGR
extern bool g_Unknown596500;         // 0x596500 copy of g_Unknown591A98 for the IGR
