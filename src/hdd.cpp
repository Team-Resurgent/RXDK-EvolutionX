// Drive set-up (InitHardDisk, 0x23820 in the original) and the raw partition check it uses.
#include <xtl.h>
#include <stdlib.h>
#include <string.h>
#include "evox/globals.h"
#include "evox/guard.h"
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/system.h"

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
alignas(16) BYTE g_SmartData[0x200]; // 0x590548 SMART attributes
// 0x590C50. Raw disk reads need an aligned buffer (an odd address fails with
// STATUS_INVALID_PARAMETER); the original's was aligned by where it happened to land.
alignas(16) static BYTE g_Sector[0x200];
BYTE g_HddPassword[0x20];                       // 0x590314
alignas(16) static BYTE g_IdentifyBuffer[0x200]; // 0x590A48
BYTE *g_DriveIdentify = g_IdentifyBuffer;        // 0x58FB08 (null in the original until IdentifyDrive)
bool g_DriveIdentified;                          // 0x590E71 IDENTIFY succeeded (not read anywhere)
bool g_DriveHasSecurity;                         // 0x590E50 the drive has the security feature set (not read)

// 0x23530: an ATA command through IOCTL_IDE_PASS_THROUGH on the whole disk. The registers are
// sent and read back in place; the result is whether the disk opened, not the command's status.
bool AtaCommand(AtaRegisters *r)
{
    ANSI_STRING name = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
    OBJECT_ATTRIBUTES oa = {nullptr, &name, OBJ_CASE_INSENSITIVE};
    IO_STATUS_BLOCK iosb;
    HANDLE h = INVALID_HANDLE_VALUE;
    NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,
               FILE_SYNCHRONOUS_IO_ALERT);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    NtDeviceIoControlFile(h, nullptr, nullptr, nullptr, &iosb, 0x4D028, r, sizeof *r, r, sizeof *r);
    NtClose(h);
    return true;
}

// 0x236E0: IDENTIFY DEVICE into g_DriveIdentify, and the settings screen's Security line: "N/A"
// without the security feature set (word 82 bit 1, word 128 bit 0), else "Lock Enabled" or
// "Lock Disabled", or "Unlock Error" while the drive is locked (word 128 bit 2)
void IdentifyDrive()
{
    g_DriveIdentify = g_IdentifyBuffer;
    memset(g_IdentifyBuffer, 0, sizeof g_IdentifyBuffer);
    AtaRegisters r = {};
    r.command = 0xEC;
    r.dataSize = 0x200;
    r.data = g_IdentifyBuffer;
    if (!AtaCommand(&r))
        return;
    g_DriveIdentified = true;
    const BYTE *id = g_DriveIdentify;
    if (!(id[0xA4] & 2) || !(id[0x100] & 1)) {
        wcscpy(g_InfoSecurity, L"N/A");
        return;
    }
    g_DriveHasSecurity = true;
    wcscpy(g_InfoSecurity, (id[0x100] & 2) ? L"Lock Enabled" : L"Lock Disabled");
    if (id[0x100] & 4)
        wcscpy(g_InfoSecurity, L"Unlock Error");
}

// 0x237C0: SMART READ DATA into g_SmartData; SMART is taken as off if the disk did not open
void SmartReadData()
{
    memset(g_SmartData, 0, sizeof g_SmartData);
    AtaRegisters r = {};
    r.features = 0xD0;
    r.lbaMid = 0x4F;
    r.lbaHigh = 0xC2;
    r.command = 0xB0;
    r.dataSize = 0x200;
    r.data = g_SmartData;
    if (!AtaCommand(&r))
        g_SmartOn = false;
}

// 0x235F0: the drive's lock password, HMAC-SHA1(XboxHDKey, model number + serial number)
void ComputeHddPassword()
{
    memset(g_HddPassword, 0, sizeof g_HddPassword);
    XcHMAC(XboxHDKey, 0x10, (PUCHAR)HalDiskModelNumber.Buffer, HalDiskModelNumber.Length,
           (PUCHAR)HalDiskSerialNumber.Buffer, HalDiskSerialNumber.Length, g_HddPassword);
}

// 0x235D0
bool HddPasswordMissing()
{
    for (int i = 0; i < 0x10; i++)
        if (g_HddPassword[i])
            return false;
    return true;
}

// 0x23650: an ATA security command (0xF1 SET PASSWORD, 0xF2 UNLOCK, 0xF6 DISABLE PASSWORD) with
// its 512-byte data block: the control word (0 user, 1 master password) and the 32-byte password.
// The IDENTIFY data is cleared first (the callers read it again). (The original sends a stack
// buffer with only those first 34 bytes set; the rest is zero here.)
bool AtaSecurityCommand(BYTE command, WORD control, const BYTE *password)
{
    g_DriveIdentify = g_IdentifyBuffer;
    memset(g_IdentifyBuffer, 0, sizeof g_IdentifyBuffer);
    alignas(16) BYTE data[0x200] = {};
    memcpy(data, &control, 2);
    memcpy(data + 2, password, 0x20);
    AtaRegisters r = {};
    r.command = command;
    r.hostSendsData = 1;
    r.dataSize = 0x200;
    r.data = data;
    return AtaCommand(&r);
}

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

// 0x23150: after a format, link each drive letter whose link status is still 0 (never linked, or
// linked successfully: a second link then fails and stores its error, as in the original)
void RelinkDrives()
{
    if (g_LinkStatusE == 0 && PartitionIsFatx(0x55F400))
        g_LinkStatusE = Link(s_DriveE);
    if (g_LinkStatusC == 0 && PartitionIsFatx(0x465400))
        g_LinkStatusC = Link(s_DriveC);
    if (g_LinkStatusF == 0 && PartitionIsFatx(0xEE8AB0) && g_HasFDrive)
        g_LinkStatusF = Link(s_DriveF);
    if (g_LinkStatusG == 0 && g_HasGDrive)
        g_LinkStatusG = Link(s_DriveG);
    if (g_LinkStatusX == 0 && PartitionIsFatx(0x400))
        g_LinkStatusX = Link(s_DriveX);
    if (g_LinkStatusY == 0 && PartitionIsFatx(0x177400))
        g_LinkStatusY = Link(s_DriveY);
    if (g_LinkStatusZ == 0 && PartitionIsFatx(0x2EE400))
        g_LinkStatusZ = Link(s_DriveZ);
}

// F:'s device (0x95D0C), which the FTP server's LIST of "/" validates
ANSI_STRING *FDriveDevice() { return &s_DriveF.device; }

// 0x23820: drive letters (D: the DVD drive; C: and E: when their partitions are FATX, F: with
// [MISC] UseFDrive, G: with UseGDrive, X: Y: Z: the cache partitions), the file cache, and SMART
void InitHardDisk()
{
    // catch (...) 0x23ABF: release the lock (the Unlock after the block serves both ways)
    CrashGuard([&] {
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
    });
    g_Ini->Unlock();

    // catch (...) 0x23AD0: release the lock (likewise)
    CrashGuard([&] {
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
                r.hostSendsData = 1;
                AtaCommand(&r);
                IdentifyDrive();
                id = g_DriveIdentify;
            }
            if (id[0xAA] & 1) {
                g_SmartOn = true;
                SmartReadData();
            }
        }
        ComputeHddPassword();
    });
    g_Ini->Unlock();
}

// 0x234F0: the raw value of the SMART attribute [MISC] HDD_Temp_ID (12-byte entries from offset
// 2); if there is none, SMART is taken as off
int SmartTemperature()
{
    for (int i = 2; i < 0x160; i += 12)
        if (g_SmartData[i] && g_SmartData[i] == g_HddTempId)
            return g_SmartData[i + 5];
    g_SmartOn = false;
    return -1;
}
