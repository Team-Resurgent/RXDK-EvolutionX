// Flashing the BIOS and the "Flash BIOS" box (0x18E30, 0x1EF60, 0x21070..0x2181B, 0x5958B2 in the
// original); see include/evox/flash.h. The chip is reached through its 1 MB window at 0xFF000000
// with JEDEC command sequences (AA to the first unlock address, 55 to the second, then the
// command); the image is written to the top of the window. Nothing here runs unless the user
// confirms in the box (or an FTP FLASHBIOS / a flashbios action step asks for it).
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "evox/dash.h"
#include "evox/flash.h"
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/pending.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

volatile int g_FlashMode;                // 0x57F5C8
HANDLE g_FlashThread;                    // 0x57F5CC
volatile DWORD g_FlashProgress;          // 0x57F5D0
DWORD g_FlashSize = 1;                   // 0x95BC4
bool g_FlashBusy;                        // 0x47BF20
char g_FlashFile[0x100];                 // 0x47BF88
BYTE g_FlashManufacturer = 0xFF;         // 0x95BC0
BYTE g_FlashDevice = 0xFF;               // 0x95BC1
WCHAR g_FlashLine1[0x400];               // 0x57E5C8
WCHAR g_FlashLine2[0x400];               // 0x57EDC8

static volatile BYTE *g_FlashRom;        // 0x47BF40 the mapped window
static DWORD g_FlashUnlock1 = 0x5555;    // 0x47C118 first unlock address (0x5555 or 0xAAAA)
static DWORD g_FlashUnlock2 = 0x2AAA;    // 0x47C398 second (0x2AAA or 0x5555)
static WORD g_FlashId;                   // 0x57F5D4 manufacturer << 8 | device
static WORD g_FlashIdOfLine;             // 0x47C164 the id of the [BIOS] Flash line being compared
static bool g_FlashKnown;                // 0x47C166 a [BIOS] Flash line matched
static DWORD g_FlashFileSize;            // 0x47C160
static BYTE g_FlashErase1 = 0x80;        // 0x95B18 the erase command
static BYTE g_FlashErase2 = 0x30;        // 0x95B19 the sector erase command
static BYTE g_FlashProgram = 0xA0;       // 0x95B1A the byte program command
static WCHAR g_FlashType[0x40];          // 0x57D428 "Flash Type <name>"
static BYTE g_FlashImage[0x100000];      // 0x47D260 the file, repeated to the chip's size
int g_FlashArgc;                         // 0x47C310
char *g_FlashArgv[0x100];                // 0x57DCB0

static void ResetScreenSaver() { g_ScreenSaverDeadline = GetTickCount() + g_ScreenSaverMinutes * 60000; }

// 0x1EF60: split the line in place into g_FlashArgv, like CEvoxDash::Tokenize (0x199E0) but
// into globals of its own (also used by the action scripts)
bool FlashTokenize(char *line)
{
    g_FlashArgc = 0;
    if (!line)
        return false;
    for (char *&a : g_FlashArgv)
        a = g_EmptyArg;
    int state = 0; // 0 between words, 1 in a quoted word, 2 in a plain word
    for (int i = 0; line[i]; i++) {
        char c = line[i];
        if (state == 0) {
            if (c == ' ' || c == '\t' || c == ',')
                continue;
            if (c == '"' && line[i + 1]) {
                state = 1;
                g_FlashArgv[g_FlashArgc++] = &line[i + 1];
            } else {
                state = 2;
                g_FlashArgv[g_FlashArgc++] = &line[i];
            }
        } else if (state == 1) {
            if (c == '"') {
                line[i] = 0;
                state = 0;
            }
        } else if (c == ',' || c == ' ' || c == '\t') {
            line[i] = 0;
            state = 0;
        }
    }
    return g_FlashArgc != 0;
}

static ULONGLONG ReadMsr(DWORD msr)
{
    DWORD lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (ULONGLONG)hi << 32 | lo;
}

static void WriteMsr(DWORD msr, ULONGLONG value)
{
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((DWORD)value), "d"((DWORD)(value >> 32)));
}

static void FlushCaches()
{
    DWORD cr3;
    __asm__ volatile("wbinvd\n\tmov %%cr3, %0\n\tmov %0, %%cr3" : "=r"(cr3) : : "memory");
}

// 0x5958B2 (in the _text section): with the caches off and the MTRRs disabled, set (on) or
// clear the valid bit of each variable MTRR whose range starts in the top 1 MB (the flash) and
// is not uncached, so the flash can be written through
static void FlashCaching(int on)
{
    DWORD cr0, cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0 | 0x60000000));
    FlushCaches();
    ULONGLONG defType = ReadMsr(0x2FF);
    WriteMsr(0x2FF, 0);
    for (DWORD msr = 0x200; msr < 0x20F; msr += 2) {
        ULONGLONG base = ReadMsr(msr);
        if (((base >> 12) & 0xFFFFFF) >= 0xFFF00 && (BYTE)base != 0) {
            ULONGLONG mask = ReadMsr(msr + 1);
            if (on)
                mask |= 0x800;
            else
                mask &= ~0x800ULL;
            WriteMsr(msr + 1, mask);
        }
    }
    FlushCaches();
    WriteMsr(0x2FF, defType);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

static void FlashCommand(DWORD at, BYTE command) // AA 55 <command> (at: the top of the window)
{
    g_FlashRom[g_FlashUnlock1 + at] = 0xAA;
    g_FlashRom[g_FlashUnlock2 + at] = 0x55;
    g_FlashRom[g_FlashUnlock1 + at] = command;
}

// 0x21070: the chip's id (autoselect 90, read at the two offsets, reset F0)
static void ReadFlashId(int manufacturerAt, int deviceAt)
{
    g_FlashRom = (volatile BYTE *)MmMapIoSpace(0xFF000000, 0x100000, PAGE_READWRITE | PAGE_NOCACHE);
    FlashCaching(0);
    Sleep(300);
    FlashCommand(0, 0x90);
    Sleep(20);
    g_FlashManufacturer = g_FlashRom[manufacturerAt];
    g_FlashDevice = g_FlashRom[deviceAt];
    FlashCommand(0, 0xF0);
    MmUnmapIoSpace((PVOID)g_FlashRom, 0x100000);
    FlashCaching(1);
    g_FlashId = (WORD)(g_FlashManufacturer << 8 | g_FlashDevice);
}

// 0x21150: the [BIOS] Flash line for the chip's id: its name, size and (with six words) commands
static bool FindFlashType()
{
    g_FlashKnown = false;
    g_Ini->Lock();
    wcscpy(g_FlashType, L"!! Unknown Flash Type !!");
    for (char *v = g_Ini->GetValue("BIOS", "Flash"); v; v = g_Ini->GetNextValue()) {
        FlashTokenize(v);
        if (g_FlashArgv[0][1] != 'x')
            continue;
        g_FlashIdOfLine = (WORD)strtoul(g_FlashArgv[0] + 2, nullptr, 16);
        if (g_FlashIdOfLine != g_FlashId || g_FlashArgc < 2)
            continue;
        swprintf(g_FlashType, 0x40, L"Flash Type %hs", g_FlashArgv[1]);
        if (g_FlashArgc < 3)
            continue;
        g_FlashSize = strtoul(g_FlashArgv[2] + 2, nullptr, 16);
        g_FlashKnown = true;
        if (g_FlashArgc == 6) {
            g_FlashErase1 = (BYTE)strtoul(g_FlashArgv[3] + 2, nullptr, 16);
            g_FlashErase2 = (BYTE)strtoul(g_FlashArgv[4] + 2, nullptr, 16);
            g_FlashProgram = (BYTE)strtoul(g_FlashArgv[5] + 2, nullptr, 16);
        } else {
            g_FlashErase1 = 0x80;
            g_FlashErase2 = 0x30;
            g_FlashProgram = 0xA0;
        }
        break;
    }
    g_Ini->Unlock();
    return g_FlashKnown;
}

static void DisableInterrupts() { __asm__ volatile("pushfl\n\tcli" : : : "memory"); }
static void RestoreInterrupts() { __asm__ volatile("popfl" : : : "memory", "cc"); }

// 0x21340: mode 1 identifies the chip and reads the file; mode 2 erases and writes it, then
// powers the console off. g_FlashMode ends 0 (done) or -1 (failed); mode 2 does not return.
static DWORD WINAPI FlashThread(LPVOID)
{
    if (g_FlashMode == 1) {
        g_FlashKnown = false;
        g_FlashSize = 1;
        g_FlashUnlock1 = 0x5555;
        g_FlashUnlock2 = 0x2AAA;
        ReadFlashId(0, 1);
        WORD first = g_FlashId;
        if (!FindFlashType()) {
            g_FlashUnlock1 = 0xAAAA;
            g_FlashUnlock2 = 0x5555;
            ReadFlashId(0, 2);
            FindFlashType();
        }
        if (!g_FlashKnown && g_FlashId == first) { // the same answer both ways: not a flash chip
            wcscpy(g_FlashLine1, L"!! Flash Not Writeable !!");
            g_FlashKnown = false;
            g_FlashSize = 1;
            g_FlashMode = -1;
        } else {
            wcscpy(g_FlashLine1, g_FlashType);
            if (!g_FlashKnown) {
                g_FlashMode = -1;
            } else {
                wcscpy(g_FlashLine1, g_FlashType);
                FILE *f = fopen(g_FlashFile, "rb");
                if (!f) {
                    wcscpy(g_FlashLine2, L"Error reading bios file");
                    g_FlashMode = -1;
                } else {
                    DWORD n = g_FlashFileSize = fread(g_FlashImage, 1, 0x100000, f);
                    fclose(f);
                    // a smaller image is repeated to fill the chip
                    if (n * 2 == g_FlashSize) {
                        memcpy(g_FlashImage + n, g_FlashImage, n);
                        g_FlashFileSize = n = n * 2;
                    }
                    if (n * 4 == g_FlashSize) {
                        memcpy(g_FlashImage + n, g_FlashImage, n);
                        memcpy(g_FlashImage + n * 2, g_FlashImage, n);
                        memcpy(g_FlashImage + n * 3, g_FlashImage, n);
                        g_FlashFileSize = n = n * 4;
                    }
                    if (n != g_FlashSize) {
                        wcscpy(g_FlashLine2, L"Error file size error");
                        g_FlashMode = -1;
                    } else {
                        g_FlashMode = 0;
                    }
                }
            }
        }
    } else if (g_FlashMode == 2) {
        PrepareForExit();
        g_FlashRom = (volatile BYTE *)MmMapIoSpace(0xFF000000, 0x100000, PAGE_READWRITE | PAGE_NOCACHE);
        Sleep(300);
        FlashCaching(0);
        DWORD top = 0x100000 - g_FlashSize; // the chip sits at the top of the window

        // erase: each 256-byte block that is not blank, by a sector erase at its address
        for (g_FlashProgress = 0; g_FlashProgress < g_FlashSize; g_FlashProgress += 0x100) {
            DWORD at = top + g_FlashProgress;
            int i = 0;
            while (i < 0x100 && g_FlashRom[at + i] == 0xFF)
                i++;
            if (i == 0x100)
                continue;
            DisableInterrupts();
            FlashCommand(top, g_FlashErase1);
            g_FlashRom[g_FlashUnlock1 + top] = 0xAA;
            g_FlashRom[g_FlashUnlock2 + top] = 0x55;
            g_FlashRom[at] = g_FlashErase2;
            RestoreInterrupts();
            while (g_FlashRom[top + g_FlashProgress] != 0xFF)
                NtYieldExecution();
        }

        // program each byte that differs (0xFF needs no programming after the erase)
        wcscpy(g_FlashLine2, L"Flashing BIOS now!");
        for (g_FlashProgress = 0; g_FlashProgress < g_FlashSize; g_FlashProgress = g_FlashProgress + 1) {
            DWORD i = g_FlashProgress;
            BYTE b = g_FlashImage[i];
            if (b != 0xFF && g_FlashRom[top + i] != b) {
                DisableInterrupts();
                FlashCommand(top, g_FlashProgram);
                g_FlashRom[top + i] = b;
                RestoreInterrupts();
                while (g_FlashRom[top + i] != b)
                    ;
            }
            if ((i & 0x3FF) == 0)
                NtYieldExecution();
        }

        DisableInterrupts();
        FlashCommand(top, 0xF0); // reset to reading
        g_FlashRom[g_FlashUnlock1 + top] = 0xFF;
        RestoreInterrupts();
        Sleep(100);
        MmUnmapIoSpace((PVOID)g_FlashRom, 0x100000);
        FlashCaching(1);
        wcscpy(g_FlashLine2, L"Flashing BIOS now!\n\rThe XBOX will turn off!!");
        Sleep(2000);
        HalWriteSMBusValue(0x20, 0x02, FALSE, 0x80); // the SMC: power off
        for (;;)
            ;
    }
    CloseHandle(g_FlashThread);
    g_FlashThread = nullptr;
    return 0;
}

// 0x217F0. (The mode is stored after the thread is created, as in the original.)
void StartFlash(int mode)
{
    if (g_FlashThread)
        return;
    g_FlashThread = CreateThread(nullptr, 0, FlashThread, nullptr, 0, nullptr);
    g_FlashMode = mode;
}

// 0x18E30 (m_drawMessage): the "Flash BIOS" box. In state 23 it steps m_flashStep: 0 read the
// chip and the file (m_message), 1 wait for that, then ask for Y (or go on at once with
// m_flashNow), 3 wait for Y, 4 "Erasing." and write. B / Back closes it (state 24) until the
// writing starts.
void CEvoxDash::DrawMessage()
{
    int a = m_messageFade;
    DrawRect(F32(0x430C0000), F32(0x43960000), F32(0x43FA0000), F32(0x43DC0000), (DWORD)a << 24 | 0x80,
             (DWORD)(a / 2) << 24 | 0x8000); // (140, 300)-(500, 440)
    DrawRect(F32(0x43100000), F32(0x43980000), F32(0x43F80000), F32(0x43A40000), (DWORD)a << 24 | 0x80,
             (DWORD)(a / 2) << 24 | 0xFFFFFF); // (144, 304)-(496, 328): the title
    DrawRect(F32(0x43100000), F32(0x43D00000), F32(0x43F80000), F32(0x43DA0000), (DWORD)a << 24 | 0x80,
             (DWORD)(a / 2) << 24 | 0xFFFFFF); // (144, 416)-(496, 436): the progress bar
    if (g_FlashBusy == true) {
        float right = (float)((long double)g_FlashProgress / (long double)g_FlashSize * F32(0x43B00000) +
                              F32(0x43100000)); // 144 + 352 * done
        DrawRect(F32(0x43100000), F32(0x43D00000), right, F32(0x43DA0000), (DWORD)a << 24 | 0xFF00,
                 (DWORD)(a / 2) << 24 | 0xFF00);
    }
    WCHAR id[0x200];
    m_font.Begin();
    m_font.DrawText(F32(0x43A00000), F32(0x43980000), (DWORD)a << 24 | 0xFFFFFF, L"Flash BIOS", XBFONT_CENTER_X);
    swprintf(id, 0x200, L"Manufacturer’s ID %02x Device ID %02x", g_FlashManufacturer, g_FlashDevice);
    m_font.DrawText(F32(0x43160000), F32(0x43A50000), (DWORD)a << 24 | 0xFFFFFF, id, 0);
    m_font.DrawText(F32(0x43160000), F32(0x43AF0000), (DWORD)a << 24 | 0xFFFFFF, g_FlashLine1, 0);
    m_font.DrawText(F32(0x43160000), F32(0x43B90000), (DWORD)a << 24 | 0xFFFFFF, g_FlashLine2, 0);
    m_font.End();

    if (m_state != 23)
        return;
    switch (m_flashStep) {
    case 0:
        strcpy(g_FlashFile, m_message);
        StartFlash(1);
        m_flashStep = 1;
        break;
    case 1:
        if (g_FlashMode == -1)
            m_flashStep = 10; // (no step: only B / Back is left)
        if (g_FlashMode != 0)
            break;
        if (m_flashNow) {
            m_flashStep = 4;
            m_flashNow = false;
        } else {
            wcscpy(g_FlashLine2, L"Press Y to Flash BIOS");
            m_flashStep = 3;
        }
        break;
    case 3:
        if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y]) {
            ResetScreenSaver();
            m_flashStep = 4;
        }
        break;
    case 4:
        wcscpy(g_FlashLine2, L"Erasing.");
        m_flashStep = 5;
        g_FlashBusy = true;
        StartFlash(2);
        break;
    }
    if (g_FlashBusy)
        return;
    if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_BACK) || m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
        m_key == 8 || m_keyboard == 0x27) {
        ResetScreenSaver();
        m_state = 24;
    }
}
