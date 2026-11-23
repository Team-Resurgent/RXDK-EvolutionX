// The RDTOOLS debug server (0x21900..0x2305A in the original): an xbdm-style remote console on
// TCP port 731 for PC tools (file transfer, directory listings, time, launching a title), and
// name lookup on UDP port 731. Started by Create when the network is up, with [RDTOOLS]
// enable (default yes) and Name (default "evox", the name it answers lookups for).
#pragma once
#include <xtl.h>
#include <winsockx.h>
#include "evox/socket.h"

#pragma pack(push, 2) // 0x41E bytes, as the original allocated it
class CDebugServer {
public:
    CDebugServer();                  // 0x22FA0
    virtual ~CDebugServer();         // 0x21900 (vtable 0x80FB8, deleting destructor 0x21B70)

    void Listen();                   // 0x22C10 the listening thread's loop
    void Session(SOCKET s);          // 0x21C00 one connection's commands

    CSocket *m_listen;               // +0x004 TCP 731
    sockaddr_in m_addr;              // +0x008
    HANDLE m_thread;                 // +0x018 the listening thread
    bool m_1C;                       // +0x01C (cleared by the constructor, otherwise unused)
    char m_titleDir[0x200];          // +0x01D TITLE dir=, with a trailing backslash
    char m_titleName[0x200];         // +0x21D TITLE name=
    bool m_quit;                     // +0x41D a GO / MAGICBOOT session ended: the dashboard launches

private:
    bool GetParam(const char *line, const char *key, char *out);     // 0x21A20
    DWORD GetParamNumber(const char *line, const char *key);         // 0x21B90
    int FileAttributes(const char *path, char *out);                 // 0x21980 200, or 402
};
#pragma pack(pop)
