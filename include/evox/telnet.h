// The Telnet server, "RemoteX Telnet Server V1.1" (0x14EC0..0x17390 in the original): one client
// at a time on port 23, a line editor with a 16-line history, and debugging commands (directory
// listing, CD / HDD sector and memory / I/O / SMBus / PCI dumps, I/O and PCI writes, saving
// memory to a file, SMART and IDENTIFY data, and the drive's security (lock) commands).
// main creates it ([Telnet] Enable) and never deletes it.
#pragma once
#include <xtl.h>
#include <winsockx.h>
#include "evox/socket.h"

extern volatile bool g_StopNetwork; // 0x591AA2 asks the network threads to end (launch.cpp)
extern volatile int g_NetThreads;   // 0x591AA4 network threads still running

class CTelnetServer {               // 0x1220 bytes
public:
    CTelnetServer();                // 0x17390 starts the server thread
    virtual ~CTelnetServer() {}     // 0x14EC0 (scalar deleting destructor 0x16760; never called)

    void Serve();                   // 0x17130 the listening loop, one client at a time
    void Session();                 // 0x16D40 a client: the line editor
    void Dispatch(char *line);      // 0x16B90
    int GetChar(bool wait);         // 0x14F10 a byte, or <= 0 for none (see telnet.cpp)
    void Printf(const char *fmt, ...); // 0x14ED0
    void Prompt();                  // 0x15120

    // the commands (src/telnet.cpp, src/telnet_cmds.cpp)
    void Dir();                     // 0x15230 DIR
    void CdSectorDump();            // 0x15440 CDSECTORDUMP
    void Smart();                   // 0x15640 smart
    void HddInfo();                 // 0x157E0 hddinfo
    void HddLockDisable();          // 0x15920 hddlockdisable
    void HddLockEnable();           // 0x15960 hddlockenable
    void HddMasterDisable();        // 0x15A00 hddmasterdisable
    void HddLockDisableXboxHdKey(); // 0x15A90 hddlockdisablexboxhdkey
    void HddLockDisablePassword();  // 0x15B50 hddlockdisablepassword
    void HddSectorDump();           // 0x15BD0 HDDSECTORDUMP
    void SmbusDump();               // 0x15DF0 SMBUSDUMP
    void MemDump();                 // 0x15FD0 MEMDUMP
    void SaveMem();                 // 0x16140 SaveMem
    void Test2();                   // 0x16210 TEST2
    void IoOut();                   // 0x16320 O
    void IoDump();                  // 0x16390 IODUMP
    void PciDumpDevice(DWORD *address); // 0x16690
    void PciScan(bool dump);        // 0x16780
    void PciDump();                 // 0x16910 PCIDUMP

    CSocket *m_listen;              // +0x004
    CSocket *m_client;              // +0x008
    sockaddr_in m_addr;             // +0x00C port 23
    bool m_quit;                    // +0x01C EXIT / QUIT
    bool m_noBye;                   // +0x01D never set: "Bye." is always sent
    char m_cwd[0x200];              // +0x01E the prompt and DIR's directory, "c:\" (never changed)
    char m_history[16][0x100];      // +0x21E the last lines entered
    char m_historyIndex;            // +0x121E the next line to fill, and the one shown by up / down
    BYTE pad121F;
};

// the command line's words (0x15030)
extern int g_TelnetArgc;            // 0x3D6FE0
extern char **g_TelnetArgv;         // 0x3D6FE4 (PCIDUMP steps it past a "b:<bus>" word)
