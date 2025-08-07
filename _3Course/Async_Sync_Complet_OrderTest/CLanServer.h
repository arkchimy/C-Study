#pragma once
#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

BOOL DomainToIP(const wchar_t *szDomain, IN_ADDR *pAddr);

class CLanServer
{
  public:
    BOOL OnServer(const wchar_t *addr, short port);
    ~CLanServer();
  public:
    SOCKET listen_sock;
};
