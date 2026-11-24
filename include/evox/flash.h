// Flashing the BIOS (0x1EF60, 0x21070..0x2181B and the MTRR switch at 0x5958B2 in the original).
// The "Flash BIOS" message box (CEvoxDash::DrawMessage) drives it in two steps on a thread of its
// own: identify the chip and read the file (mode 1), then erase and write it (mode 2), after which
// the console powers off. The chip types come from evox.ini:
//     [BIOS]
//     Flash = 0xMMDD,"name",0xSIZE[,0xERASE1,0xERASE2,0xPROGRAM]
// (manufacturer and device id, the size to write, and the JEDEC command bytes; 80/30/A0 if absent).
#pragma once
#include <xtl.h>

extern volatile int g_FlashMode;     // 0x57F5C8 1 identify, 2 write; 0 done, -1 failed
extern HANDLE g_FlashThread;         // 0x57F5CC
extern volatile DWORD g_FlashProgress; // 0x57F5D0 bytes done (the box's bar)
extern DWORD g_FlashSize;            // 0x95BC4 bytes to write (from the [BIOS] Flash line)
extern bool g_FlashBusy;             // 0x47BF20 writing: the box can no longer be closed
extern char g_FlashFile[0x100];      // 0x47BF88 the BIOS file
extern BYTE g_FlashManufacturer;     // 0x95BC0 (0xFF until read)
extern BYTE g_FlashDevice;           // 0x95BC1
extern WCHAR g_FlashLine1[0x400];    // 0x57E5C8 the box's two status lines
extern WCHAR g_FlashLine2[0x400];    // 0x57EDC8

void StartFlash(int mode);           // 0x217F0

// 0x1EF60: split a line in place into g_FlashArgv (as CEvoxDash::Tokenize does); used for the
// [BIOS] Flash lines and the action scripts
bool FlashTokenize(char *line);
extern int g_FlashArgc;              // 0x47C310
extern char *g_FlashArgv[0x100];     // 0x57DCB0 (unused entries point at g_EmptyArg)

// the XAPI calls a format makes (not in the SDK's headers)
extern "C" BOOL __stdcall XapiFormatFATVolume(POBJECT_STRING volume);    // 0x28A40
extern "C" NTSTATUS XapiValidateDiskPartition(POBJECT_STRING partition); // 0x27D7E
