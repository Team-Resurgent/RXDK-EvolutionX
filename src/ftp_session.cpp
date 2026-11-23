// EvoX's FTP server: start-up, the listening thread and the command loop of a session
// (0x13BF0..0x14ECF in the original); see include/evox/ftp.h.
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/ftp.h"
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/pending.h"

extern int g_FtpUsers;       // 0x3D6FD8 (ftp.cpp)
extern bool g_AutoConvNames; // 0x3D6DD4 (ftp.cpp)
void FtpReply(CFtpServer *server, CSocket *s, const char *fmt, ...);

// 0x94790: the commands
enum {
    FTP_SYST, FTP_PWD, FTP_TYPE, FTP_PORT, FTP_LIST, FTP_QUIT, FTP_CWD, FTP_RETR, FTP_DELE, FTP_STOR,
    FTP_MKD, FTP_RMD, FTP_USER, FTP_PASS, FTP_FLASHBIOS, FTP_EXECUTE, FTP_RNFR, FTP_RNTO, FTP_CDUP,
    FTP_DUMPMEM, FTP_DUMPCDSECTOR, FTP_DUMPHDDSECTOR, FTP_22, FTP_HDDINFO, FTP_FORMATPATH,
    FTP_FORMATDRIVE, FTP_AUTOCONVNAMES,
};
static const struct {
    const char *name;
    int id;
} s_FtpCommands[] = {
    {"SYST", FTP_SYST},         {"PWD", FTP_PWD},
    {"TYPE", FTP_TYPE},         {"PORT", FTP_PORT},
    {"LIST", FTP_LIST},         {"QUIT", FTP_QUIT},
    {"CWD", FTP_CWD},           {"RETR", FTP_RETR},
    {"DELE", FTP_DELE},         {"STOR", FTP_STOR},
    {"MKD", FTP_MKD},           {"RMD", FTP_RMD},
    {"USER", FTP_USER},         {"PASS", FTP_PASS},
    {"RNFR", FTP_RNFR},         {"RNTO", FTP_RNTO},
    {"CDUP", FTP_CDUP},         {"FLASHBIOS", FTP_FLASHBIOS},
    {"EXECUTE", FTP_EXECUTE},   {"DUMPMEM", FTP_DUMPMEM},
    {"DUMPHDDSECTOR", FTP_DUMPHDDSECTOR}, {"DUMPCDSECTOR", FTP_DUMPCDSECTOR},
    {"HDDINFO", FTP_HDDINFO},   {"FORMATPATH", FTP_FORMATPATH},
    {"FORMATDRIVE", FTP_FORMATDRIVE}, {"AUTOCONVNAMES", FTP_AUTOCONVNAMES},
};
static_assert(sizeof s_FtpCommands / sizeof s_FtpCommands[0] == 26, "s_FtpCommands");

struct FtpConnection { // the session thread's argument
    CFtpServer *server;
    SOCKET s;
};

// 0x14B80
static DWORD WINAPI SessionThread(LPVOID arg)
{
    FtpConnection *c = (FtpConnection *)arg;
    g_NetThreads = g_NetThreads + 1;
    c->server->Session(c->s);
    g_NetThreads = g_NetThreads - 1;
    delete c;
    return 0;
}

// 0x14D20
static DWORD WINAPI ServerThread(LPVOID arg)
{
    g_NetThreads = g_NetThreads + 1;
    ((CFtpServer *)arg)->ListenLoop();
    g_NetThreads = g_NetThreads - 1;
    return 0;
}

// 0x14D90
CFtpServer::CFtpServer()
{
    m_listen = new CSocket(true);
    memset(&m_addr, 0, sizeof m_addr);
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(21);
    m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_listen->Bind(&m_addr);
    m_message = false;
    m_quit = false;
    m_formatKey = 0xFFFFFFFF; // (not set in the original until FORMATPATH / FORMATDRIVE)
    g_Ini->Lock();
    if (char *v = g_Ini->GetValue("FTP", "password"))
        strcpy(m_password, v);
    else
        strcpy(m_password, "xbox");
    if (g_Ini->GetBool("FTP", "enable", true))
        CreateThread(nullptr, 0, ServerThread, this, 0, nullptr);
    g_Ini->Unlock();
}

// 0x14C10: accept connections until g_StopNetwork, each on a thread of its own
void CFtpServer::ListenLoop()
{
    timeval wait = {1, 0};
    m_listen->Listen(10);
    while (g_StopNetwork != true) {
        fd_set reads, errors;
        reads.fd_count = 0;
        reads.fd_array[reads.fd_count++] = m_listen->Handle();
        errors.fd_count = 0;
        errors.fd_array[errors.fd_count++] = m_listen->Handle();
        if (select(0, &reads, nullptr, &errors, &wait) == SOCKET_ERROR || !FD_ISSET(m_listen->Handle(), &reads))
            continue;
        SOCKET s = m_listen->Accept(nullptr);
        if (s == INVALID_SOCKET)
            continue;
        FtpConnection *c = new FtpConnection{this, s};
        HANDLE h = CreateThread(nullptr, 0, SessionThread, c, 0, nullptr);
        SetThreadPriority(h, THREAD_PRIORITY_HIGHEST);
        CloseHandle(h);
    }
    m_listen->Close();
    delete m_listen;
}

static void CloseSocket(CSocket *s)
{
    if (s) {
        if (s->IsValid())
            s->Close();
        delete s;
    }
}

// 0x13BF0: one connection. Commands are a word and the rest of the line; before PASS only USER
// and PASS are taken (any other command ends the session).
void CFtpServer::Session(SOCKET sock)
{
    char line[0x200], cwd[0x400], user[0x80] = {}, arg0[0x200], path[0x200], tmp[0x200], target[0x200];
    bool loggedIn = false, running = true;
    g_FtpUsers++;
    CSocket *data = new CSocket(true);
    CSocket *ctrl = new CSocket(sock);
    strcpy(cwd, "/");
    if (g_FtpUsers == 5) {
        running = false;
        FtpReply(this, ctrl, "421- No More Users Allowed.\r\n");
    } else {
        FtpReply(this, ctrl, "220 Please enter your login name now.\r\n");
    }

    while (running) {
        if (g_StopNetwork == true) {
            running = false;
            continue;
        }
        if (!ReadLine(ctrl, line)) {
            running = false;
            continue;
        }
        char *word = strtok(line, " ");
        if (!word)
            continue;
        strlwr(word);
        bool found = false;
        int id = -1;
        for (int i = 0; i < 26; i++) {
            if (_stricmp(s_FtpCommands[i].name, word) != 0)
                continue;
            found = true;
            if (loggedIn || s_FtpCommands[i].id == FTP_USER || s_FtpCommands[i].id == FTP_PASS) {
                id = s_FtpCommands[i].id;
                break;
            }
            running = false; // not logged in
        }
        if (!found) {
            FtpReply(this, ctrl, "502 %s is not implemented.\r\n", word);
            continue;
        }
        char *arg;
        switch (id) {
        case FTP_SYST:
            FtpReply(this, ctrl, "215 UNIX Type: L8\r\n");
            break;

        case FTP_PWD:
            FtpReply(this, ctrl, "257 \"%s\" is current directory\r\n", cwd);
            break;

        case FTP_TYPE:
            arg = strtok(nullptr, " ");
            if (arg && (arg[0] == 'I' || arg[0] == 'i')) // (no argument crashed the original)
                FtpReply(this, ctrl, "200 Type set to IMAGE.\r\n");
            else
                FtpReply(this, ctrl, "200 Type set to ASCII.\r\n");
            break;

        case FTP_PORT: { // h1,h2,h3,h4,p1,p2: connect to it, 10 tries a second apart
            memset(&m_dataAddr, 0, sizeof m_dataAddr);
            m_dataAddr.sin_family = AF_INET;
            BYTE *ip = (BYTE *)&m_dataAddr.sin_addr;
            for (int i = 0; i < 4; i++)
                ip[i] = (BYTE)strtol(strtok(nullptr, ","), nullptr, 10);
            int p1 = strtol(strtok(nullptr, ","), nullptr, 10);
            int p2 = strtol(strtok(nullptr, ","), nullptr, 10);
            int port = p1 + (p2 << 8); // network order in memory: p1 is the high byte
            m_dataAddr.sin_port = (u_short)port;
            data->Create(true);
            int result = 0;
            for (int tries = 10; tries; tries--) {
                if ((result = data->Connect(&m_dataAddr)) == 0)
                    break;
                Sleep(1000);
            }
            if (result == 0)
                FtpReply(this, ctrl, "200 Port command ok. \r\n");
            else // (the port shown is the stored, byte-swapped value)
                FtpReply(this, ctrl, "425 Can't open data connection to %d.%d.%d.%d:%d\r\n", ip[0], ip[1], ip[2],
                         ip[3], port);
            break;
        }

        case FTP_LIST:
            FtpReply(this, ctrl, "150 Opening ASCII data connection for ls %s.\r\n", cwd);
            List(data, cwd);
            if (cwd[1]) {
                char drive[4] = {cwd[1], ':', '\\', 0};
                FreeSpace(drive);
                FtpReply(this, ctrl, "226 free disk space under this directory : %s\r\n", m_freeSpace);
            } else {
                FtpReply(this, ctrl, "226 Transfer finished successfully. Data connection closed.\r\n");
            }
            data->Shutdown(SD_SEND);
            data->Close();
            break;

        case FTP_RETR:
            strcpy(path, MakePath(strtok(nullptr, ""), tmp, cwd));
            FtpReply(this, ctrl, "150 Sending %s\r\n", path);
            SendFile(data, path);
            FtpReply(this, ctrl, "226 Transfer finished successfully. Data connection closed.\r\n");
            data->Shutdown(SD_SEND);
            data->Close();
            break;

        case FTP_STOR: {
            arg = strtok(nullptr, "");
            if (g_AutoConvNames == true)
                arg = ConvertName(arg, false);
            strcpy(path, MakePath(arg, tmp, cwd));
            HANDLE h = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                FtpReply(this, ctrl, "150 Opening BINARY data connection for %s\r\n", path);
                if (ReceiveFile(data, path))
                    FtpReply(this, ctrl, "226 Transfer finished successfully. Data connection closed.\r\n");
                else
                    FtpReply(this, ctrl, "426 Connection closed , file opening failed.\r\n");
            } else {
                FtpReply(this, ctrl, "426 Connection closed , file opening failed.\r\n");
            }
            data->Shutdown(SD_SEND);
            data->Close();
            break;
        }

        case FTP_DELE:
            strcpy(path, MakePath(strtok(nullptr, ""), tmp, cwd));
            SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);
            if (DeleteFile(path))
                FtpReply(this, ctrl, "250 Requested file action ok.\r\n");
            else
                FtpReply(this, ctrl, "553 Requested action not taken.\r\n");
            break;

        case FTP_MKD:
            arg = strtok(nullptr, "");
            if (g_AutoConvNames == true)
                arg = ConvertName(arg, true);
            strcpy(path, MakePath(arg, tmp, cwd));
            if (CreateDirectory(path, nullptr))
                FtpReply(this, ctrl, "250 Requested file action ok.\r\n");
            else
                FtpReply(this, ctrl, "553 Requested action not taken.\r\n");
            break;

        case FTP_RMD:
            strcpy(path, MakePath(strtok(nullptr, ""), tmp, cwd));
            if (RemoveDirectory(path))
                FtpReply(this, ctrl, "250 Requested file action ok.\r\n");
            else
                FtpReply(this, ctrl, "553 Requested action not taken.\r\n");
            break;

        case FTP_RNFR: // the source is kept in path (later commands that use path overwrite it)
            strcpy(path, MakePath(strtok(nullptr, ""), tmp, cwd));
            FtpReply(this, ctrl, "350 Requested file action pending further information.\r\n");
            break;

        case FTP_RNTO:
            strcpy(target, MakePath(strtok(nullptr, ""), tmp, cwd));
            if (MoveFile(path, target))
                FtpReply(this, ctrl, "250 Requested file action ok.\r\n");
            else
                FtpReply(this, ctrl, "553 Requested action not taken.\r\n");
            break;

        case FTP_CDUP: {
            char *slash = strrchr(cwd, '/');
            if (slash != cwd) {
                *slash = 0;
                strrchr(cwd, '/')[1] = 0;
            }
            FtpReply(this, ctrl, "250 \"%s\" is current directory. \r\n", cwd);
            break;
        }

        case FTP_CWD: {
            arg = strtok(nullptr, "");
            strcpy(arg0, cwd);
            if (arg[0] == '/') {
                strcpy(arg0, arg);
                if (arg0[strlen(arg0) - 1] != '/')
                    strcat(arg0, "/");
            } else if (_stricmp("..", arg) == 0) {
                char *slash = strrchr(arg0, '/');
                if (slash != arg0) {
                    *slash = 0;
                    strrchr(arg0, '/')[1] = 0;
                }
            } else {
                strcat(arg0, arg);
                strcat(arg0, "/");
            }
            if (_stricmp(arg0, "/") == 0) {
                strcpy(cwd, arg0);
                FtpReply(this, ctrl, "250 \"%s\" is current directory. \r\n", cwd);
            } else if (DirectoryExists(arg0, cwd) == true) {
                strcpy(cwd, arg0);
                FtpReply(this, ctrl, "250 \"%s\" is current directory. \r\n", cwd);
            } else {
                FtpReply(this, ctrl, "553 Requested action not taken. %s\r\n", arg0);
            }
            break;
        }

        case FTP_USER:
            arg = strtok(nullptr, "");
            if (!arg)
                arg = (char *)"";
            strcpy(user, arg);
            if (_stricmp(arg, "xbox") == 0) {
                FtpReply(this, ctrl, "331 Password required for %s.\r\n", user);
            } else {
                FtpReply(this, ctrl, "530 Unknown User (%s), Bye.\r\n", user);
                running = false;
            }
            break;

        case FTP_PASS: // (USER is not required first)
            arg = strtok(nullptr, "");
            if (!arg)
                arg = (char *)"";
            if (_stricmp(arg, m_password) == 0) {
                FtpReply(this, ctrl, "230 User %s logged in , proceed.\r\n", user);
                loggedIn = true;
            } else {
                FtpReply(this, ctrl, "530 Wrong Password.\r\n");
                running = false;
            }
            break;

        case FTP_QUIT:
            FtpReply(this, ctrl, "221 Goodbye.\r\n");
            running = false;
            break;

        case FTP_FLASHBIOS: // the dashboard shows the file and flashes it (Run, m_message)
            arg = strtok(nullptr, "");
            if (arg[1] == ':') {
                strcpy(path, arg);
            } else {
                strcpy(path, MakePath(cwd, tmp, cwd));
                strcat(path, arg);
            }
            strcpy(m_messageText, path);
            m_message = true;
            FtpReply(this, ctrl, "221 Goodbye. Now trying to flash %s\r\n", path);
            running = false;
            break;

        case FTP_EXECUTE:
            arg = strtok(nullptr, "");
            if (!arg) {
                FtpReply(this, ctrl, "200 Usage : Execute <filename.xbe>\r\n");
                break;
            }
            if (arg[1] == ':') {
                strcpy(path, arg);
            } else {
                strcpy(path, MakePath(cwd, tmp, cwd));
                strcat(path, arg);
            }
            for (char *p = path; *p; p++)
                if (*p == '/')
                    *p = '\\';
            FtpReply(this, ctrl, "221 Goodbye. Now trying to launch %s\r\n", path);
            LaunchXbe(path);
            m_quit = true;
            g_StopNetwork = true;
            running = false;
            break;

        case FTP_DUMPMEM: {
            DWORD address = 0, length;
            if ((arg = strtok(nullptr, " ")))
                address = strtoul(arg, nullptr, 16);
            if ((arg = strtok(nullptr, " ")) && (length = strtoul(arg, nullptr, 16)))
                DumpMem(ctrl, address, length);
            else
                FtpReply(this, ctrl, "200 Usage : Dumpmem <adr> <len>\r\n");
            break;
        }

        case FTP_DUMPCDSECTOR: {
            DWORD sector = 0, count;
            if ((arg = strtok(nullptr, " ")))
                sector = strtol(arg, nullptr, 16);
            if ((arg = strtok(nullptr, " ")) && (count = strtol(arg, nullptr, 16)))
                DumpCdSector(ctrl, sector, count);
            else
                FtpReply(this, ctrl, "200 Usage : DumpCdSector <Sector> <Numbers>\r\n");
            break;
        }

        case FTP_DUMPHDDSECTOR: {
            DWORD sector = 0, count;
            if ((arg = strtok(nullptr, " ")))
                sector = strtol(arg, nullptr, 16);
            if ((arg = strtok(nullptr, " ")) && (count = strtol(arg, nullptr, 16)))
                DumpHddSector(ctrl, sector, count);
            else
                FtpReply(this, ctrl, "200 Usage : DumpHddSector <Sector> <Numbers>\r\n");
            break;
        }

        case FTP_HDDINFO:
            DiskInfo(ctrl, "c:\\");
            DiskInfo(ctrl, "e:\\");
            DiskInfo(ctrl, "f:\\");
            DiskInfo(ctrl, "g:\\");
            DiskInfo(ctrl, "x:\\");
            DiskInfo(ctrl, "y:\\");
            DiskInfo(ctrl, "z:\\");
            FtpReply(this, ctrl, "200 OK\r\n");
            break;

        case FTP_FORMATPATH:
            if ((arg = strtok(nullptr, "")))
                FormatPath(ctrl, arg);
            else
                FtpReply(this, ctrl, "200 Usage : Formatpath <\\Device\\Harddisk0\\Partition[1-6]>\r\n");
            break;

        case FTP_FORMATDRIVE:
            if ((arg = strtok(nullptr, " "))) {
                FormatDrive(ctrl, strtoul(arg, nullptr, 16));
            } else {
                m_formatKey = 0xFFFFFFFF;
                m_formatVolume.Length = 0;
                FtpReply(this, ctrl, "200 Usage : Formatdrive <key>\r\n");
            }
            break;

        case FTP_AUTOCONVNAMES:
            g_AutoConvNames = true;
            FtpReply(this, ctrl, "200 OK\r\n");
            break;
        }
    }

    g_FtpUsers--;
    CloseSocket(ctrl);
    CloseSocket(data);
}
