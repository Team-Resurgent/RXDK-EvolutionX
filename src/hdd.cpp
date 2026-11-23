// Drive set-up (InitHardDisk, 0x23820 in the original) and the raw partition check it uses.
#include <xtl.h>
#include <stdlib.h>
#include <string.h>
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/pending.h"

// a drive letter and its device
struct DriveLink {
    ANSI_STRING letter, device;
};
#define DRIVE(l, d) {{6, 7, (PCHAR)l}, {sizeof(d) - 1, sizeof(d), (PCHAR)d}}
static DriveLink s_DriveD = DRIVE("\\??\\D:", "\\Device\\CdRom0");                // 0x95CE4
static DriveLink s_DriveC = DRIVE("\\??\\C:", "\\Device\\Harddisk0\\Partition2"); // 0x95CD4
static DriveLink s_DriveE = DRIVE("\\??\\E:", "\\Device\\Harddisk0\\Partition1"); // 0x95CF4
static DriveLink s_DriveF = DRIVE("\\??\\F:", "\\Device\\Harddisk0\\Partition6"); // 0x95D04
static DriveLink s_DriveG = DRIVE("\\??\\G:", "\\Device\\Harddisk0\\Partition7"); // 0x95D14
static DriveLink s_DriveX = DRIVE("\\??\\X:", "\\Device\\Harddisk0\\Partition3"); // 0x95D24
static DriveLink s_DriveY = DRIVE("\\??\\Y:", "\\Device\\Harddisk0\\Partition4"); // 0x95D34
static DriveLink s_DriveZ = DRIVE("\\??\\Z:", "\\Device\\Harddisk0\\Partition5"); // 0x95D44
#undef DRIVE

// IoCreateSymbolicLink results (0x590E54..0x590E6C)
NTSTATUS g_LinkStatusC, g_LinkStatusE, g_LinkStatusF, g_LinkStatusG, g_LinkStatusX, g_LinkStatusY, g_LinkStatusZ;
bool g_EnableSmart;   // 0x590539 [MISC] EnableSMART
BYTE g_HddTempId;     // 0x95CD0 [MISC] HDD_Temp_ID: the SMART attribute with the temperature
bool g_SmartOn;       // 0x58FB00 SMART is enabled on the drive
// 0x590C50. Raw disk reads need an aligned buffer (an odd address fails with
// STATUS_INVALID_PARAMETER); the original's was aligned by where it happened to land.
alignas(16) static BYTE g_Sector[0x200];

static NTSTATUS Link(DriveLink &d) { return IoCreateSymbolicLink(&d.letter, &d.device); }

// 0x23060: one sector of the whole disk (partition0)
bool ReadDiskSector(DWORD sector, void *buf)
{
    ANSI_STRING name = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
    OBJECT_ATTRIBUTES oa = {nullptr, &name, OBJ_CASE_INSENSITIVE};
    IO_STATUS_BLOCK iosb;
    HANDLE h = INVALID_HANDLE_VALUE; // (not initialised in the original; NtOpenFile sets it on success)
    NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,
               FILE_SYNCHRONOUS_IO_ALERT);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)(int)sector * 0x200;
    SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
    DWORD read;
    bool ok = ReadFile(h, buf, 0x200, &read, nullptr) != FALSE;
    CloseHandle(h);
    return ok;
}

// 0x23110: does a FATX partition start at this sector?
static bool PartitionIsFatx(DWORD sector)
{
    memset(g_Sector, 0, sizeof g_Sector);
    ReadDiskSector(sector, g_Sector);
    return memcmp(g_Sector, "FATX", 4) == 0;
}

// 0x23820: drive letters (D: the DVD drive; C: and E: when their partitions are FATX, F: with
// [MISC] UseFDrive, G: with UseGDrive, X: Y: Z: the cache partitions), the file cache, and SMART
void InitHardDisk()
{
    bool both = true;
    g_Ini->Lock();
    g_HasFDrive = g_Ini->GetBool("MISC", "UseFDrive", true);
    g_HasGDrive = g_Ini->GetBool("MISC", "UseGDrive", true);
    IoDeleteSymbolicLink(&s_DriveD.letter);
    Link(s_DriveD);
    if (PartitionIsFatx(0x55F400))
        g_LinkStatusE = Link(s_DriveE);
    else
        both = false;
    if (PartitionIsFatx(0x465400))
        g_LinkStatusC = Link(s_DriveC);
    else
        both = false;
    if (PartitionIsFatx(0xEE8AB0) && g_HasFDrive)
        g_LinkStatusF = Link(s_DriveF);
    if (g_HasGDrive)
        g_LinkStatusG = Link(s_DriveG);
    if (PartitionIsFatx(0x400))
        g_LinkStatusX = Link(s_DriveX);
    if (PartitionIsFatx(0x177400))
        g_LinkStatusY = Link(s_DriveY);
    if (PartitionIsFatx(0x2EE400))
        g_LinkStatusZ = Link(s_DriveZ);
    if (both)
        XSetFileCacheSize(0x400000);
#ifdef EVOX_TEST_SCRIPT
    Log("test: E: fatx %d link %08x, C: fatx %d link %08x\n", PartitionIsFatx(0x55F400), g_LinkStatusE,
        PartitionIsFatx(0x465400), g_LinkStatusC);
#endif
    g_Ini->Unlock();

    g_Ini->Lock();
    IdentifyDrive();
    g_EnableSmart = g_Ini->GetBool("MISC", "EnableSMART", false);
    g_HddTempId = (BYTE)strtol(g_Ini->GetString("MISC", "HDD_Temp_ID", "194"), nullptr, 10);
    g_SmartOn = false;
    if (g_EnableSmart) {
        const BYTE *id = g_DriveIdentify;
        if ((id[0xA4] & 1) && !(id[0xAA] & 1)) { // supported (word 82) but off (word 85): SMART ENABLE
            AtaRegisters r = {};
            r.features = 0xD8;
            r.lbaMid = 0x4F;
            r.lbaHigh = 0xC2;
            r.command = 0xB0;
            r.unk07 = 1;
            AtaCommand(&r);
            IdentifyDrive();
            id = g_DriveIdentify;
        }
        if (id[0xAA] & 1) {
            g_SmartOn = true;
            SmartEnabled237C0();
        }
    }
    Func235F0();
    g_Ini->Unlock();
}
