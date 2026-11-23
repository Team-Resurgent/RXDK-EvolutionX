// Leaving the dashboard: choosing what to launch, the network shutdown and the launch itself
// (original addresses per function). The dashboard records the target with LaunchXbe; after Run
// returns, main calls PrepareForExit, then (with [Misc] TSR_Type) IgrInstall and IgrFinish, which
// writes the launch data and quick-reboots into the XBE.
#include <xtl.h>
#include <winsockx.h>
#include <string.h>
#include "evox/globals.h"
#include "evox/log.h"
#include "evox/pending.h"
#include "evox/socket.h"

// in the SDK's libxapi, not declared in its headers (see main.cpp)
extern "C" NTSTATUS __stdcall XWriteTitleInfoNoReboot(LPCSTR pszLaunchPath, LPCSTR pszDDrivePath,
                                                      DWORD dwLaunchDataType, DWORD dwTitleId,
                                                      PLAUNCH_DATA pLaunchData);

char g_LaunchDevice[0x410]; // 0x590338 device path of the XBE's folder
char g_LaunchXbe[0x200];    // 0x590748 XBE file name
char g_Trainer[0x828];      // 0x58FB10 trainer for the launch (SetTrainer)
bool g_HasTrainer;          // 0x590E70
CSocket *g_SntpSocket;      // 0x3D6DC8 the clock's socket (ClockTick)
volatile bool g_StopNetwork; // 0x591AA2 asks the network threads to end
volatile int g_NetThreads;  // 0x591AA4 network threads still running

// 0x232A0: "<drive>:\dir\name.xbe" -> g_LaunchDevice (device + dir), g_LaunchXbe (name). Drives
// other than C D E F G X Y Z leave g_LaunchDevice as it was. (As in the original, a path without
// '\' is not handled.)
void LaunchXbe(const char *path)
{
    char p[0x200];
    strcpy(p, path);
    const char *device = nullptr;
    switch (p[0]) {
    case 'C': case 'c': device = "\\Device\\Harddisk0\\Partition2"; break;
    case 'E': case 'e': device = "\\Device\\Harddisk0\\Partition1"; break;
    case 'F': case 'f': device = "\\Device\\Harddisk0\\Partition6"; break;
    case 'G': case 'g': device = "\\Device\\Harddisk0\\Partition7"; break;
    case 'X': case 'x': device = "\\Device\\Harddisk0\\Partition3"; break;
    case 'Y': case 'y': device = "\\Device\\Harddisk0\\Partition4"; break;
    case 'Z': case 'z': device = "\\Device\\Harddisk0\\Partition5"; break;
    case 'D': case 'd': device = "\\Device\\Cdrom0"; break;
    }
    if (device)
        strcpy(g_LaunchDevice, device);
    char *name = strrchr(p, '\\');
    strcpy(g_LaunchXbe, name + 1);
    if (name != p + 2) { // in a folder: device + "\dir"
        *name = 0;
        strcat(g_LaunchDevice, p + 2);
    }
}

// 0x23420
void SetTrainer(const char *trainer)
{
    strcpy(g_Trainer, trainer);
    g_HasTrainer = true;
}

// 0x24140
void PrepareForExit()
{
    if (g_SntpSocket) {
        if (g_SntpSocket->IsValid())
            g_SntpSocket->Close();
        delete g_SntpSocket;
        g_SntpSocket = nullptr;
    }
    Log("\n\r\n\nMain : StopNetWork!\n\r");
    g_StopNetwork = true;
    while (g_NetThreads)
        Sleep(10);
    Log("Main : WSACleanup!\n\r");
    WSACleanup();
    Log("Main : XNetCleanup!\n\r");
    XNetCleanup();
    Log("Main : Clear Controller!\n\r");
    ResetNicRegisters();
    Log("Main : Done!\n\r");
}

// 0x23450: write the launch data and quick-reboot into g_LaunchDevice\g_LaunchXbe. EvoX marks
// its launches with type 0xDEADBEEF and title id 0x1234578 (for its in-game reset); with a
// trainer the launch data holds the trainer's path and the type becomes 3.
void IgrFinish()
{
    LAUNCH_DATA data;
    memset(&data, 0, sizeof data);
    if (g_HasTrainer == true)
        strcpy((char *)&data, g_Trainer);
    XWriteTitleInfoNoReboot(g_LaunchXbe, g_LaunchDevice, 0xDEADBEEF, 0x1234578, &data);
    if (g_HasTrainer == true)
        LaunchDataPage->Header.dwLaunchDataType = 3;
    if (g_Unknown95D64 != 1) // items with flag 0x100: "device\dir\name.xbe" instead of "device\dir;name.xbe"
        if (char *p = strrchr(LaunchDataPage->Header.szLaunchPath, ';'))
            *p = '\\';
#ifdef EVOX_TEST_ACTION
    Log("test: launch '%s' type %08x title %08x\n", LaunchDataPage->Header.szLaunchPath,
        LaunchDataPage->Header.dwLaunchDataType, LaunchDataPage->Header.dwTitleId);
#endif
    HalReturnToFirmware(HalQuickRebootRoutine);
}
