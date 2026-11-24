// Functions shared across files that have no header of their own: start-up, leaving the
// dashboard, the drive and Backup.
#pragma once
#include <xtl.h>
#include <string.h>
#include "evox/chameleon.h"

// a DWORD at any alignment in file data (XBE headers, .etm trainers)
inline DWORD Read32(const BYTE *p)
{
    DWORD v;
    memcpy(&v, p, 4);
    return v;
}

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
void RelinkDrives();               // 0x23150 drive letters again after a format (hdd.cpp)
ANSI_STRING *FDriveDevice();       // F:'s device, \Device\Harddisk0\Partition6 (hdd.cpp)

// the drive (hdd.cpp)
struct AtaRegisters {              // 16 bytes, ATA_PASS_THROUGH
    BYTE features, sectorCount, lbaLow, lbaMid, lbaHigh, device, command;
    BYTE hostSendsData;            // 1: the data block goes to the drive (security), or there is none
    DWORD dataSize;                // +0x08 the data block (0x200), if any
    void *data;                    // +0x0C
};
extern BYTE *g_DriveIdentify;      // 0x58FB08 the drive's IDENTIFY data (512 bytes)
void IdentifyDrive();              // 0x236E0
bool AtaCommand(AtaRegisters *r);  // 0x23530
void SmartReadData();              // 0x237C0 into g_SmartData
void ComputeHddPassword();         // 0x235F0 into g_HddPassword, at the end of InitHardDisk
extern BYTE g_HddPassword[0x20];   // 0x590314 the drive's lock password (ComputeHddPassword)
bool HddPasswordMissing();         // 0x235D0 its first 16 bytes are all zero
bool AtaSecurityCommand(BYTE command, WORD control, const BYTE *password); // 0x23650

// the dashboard
void StartBackup();                       // 0x20460 state 5: the backup thread (backup.cpp)
