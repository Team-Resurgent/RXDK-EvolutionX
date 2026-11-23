// EvoX's socket wrapper over XNet's winsock (0x26290..0x26488 in the original).
#pragma once
#include <xtl.h>
#include <winsockx.h>

class CSocket { // 4 bytes
public:
    explicit CSocket(SOCKET s) : m_sock(s) {}   // 0x26290
    explicit CSocket(bool tcp);                 // 0x26470
    ~CSocket() { Close(); }                     // 0x26410
    bool IsValid() const { return m_sock != INVALID_SOCKET; } // 0x262A0
    int Close();                                // 0x262B0
    bool Create(int type, int protocol);        // 0x26420 AF_INET
    bool Create(bool tcp);                      // 0x26450 TCP or UDP
    SOCKET Accept(sockaddr_in *from);           // 0x262D0
    int Bind(const sockaddr_in *addr);          // 0x262F0
    int Connect(const sockaddr_in *addr);       // 0x26310
    int Listen(int backlog);                    // 0x26330
    int Recv(char *buf, int len);               // 0x26340
    int RecvFrom(char *buf, int len, sockaddr_in *from); // 0x26360
    int Send(const char *buf, int len);         // 0x26390
    int SendTo(const char *buf, int len, const sockaddr_in *to); // 0x263B0
    int SetOption(int level, int name, const char *value, int len); // 0x263D0
    int Shutdown(int how);                      // 0x263F0
    SOCKET Handle() const { return m_sock; }    // 0x26400

    SOCKET m_sock;
};
