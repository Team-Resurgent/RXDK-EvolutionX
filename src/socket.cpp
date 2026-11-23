// EvoX's socket wrapper, 0x26290..0x26488 in the original. See include/evox/socket.h.
#include "evox/socket.h"

static_assert(sizeof(CSocket) == 4, "CSocket");

CSocket::CSocket(bool tcp)
{
    m_sock = INVALID_SOCKET;
    Create(tcp);
}

// (returns 0 also when the socket was open, as the original: closesocket's result is dropped)
int CSocket::Close()
{
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    return 0;
}

bool CSocket::Create(int type, int protocol)
{
    Close();
    m_sock = socket(AF_INET, type, protocol);
    return m_sock != INVALID_SOCKET;
}

bool CSocket::Create(bool tcp)
{
    return tcp ? Create(SOCK_STREAM, IPPROTO_TCP) : Create(SOCK_DGRAM, IPPROTO_UDP);
}

SOCKET CSocket::Accept(sockaddr_in *from)
{
    int len = sizeof(sockaddr_in);
    return accept(m_sock, (sockaddr *)from, &len);
}

int CSocket::Bind(const sockaddr_in *addr) { return bind(m_sock, (const sockaddr *)addr, sizeof(sockaddr_in)); }
int CSocket::Connect(const sockaddr_in *addr) { return connect(m_sock, (const sockaddr *)addr, sizeof(sockaddr_in)); }
int CSocket::Listen(int backlog) { return listen(m_sock, backlog); }
int CSocket::Recv(char *buf, int len) { return recv(m_sock, buf, len, 0); }

int CSocket::RecvFrom(char *buf, int len, sockaddr_in *from)
{
    int fromLen = sizeof(sockaddr_in);
    return recvfrom(m_sock, buf, len, 0, (sockaddr *)from, &fromLen);
}

int CSocket::Send(const char *buf, int len) { return send(m_sock, buf, len, 0); }

int CSocket::SendTo(const char *buf, int len, const sockaddr_in *to)
{
    return sendto(m_sock, buf, len, 0, (const sockaddr *)to, sizeof(sockaddr_in));
}

int CSocket::SetOption(int level, int name, const char *value, int len)
{
    return setsockopt(m_sock, level, name, value, len);
}

int CSocket::Shutdown(int how) { return shutdown(m_sock, how); }
