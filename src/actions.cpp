// The menu's scripted actions (0x1CAC0 OpenItem, 0x187B0 its dialog, 0x20F20 / 0x206D0 the
// steps, 0x20490 a folder copy in the original); see include/evox/actions.h. The steps run on a
// thread of their own with evox.ini locked, and log to [Action_NN] LogFile when there is one.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "evox/actions.h"
#include "evox/dash.h"
#include "evox/flash.h"
#include "evox/globals.h"
#include "evox/guard.h"
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/system.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

HANDLE g_ActionThread;            // 0x57E5BC
bool g_ActionFlash;               // 0x57E5C0
char g_ActionFlashFile[0x100];    // 0x47CFE0
static FILE *g_ActionLog;         // 0x47C208 [Action_NN] LogFile
static int g_DialogRepeat;        // 0x47BD18 frames until left / right / A count again

// 0x1CAC0: the dialog for [Action_<g_SelectedItem>]: info (the title), Warning (the lines) and
// progress (shown while the steps run); "Action Error" if there is no such section
void CEvoxDash::OpenItem()
{
    char section[0x200];
    m_actionLines = 0;
    sprintf(section, "Action_%02d", g_SelectedItem);
    // catch (...) 0x1CC68: release the lock
    if (!CrashGuard([&] {
        g_Ini->Lock();
        if (char *v = g_Ini->GetValue(section, "info")) {
            if (char *t = strtok(v, "\"")) {
                swprintf(m_actionTitle, 0x100, L"%hs", t);
                m_actionText[0] = 0;
                int n = 0;
                for (v = g_Ini->GetValue(section, "Warning"); v; v = g_Ini->GetNextValue()) {
                    if ((t = strtok(v, "\""))) {
                        n += swprintf(m_actionText + n, 0x400 - n, L"%hs\n", t);
                        m_actionLines++;
                    }
                }
            }
            char *v2 = g_Ini->GetValue(section, "progress");
            if (char *t; v2 && (t = strtok(v2, "\"")))
                swprintf(m_actionProgress, 0x100, L"%hs", t);
        } else {
            wcscpy(m_actionTitle, L"Action Error");
            swprintf(m_actionText, 0x400, L"Error in action number\nAction : %d\n\n\n", g_SelectedItem);
            m_actionLines = 3;
            g_SelectedItem = -1;
        }
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
}

// (alpha * 0.5) << 24 | fill and (alpha * 0.2) << 24 | border, as the boxes use them
static DWORD FadeColour(int alpha, float scale, DWORD rgb)
{
    return (DWORD)(long long)((long double)alpha * scale) << 24 | rgb;
}

// 0x187B0 (m_drawAction): the dialog (title, Warning lines, "Continue ?" with Yes / No) faded by
// m_actionFade, and the progress box faded by m_progressFade. In state 12 left / right choose,
// A or Enter answers (state 15 runs the steps, 13 closes), B / Back closes.
void CEvoxDash::DrawActionBox()
{
    const float half = F32(0x3F000000), fifth = F32(0x3E4CCCCD);
    int a = m_actionFade, p = m_progressFade;
    float h = (float)((m_actionLines + 3) * 0x1C); // the dialog's height
    DrawRect(F32(0x430C0000), F32(0x42C80000), F32(0x43FA0000), h + F32(0x42C80000), FadeColour(a, half, 0x80),
             FadeColour(a, fifth, 0xC0C0C0)); // (140, 100)-(500, 100 + h)
    DrawRect(F32(0x43100000), F32(0x42D00000), F32(0x43F80000), F32(0x43000000), FadeColour(a, half, 0x80),
             FadeColour(a, fifth, 0xFFFFFF)); // (144, 104)-(496, 128): the title
    DrawRect(F32(0x43100000), h + F32(0x42100000), F32(0x43F80000), h + F32(0x42C00000), FadeColour(a, half, 0x80),
             FadeColour(a, fifth, 0xFFFFFF)); // "Continue ?"
    DWORD yes = m_actionYes ? 0x8000 : 0x80, no = m_actionYes ? 0x80 : 0x8000;
    DrawRect(F32(0x43140000), h + F32(0x42800000), F32(0x439E0000), h + F32(0x42B80000), FadeColour(a, half, yes),
             FadeColour(a, fifth, 0xFFFFFF)); // Yes (148..316)
    DrawRect(F32(0x43A20000), h + F32(0x42800000), F32(0x43F60000), h + F32(0x42B80000), FadeColour(a, half, no),
             FadeColour(a, fifth, 0xFFFFFF)); // No (324..492)
    DrawRect(F32(0x42C80000), F32(0x43520000), F32(0x44070000), F32(0x43870000), FadeColour(p, half, 0x80),
             FadeColour(p, fifth, 0xC0C0C0)); // the progress box (100, 210)-(540, 270)
    DrawRect(F32(0x42D00000), F32(0x43560000), F32(0x44060000), F32(0x436E0000), FadeColour(p, half, 0x80),
             FadeColour(p, fifth, 0xFFFFFF)); // (104, 214)-(536, 238)

    m_font.Begin();
    m_font.DrawText(F32(0x43A00000), F32(0x42D00000), (DWORD)a << 24 | 0xFFFFFF, m_actionTitle, XBFONT_CENTER_X);
    m_font.DrawText(F32(0x43A00000), F32(0x43560000), (DWORD)p << 24 | 0xFFFFFF, m_actionProgress, XBFONT_CENTER_X);
    m_font.DrawText(F32(0x43A00000), F32(0x43720000), (DWORD)p << 24 | 0xFFFFFF, g_ProgressText, XBFONT_CENTER_X);
    m_font.DrawText(F32(0x43160000), F32(0x43020000), (DWORD)a << 24 | 0xFFFFFF, m_actionText, 0);
    m_font.DrawText(F32(0x43A00000), h + F32(0x42180000), (DWORD)a << 24 | 0xFFFFFF, L"Continue ?", XBFONT_CENTER_X);
    m_font.DrawText(F32(0x436A0000), h + F32(0x42820000), (DWORD)a << 24 | 0xFFFFFF, L"Yes", XBFONT_CENTER_X);
    m_font.DrawText(F32(0x43CC0000), h + F32(0x42820000), (DWORD)a << 24 | 0xFFFFFF, L"No", XBFONT_CENTER_X);
    m_font.End();

    if (m_state != STATE_ACTION)
        return;
    if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_BACK) || m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
        m_key == 8 || m_remoteButton == 0x27) {
        ResetScreenSaver();
        m_state = STATE_ACTION_OUT;
    }
    if (g_DialogRepeat) {
        g_DialogRepeat--;
        return;
    }
    if (m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        ResetScreenSaver();
        m_actionYes = true;
        g_DialogRepeat += 8;
    }
    if (m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        ResetScreenSaver();
        m_actionYes = false;
        g_DialogRepeat += 8;
    }
    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] || m_key == 0x0D) {
        ResetScreenSaver();
        m_state = m_actionYes ? STATE_PROGRESS_IN : STATE_ACTION_OUT;
    }
}

// 0x20490: copy a folder's files and subfolders (both paths end in '\'), clearing the copies'
// attributes
static void CopyFolder(const char *from, const char *to)
{
    char search[0x200], src[0x200], dst[0x200];
    WIN32_FIND_DATA fd;
    strcpy(search, from);
    strcat(search, "*.*");
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        strcpy(src, from);
        strcat(src, fd.cFileName);
        strcpy(dst, to);
        strcat(dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CreateDirectory(dst, nullptr);
            strcat(src, "\\");
            strcat(dst, "\\");
            CopyFolder(src, dst);
            continue;
        }
        if (g_ActionLog)
            fprintf(g_ActionLog, "%s -> %s\n", src, dst);
        swprintf(g_ProgressText, 0x200, L"Copying %hs", fd.cFileName);
        if (CopyFile(src, dst, FALSE)) {
            SetFileAttributes(dst, FILE_ATTRIBUTE_NORMAL);
            if (g_ActionLog)
                fprintf(g_ActionLog, "OK\n");
        } else if (g_ActionLog) {
            fprintf(g_ActionLog, "Error : %d\n", (int)GetLastError());
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

// a step's path: one starting with '\' is on Q:
static void StepPath(char *out, const char *arg)
{
    strcpy(out, arg[0] == '\\' ? "q:" : "");
    strcat(out, arg);
}

// "TEAMASSEMBLY" in a 32-byte password block (the drive's master password)
static void MasterPassword(BYTE pw[0x20])
{
    memset(pw, 0, 0x20);
    memcpy(pw, "TEAMASSEMBLY", 13);
}

// 0x206D0: the steps of [Action_<g_SelectedItem>], one per line (see actions.h)
static DWORD WINAPI ActionThread(LPVOID)
{
    char section[0x100];
    char path[0x200] = {}; // (not cleared in the original: an unknown format letter reuses it)
    char path2[0x200];
    bool flash = false;
    sprintf(section, "Action_%02d", g_SelectedItem);
    ANSI_STRING disk = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
    // catch (...) 0x20EAC: release the lock, then close the log as usual
    if (!CrashGuard([&] {
        g_Ini->Lock();
        g_ActionLog = nullptr;
        if (FlashTokenize(g_Ini->GetValue(section, "LogFile")))
            g_ActionLog = fopen(g_FlashArgv[0], "wt");
        if (g_ActionLog)
            fprintf(g_ActionLog, "%s\n", section);
        for (char *line = g_Ini->FindSection(section); FlashTokenize(line), line; line = g_Ini->NextLine()) {
            char **argv = g_FlashArgv;
            if (argv[0][0] == '#')
                continue;
            if (g_ActionLog) {
                for (int i = 0; i < g_FlashArgc; i++)
                    fprintf(g_ActionLog, "%s ", argv[i]);
                fprintf(g_ActionLog, "\n");
            }

            // configsector <file>: the file's first 512 KB over the start of the disk
            if (_stricmp(argv[0], "configsector") == 0) {
                StepPath(path, argv[1]);
                if (g_ActionLog)
                    fprintf(g_ActionLog, "DoConfigsector %s\n", path);
                swprintf(g_ProgressText, 0x200, L"Writing Config Sector");
                if (FILE *f = fopen(path, "rb")) {
                    BYTE *buf = (BYTE *)VirtualAlloc(nullptr, 0x80000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                    OBJECT_ATTRIBUTES oa = {nullptr, &disk, OBJ_CASE_INSENSITIVE};
                    IO_STATUS_BLOCK iosb;
                    HANDLE h = INVALID_HANDLE_VALUE; // (not initialised in the original)
                    NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_ALERT);
                    if (h != INVALID_HANDLE_VALUE) {
                        LONG high = 0;
                        SetFilePointer(h, 0, &high, FILE_BEGIN);
                        fread(buf, 1, 0x80000, f);
                        DWORD written;
                        WriteFile(h, buf, 0x80000, &written, nullptr);
                        CloseHandle(h);
                    }
                    fclose(f);
                    VirtualFree(buf, 0, MEM_RELEASE);
                }
            }

            // format <drive letter>
            if (_stricmp(argv[0], "format") == 0) {
                swprintf(g_ProgressText, 0x200, L"Formating %hs", argv[1]);
                switch (argv[1][0]) {
                case 'C': case 'c': strcpy(path, "\\Device\\Harddisk0\\Partition2"); break;
                case 'E': case 'e': strcpy(path, "\\Device\\Harddisk0\\Partition1"); break;
                case 'F': case 'f': strcpy(path, "\\Device\\Harddisk0\\Partition6"); break;
                case 'G': case 'g': strcpy(path, "\\Device\\Harddisk0\\Partition7"); break;
                case 'X': case 'x': strcpy(path, "\\Device\\Harddisk0\\Partition3"); break;
                case 'Y': case 'y': strcpy(path, "\\Device\\Harddisk0\\Partition4"); break;
                case 'Z': case 'z': strcpy(path, "\\Device\\Harddisk0\\Partition5"); break;
                }
                if (g_ActionLog)
                    fprintf(g_ActionLog, "DoFormat %s\n", path);
                ANSI_STRING volume;
                RtlInitAnsiString(&volume, path);
                XapiFormatFATVolume((POBJECT_STRING)&volume);
                XapiValidateDiskPartition((POBJECT_STRING)&volume);
                RelinkDrives();
            }

            // rename <from> <to>
            if (_stricmp(argv[0], "rename") == 0) {
                if (g_ActionLog)
                    fprintf(g_ActionLog, "MoveFile %s -> %s\n", argv[1], argv[2]);
                if (MoveFile(argv[1], argv[2])) {
                    if (g_ActionLog)
                        fprintf(g_ActionLog, "OK\n");
                } else if (g_ActionLog) {
                    fprintf(g_ActionLog, "Error : %d\n", (int)GetLastError());
                }
            }

            // copy <from> <to>: a folder when <from> ends in '\'
            if (_stricmp(argv[0], "copy") == 0) {
                StepPath(path, argv[1]);
                StepPath(path2, argv[2]);
                if (g_ActionLog)
                    fprintf(g_ActionLog, "Copy %s -> %s\n", path, path2);
                if (argv[1][strlen(argv[1]) - 1] == '\\') {
                    CopyFolder(path, path2);
                } else {
                    swprintf(g_ProgressText, 0x200, L"Copying %hs", path);
                    if (CopyFile(path, path2, FALSE)) {
                        if (g_ActionLog)
                            fprintf(g_ActionLog, "OK\n");
                    } else if (g_ActionLog) {
                        fprintf(g_ActionLog, "Error : %d\n", (int)GetLastError());
                    }
                }
            }

            // flashbios <file>: the "Flash BIOS" box after the steps
            if (_stricmp(argv[0], "flashbios") == 0) {
                StepPath(path, argv[1]);
                strcpy(g_ActionFlashFile, path);
                flash = true;
            }

            // the drive's lock (when its password is known)
            if (_stricmp(argv[0], "hddlockdisable") == 0 && !HddPasswordMissing()) {
                AtaSecurityCommand(0xF6, 0, g_HddPassword); // SECURITY DISABLE PASSWORD (user)
                IdentifyDrive();
            }
            if (_stricmp(argv[0], "hddlockenable") == 0 && !HddPasswordMissing()) {
                BYTE master[0x20];
                MasterPassword(master);
                AtaSecurityCommand(0xF1, 1, master);        // SECURITY SET PASSWORD (master)
                AtaSecurityCommand(0xF1, 0, g_HddPassword); // and the user password
                IdentifyDrive();
            }
            if (_stricmp(argv[0], "hddmasterdisable") == 0 && !HddPasswordMissing()) {
                BYTE master[0x20];
                MasterPassword(master);
                AtaSecurityCommand(0xF6, 1, master);        // SECURITY DISABLE PASSWORD (master)
                IdentifyDrive();
            }
        }
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
    if (g_ActionLog) {
        fprintf(g_ActionLog, "End of Log\n");
        fclose(g_ActionLog);
    }
    CloseHandle(g_ActionThread);
    g_ActionThread = nullptr;
    g_ActionFlash = flash;
    swprintf(g_ProgressText, 0x200, L"Done");
    return 0;
}

// 0x20F20
void StartActionScript()
{
    if (!g_ActionThread)
        g_ActionThread = CreateThread(nullptr, 0, ActionThread, nullptr, 0, nullptr);
}
