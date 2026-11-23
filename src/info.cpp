// The live information (original addresses per function): InfoTick and its helpers (free space,
// BIOS version, the temperature thread), the DVD tray and its disc check, and the memory units.
// See include/evox/info.h.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <winsockx.h>
#include "evox/cdaudio.h"
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/md5.h"
#include "evox/netcfg.h"
#include "evox/pending.h"
#include "evox/skinvars.h"

WCHAR g_InfoNetType[0x40];
WCHAR g_InfoLineStatus[0x40];
WCHAR g_InfoMac[0x40];
WCHAR g_InfoSecurity[0x40];
WCHAR g_InfoHardware[0x40];
WCHAR g_InfoChameleonRaw[0x20];
WCHAR g_InfoChameleonSw1[0x20];
WCHAR g_InfoChameleonSw2[0x20];
WCHAR g_InfoChameleonMode[0x20];
WCHAR g_InfoChameleonBank[0x20];
WCHAR g_ProgressText[0x200];
char g_BiosMd5Text[0x48];
char g_MuDrive[8];

static bool g_InfoFirstRun = true;      // 0x95C28
static int g_SensorCountdown = 1;       // 0x95C2C frames until the temperature thread runs again
static volatile bool g_SensorBusy;      // 0x57E5B4 the temperature thread is running
static bool g_Use24 = true;             // 0x95B1B [Clock] Use24
static bool g_SwapDate;                 // 0x57E5B2 [Clock] SwapDate
static bool g_SafeX;                    // 0x57E5B3 (never set: the name is always EvolutionX)
static const int g_BuildNumber = 3935;  // 0x595963 $Version$
static WCHAR g_FreeSpace[0x40];         // 0x57F5E0 FreeSpaceText's result
static BYTE g_BiosMd5[16];              // 0x47C088
static char g_BiosName[0x100];          // 0x47C1E8 (0x28 bytes in the original)
static ULONG g_TrayCount = 0xFFFFFFFF;  // 0x95C30 the tray's change count last seen
static HANDLE g_DiscThread;             // 0x57DCA8
static BYTE g_DiscXbe[0x200];           // 0x57F7E0 the disc's XBE header, then its certificate

// 0x1F200: the free bytes in groups of three digits ("4.598.104.064"); from a terabyte on only
// the last 12 digits show (as in the original).
const WCHAR *FreeSpaceText(const char *root)
{
    ULARGE_INTEGER free;
    free.QuadPart = 0;
    GetDiskFreeSpaceEx(root, nullptr, nullptr, &free);
    ULONGLONG n = free.QuadPart;
    int units = (int)(n % 1000);
    n /= 1000;
    int thousands = (int)(n % 1000);
    n /= 1000;
    int millions = (int)(n % 1000);
    n /= 1000;
    int billions = (int)(n % 1000);
    if (billions)
        swprintf(g_FreeSpace, 0x40, L"%d.%03d.%03d.%03d", billions, millions, thousands, units);
    else if (millions)
        swprintf(g_FreeSpace, 0x40, L"%d.%03d.%03d", millions, thousands, units);
    else if (thousands)
        swprintf(g_FreeSpace, 0x40, L"%d.%03d", thousands, units);
    else
        swprintf(g_FreeSpace, 0x40, L"%d", units);
    return g_FreeSpace;
}

// 0x1F020: the MD5 of the flash's first 256 KB, looked up in evox.ini [BIOS] ROM = "<name>",
// "<md5>" lines; saved as [BIOS] Current.
void ReadBiosVersion()
{
    bool found = false;
    MD5_CTX ctx;
    MD5Init(&ctx);
    BYTE *flash = (BYTE *)MmMapIoSpace(0xFF000000, 0x100000, PAGE_READWRITE | PAGE_NOCACHE);
    MD5Update(&ctx, flash, 0x40000);
    MmUnmapIoSpace(flash, 0x100000);
    MD5Final(g_BiosMd5, &ctx);
    const BYTE *m = g_BiosMd5;
    sprintf(g_BiosMd5Text, "0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2],
            m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    g_Ini->Lock();
    for (char *v = g_Ini->GetValue("BIOS", "ROM"); v; v = g_Ini->GetNextValue()) {
        char *name = strtok(v, "\",\n\r");
        if (!name)
            continue;
        strcpy(g_BiosName, name);
        char *md5 = strtok(nullptr, "\",\n\r");
        if (md5 && _stricmp(md5, g_BiosMd5Text) == 0) {
            found = true;
            break;
        }
    }
    g_Ini->SetValue("BIOS", "Current", g_BiosMd5Text);
    g_Ini->Save();
    g_Ini->Unlock();
    if (!found)
        strcpy(g_BiosName, "Unknown");
    swprintf(g_VarBiosVer, 0x40, L"%hs", g_BiosName);
}

static void TemperatureText(WCHAR *out, double celsius)
{
    if (g_Fahrenheit == 1)
        swprintf(out, 0x20, L"%.1f F", celsius * 1.8 + 32.0f);
    else
        swprintf(out, 0x20, L"%.1f C", celsius);
}

// 0x1F450: $Temp1$ and $Temp2$ from the SMBus temperature sensor (0x98: the board, and the CPU
// with eighths of a degree), $Temp3$ from the drive's SMART data
static DWORD WINAPI TemperatureThread(LPVOID)
{
    ULONG v;
    g_SensorBusy = true;
    HalReadSMBusValue(0x99, 8, FALSE, &v);
    v = 0;
    HalReadSMBusValue(0x99, 0, FALSE, &v);
    TemperatureText(g_VarTemp1, (double)v);
    v = 0;
    HalReadSMBusValue(0x99, 1, FALSE, &v);
    float cpu = (float)v;
    v = 0;
    HalReadSMBusValue(0x99, 0x10, FALSE, &v);
    TemperatureText(g_VarTemp2, (double)(v >> 5) * 0.125 + cpu);
    int hdd;
    if (g_SmartOn) {
        SmartEnabled237C0();
        if ((hdd = SmartTemperature()) != -1) {
            TemperatureText(g_VarTemp3, (double)hdd);
            g_SensorBusy = false;
            return 0;
        }
    }
    swprintf(g_VarTemp3, 0x20, L"N/A");
    g_SensorBusy = false;
    return 0;
}

// the Chameleon's bank and mode texts (part of InfoTick)
static void ChameleonInfo()
{
    g_InfoChameleonMode[0] = 0;
    g_InfoChameleonBank[0] = 0;
    g_InfoChameleonSw2[0] = 0;
    g_InfoChameleonSw1[0] = 0;
    if (ChameleonRead(0) != 0xAA)
        return;
    int raw = ChameleonRead(1);
    swprintf(g_InfoChameleonRaw, 0x20, L"%02x : ", raw);
    for (int bit = 0x80; bit; bit >>= 1)
        wcscat(g_InfoChameleonRaw, raw & bit ? L"1" : L"0");
    wcscat(g_InfoChameleonSw1, raw & 0x04 ? L"Off  " : L"On  ");
    wcscat(g_InfoChameleonSw1, raw & 0x08 ? L"Off  " : L"On  ");
    wcscat(g_InfoChameleonSw2, raw & 0x10 ? L"Off  " : L"On  ");
    wcscat(g_InfoChameleonSw2, raw & 0x20 ? L"Off  " : L"On  ");
    switch ((raw >> 2) & 3) {
    case 0: {
        wcscat(g_InfoChameleonMode, L"1 : 4x256");
        static const WCHAR *const banks[4] = {L"0 : 00000 - 3FFFF", L"1 : 40000 - 7FFFF", L"2 : 80000 - BFFFF",
                                              L"3 : c0000 - FFFFF"};
        wcscat(g_InfoChameleonBank, banks[((raw & 0x40) ? raw : raw >> 4) & 3]);
        break;
    }
    case 1:
        wcscat(g_InfoChameleonMode, L"3 : 1x1024");
        wcscat(g_InfoChameleonBank, L"0 : 00000 - FFFFF");
        break;
    case 2:
        wcscat(g_InfoChameleonMode, L"2 : 2x512");
        wcscat(g_InfoChameleonBank,
               (((raw & 0x40) ? raw >> 1 : raw >> 5) & 1) ? L"1 : 80000 - FFFFF" : L"0 : 00000 - 7FFFF");
        break;
    case 3:
        if (raw & 0x40)
            wcscat(g_InfoChameleonBank, (raw & 3) ? L"2 : 80000 - FFFFF" : L"0 : 00000 - 3FFFF");
        else
            wcscat(g_InfoChameleonBank, (raw & 3) ? L"0 : 00000 - 3FFFF" : L"1 : 40000 - 7FFFF");
        wcscat(g_InfoChameleonMode, L"4 : 2x256 1x512");
        break;
    }
}

// 0x1F610: after each frame (and from InitItems)
void InfoTick()
{
    ChameleonInfo();

    // the temperatures, every 1000 frames on a thread of their own
    if (--g_SensorCountdown == 0) {
        g_SensorCountdown = 1000;
        if (!g_SensorBusy)
            CloseHandle(CreateThread(nullptr, 0, TemperatureThread, nullptr, 0, nullptr));
    }

    wcscpy(g_VarSpaceC, FreeSpaceText("c:\\"));
    wcscpy(g_VarSpaceE, FreeSpaceText("e:\\"));
    if (g_HasFDrive)
        wcscpy(g_VarSpaceF, FreeSpaceText("f:\\"));
    if (g_HasGDrive)
        wcscpy(g_VarSpaceG, FreeSpaceText("g:\\"));
    wcscpy(g_VarSpaceX, FreeSpaceText("x:\\"));
    wcscpy(g_VarSpaceY, FreeSpaceText("y:\\"));
    wcscpy(g_VarSpaceZ, FreeSpaceText("z:\\"));

    // the network
    XNADDR xna;
    DWORD link = EthernetLinkStatus();
    if (link == 0) {
        wcscpy(g_InfoNetType, L"");
        wcscpy(g_VarIP, L"");
        wcscpy(g_InfoLineStatus, L"No Link!");
        XNetGetTitleXnAddr(&xna);
    } else {
        DWORD flags = XNetGetTitleXnAddr(&xna);
        const BYTE *ip = (const BYTE *)&xna.ina;
        swprintf(g_VarIP, 0x20, L"%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        swprintf(g_InfoMac, 0x40, L"%02X:%02X:%02X:%02X:%02X:%02X", xna.abEnet[0], xna.abEnet[1], xna.abEnet[2],
                 xna.abEnet[3], xna.abEnet[4], xna.abEnet[5]);
        if (flags & XNET_GET_XNADDR_DHCP)
            swprintf(g_InfoNetType, 0x40, L"DHCP");
        if (flags & XNET_GET_XNADDR_PPPOE)
            swprintf(g_InfoNetType, 0x40, L"PPPOE");
        if (flags & XNET_GET_XNADDR_STATIC)
            swprintf(g_InfoNetType, 0x40, L"Static");
        int n = swprintf(g_InfoLineStatus, 0x40, (link & XNET_ETHERNET_LINK_100MBPS) ? L"100/" : L"10/");
        swprintf(g_InfoLineStatus + n, 0x40 - n,
                 (link & XNET_ETHERNET_LINK_FULL_DUPLEX) ? L"Full Duplex" : L"Half Duplex");
        goto once;
    }
    swprintf(g_InfoMac, 0x40, L"%02X:%02X:%02X:%02X:%02X:%02X", xna.abEnet[0], xna.abEnet[1], xna.abEnet[2],
             xna.abEnet[3], xna.abEnet[4], xna.abEnet[5]);

once:
    if (g_InfoFirstRun) {
        swprintf(g_VarKernelVer, 0x40, L"%d.%02d.%04d.%02d", XboxKrnlVersion.Major, XboxKrnlVersion.Minor,
                 XboxKrnlVersion.Build, XboxKrnlVersion.Qfe);
        swprintf(g_InfoHardware, 0x40, L"%08X %02X %02X", XboxHardwareInfo.Flags, XboxHardwareInfo.GpuRevision,
                 XboxHardwareInfo.McpRevision);
        ReadBiosVersion();
        swprintf(g_ProgressText, 0x200, L"");
        g_Ini->Lock();
        swprintf(g_VarRdName, 0x40, L"%hs", g_Ini->GetString("RDTOOLS", "Name", "EvoX"));
        g_Use24 = g_Ini->GetBool("CLOCK", "Use24", true);
        g_SwapDate = g_Ini->GetBool("CLOCK", "SwapDate", false);
        g_Ini->Unlock();
        wcscpy(g_VarName, L"EvolutionX");
        if (g_SafeX)
            wcscpy(g_VarName, L"SafeX");
        swprintf(g_VarVersion, 0x40, L"+%03d", g_BuildNumber);
    }
    g_InfoFirstRun = false;

    // $Time$: "13:05:09- 23/11-2026" ("1:05:09pm- ..." without Use24; month first with SwapDate)
    SYSTEMTIME st;
    GetLocalTime(&st);
    int n;
    if (g_Use24) {
        n = swprintf(g_VarTime, 0x40, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    } else {
        const WCHAR *ampm;
        if (st.wHour == 0) {
            ampm = L"am";
            st.wHour = 12;
        } else if (st.wHour <= 11) {
            ampm = L"am";
        } else if (st.wHour == 12) {
            ampm = L"pm";
        } else {
            ampm = L"pm";
            st.wHour -= 12;
        }
        n = swprintf(g_VarTime, 0x40, L"%d:%02d:%02d%ls", st.wHour, st.wMinute, st.wSecond, ampm);
    }
    if (g_SwapDate)
        swprintf(g_VarTime + n, 0x40 - n, L"- %d/%d-%d", st.wMonth, st.wDay, st.wYear);
    else
        swprintf(g_VarTime + n, 0x40 - n, L"- %d/%d-%d", st.wDay, st.wMonth, st.wYear);
}

// 0x1FCC0: what is in the drive, by its files: an audio CD (tracks or track01.cda), a video DVD,
// an EvoX disc or a game (by its default.xbe's title id), else unknown
static DWORD WINAPI DiscCheckThread(LPVOID)
{
    CloseHandle(g_DiscThread);
    ANSI_STRING cd = {11, 12, (PCHAR) "\\??\\CdRom0:"};
    IoDismountVolumeByName(&cd);
    g_CdAudio.Open();
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (g_CdAudio.m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_CdAudio.m_handle);
    } else if (!GetFileAttributesEx("CDROM0:\\track01.cda", GetFileExInfoStandard, &fa)) {
        if (GetFileAttributesEx("CDROM0:\\video_ts\\video_ts.ifo", GetFileExInfoStandard, &fa)) {
            wcscpy(g_VarCD, L"Video");
            g_DiscType = 0;
            return 0;
        }
        if (!GetFileAttributesEx("CDROM0:\\default.xbe", GetFileExInfoStandard, &fa)) {
            wcscpy(g_VarCD, L"Unknown");
            g_DiscType = 3;
            return 0;
        }
        // (the original uses fopen / fread / fseek; the SDK's fopen only takes drive-letter
        // paths, so the file is read with CreateFile here, which takes "CDROM0:")
        HANDLE f = CreateFile("CDROM0:\\default.xbe", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD got;
            ReadFile(f, g_DiscXbe, 0x200, &got, nullptr);
            DWORD cert, base;
            memcpy(&cert, g_DiscXbe + 0x118, 4);
            memcpy(&base, g_DiscXbe + 0x104, 4);
            SetFilePointer(f, cert - base, nullptr, FILE_BEGIN);
            ReadFile(f, g_DiscXbe, 0x200, &got, nullptr);
            CloseHandle(f);
            DWORD titleId;
            memcpy(&titleId, g_DiscXbe + 8, 4);
            if (titleId == 0x586F7645) { // EvoX's own
                wcscpy(g_VarCD, L"EvoX CD!");
                g_DiscType = 4;
                return 0;
            }
        }
        wcscpy(g_VarCD, L"Game");
        g_DiscType = 2;
        return 0;
    }
    wcscpy(g_VarCD, L"Audio");
    g_DiscType = 1;
    return 0;
}

// 0x1FE70: on each change of the tray state
void TrayTick()
{
    ULONG state, count;
    HalReadSMCTrayState(&state, &count);
    if (g_TrayCount == count)
        return;
    g_TrayCount = count;
    wcscpy(g_VarCD, L"Unknown");
    switch (state) {
    case SMC_TRAY_STATE_CLOSED:
        wcscpy(g_VarCD, L"Init");
        break;
    case SMC_TRAY_STATE_OPEN:
        wcscpy(g_VarCD, L"Open");
        break;
    case SMC_TRAY_STATE_UNLOADING:
        wcscpy(g_VarCD, L"Ejecting");
        g_DiscType = 3;
        break;
    case SMC_TRAY_STATE_NO_MEDIA:
        wcscpy(g_VarCD, L"No Disc");
        g_DiscType = 3;
        break;
    case SMC_TRAY_STATE_MEDIA_DETECT:
        wcscpy(g_VarCD, L"Checking");
        g_DiscThread = CreateThread(nullptr, 0, DiscCheckThread, nullptr, 0, nullptr);
        break;
    }
}

// 0x21820: memory units in and out (ports 0..3, top and bottom slots)
void MuTick()
{
    static const DWORD masks[8] = {XDEVICE_PORT0_TOP_MASK, XDEVICE_PORT0_BOTTOM_MASK, XDEVICE_PORT1_TOP_MASK,
                                   XDEVICE_PORT1_BOTTOM_MASK, XDEVICE_PORT2_TOP_MASK, XDEVICE_PORT2_BOTTOM_MASK,
                                   XDEVICE_PORT3_TOP_MASK, XDEVICE_PORT3_BOTTOM_MASK}; // 0x95BC8
    DWORD insertions, removals;
    if (!XGetDeviceChanges(XDEVICE_TYPE_MEMORY_UNIT, &insertions, &removals))
        return;
    for (int i = 0; i < 8; i++) {
        DWORD port = i / 2, slot = i % 2; // 0x95BE8, 0x95C08
        if (insertions & masks[i])
            XMountMU(port, slot, &g_MuDrive[i]);
        if (removals & masks[i]) {
            XUnmountMU(port, slot);
            g_MuDrive[i] = 0;
        }
    }
}
