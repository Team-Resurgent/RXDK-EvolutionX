// EvoX's FTP server (0x12750..0x14ECF in the original); see include/evox/ftp.h. Paths are FTP
// style, "/<drive letter>/dir/", and become "<drive>:\dir\" (MakePath).
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/ftp.h"
#include "evox/log.h"
#include "evox/globals.h"
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/system.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (the layout checks below are on purpose)

static_assert(offsetof(CFtpServer, m_dataAddr) == 0x018, "m_dataAddr");
static_assert(offsetof(CFtpServer, m_convName) == 0x050, "m_convName");
static_assert(offsetof(CFtpServer, m_formatVolume) == 0x250, "m_formatVolume");
static_assert(offsetof(CFtpServer, m_freeSpace) == 0x25C, "m_freeSpace");
static_assert(offsetof(CFtpServer, m_message) == 0x35C, "m_message");
static_assert(offsetof(CFtpServer, m_quit) == 0x55D, "m_quit");
static_assert(offsetof(CFtpServer, m_password) == 0x55E, "m_password");
static_assert(sizeof(CFtpServer) == 0x5E0, "CFtpServer");

// the XAPI calls FORMATDRIVE makes (not in the SDK's headers)
extern "C" BOOL __stdcall XapiFormatFATVolume(POBJECT_STRING volume);
extern "C" NTSTATUS XapiValidateDiskPartition(POBJECT_STRING partition);

static char g_FtpReply[0x200];      // 0x3D6DD8 (one buffer for all sessions, as in the original)
int g_FtpUsers;                     // 0x3D6FD8 sessions open
bool g_AutoConvNames;               // 0x3D6DD4 AUTOCONVNAMES: STOR and MKD names made FATX-safe
NTSTATUS g_FDriveValid;             // 0x58FB04 XapiValidateDiskPartition(F:) at each LIST of "/"

// 0x12760: a reply on the control connection (the server argument is unused)
void FtpReply(CFtpServer *, CSocket *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(g_FtpReply, fmt, args);
    va_end(args);
    s->Send(g_FtpReply, strlen(g_FtpReply));
}

// 0x127A0: wait for a line (polling once a second for g_StopNetwork); the line ends at the
// first \r or \n of one receive
bool CFtpServer::ReadLine(CSocket *s, char *line)
{
    timeval wait = {1, 0};
    char buf[0x400];
    while (g_StopNetwork != true) {
        fd_set fds;
        fds.fd_count = 0;
        fds.fd_array[fds.fd_count++] = s->Handle();
        if (select(0, &fds, nullptr, nullptr, &wait) == SOCKET_ERROR) {
            line[0] = 0;
            return false;
        }
        if (!FD_ISSET(s->Handle(), &fds))
            continue;
        int n = s->Recv(buf, 0x400);
        if (n == SOCKET_ERROR || n == 0)
            break;
        int i;
        for (i = 0; i != n && buf[i] != '\n' && buf[i] != '\r'; i++)
            line[i] = buf[i];
        line[i] = 0;
        return true;
    }
    line[0] = 0;
    return false;
}

// 0x128C0: the free bytes of root in groups of three digits, into m_freeSpace
void CFtpServer::FreeSpace(const char *root)
{
    ULARGE_INTEGER avail, total, free;
    free.QuadPart = 0; // (not initialised in the original)
    GetDiskFreeSpaceEx(root, &avail, &total, &free);
    ULONGLONG n = free.QuadPart;
    int units = (int)(n % 1000);
    n /= 1000;
    int thousands = (int)(n % 1000);
    n /= 1000;
    int millions = (int)(n % 1000);
    n /= 1000;
    int billions = (int)(n % 1000);
    if (billions)
        sprintf(m_freeSpace, "%d.%03d.%03d.%03d", billions, millions, thousands, units);
    else if (millions)
        sprintf(m_freeSpace, "%d.%03d.%03d", millions, thousands, units);
    else if (thousands)
        sprintf(m_freeSpace, "%d.%03d", thousands, units);
    else
        sprintf(m_freeSpace, "%d", units);
}

// 0x129C0: "/c/dir/x" (or "\c\...") -> "c:\dir\x"; a relative arg is added to cwd, converted the
// same way. (For cwd "/" the original reads the bytes after its terminator; so does this.)
char *CFtpServer::MakePath(const char *arg, char *out, const char *cwd)
{
    if (!arg) {
        out[0] = 0;
        return out;
    }
    bool relative = arg[0] != '/' && arg[0] != '\\'; // (LIST passes cwd as arg: absolute)
    const char *src = relative ? cwd : arg;
    char *o = out;
    *o++ = src[1];
    *o++ = ':';
    for (const char *p = src + 2; *p; p++)
        *o++ = *p == '/' ? '\\' : *p;
    *o = 0;
    if (relative) {
        if (out[strlen(out) - 1] != '\\')
            strcat(out, "\\");
        strcat(out, arg);
    }
    return out;
}

// 0x12AC0: RETR, 4 KB at a time
void CFtpServer::SendFile(CSocket *data, const char *path)
{
    char *buf = (char *)operator new(0x10000);
    if (FILE *f = fopen(path, "rb")) {
        while (!feof(f)) {
            int n = fread(buf, 1, 0x1000, f);
            for (int at = 0; n;) {
                int sent = data->Send(buf + at, n);
                if (sent == SOCKET_ERROR) {
                    fclose(f);
                    operator delete(buf);
                    return;
                }
                at += sent;
                n -= sent;
            }
        }
        fclose(f);
    }
    operator delete(buf);
}

// 0x12B70: STOR, into a new file
bool CFtpServer::ReceiveFile(CSocket *data, const char *path)
{
    int size = 0x8000;
    char *buf = (char *)operator new(0x10000);
    data->SetOption(SOL_SOCKET, SO_RCVBUF, (const char *)&size, 4);
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        operator delete(buf);
        return false;
    }
    for (;;) {
        int n = data->Recv(buf, 0x10000);
        if (n == SOCKET_ERROR)
            break;
        if (n == 0) {
            CloseHandle(h);
            operator delete(buf);
            return true;
        }
        for (int at = 0; n;) {
            DWORD written;
            if (!WriteFile(h, buf + at, n, &written, nullptr))
                goto failed;
            at += written;
            n -= written;
        }
    }
failed:
    CloseHandle(h);
    operator delete(buf);
    return false;
}

// "*<address> : 16 bytes in hex | 16 characters" ('.' for 0..0x10), as the dumps send it
static void DumpLine(CFtpServer *server, CSocket *s, DWORD address, const BYTE *p)
{
    char line[0x80];
    int n = sprintf(line, "*%08x : ", (unsigned)address);
    for (int i = 0; i < 0x10; i++)
        n += sprintf(line + n, "%02x ", p[i]);
    n += sprintf(line + n, " | ");
    for (int i = 0; i < 0x10; i++)
        n += p[i] > 0x10 ? sprintf(line + n, "%c", p[i]) : sprintf(line + n, ".");
    FtpReply(server, s, "%s\r\n", line); // ("%hs" in the original)
}

// 0x12C50: DUMPMEM <address> <length> (hex): physical memory, mapped and never unmapped (as in
// the original)
void CFtpServer::DumpMem(CSocket *s, DWORD address, DWORD length)
{
    const BYTE *p = (const BYTE *)MmMapIoSpace(address, length, PAGE_READWRITE | PAGE_NOCACHE);
    for (DWORD at = 0; at < length; at += 0x10)
        DumpLine(this, s, address + at, p + at);
    FtpReply(this, s, "200 OK\r\n");
}

// 0x12D70: DUMPHDDSECTOR <sector> <count> (hex), from the whole disk
void CFtpServer::DumpHddSector(CSocket *s, DWORD sector, DWORD count)
{
    ANSI_STRING name = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
    OBJECT_ATTRIBUTES oa = {nullptr, &name, OBJ_CASE_INSENSITIVE};
    IO_STATUS_BLOCK iosb;
    HANDLE h = INVALID_HANDLE_VALUE;
    NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,
               FILE_SYNCHRONOUS_IO_ALERT);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD size = count << 9;
        BYTE *buf = (BYTE *)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        ULONGLONG offset = (ULONGLONG)sector * 0x200;
        LONG high = (LONG)(offset >> 32);
        SetFilePointer(h, (LONG)offset, &high, FILE_BEGIN);
        DWORD read;
        if (ReadFile(h, buf, size, &read, nullptr)) {
            for (DWORD at = 0; at < size; at += 0x10)
                DumpLine(this, s, (sector << 9) + at, buf + at);
        } else {
            FtpReply(this, s, "Error Reading CD!\r\n"); // (sic, for the hard disk)
        }
        VirtualFree(buf, 0, MEM_RELEASE);
        CloseHandle(h);
    }
    FtpReply(this, s, "200 OK\r\n");
}

// 0x12F70: DUMPCDSECTOR <sector> <count> (hex). It reads count 2 KB sectors but seeks to
// sector * 512 (as in the original), and labels them from sector * 2048.
void CFtpServer::DumpCdSector(CSocket *s, DWORD sector, DWORD count)
{
    HANDLE h = CreateFile("CdRom0:", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD size = count << 11;
        BYTE *buf = (BYTE *)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        LONG high = 0;
        SetFilePointer(h, sector << 9, &high, FILE_BEGIN);
        DWORD read;
        if (ReadFile(h, buf, size, &read, nullptr)) {
            for (DWORD at = 0; at < size; at += 0x10)
                DumpLine(this, s, (sector << 11) + at, buf + at);
        } else {
            FtpReply(this, s, "Error Reading CD!\r\n");
        }
        VirtualFree(buf, 0, MEM_RELEASE);
        CloseHandle(h);
    }
    FtpReply(this, s, "200 OK\r\n");
}

// 0x13150: HDDINFO, for one drive: its "size" (the free bytes, as in the original), cluster and
// sector size
void CFtpServer::DiskInfo(CSocket *s, const char *root)
{
    ULARGE_INTEGER avail, total, free;
    GetDiskFreeSpaceEx(root, &avail, &total, &free);
    DWORD cluster = XGetDiskClusterSize(root);
    DWORD sectorSize = XGetDiskSectorSize(root);
    ULONGLONG n = free.QuadPart;
    int units = (int)(n % 1000);
    n /= 1000;
    int thousands = (int)(n % 1000);
    n /= 1000;
    int millions = (int)(n % 1000);
    n /= 1000;
    int billions = (int)(n % 1000);
    char line[0x200];
    sprintf(line, "\r\n.Disk Info for '%s'\r\n", root);
    FtpReply(this, s, "%s", line); // (the original passes the line as the format)
    if (billions)
        sprintf(line, ".Disk Size %d.%03d.%03d.%03d\r\n", billions, millions, thousands, units);
    else if (millions)
        sprintf(line, ".Disk Size %d.%03d.%03d\r\n", millions, thousands, units);
    else if (thousands)
        sprintf(line, ".Disk Size %d.%03d\r\n", thousands, units);
    else
        sprintf(line, ".Disk Size %d\r\n", units);
    FtpReply(this, s, "%s", line);
    sprintf(line, ".ClusterSize %d\r\n", (int)cluster);
    FtpReply(this, s, "%s", line);
    sprintf(line, ".SectorSize %d\r\n", (int)sectorSize);
    FtpReply(this, s, "%s", line);
    FtpReply(this, s, ".\r\n");
}

// 0x132F0: FORMATPATH <\Device\Harddisk0\PartitionN>: remember the partition and make a key
// from its name and a random number; FORMATDRIVE needs that key
void CFtpServer::FormatPath(CSocket *s, const char *path)
{
    DWORD random;
    XNetRandom((BYTE *)&random, 4);
    DWORD key = 0;
    for (size_t i = 0; i < strlen(path); i++)
        key = random ^ ((DWORD)(int)(signed char)path[i] ^ (key >> 24) ^ (key << 8));
    m_formatKey = key;
    strcpy(m_formatPath, path);
    RtlInitAnsiString(&m_formatVolume, m_formatPath);
    char line[0x80];
    sprintf(line, "200 FormatKey %08X for '%s'\r\n", (unsigned)key, m_formatVolume.Buffer);
    FtpReply(this, s, "%s", line); // (the original passes the line as the format)
}

// 0x133C0: FORMATDRIVE <key>: with FORMATPATH's key, format that partition as FATX, set it up
// and link the drive letters again. The key is used once either way.
void CFtpServer::FormatDrive(CSocket *s, DWORD key)
{
    char line[0x80];
    if (m_formatKey == key && m_formatKey != 0xFFFFFFFF) {
        XapiFormatFATVolume((POBJECT_STRING)&m_formatVolume);
        XapiValidateDiskPartition((POBJECT_STRING)&m_formatVolume);
        RelinkDrives();
        sprintf(line, "200 Drive Formated\r\n");
        FtpReply(this, s, "%s", line);
    } else {
        sprintf(line, ".Error in key\r\n");
        FtpReply(this, s, "%s", line);
        sprintf(line, "200 Use Formatpath to get a key\r\n");
        FtpReply(this, s, "%s", line);
    }
    m_formatKey = 0xFFFFFFFF;
    m_formatVolume.Length = 0;
}

// 0x13490: a name made safe for FATX: runs of spaces and '_' become one space, "*+,/:;<=>?\|
// and control characters are dropped, at most 42 characters with the extension (files)
char *CFtpServer::ConvertName(const char *name, bool directory)
{
    char buf[0x100];
    strcpy(buf, name);
    char *out = m_convName;
    out[0] = 0;
    int limit = 42;
    char *ext = nullptr;
    if (!directory) {
        if ((ext = strrchr(buf, '.'))) {
            *ext++ = 0;
            limit = 41 - (int)strlen(ext);
        }
    }
    int n = 0;
    for (const char *p = buf; *p;) {
        unsigned char c = *p;
        if (c == ' ' || c == '_') {
            out[n++] = ' ';
            while (*p == ' ' || *p == '_')
                p++;
        } else if (c >= 0x20 && c < 0x7F && strchr("!#$%&'()-.@[]^`{}~", c)) {
            out[n++] = c;
            p++;
        } else if (c >= 0x20 && c < 0x7F && strchr("\"*+,/:;<=>?\\|", c)) {
            p++;
        } else { // letters, digits and characters from 0x80 on; control characters dropped
            if (c >= '0')
                out[n++] = c;
            p++;
        }
        if (n == limit)
            break;
    }
    out[n] = 0;
    if (!directory && ext) {
        strcat(out, ".");
        strcat(out, ext);
    }
    return out;
}

static void SendText(CSocket *s, const char *text) { s->Send(text, strlen(text)); }

// 0x13650: LIST. "/" lists the drives (and validates F:, which formats it if it is not FATX);
// elsewhere "ls -l" style lines, dated by the creation time.
void CFtpServer::List(CSocket *data, const char *cwd)
{
    // (the original's table sits on the stack one slot above where it is indexed with 1..12)
    static const char *const months[13] = {"", "jan", "feb", "mar", "apr", "may", "jun",
                                           "jul", "aug", "sep", "oct", "nov", "dec"};
    char line[0x200];
    if (_stricmp(cwd, "/") == 0) {
        g_FDriveValid = XapiValidateDiskPartition((POBJECT_STRING)FDriveDevice());
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 C\r\n");
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 D\r\n");
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 E\r\n");
        if (g_HasFDrive)
            SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 F\r\n");
        if (g_HasGDrive)
            SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 G\r\n");
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 X\r\n");
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 Y\r\n");
        SendText(data, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 Z\r\n");
        for (int i = 0; i < 8; i++) {
            if (g_MuDrive[i]) {
                sprintf(line, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 10:41 %c\r\n", g_MuDrive[i]);
                SendText(data, line);
            }
        }
        return;
    }
    char path[0x200], search[0x200];
    strcpy(search, MakePath(cwd, path, cwd));
    strcat(search, "*.*");
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        sprintf(line, "drw-r--r--   1 XBOX      XBOX        0 May 11 11:41 ..\r\n");
        SendText(data, line);
        return;
    }
    sprintf(line, "drwxr-xr-x   1 XBOX      XBOX        0 May 11 11:41 ..\r\n");
    SendText(data, line);
    do {
        FILETIME local;
        SYSTEMTIME st;
        char date[0x40];
        FileTimeToLocalFileTime(&fd.ftCreationTime, &local);
        FileTimeToSystemTime(&local, &st);
        sprintf(date, "%s %02d %02d:%02d", months[st.wMonth], st.wDay, st.wHour, st.wMinute);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            sprintf(line, "drwxr-xr-x   1 XBOX      XBOX        0 %s %s\r\n", date, fd.cFileName);
        else
            sprintf(line, "-rw-r--r--   1 XBOX      XBOX        %d %s %s\r\n", (int)fd.nFileSizeLow, date,
                    fd.cFileName);
        SendText(data, line);
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

// 0x13A90: is path (made from cwd) an existing directory? "x:\" always is.
bool CFtpServer::DirectoryExists(const char *path, const char *cwd)
{
    char tmp[0x200], dir[0x200], name[0x140];
    strcpy(dir, MakePath(path, tmp, cwd));
    size_t len = strlen(dir);
    if (len == 0)
        return false;
    if (len == 3)
        return true;
    if (dir[len - 1] == '\\')
        dir[len - 1] = 0;
    char *slash = strrchr(dir, '\\');
    if (!slash)
        return false;
    strcpy(name, slash + 1);
    *slash = 0;
    strcat(dir, "\\*.*");
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(dir, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && _stricmp(fd.cFileName, name) == 0) {
            FindClose(h);
            return true;
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
    return false;
}
