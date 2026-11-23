// The Telnet server (0x14EC0..0x17390 in the original): the listening loop, the session's line
// editor, the command table and DIR. The other commands are in src/telnet_cmds.cpp. See
// include/evox/telnet.h.
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "evox/telnet.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (the layout checks below are on purpose)

static_assert(offsetof(CTelnetServer, m_listen) == 0x004, "m_listen");
static_assert(offsetof(CTelnetServer, m_addr) == 0x00C, "m_addr");
static_assert(offsetof(CTelnetServer, m_quit) == 0x01C, "m_quit");
static_assert(offsetof(CTelnetServer, m_cwd) == 0x01E, "m_cwd");
static_assert(offsetof(CTelnetServer, m_history) == 0x21E, "m_history");
static_assert(offsetof(CTelnetServer, m_historyIndex) == 0x121E, "m_historyIndex");
static_assert(sizeof(CTelnetServer) == 0x1220, "CTelnetServer");

int g_TelnetArgc;                     // 0x3D6FE0
char **g_TelnetArgv;                  // 0x3D6FE4
static char *g_TelnetArgvBuf[0x100];  // 0x3D6FE8
static char g_TelnetNoArg[8];         // 0x3D73E8 what the unused argv entries point at
static char g_TelnetText[0x200];      // 0x3D73F0 Printf's buffer
static char g_TelnetSize[0x20];       // 0x3D75F0 SizeText's result

// 0x94860
enum {
    CMD_DIR, CMD_MEMDUMP, CMD_CDSECTORDUMP, CMD_HDDSECTORDUMP, CMD_EXIT, CMD_PCIDUMP, CMD_IODUMP, CMD_O,
    CMD_SAVEMEM, CMD_TEST2, CMD_SMBUSDUMP, CMD_SMART, CMD_HDDINFO, CMD_HDDLOCKDISABLE, CMD_HDDLOCKENABLE,
    CMD_HDDMASTERDISABLE, CMD_HDDLOCKDISABLEXBOXHDKEY, CMD_HDDLOCKDISABLEPASSWORD,
};
static const struct {
    const char *name;
    int id;
} s_Commands[0x13] = {
    {"DIR", CMD_DIR},
    {"SMBUSDUMP", CMD_SMBUSDUMP},
    {"MEMDUMP", CMD_MEMDUMP},
    {"CDSECTORDUMP", CMD_CDSECTORDUMP},
    {"HDDSECTORDUMP", CMD_HDDSECTORDUMP},
    {"PCIDUMP", CMD_PCIDUMP},
    {"IODUMP", CMD_IODUMP},
    {"O", CMD_O},
    {"EXIT", CMD_EXIT},
    {"QUIT", CMD_EXIT},
    {"SaveMem", CMD_SAVEMEM},
    {"TEST2", CMD_TEST2},
    {"smart", CMD_SMART},
    {"hddinfo", CMD_HDDINFO},
    {"hddlockdisable", CMD_HDDLOCKDISABLE},
    {"hddlockenable", CMD_HDDLOCKENABLE},
    {"hddmasterdisable", CMD_HDDMASTERDISABLE},
    {"hddlockdisablexboxhdkey", CMD_HDDLOCKDISABLEXBOXHDKEY},
    {"hddlockdisablepassword", CMD_HDDLOCKDISABLEPASSWORD},
};

// 0x17320
static DWORD WINAPI TelnetThread(LPVOID server)
{
    g_NetThreads = g_NetThreads + 1;
    ((CTelnetServer *)server)->Serve();
    g_NetThreads = g_NetThreads - 1;
    return 0;
}

// 0x17390 (the thread handle is not closed, as in the original)
CTelnetServer::CTelnetServer()
{
    CreateThread(nullptr, 0, TelnetThread, this, 0, nullptr);
    m_quit = false;
}

// 0x14ED0 (the original formats with vsprintf into its 0x200-byte buffer, unbounded)
void CTelnetServer::Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_TelnetText, sizeof g_TelnetText, fmt, args);
    va_end(args);
    m_client->Send(g_TelnetText, strlen(g_TelnetText));
}

// 0x15120
void CTelnetServer::Prompt() { Printf("\n\r%s>", m_cwd); }

// 0x14F10: a byte from the client, waiting up to a second if asked. Otherwise 0x80000000 (select
// failed), 0x80000001 (error on the socket), 0x80000002 (nothing to read, or recv did not give
// one byte: also when the client has gone) or 0x80000003 (quitting).
int CTelnetServer::GetChar(bool wait)
{
    if (g_StopNetwork || m_quit || m_noBye)
        return (int)0x80000003;
    fd_set readable, failed;
    readable.fd_count = 0;
    readable.fd_array[readable.fd_count++] = m_client->Handle();
    failed.fd_count = 0;
    failed.fd_array[failed.fd_count++] = m_client->Handle();
    timeval timeout = {wait ? 1 : 0, 0};
    if (select(0, &readable, nullptr, &failed, &timeout) == -1)
        return (int)0x80000000;
    if (FD_ISSET(m_client->Handle(), &failed))
        return (int)0x80000001;
    if (FD_ISSET(m_client->Handle(), &readable)) {
        BYTE c;
        if (m_client->Recv((char *)&c, 1) == 1)
            return c;
    }
    return (int)0x80000002;
}

// 0x15030: split the line in place into g_TelnetArgv (words end at a space, tab or comma; a
// "quoted" word at its closing quote), as CEvoxDash::Tokenize does
static bool Tokenize(char *line)
{
    g_TelnetArgc = 0;
    g_TelnetArgv = g_TelnetArgvBuf;
    if (!line)
        return false;
    for (char *&a : g_TelnetArgvBuf)
        a = g_TelnetNoArg;
    int state = 0; // 0 between words, 1 in a quoted word, 2 in a plain word
    for (int i = 0; line[i]; i++) {
        char c = line[i];
        if (state == 0) {
            if (c == ' ' || c == '\t' || c == ',')
                continue;
            if (c == '"' && line[i + 1]) {
                state = 1;
                g_TelnetArgv[g_TelnetArgc++] = &line[i + 1];
            } else {
                state = 2;
                g_TelnetArgv[g_TelnetArgc++] = &line[i];
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
    return g_TelnetArgc != 0;
}

// 0x16B90
void CTelnetServer::Dispatch(char *line)
{
    if (!Tokenize(line))
        return;
    for (const auto &c : s_Commands) {
        if (_stricmp(c.name, g_TelnetArgv[0]) != 0)
            continue;
        switch (c.id) {
        case CMD_DIR: Dir(); break;
        case CMD_MEMDUMP: MemDump(); break;
        case CMD_CDSECTORDUMP: CdSectorDump(); break;
        case CMD_HDDSECTORDUMP: HddSectorDump(); break;
        case CMD_EXIT: m_quit = true; break;
        case CMD_PCIDUMP: PciDump(); break;
        case CMD_IODUMP: IoDump(); break;
        case CMD_O: IoOut(); break;
        case CMD_SAVEMEM: SaveMem(); break;
        case CMD_TEST2: Test2(); break;
        case CMD_SMBUSDUMP: SmbusDump(); break;
        case CMD_SMART: Smart(); break;
        case CMD_HDDINFO: HddInfo(); break;
        case CMD_HDDLOCKDISABLE: HddLockDisable(); break;
        case CMD_HDDLOCKENABLE: HddLockEnable(); break;
        case CMD_HDDMASTERDISABLE: HddMasterDisable(); break;
        case CMD_HDDLOCKDISABLEXBOXHDKEY: HddLockDisableXboxHdKey(); break;
        case CMD_HDDLOCKDISABLEPASSWORD: HddLockDisablePassword(); break;
        }
        return;
    }
    Printf("Unknown Command \"%s\"\r\n", g_TelnetArgv[0]);
}

// up / down through the history (part of Session): the next non-empty line, shown for editing
static void HistoryStep(CTelnetServer *t, char *line, int &len, int step)
{
    for (int tries = 0; tries < 16; tries++) {
        t->m_historyIndex = (char)((t->m_historyIndex + step) & 0xF);
        if (t->m_history[(int)t->m_historyIndex][0]) {
            strcpy(line, t->m_history[(int)t->m_historyIndex]);
            len = strlen(line);
            t->Printf("\r\x1b[K%s>%s", t->m_cwd, line);
            return;
        }
    }
}

// 0x16D40: the client's lines, echoed back, until EXIT / QUIT. A client that goes away without
// EXIT is not noticed: GetChar keeps returning "nothing" and the session goes on until the
// network stops (as in the original). The line is not bounds-checked (0x200 bytes, as there).
void CTelnetServer::Session()
{
    char line[0x200];
    int len = 0;
    memcpy(m_cwd, "c:\\", 4);
    m_client->Send("\xFF\xFB\x01", 3); // IAC WILL ECHO
    m_client->Send("\xFF\xFB\x03", 3); // IAC WILL SUPPRESS-GO-AHEAD
    Printf("RemoteX Telnet Server V1.1\n\r");
    Prompt();
    while (!g_StopNetwork) {
        if (m_quit || m_noBye)
            break;
        int c = GetChar(true);
        if (c <= 0)
            continue;
        switch (c) {
        case 0x08: // backspace
            if (len) {
                Printf("\b \b");
                len--;
            }
            break;
        case 0x0A:
            break;
        case 0x0D: // the line: into the history, then run
            if (len) {
                line[len] = 0;
                strcpy(m_history[(int)m_historyIndex], line);
                m_historyIndex = (char)((m_historyIndex + 1) & 0xF);
                Printf("\r\x1b[K%s>%s\n\r", m_cwd, line);
                len = 0;
                Dispatch(line);
                Prompt();
            }
            break;
        case 0x1B: // ESC O P (F1), ESC [ A / B (up / down)
            c = GetChar(false);
            if (c == 'O') {
                if (GetChar(false) == 'P')
                    Printf("f1");
            } else if (c == '[') {
                c = GetChar(false);
                if (c == 'A')
                    HistoryStep(this, line, len, -1);
                else if (c == 'B')
                    HistoryStep(this, line, len, 1);
            }
            break;
        case 0xFF: // IAC: skip the command and its option
            GetChar(false);
            GetChar(false);
            break;
        default:
            line[len++] = (char)c;
            Printf("%c", c);
            break;
        }
    }
}

// 0x17130: port 23, one client at a time
void CTelnetServer::Serve()
{
    m_historyIndex = 0;
    memset(m_history, 0, sizeof m_history);
    m_listen = new CSocket(true);
    memset(&m_addr, 0, sizeof m_addr);
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(23);
    m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_listen->Bind(&m_addr);
    m_listen->Listen(10);
    timeval wait = {1, 0};
    while (!g_StopNetwork) {
        fd_set fds;
        fds.fd_count = 0;
        fds.fd_array[fds.fd_count++] = m_listen->Handle();
        if (select(0, &fds, nullptr, nullptr, &wait) == -1 || !FD_ISSET(m_listen->Handle(), &fds))
            continue;
        SOCKET s = m_listen->Accept(nullptr);
        if (s == INVALID_SOCKET)
            continue;
        m_client = new CSocket(s);
        m_quit = false;
        m_noBye = false;
        Session();
        if (!m_noBye) {
            Printf("Bye.\r\n");
            Sleep(200);
        }
        m_client->Close();
        delete m_client;
    }
    m_listen->Close();
    delete m_listen;
}

// 0x15140: bytes in groups of three digits (as FreeSpaceText; from a terabyte on only the last 12
// digits show)
static const char *SizeText(ULONGLONG n)
{
    int units = (int)(n % 1000);
    n /= 1000;
    int thousands = (int)(n % 1000);
    n /= 1000;
    int millions = (int)(n % 1000);
    n /= 1000;
    int billions = (int)(n % 1000);
    if (billions)
        sprintf(g_TelnetSize, "%d.%03d.%03d.%03d", billions, millions, thousands, units);
    else if (millions)
        sprintf(g_TelnetSize, "%d.%03d.%03d", millions, thousands, units);
    else if (thousands)
        sprintf(g_TelnetSize, "%d.%03d", thousands, units);
    else
        sprintf(g_TelnetSize, "%d", units);
    return g_TelnetSize;
}

// 0x15230: DIR [<pattern>] in m_cwd. Dates are the creation times; sizes use the low 32 bits;
// the File(s) total is always 0 (the original passes 0 for it).
void CTelnetServer::Dir()
{
    int files = 0, dirs = 0;
    Printf("Directory of %s\n\r", m_cwd);
    Printf("xx-xx-xxxx  xx:xx  <DIR>          .\n\r");
    Printf("xx-xx-xxxx  xx:xx  <DIR>          ..\n\r");
    char path[0x200];
    strcpy(path, m_cwd);
    if (g_TelnetArgc != 2)
        strcat(path, "*.*");
    else
        strcat(path, g_TelnetArgv[1]);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(path, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            FILETIME local;
            SYSTEMTIME st;
            FileTimeToLocalFileTime(&fd.ftCreationTime, &local);
            FileTimeToSystemTime(&local, &st);
            char date[0x40];
            sprintf(date, "%02d-%02d-%04d  %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                Printf("%s  %-14.14s %s\r\n", date, "<DIR>", fd.cFileName);
                dirs++;
            } else {
                Printf("%s  %14.14s %s\r\n", date, SizeText(fd.nFileSizeLow), fd.cFileName);
                files++;
            }
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }
    ULARGE_INTEGER free;
    free.QuadPart = 0;
    GetDiskFreeSpaceEx(m_cwd, nullptr, nullptr, &free);
    Printf("\t%8d File(s)  %14.14s bytes\n\r", files, SizeText(0));
    Printf("\t%8d Dir(s)   %14.14s bytes free\n\r", dirs, SizeText(free.QuadPart));
}
