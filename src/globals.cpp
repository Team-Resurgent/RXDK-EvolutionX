// Definitions for include/evox/globals.h (original addresses there), with the original's
// initial values from .data where they are not zero.
#include "evox/globals.h"

int g_XboxVersion;
char g_DashRoot[0x200];
char g_ImagePath[0x81];
bool g_RunningFromCd;
char g_MsDashPath[0x208];
int g_NetSetupDelay;
bool g_HasChameleon;
BYTE g_ChameleonState = 4;
bool g_IgrEnabled;
int g_TsrType;
bool g_UseItems = true;
BYTE g_IgrButtons;
WORD g_IgrButtons2;
bool g_Fahrenheit;
int g_ScreenSaverMinutes;
int g_ShadeLevel = 90;
DWORD g_ScreenSaverDeadline;
int g_ChameleonLed = 2;
bool g_NetworkStarted;
bool g_IgrOnLaunch;
BYTE g_GameRegionOverride;
BYTE g_GameRegion;
bool g_LoaderIgr;
BYTE g_IgrFlags;
BYTE g_LaunchMountsD = 1;
bool g_HasFDrive;
bool g_HasGDrive;
