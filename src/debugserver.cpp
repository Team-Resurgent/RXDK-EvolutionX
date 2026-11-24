// The RDTOOLS debug server (0x21900..0x2305A in the original); see include/evox/debugserver.h.
//
// TCP 731: "201- connected", then one command per line, "COMMAND key=value key="value" ...":
//   GETFILEATTRIBUTES name=      DIRLIST name=           DRIVEFREESPACE name=   DRIVELIST
//   SETFILEATTRIBUTES name= createlo= createhi= changelo= changehi= hidden= readonly=
//   DELETE name= [dir]           MKDIR name=             RENAME name= newname=
//   SENDFILE name= length= (then the bytes)              GETFILE name=
//   SYSTIME   SETSYSTIME clockhi= clocklo=   TITLE dir= name=   GO   MAGICBOOT title=<path>
//   NOTIFYAT port= [drop]        REBOOT (only tells the notification connection)
//   XBEINFO   BYE
// UDP 731: an xbdm name lookup (byte 1: the name's length, then the name) for [RDTOOLS] Name is
// answered with the same packet, type 2.
// A path without ':' as its second character loses its first character (as in the original).
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/debugserver.h"
#include "evox/guard.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/system.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (the layout checks below are on purpose)
static_assert(offsetof(CDebugServer, m_listen) == 0x004, "m_listen");
static_assert(offsetof(CDebugServer, m_thread) == 0x018, "m_thread");
static_assert(offsetof(CDebugServer, m_titleDir) == 0x01D, "m_titleDir");
static_assert(offsetof(CDebugServer, m_titleName) == 0x21D, "m_titleName");
static_assert(offsetof(CDebugServer, m_quit) == 0x41D, "m_quit");
static_assert(sizeof(CDebugServer) == 0x41E, "CDebugServer");

extern volatile bool g_StopNetwork; // 0x591AA2 (launch.cpp)
extern volatile int g_NetThreads;   // 0x591AA4 (launch.cpp)

static int g_DebugServers;          // 0x58FAF8 constructed servers
static char g_DebugName[0x100];     // 0x57F9E0 [RDTOOLS] Name
static CSocket *g_NotifySocket;     // 0x57FAE0 NOTIFYAT's connection back to the tool
static sockaddr_in g_NotifyAddr;    // 0x57FAE4
static char g_DebugBuf[0x10000];    // 0x57FAF8 SENDFILE / GETFILE data

// 0x95C38: the commands; the ids are the original's switch cases
enum {
    CMD_GETFILEATTRIBUTES = 1, CMD_DIRLIST, CMD_SYSTIME, CMD_DRIVEFREESPACE, CMD_DRIVELIST, CMD_BYE,
    CMD_SETFILEATTRIBUTES, CMD_DELETE, CMD_SENDFILE, CMD_NOTIFYAT, CMD_REBOOT, CMD_TITLE, CMD_GO, CMD_MKDIR,
    CMD_RENAME, CMD_MAGICBOOT, CMD_GETFILE, CMD_SETSYSTIME, CMD_XBEINFO,
};
static const struct {
    const char *name;
    int id;
} s_Commands[19] = {
    {"GETFILEATTRIBUTES", CMD_GETFILEATTRIBUTES}, {"DIRLIST", CMD_DIRLIST},
    {"SYSTIME", CMD_SYSTIME},                     {"DRIVEFREESPACE", CMD_DRIVEFREESPACE},
    {"DRIVELIST", CMD_DRIVELIST},                 {"BYE", CMD_BYE},
    {"SETFILEATTRIBUTES", CMD_SETFILEATTRIBUTES}, {"DELETE", CMD_DELETE},
    {"SENDFILE", CMD_SENDFILE},                   {"NOTIFYAT", CMD_NOTIFYAT},
    {"REBOOT", CMD_REBOOT},                       {"TITLE", CMD_TITLE},
    {"GO", CMD_GO},                               {"MKDIR", CMD_MKDIR},
    {"RENAME", CMD_RENAME},                       {"MAGICBOOT", CMD_MAGICBOOT},
    {"SETSYSTIME", CMD_SETSYSTIME},               {"GETFILE", CMD_GETFILE},
    {"XBEINFO", CMD_XBEINFO},
};

static void SendText(CSocket *s, const char *text) { s->Send(text, strlen(text)); }
static DWORD WINAPI DebugListenThread(LPVOID param);

// 0x22FA0: the first server reads [RDTOOLS] and starts the listening thread
CDebugServer::CDebugServer()
{
    // (not cleared in the original, whose heap was not zeroed: GO before TITLE launched garbage)
    m_titleDir[0] = m_titleName[0] = 0;
    if (g_DebugServers == 0) {
        g_Ini->Lock();
        if (char *name = g_Ini->GetValue("RDTOOLS", "Name"))
            strcpy(g_DebugName, name);
        else
            strcpy(g_DebugName, "evox");
        if (g_Ini->GetBool("RDTOOLS", "enable", true))
            m_thread = CreateThread(nullptr, 0, DebugListenThread, this, 0, nullptr);
        g_Ini->Unlock();
        m_quit = false;
    }
    g_DebugServers++;
    m_1C = false;
}

// 0x21900
CDebugServer::~CDebugServer() { g_DebugServers--; }

// 0x21910: one line, a byte at a time; the \n ends it. A \r is dropped, but one at the start
// skips buf[0] (as in the original). On an error only buf[0] is cleared; 0x200 bytes without a
// \n come back unterminated.
static bool ReadLine(CSocket *s, char *buf)
{
    int i = 0;
    for (;;) {
        char c = 0;
        int n = s->Recv(&c, 1);
        // (the original goes on after a closed connection (0): it then read empty lines for good,
        // spinning its high-priority thread; here a closed connection ends the session)
        if (n == -1 || n == 0 || g_StopNetwork == true) {
            buf[0] = 0;
            return false;
        }
        if (c == '\n') {
            buf[i] = 0;
            return true;
        }
        if (c == '\r') {
            if (i)
                i--;
        } else {
            buf[i] = c;
        }
        if (++i >= 0x200)
            return true;
    }
}

// 0x21A20: key=value (up to a space) or key="value" from the line, any case. True if the key
// is there at all, even without a value.
bool CDebugServer::GetParam(const char *line, const char *key, char *out)
{
    bool found = false;
    int keyLength = strlen(key);
    int count = strlen(line) - keyLength + 1;
    out[0] = 0;
    for (int i = 0; i < count; i++) {
        if (_strnicmp(key, line + i, keyLength) != 0)
            continue;
        i += keyLength;
        found = true;
        if (line[i] != '=')
            continue;
        int n = 0;
        if (line[i + 1] == '"') {
            // (the original has no end-of-line check here: an unclosed quote read on)
            for (const char *p = line + i + 2; *p && *p != '"'; p++)
                out[n++] = *p;
        } else {
            for (const char *p = line + i + 1; *p && *p != ' '; p++)
                out[n++] = *p;
        }
        out[n] = 0;
        return found;
    }
    return found;
}

// 0x21B90: a number parameter, hex with 0x
DWORD CDebugServer::GetParamNumber(const char *line, const char *key)
{
    char value[0x100] = {};
    GetParam(line, key, value);
    if (value[1] == 'x')
        return strtoul(value + 2, nullptr, 16);
    return strtoul(value, nullptr, 10);
}

// 0x21980: GETFILEATTRIBUTES / DIRLIST's text for a file
int CDebugServer::FileAttributes(const char *path, char *out)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fa)) {
        out[0] = 0;
        return 402;
    }
    sprintf(out,
            "sizehi=0x%x sizelo=0x%x createhi=0x%08x createlo=0x%08x changehi=0x%08x changelo=0x%08x%s%s%s",
            (unsigned)fa.nFileSizeHigh, (unsigned)fa.nFileSizeLow, (unsigned)fa.ftCreationTime.dwHighDateTime,
            (unsigned)fa.ftCreationTime.dwLowDateTime, (unsigned)fa.ftLastWriteTime.dwHighDateTime,
            (unsigned)fa.ftLastWriteTime.dwLowDateTime,
            (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? " directory" : "",
            (fa.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? " readonly" : "",
            (fa.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? " hidden" : "");
    return 200;
}

// 0x21B10: SETFILEATTRIBUTES' times and attributes. The share mode is 0x10000000 in the
// original (GENERIC_ALL's bit), which the open may refuse; the attributes are set either way.
static bool SetFileTimesAndAttributes(const char *path, const WIN32_FILE_ATTRIBUTE_DATA *fa)
{
    HANDLE h = CreateFile(path, GENERIC_WRITE, 0x10000000, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        SetFileTime(h, &fa->ftCreationTime, &fa->ftLastWriteTime, &fa->ftLastWriteTime);
        CloseHandle(h);
    }
    SetFileAttributes(path, fa->dwFileAttributes);
    return true;
}

// a NAME= path: as given with "x:" at the start, else without its first character
static void PathParam(char *out, const char *value)
{
    strcpy(out, value[1] == ':' ? value : value + 1);
}

// 0x21C00
void CDebugServer::Session(SOCKET s)
{
    bool go = false, keepGoing = true;
    int receiveBuffer = 0x8000;
    char value[0x100] = {};    // a parameter
    char name[0x200] = {};     // NAME= as a path
    char reply[0x200];
    char attributes[0x100];
    char newName[0x200] = {};  // RENAME's NEWNAME=, DIRLIST's paths
    WIN32_FIND_DATA fd;
    char dir[0x200] = {};      // DIRLIST's directory, MAGICBOOT's
    char line[0x201];
    line[0x200] = 0;           // (the original's 0x200-byte buffer could stay unterminated)

    CSocket *sock = new CSocket(s);
    SendText(sock, "201- connected\r\n");
    sock->SetOption(SOL_SOCKET, SO_RCVBUF, (const char *)&receiveBuffer, 4);
    while (g_StopNetwork != true && ReadLine(sock, line)) {
        char *command = strtok(line, " ");
        if (command) {
            char *args = strtok(nullptr, "");
            if (!args)
                args = (char *)"";
            bool handled = false;
            for (int i = 0; i < 19; i++) { // (a match does not end the search)
                if (_stricmp(s_Commands[i].name, command) != 0)
                    continue;
                handled = true;
                int n;
                switch (s_Commands[i].id) {
                case CMD_GETFILEATTRIBUTES:
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    if (FileAttributes(name, attributes) != 200) {
                        SendText(sock, "402\r\n");
                        break;
                    }
                    SendText(sock, "202\r\n");
                    n = sprintf(reply, "%3d- %s\r\n", 200, attributes);
                    sock->Send(reply, n);
                    SendText(sock, ".\r\n");
                    break;

                case CMD_DIRLIST: {
                    GetParam(args, "NAME", value);
                    if (value[1] != ':')
                        memmove(value, value + 1, strlen(value));
                    size_t length = strlen(value);
                    if (length == 0 || value[length - 1] != '\\')
                        strcat(value, "\\");
                    strcpy(dir, value);
                    strcat(value, "*.*");
                    HANDLE find = FindFirstFile(value, &fd);
                    SendText(sock, "202\r\n");
                    if (find != INVALID_HANDLE_VALUE) {
                        do {
                            sprintf(newName, "%s%s", dir, fd.cFileName);
                            FileAttributes(newName, attributes);
                            n = sprintf(reply, "200- name=\"%s\" %s\r\n", fd.cFileName, attributes);
                            sock->Send(reply, n);
                        } while (FindNextFile(find, &fd));
                        FindClose(find);
                    }
                    SendText(sock, ".\r\n");
                    break;
                }

                case CMD_DRIVEFREESPACE: {
                    SendText(sock, "202\r\n");
                    GetParam(args, "NAME", value);
                    if (value[1] != ':')
                        memmove(value, value + 1, strlen(value));
                    ULARGE_INTEGER toCaller = {}, total = {}, free = {};
                    GetDiskFreeSpaceEx(value, &toCaller, &total, &free);
                    n = sprintf(reply,
                                "200- freetocallerlo=0x%08x freetocallerhi=0x%08x totalbyteslo=0x%08x "
                                "totalbyteshi=0x%08x totalfreebyteslo=0x%08x totalfreebyteshi=0x%08x\r\n",
                                (unsigned)toCaller.LowPart, (unsigned)toCaller.HighPart, (unsigned)total.LowPart,
                                (unsigned)total.HighPart, (unsigned)free.LowPart, (unsigned)free.HighPart);
                    sock->Send(reply, n);
                    SendText(sock, ".\r\n");
                    break;
                }

                case CMD_SETFILEATTRIBUTES: {
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    WIN32_FILE_ATTRIBUTE_DATA fa = {};
                    fa.ftCreationTime.dwLowDateTime = GetParamNumber(args, "CREATELO");
                    fa.ftCreationTime.dwHighDateTime = GetParamNumber(args, "CREATEHI");
                    fa.ftLastWriteTime.dwLowDateTime = GetParamNumber(args, "CHANGELO");
                    fa.ftLastWriteTime.dwHighDateTime = GetParamNumber(args, "CHANGEHI");
                    if (GetParamNumber(args, "HIDDEN"))
                        fa.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
                    if (GetParamNumber(args, "READONLY"))
                        fa.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
                    // (a "200-" line with what was read, and no "." after it)
                    n = sprintf(reply, "200- %x %x %x %x %x\r\n", (unsigned)fa.ftCreationTime.dwHighDateTime,
                                (unsigned)fa.ftCreationTime.dwLowDateTime, (unsigned)fa.ftLastWriteTime.dwHighDateTime,
                                (unsigned)fa.ftLastWriteTime.dwLowDateTime, (unsigned)fa.dwFileAttributes);
                    sock->Send(reply, n);
                    SetFileTimesAndAttributes(name, &fa);
                    break;
                }

                case CMD_DRIVELIST:
                    n = sprintf(reply, "200- CDEFHIJKLMNOXYZ\n");
                    sock->Send(reply, n);
                    break;

                case CMD_DELETE:
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    if (GetParam(args, "DIR", value)) {
                        RemoveDirectory(name);
                    } else {
                        SetFileAttributes(name, FILE_ATTRIBUTE_NORMAL);
                        DeleteFile(name);
                    }
                    SendText(sock, "200\r\n");
                    break;

                case CMD_SENDFILE: {
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    DWORD left = GetParamNumber(args, "LENGTH");
                    n = sprintf(reply, "204- %x\r\n", (unsigned)left);
                    sock->Send(reply, n);
                    HANDLE h = CreateFile(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (h != INVALID_HANDLE_VALUE) { // (else the data is read as commands, as in the original)
                        do {
                            int got = sock->Recv(g_DebugBuf, sizeof g_DebugBuf);
                            // (the original took an error (-1) as data and a closed connection (0)
                            // as nothing, looping for good on either; here they end the transfer)
                            if (got <= 0)
                                break;
                            left -= got;
                            for (int done = 0; got;) {
                                DWORD written;
                                WriteFile(h, g_DebugBuf + done, got, &written, nullptr);
                                done += written;
                                got -= written;
                            }
                        } while (left);
                        CloseHandle(h);
                    }
                    SendText(sock, "200\r\n");
                    break;
                }

                case CMD_GETFILE: {
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    n = sprintf(reply, "203\r\n");
                    sock->Send(reply, n);
                    // the length (4 bytes), then the data
                    if (FILE *f = fopen(name, "rb")) {
                        fseek(f, 0, SEEK_END);
                        DWORD size = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        sock->Send((const char *)&size, 4);
                        bool failed = false;
                        while (!failed && !feof(f)) {
                            int got = fread(g_DebugBuf, 1, 0x1000, f);
                            for (int done = 0; got;) {
                                int sent = sock->Send(g_DebugBuf + done, got);
                                // (the original closed the file here and went on using it)
                                if (sent == -1) {
                                    failed = true;
                                    break;
                                }
                                done += sent;
                                got -= sent;
                            }
                        }
                        fclose(f);
                    }
                    SendText(sock, "200- xxxxxxxx\r\n");
                    break;
                }

                case CMD_NOTIFYAT:
                    if (GetParam(args, "drop", value)) {
                        g_NotifySocket->Close();
                        SendText(sock, "200- Drop\r\n");
                    } else {
                        // the tool's address, port PORT=. (A zero length is passed, so the
                        // address is not filled in; the socket is bound before it is made.)
                        int length = 0;
                        getpeername(s, (sockaddr *)&g_NotifyAddr, &length);
                        g_NotifyAddr.sin_family = AF_INET;
                        g_NotifyAddr.sin_port = htons((u_short)GetParamNumber(args, "PORT"));
                        g_NotifySocket->Bind(&g_NotifyAddr);
                        g_NotifySocket->Create(true);
                        g_NotifySocket->Connect(&g_NotifyAddr);
                        SendText(sock, "200- Open\r\n");
                        SendText(g_NotifySocket, "execution pending\r\n");
                    }
                    break;

                case CMD_REBOOT: // (nothing reboots: the notification connection is told and remade)
                    SendText(sock, "200\r\n");
                    SendText(g_NotifySocket, "execution rebooting\r\n");
                    g_NotifySocket->Close();
                    g_NotifySocket->Create(true);
                    g_NotifySocket->Connect(&g_NotifyAddr);
                    SendText(g_NotifySocket, "execution pending\r\n");
                    break;

                case CMD_TITLE: // what GO / MAGICBOOT launch when the session ends
                    if (GetParam(args, "NAME", value))
                        strcpy(m_titleName, value);
                    if (GetParam(args, "DIR", value)) {
                        strcpy(m_titleDir, (value[0] == 'X' || value[0] == 'x') ? value + 1 : value);
                        strcat(m_titleDir, "\\");
                    }
                    SendText(sock, "200\r\n");
                    break;

                case CMD_GO: // (shows MAGICBOOT's or DIRLIST's last dir and NAME=, not TITLE's)
                    n = sprintf(reply, "200- Executing %s%s\r\n", dir, name);
                    sock->Send(reply, n);
                    go = true;
                    break;

                case CMD_MKDIR:
                    GetParam(args, "NAME", value);
                    PathParam(name, value);
                    SendText(sock, CreateDirectory(name, nullptr) ? "200\r\n" : "402\r\n");
                    break;

                case CMD_RENAME:
                    if (GetParam(args, "NAME", value))
                        PathParam(name, value);
                    if (GetParam(args, "NEWNAME", value))
                        PathParam(newName, value);
                    SendText(sock, MoveFile(name, newName) ? "200\r\n" : "402\r\n");
                    break;

                case CMD_MAGICBOOT: {
                    // title=<dir>\<file>[ DEBUG]: shown; the launch uses TITLE's (as in the original)
                    if (_strnicmp(args, "title=", 6) != 0) {
                        SendText(sock, "400\r\n");
                        break;
                    }
                    size_t length = strlen(args);
                    if (length <= 6)
                        break;
                    char *tail = args + length - 6;
                    if (_stricmp(tail, " DEBUG") == 0)
                        *tail = 0;
                    char *slash = strrchr(args, '\\');
                    if (!slash) // (the original went on with a null pointer)
                        break;
                    strcpy(name, slash + 1);
                    slash[1] = 0;
                    strcpy(dir, args + 6);
                    n = sprintf(reply, "200- Executing %s%s\r\n", dir, name);
                    sock->Send(reply, n);
                    go = true;
                    break;
                }

                case CMD_SYSTIME: {
                    LARGE_INTEGER now;
                    KeQuerySystemTime(&now);
                    n = sprintf(reply, "200- high=0x%x low=0x%x\r\n", (unsigned)now.HighPart, (unsigned)now.LowPart);
                    sock->Send(reply, n);
                    break;
                }

                case CMD_SETSYSTIME: { // (answers "bye")
                    SendText(sock, "200- bye\r\n");
                    LARGE_INTEGER now;
                    now.HighPart = GetParamNumber(args, "clockhi");
                    now.LowPart = GetParamNumber(args, "clocklo");
                    NtSetSystemTime(&now, nullptr);
                    break;
                }

                case CMD_XBEINFO: // (no information follows)
                    SendText(sock, "202- multiline response follows\r\n");
                    SendText(sock, ".\r\n");
                    break;

                case CMD_BYE:
                    SendText(sock, "200- bye\r\n");
                    keepGoing = false;
                    break;
                }
            }
            if (!handled)
                SendText(sock, "407- unknown command\r\n");
        }
        if (!keepGoing)
            break;
    }
    sock->Shutdown(SD_SEND);
    sock->Close(); // (the CSocket is not deleted, as in the original)
    if (go) {
        sprintf(newName, "%s%s", m_titleDir, m_titleName);
        LaunchXbe(newName);
        m_quit = true;
    }
}

struct DebugSession { // 8 bytes
    CDebugServer *server;
    SOCKET socket;
};

// 0x22B90
static DWORD WINAPI DebugSessionThread(LPVOID param)
{
    DebugSession *session = (DebugSession *)param;
    g_NetThreads = g_NetThreads + 1;
    if (!CrashGuard([&] { session->server->Session(session->socket); })) // catch (...) 0x22BD1
        Log("Debug : DebugConnect Error\n\r");
    g_NetThreads = g_NetThreads - 1;
    delete session;
    ExitThread(0);
}

// 0x22C10: accept connections, each on a thread of its own, and answer name lookups
void CDebugServer::Listen()
{
    timeval wait = {1, 0};
    CSocket *lookup = nullptr;
    CrashGuard([&] { // catch (...) 0x22EEA: a fault stops the server
        m_listen = new CSocket(true);
        memset(&m_addr, 0, sizeof m_addr);
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(731);
        m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        m_listen->Bind(&m_addr);
        lookup = new CSocket(false);
        int reuse = 1;
        lookup->SetOption(SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, 4);
        lookup->Bind(&m_addr);
        g_NotifySocket = new CSocket(true);
        m_listen->Listen(10);
        while (g_StopNetwork != true) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_listen->Handle(), &fds);
            FD_SET(lookup->Handle(), &fds);
            if (select(0, &fds, nullptr, nullptr, &wait) == SOCKET_ERROR)
                continue;
            if (FD_ISSET(m_listen->Handle(), &fds)) {
                SOCKET s = m_listen->Accept(nullptr);
                if (s != INVALID_SOCKET) {
                    DebugSession *session = new DebugSession{this, s};
                    // (the thread handle is not closed, as in the original)
                    SetThreadPriority(CreateThread(nullptr, 0, DebugSessionThread, session, 0, nullptr),
                                      THREAD_PRIORITY_HIGHEST);
                }
            }
            if (FD_ISSET(lookup->Handle(), &fds)) {
                char packet[0x104];
                sockaddr_in from;
                lookup->RecvFrom(packet, 0x102, &from);
                int length = (signed char)packet[1];
                packet[2 + length] = 0;
                if (_stricmp(packet + 2, g_DebugName) == 0) {
                    packet[0] = 2;
                    lookup->SendTo(packet, length + 2, &from);
                }
            }
        }
    });
    if (m_listen) { // (closed, not deleted)
        m_listen->Close();
        m_listen = nullptr;
    }
    if (lookup)
        lookup->Close();
    if (g_NotifySocket) {
        g_NotifySocket->Close();
        g_NotifySocket = nullptr;
    }
}

// 0x22F40
static DWORD WINAPI DebugListenThread(LPVOID param)
{
    g_NetThreads = g_NetThreads + 1;
    if (!CrashGuard([&] { ((CDebugServer *)param)->Listen(); })) // catch (...) 0x22F8D
        Log("Debug : DebugInitNet Error\n\r");
    g_NetThreads = g_NetThreads - 1;
    ExitThread(0);
}
