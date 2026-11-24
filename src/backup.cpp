// Backup (menu action 11; 0x1FFC0..0x20482 and the screen at 0x18D10 in the original): the BIOS
// flash, the start of the disk, the EEPROM and the drive's lock key, copied to <drive>:\backup.
// It only reads the hardware; the files are written to Q: (C: when running from the DVD).
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/log.h"
#include "evox/system.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

static HANDLE g_BackupThread; // 0x57E5B8 while the backup runs

// "<prefix><byte><sep>..." with the last separator replaced by end (the original sprintfs each
// part at the end of the buffer, then overwrites the last character)
static int HexLine(char *out, const char *prefix, const BYTE *bytes, int count, const char *fmt, const char *end,
                   bool groups)
{
    int n = sprintf(out, "%s", prefix);
    for (int i = 0; i < count; i++) {
        n += sprintf(out + n, fmt, bytes[i]);
        if (groups && (i & 7) == 7 && i != count - 1) { // "HDD Key": 8 bytes per line
            n--;
            n += sprintf(out + n, "\n                     ");
        }
    }
    return sprintf(out + n - 1, "%s", end) + n - 1;
}

// 0x1FFC0
static DWORD WINAPI BackupThread(LPVOID)
{
    char drive[2] = {g_RunningFromCd ? 'c' : 'q', 0};
    char path[0x100];
    sprintf(path, "%s:\\backup", drive);
    CreateDirectory(path, nullptr);

    // bios.bin: 1 MB of the flash
    sprintf(path, "%s:\\backup\\bios.bin", drive);
    if (FILE *f = fopen(path, "wb")) {
        BYTE *buf = (BYTE *)VirtualAlloc(nullptr, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        void *flash = MmMapIoSpace(0xFFC00000, 0x100000, PAGE_READWRITE | PAGE_NOCACHE);
        memcpy(buf, flash, 0x100000);
        fwrite(buf, 1, 0x100000, f);
        fclose(f);
        VirtualFree(buf, 0, MEM_RELEASE);
        MmUnmapIoSpace(flash, 0xFFFFE); // (sic)
    }

    // disk.bin: the first 512 KB of the disk (the config area before the partitions)
    sprintf(path, "%s:\\backup\\disk.bin", drive);
    if (FILE *f = fopen(path, "wb")) {
        BYTE *buf = (BYTE *)VirtualAlloc(nullptr, 0x80000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        ANSI_STRING name = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
        OBJECT_ATTRIBUTES oa = {nullptr, &name, OBJ_CASE_INSENSITIVE};
        IO_STATUS_BLOCK iosb;
        HANDLE h = INVALID_HANDLE_VALUE;
        NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   FILE_SYNCHRONOUS_IO_ALERT);
        if (h != INVALID_HANDLE_VALUE) {
            LONG high = 0;
            SetFilePointer(h, 0, &high, FILE_BEGIN);
            DWORD read;
            if (ReadFile(h, buf, 0x80000, &read, nullptr))
                fwrite(buf, 1, 0x80000, f);
            CloseHandle(h);
        }
        fclose(f);
        VirtualFree(buf, 0, MEM_RELEASE);
    }

    // hddkey.bin: the drive's lock password, HMAC-SHA1(XboxHDKey, model number + serial number)
    BYTE key[0x20] = {};
    XcHMAC(XboxHDKey, 0x10, (PUCHAR)HalDiskModelNumber.Buffer, HalDiskModelNumber.Length,
           (PUCHAR)HalDiskSerialNumber.Buffer, HalDiskSerialNumber.Length, key);
    sprintf(path, "%s:\\backup\\hddkey.bin", drive);
    if (FILE *f = fopen(path, "wb")) {
        fwrite(key, 1, 0x20, f);
        fclose(f);
    }

    // hddinfo.txt
    sprintf(path, "%s:\\backup\\hddinfo.txt", drive);
    if (FILE *f = fopen(path, "wt")) {
        char line[0x400];
        fprintf(f, "Disk Serial Number : \"%s\"\n", HalDiskSerialNumber.Buffer);
        fprintf(f, "Disk Model Number  : \"%s\"\n", HalDiskModelNumber.Buffer);
        HexLine(line, "HDD Key            : ", key, 0x20, "%02x:", "\n\n", true);
        fputs(line, f);
        HexLine(line, "HDDLockDisablePassword ", key, 0x20, "%02x ", "\n\n", false);
        fputs(line, f);
        HexLine(line, "XboxHDKey ", XboxHDKey, 0x10, "%02x ", "\n", false);
        fputs(line, f);
        fclose(f);
    }

    // eeprom.bin: the 256-byte EEPROM, a word at a time over the SMBus
    WORD eeprom[0x80];
    for (int i = 0; i < 0x100; i += 2) {
        ULONG v;
        HalReadSMBusValue(0xA9, (UCHAR)i, TRUE, &v);
        eeprom[i / 2] = (WORD)v;
    }
    sprintf(path, "%s:\\backup\\eeprom.bin", drive);
    if (FILE *f = fopen(path, "wb")) {
        fwrite(eeprom, 1, 0x100, f);
        fclose(f);
    }

    CloseHandle(g_BackupThread);
    g_BackupThread = nullptr;
    return 0;
}

// 0x20460: Run's state 5 (the screen has faded down)
void StartBackup()
{
    if (!g_BackupThread)
        g_BackupThread = CreateThread(nullptr, 0, BackupThread, nullptr, 0, nullptr);
}

// 0x18D10 (m_drawBackup): "Backingup :)" in a bar, fading with m_backupFade; state 6 waits here for the
// thread, then fades back (state 7)
void CEvoxDash::DrawBackupBox()
{
    DWORD fill = (DWORD)(long long)((double)m_backupFade * F32(0x3F000000)) << 24 | 0x80;
    DWORD border = (DWORD)(long long)((double)m_backupFade * F32(0x3E4CCCCD)) << 24 | 0xC0C0C0;
    DrawRect(F32(0x43100000), F32(0x438E0000), F32(0x43F80000), F32(0x439A0000), fill, border); // (144, 284)-(496, 308)
    m_font.Begin();
    m_font.DrawText(F32(0x43A00000), F32(0x438E0000), (DWORD)m_backupFade << 24 | 0xC0C0C0, L"Backingup :)",
                    XBFONT_CENTER_X);
    m_font.End();
    if (m_state == STATE_BACKUP && !g_BackupThread)
        m_state = STATE_BACKUP_OUT;
}
