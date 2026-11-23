// ClockTick (0x12390 in the original): once the network is up, set the clock from an SNTP
// server ([Clock] SNTP_Server, a name or an address), one step per frame.
#include <xtl.h>
#include <string.h>
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/netcfg.h"
#include "evox/socket.h"

extern CSocket *g_SntpSocket;     // 0x3D6DC8 (launch.cpp: PrepareForExit closes it)

enum SntpState {                  // 0x3D6DCC
    SNTP_START,                   // read the server, make the request
    SNTP_WAIT_NETWORK,            // until there is a link and an address
    SNTP_RESOLVE,                 // the server's address, or a DNS lookup
    SNTP_WAIT_DNS,
    SNTP_SEND,
    SNTP_RECEIVE,                 // poll for the answer, then set the clock
    SNTP_DONE,
};

static BYTE g_SntpPacket[0x30];   // 0x3D6B80 the request, then the answer
static sockaddr_in g_SntpAddr;    // 0x3D6BB0
static WSAEVENT g_SntpEvent;      // 0x3D6BC4
static char g_SntpServer[0x200];  // 0x3D6BC8
static int g_SntpState;           // 0x3D6DCC
static XNDNS *g_SntpDns;          // 0x3D6DD0

void ClockTick()
{
    switch (g_SntpState) {
    case SNTP_START:
        memset(g_SntpPacket, 0, sizeof g_SntpPacket);
        g_Ini->Lock();
        strcpy(g_SntpServer, g_Ini->GetString("Clock", "SNTP_Server", "216.244.192.3"));
        g_Ini->Unlock();
        g_SntpSocket = new CSocket(INVALID_SOCKET);
        g_SntpPacket[0] = 0x63; // leap indicator 1, version 4, mode 3 (client)
        g_SntpPacket[1] = 0;    // stratum
        g_SntpPacket[2] = 4;    // poll interval
        g_SntpPacket[3] = 0xFA; // precision
        g_SntpState = SNTP_WAIT_NETWORK;
        break;

    case SNTP_WAIT_NETWORK: {
        XNADDR xna;
        if (EthernetLinkStatus() && XNetGetTitleXnAddr(&xna) != XNET_GET_XNADDR_PENDING)
            g_SntpState = SNTP_RESOLVE;
        break;
    }

    case SNTP_RESOLVE:
        if (!g_SntpSocket)
            break;
        g_SntpState = SNTP_SEND;
        g_SntpSocket->Create(false);
        memset(&g_SntpAddr, 0, sizeof g_SntpAddr);
        g_SntpAddr.sin_family = AF_INET;
        g_SntpAddr.sin_port = htons(123);
        g_SntpEvent = WSACreateEvent();
        g_SntpAddr.sin_addr.s_addr = inet_addr(g_SntpServer);
        if (g_SntpAddr.sin_addr.s_addr == INADDR_NONE) {
            XNetDnsLookup(g_SntpServer, g_SntpEvent, &g_SntpDns);
            g_SntpState = SNTP_WAIT_DNS;
        }
        break;

    case SNTP_WAIT_DNS:
        if (!g_SntpSocket || WaitForSingleObject(g_SntpEvent, 0) == WAIT_TIMEOUT)
            break;
        if (g_SntpDns->iStatus == 0) {
            g_SntpAddr.sin_addr = g_SntpDns->aina[0];
            g_SntpState = SNTP_SEND;
        } else {
            g_SntpState = SNTP_DONE;
        }
        XNetDnsRelease(g_SntpDns);
        break;

    case SNTP_SEND:
        if (!g_SntpSocket)
            break;
        g_SntpSocket->SendTo((const char *)g_SntpPacket, sizeof g_SntpPacket, &g_SntpAddr);
        g_SntpState = SNTP_RECEIVE;
        break;

    case SNTP_RECEIVE: {
        if (!g_SntpSocket)
            break;
        fd_set fds;
        fds.fd_count = 0;
        fds.fd_array[fds.fd_count++] = g_SntpSocket->Handle();
        timeval wait = {0, 0};
        select(0, &fds, nullptr, nullptr, &wait);
        if (!FD_ISSET(g_SntpSocket->Handle(), &fds))
            break;
        g_SntpSocket->Recv((char *)g_SntpPacket, sizeof g_SntpPacket);
        // the transmit time's seconds since 1900, to 100 ns units since 1601
        DWORD seconds;
        memcpy(&seconds, g_SntpPacket + 0x28, 4);
        LARGE_INTEGER now;
        now.QuadPart = ((ULONGLONG)ntohl(seconds) + 0x232661280ULL) * 10000000;
        NtSetSystemTime(&now, nullptr);
        g_SntpState = SNTP_DONE;
        if (g_SntpSocket->IsValid())
            g_SntpSocket->Close();
        delete g_SntpSocket;
        g_SntpSocket = nullptr;
        break;
    }
    }
}
