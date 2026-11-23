// EvoX's FTP server (0x12750..0x14ECF in the original): port 21, user "xbox" with [FTP] password,
// active mode only (PORT), one thread per session, at most 4 sessions. Besides the usual
// commands it has EvoX's own: FLASHBIOS, EXECUTE, DUMPMEM, DUMPHDDSECTOR, DUMPCDSECTOR, HDDINFO,
// FORMATPATH / FORMATDRIVE (format a partition with a one-time key) and AUTOCONVNAMES.
#pragma once
#include <xtl.h>
#include <winsockx.h>
#include "evox/socket.h"

class CFtpServer {                   // 0x5E0 bytes
public:
    CFtpServer();                    // 0x14D90 bind port 21, start the server thread ([FTP] Enable)
    virtual ~CFtpServer() {}         // 0x13630 / 0x12750 (vtable 0x7DCAC)

    void ListenLoop();                                       // 0x14C10
    void Session(SOCKET s);                                  // 0x13BF0 one connection
    static bool ReadLine(CSocket *s, char *line);            // 0x127A0 one command line
    void FreeSpace(const char *root);                        // 0x128C0 into m_freeSpace
    static char *MakePath(const char *arg, char *out, const char *cwd); // 0x129C0 "/c/x" -> "c:\x"
    static void SendFile(CSocket *data, const char *path);   // 0x12AC0 RETR
    static bool ReceiveFile(CSocket *data, const char *path); // 0x12B70 STOR
    void DumpMem(CSocket *s, DWORD address, DWORD length);   // 0x12C50 DUMPMEM
    void DumpHddSector(CSocket *s, DWORD sector, DWORD count); // 0x12D70 DUMPHDDSECTOR
    void DumpCdSector(CSocket *s, DWORD sector, DWORD count); // 0x12F70 DUMPCDSECTOR
    void DiskInfo(CSocket *s, const char *root);             // 0x13150 HDDINFO
    void FormatPath(CSocket *s, const char *path);           // 0x132F0 FORMATPATH
    void FormatDrive(CSocket *s, DWORD key);                 // 0x133C0 FORMATDRIVE
    char *ConvertName(const char *name, bool directory);     // 0x13490 AUTOCONVNAMES: FATX-safe names
    void List(CSocket *data, const char *cwd);               // 0x13650 LIST
    static bool DirectoryExists(const char *path, const char *cwd); // 0x13A90 CWD

    CSocket *m_listen;               // +0x004
    sockaddr_in m_addr;              // +0x008 port 21, any address
    sockaddr_in m_dataAddr;          // +0x018 PORT's address
    BYTE unk028[0x28];
    char m_convName[0x100];          // +0x050 ConvertName's result
    char m_formatPath[0x100];        // +0x150 FORMATPATH's partition
    ANSI_STRING m_formatVolume;      // +0x250 over m_formatPath
    DWORD m_formatKey;               // +0x258 FORMATDRIVE's key, 0xFFFFFFFF: none
    char m_freeSpace[0x100];         // +0x25C FreeSpace's text
    bool m_message;                  // +0x35C FLASHBIOS: m_messageText is the file to flash
    char m_messageText[0x200];       // +0x35D
    bool m_quit;                     // +0x55D EXECUTE: the dashboard should exit (LaunchXbe was called)
    char m_password[0x82];           // +0x55E [FTP] Password
};
