// main (0x24220 in the original) and the two start-up helpers just before it.
#include <xtl.h>
#include <winsockx.h>
#include <stdlib.h>
#include <string.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/netcfg.h"
#include "evox/system.h"
#include "evox/telnet.h"

// in the SDK's libxapi, not declared in its headers (the 5455 XAPI routine had a sixth "use the
// D: path" flag after pszDDrivePath; EvoX always passed 1 with a path)
extern "C" NTSTATUS __stdcall XWriteTitleInfoNoReboot(LPCSTR pszLaunchPath, LPCSTR pszDDrivePath,
                                                      DWORD dwLaunchDataType, DWORD dwTitleId,
                                                      PLAUNCH_DATA pLaunchData);

// 0x240F0: board revision from PCI config space (bus 0, device 2, function 0, register 0x10):
// 0x8001 on 1.0 boards
static void DetectXboxVersion()
{
    DWORD id;
    __asm__ volatile("outl %0, %1" : : "a"(0x80000810u), "Nd"((WORD)0xCF8));
    __asm__ volatile("inl %1, %0" : "=a"(id) : "Nd"((WORD)0xCFC));
    g_XboxVersion = id == 0x8001 ? 10 : 11;
    Log("Main : XboxVersion %d.%d\n\r", g_XboxVersion / 10, g_XboxVersion % 10);
}

// 0x218D0: is a debug monitor (xbdm) loaded? (KPRCB +0x250, as XAPI tests it)
bool DebugMonitorPresent()
{
    BYTE *prcb;
    __asm__ volatile("movl %%fs:0x20, %0" : "=r"(prcb));
    return *(void **)(prcb + 0x250) != nullptr;
}

// 0xCEA00 / 0xCEE00: the 1 KB register block at 0xFEF00000 (the NIC's MMIO window) as written at
// start-up without a debug monitor, for 1.0 and for later boards
static const DWORD s_NicRegs10[0x100] = {
    [0x13C / 4] = 0x00300010, [0x140 / 4] = 0x00300010, [0x144 / 4] = 0x00000008,
    [0x18C / 4] = 0x00008103, [0x19C / 4] = 0x00008103, [0x1AC / 4] = 0x00008103,
    [0x1BC / 4] = 0x00008103, [0x1CC / 4] = 0x00008103, [0x1DC / 4] = 0x00008103,
    [0x1EC / 4] = 0x00008103, [0x1FC / 4] = 0x00008103, [0x268 / 4] = 0xFE020001,
    [0x278 / 4] = 0xFE020001,
};
static const DWORD s_NicRegs11[0x100] = {
    [0x13C / 4] = 0x00300010, [0x140 / 4] = 0x00300010, [0x144 / 4] = 0x00000008,
    [0x180 / 4] = 0x00000010, [0x18C / 4] = 0x00008103, [0x19C / 4] = 0x00008103,
    [0x1A0 / 4] = 0x00000010, [0x1AC / 4] = 0x00008103, [0x1BC / 4] = 0x00008103,
    [0x1C0 / 4] = 0x00000010, [0x1CC / 4] = 0x00008103, [0x1DC / 4] = 0x00008103,
    [0x1E0 / 4] = 0x00000010, [0x1EC / 4] = 0x00008103, [0x1FC / 4] = 0x00008103,
    [0x268 / 4] = 0xFE020001, [0x278 / 4] = 0xFE020001,
};

// Without a debug monitor: write the NIC register block (main at start-up, and PrepareForExit;
// the original has the loop inline in both)
void ResetNicRegisters()
{
    if (DebugMonitorPresent())
        return;
    volatile DWORD *regs = (volatile DWORD *)0xFEF00000;
    const DWORD *src = g_XboxVersion == 10 ? s_NicRegs10 : s_NicRegs11;
    for (int i = 0; i < 0x100; i++)
        regs[i] = src[i];
}

static ANSI_STRING s_DashRootString;                                  // 0x591A90
static ANSI_STRING s_QDrive = {6, 7, (PCHAR) "\\??\\Q:"};             // 0xCF200

static int IniInt(const char *section, const char *key, const char *def)
{
    return strtol(g_Ini->GetString(section, key, def), nullptr, 10);
}

void __cdecl main()
{
    Log("\n\r\n\nMain : Start!\n\r");
    DetectXboxVersion();
    Log("Main : Fill!\n\r");
    ResetNicRegisters();
    Log("Main : Done!\n\r");

    // Q: is the directory this XBE was started from
    strncpy(g_DashRoot, XeImageFileName->Buffer, XeImageFileName->Length);
    if (char *p = strrchr(g_DashRoot, '\\'))
        *p = 0;
    Log("Main : dash root %s\n\r", g_DashRoot);
    RtlInitAnsiString(&s_DashRootString, g_DashRoot);
    IoCreateSymbolicLink(&s_QDrive, &s_DashRootString);
    strncpy(g_ImagePath, XeImageFileName->Buffer, XeImageFileName->Length);

    g_Ini = new CIniFile;
#ifdef EVOX_TEST_INI // test builds: a writable e:\evox.ini in place of q:\evox.ini
    const char *iniPath = "e:\\evox.ini";
    if (false) {
#else
    const char *iniPath = "q:\\evox.ini";
    if (_stricmp(g_DashRoot, "\\Device\\CdRom0") == 0) {
#endif
        g_Ini->m_writable = false;
        g_RunningFromCd = true;
    }
    Log("Main : Load q:\\evox.ini\n\r");
    g_Ini->Load(iniPath);
    Log("Main : Init Hdd!!\n\r");
    InitHardDisk();

    // A title that asked the dashboard for settings, memory or music is handed on to the MS
    // dashboard when [Misc] MSDashBoard exists (for the clock: per [Clock] JumpToMsDash /
    // JumpIfNoLink, after setting the clock to 2002-01-01 12:00 when it is not set).
    PLAUNCH_DATA_PAGE page = LaunchDataPage;
    Log("Main : Check Launch msdash!!\n\r");
    g_Ini->Lock();
    strcpy(g_MsDashPath, "c:\\msxboxdash.xbe");
    if (char *p = strtok(g_Ini->GetString("Misc", "MSDashBoard", "c:\\msxboxdash.xbe"), "\""))
        strcpy(g_MsDashPath, p);
    LAUNCH_DATA launchData;
    if (page && page->Header.dwLaunchDataType == LDT_LAUNCH_DASHBOARD) {
        memcpy(&launchData, &page->LaunchData, sizeof launchData);
        const LD_LAUNCH_DASHBOARD *ld = (const LD_LAUNCH_DASHBOARD *)&launchData;
        bool jump = true;
        if (ld->dwReason == XLD_LAUNCH_DASHBOARD_SETTINGS && ld->dwParameter1 == XLD_SETTINGS_CLOCK &&
            page->Header.dwTitleId == 0) {
            SYSTEMTIME st = {2002, 1, 0, 1, 12, 0, 0, 0};
            FILETIME ft;
            SystemTimeToFileTime(&st, &ft);
            NtSetSystemTime((PLARGE_INTEGER)&ft, nullptr);
            if (EthernetLinkStatus())
                jump = g_Ini->GetBool("Clock", "JumpToMsDash", false);
            else
                jump = g_Ini->GetBool("Clock", "JumpIfNoLink", true);
        }
        if (ld->dwReason != XLD_LAUNCH_DASHBOARD_ERROR && ld->dwReason != XLD_LAUNCH_DASHBOARD_MAIN_MENU &&
            jump) {
            if (FILE *f = fopen(g_MsDashPath, "rb")) {
                fclose(f);
                // ORIGINAL BUG: the XBE name passed here is a stack buffer main never fills
                // (0x2452F). A fresh thread stack is zero, so it was most likely empty; the
                // ';' that separates the D: path is then turned into '\'.
                char xbeName[0x100] = {};
                XWriteTitleInfoNoReboot(xbeName, "\\Device\\Harddisk0\\Partition2", LDT_LAUNCH_DASHBOARD,
                                        page->Header.dwTitleId, &launchData);
                if (char *p = strrchr(LaunchDataPage->Header.szLaunchPath, ';'))
                    *p = '\\';
                g_Ini->Unlock();
                HalReturnToFirmware(HalQuickRebootRoutine);
            }
        }
    }
    g_Ini->Unlock();

    Log("Main : Check LaunchInfo\n");
    DWORD launchType = 0;
    if (XGetLaunchInfo(&launchType, &launchData) == ERROR_SUCCESS) {
        Log("Main : %08x\n", launchType);
        if (launchType == LDT_FROM_DEBUGGER_CMDLINE)
            Log("Main Debug : %s\n", (const char *)&launchData);
    } else {
        Log("Main : No LaunchInfo\n");
    }

    Log("Main : Check for Chameleon!!\n\r");
    if (ChameleonRead(0) == 0xAA) {
        g_HasChameleon = true;
        g_ChameleonState = (ChameleonRead(1) & 0x43) | 4;
        Log("Main : Chameleon found :o) %02x!!\n\r", g_ChameleonState);
    }

    Log("Main : Network Setup!!\n\r");
    g_Ini->Lock();
    g_NetSetupDelay = IniInt("Network", "SetupDelay", "0");
    if (g_Ini->GetBool("Network", "SetupNetwork", false)) {
        XNetLoadConfigParams(&g_NetConfig);
        if (g_Ini->GetBool("Network", "StaticIP", false)) {
            g_NetConfig.mode = 0xC;
            g_NetConfig.v1_ip = g_NetConfig.v2_ip = inet_addr(g_Ini->GetString("Network", "Ip", "192.168.0.2"));
            g_NetConfig.v1_gateway = g_NetConfig.v2_gateway =
                inet_addr(g_Ini->GetString("Network", "Defaultgateway", "192.168.0.1"));
            g_NetConfig.v1_netmask = g_NetConfig.v2_netmask =
                inet_addr(g_Ini->GetString("Network", "Subnetmask", "255.255.255.0"));
            g_NetConfig.v1_dns1 = g_NetConfig.v2_dns1 = inet_addr(g_Ini->GetString("Network", "DNS1", "0.0.0.0"));
            g_NetConfig.v1_dns2 = g_NetConfig.v2_dns2 = inet_addr(g_Ini->GetString("Network", "DNS2", "0.0.0.0"));
        } else {
            g_NetConfig.v1_ip = 0;
            g_NetConfig.v1_netmask = 0;
            g_NetConfig.v1_gateway = 0;
            g_NetConfig.v1_dns1 = 0;
            g_NetConfig.v1_dns2 = 0;
            g_NetConfig.mode = 0;
        }
        XNetSaveConfigParams(&g_NetConfig);
    }
    g_Ini->Unlock();

    g_Ini->Lock();
    g_IgrEnabled = g_Ini->GetBool("Misc", "IGR", false);
    g_TsrType = IniInt("Misc", "TSR_Type", "1");
    g_UseItems = g_Ini->GetBool("Misc", "UseItems", false);
    g_IgrButtons = 0;
    g_IgrButtons2 = 0;
    if (g_Ini->GetBool("IGR", "Start_Button", true))
        g_IgrButtons |= 0x10;
    if (g_Ini->GetBool("IGR", "Back_Button", true))
        g_IgrButtons |= 0x20;
    if (g_Ini->GetBool("IGR", "L_Trig", true))
        g_IgrButtons2 |= 0x40;
    if (g_Ini->GetBool("IGR", "R_Trig", true))
        g_IgrButtons2 |= 0x80;
    if (g_Ini->GetBool("IGR", "White_Button", false))
        g_IgrButtons2 |= 0x20;
    if (g_Ini->GetBool("IGR", "Black_Button", false))
        g_IgrButtons2 |= 0x10;
    if (g_Ini->GetBool("IGR", "A_Button", false))
        g_IgrButtons2 |= 0x01;
    if (g_Ini->GetBool("IGR", "B_Button", false))
        g_IgrButtons2 |= 0x02;
    if (g_Ini->GetBool("IGR", "X_Button", false))
        g_IgrButtons2 |= 0x04;
    if (g_Ini->GetBool("IGR", "Y_Button", false))
        g_IgrButtons2 |= 0x08;
    g_Fahrenheit = g_Ini->GetBool("Misc", "Fahrenheit", false);
    g_UseItems = g_Ini->GetBool("Misc", "UseItems", true); // read a second time, default true
    g_ScreenSaverMinutes = IniInt("Misc", "ScreenSaver", "5");
    g_ShadeLevel = IniInt("Misc", "ShadeLevel", "90");
    if ((unsigned)g_ShadeLevel > 100)
        g_ShadeLevel = 100;
    g_ScreenSaverDeadline = GetTickCount() + g_ScreenSaverMinutes * 60000;
    g_ChameleonLed = IniInt("Misc", "ChameleonLed", "15");
    ChameleonSetLed(g_ChameleonLed != 0);
    g_Ini->Unlock();

    if (!g_Ini->GetBool("Network", "SkipIfNoLink", false) || EthernetLinkStatus()) {
        Log("Main : Start the network service!!\n\r");
        XNetStartupParams xnsp = {};
        xnsp.cfgSizeOfStruct = sizeof xnsp; // the original passed its 12-byte XDK struct
        xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
        xnsp.cfgSockMaxSockets = 0xFF;
        if (g_NetSetupDelay)
            Sleep(g_NetSetupDelay);
        Log("Main : XNetStartup %d\n\r", XNetStartup(&xnsp));
        if (g_NetSetupDelay)
            Sleep(g_NetSetupDelay);
        WSADATA wsa;
        Log("Main : WSAStartup %d\n\r", WSAStartup(MAKEWORD(2, 2), &wsa));
        if (g_NetSetupDelay)
            Sleep(g_NetSetupDelay);
        g_NetworkStarted = true;
    } else {
        Log("Main : No Link\n\r");
        g_NetworkStarted = false;
    }

    {
        CEvoxDash dash;
        Log("Main : OpenX_Dash.Create\n\r");
        if (dash.Create() == S_OK) {
            Log("Main : OK\n\r");
            g_Ini->Lock();
            Log("Main : Start Telnet\n\r");
            if (g_Ini->GetBool("Telnet", "enable", true))
                new CTelnetServer;
            Log("Main : OK\n\r");
            g_Ini->Unlock();
            Log("Main : OpenX_Dash.Run\n\r");
            dash.Run();
            Log("Main : OpenX_Dash.Run - return\n\r");
            D3DDevice_PersistDisplay();
            Log("Main : PersistDisplay\n\r");
            dash.Destroy();
            Log("Main : Dash.Destroy\n\r");
            if (g_HasChameleon)
                ChameleonSetLed(0);
        }
        if (dash.m_InstallIgr) {
            PrepareForExit();
            Log("Main : Done\n\r");
            Log("Main : Check InsIGR\n\r");
            if (g_TsrType) {
                g_Unknown596500 = g_Unknown591A98;
                Log("Main : InsIGR\n\r");
                g_GameRegion = g_GameRegionOverride ? g_GameRegionOverride : (BYTE)IniInt("Misc", "GameRegion", "0");
                IgrInstall();
            }
            IgrFinish();
            HalReturnToFirmware(HalHaltRoutine);
        }
    }
    HalReturnToFirmware(HalHaltRoutine);
}
