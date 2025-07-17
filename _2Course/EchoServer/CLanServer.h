#pragma once

#include <WS2tcpip.h>
#include <iostream>
#include <list>
#include <strsafe.h>

template <typename T>
class clsSession
{
  private:
    clsSession(SOCKET sock, SOCKADDR_IN addr) : _sock(sock), _addr(addr)
    {
        _id = s_id++;
        inet_ntop(AF_INET, &_addr.sin_addr, _ipstr, sizeof(_ipstr));
    }

    friend class CLanServer;

    SOCKET _sock;
    SOCKADDR_IN _addr;
    unsigned long long _id;
    char _ipstr[INET_ADDRSTRLEN];

    char recv_buffer[101];
    int recvSize = 0;

    

    inline static unsigned long long s_id = 0;
};

class CLanServer
{

  public:
    CLanServer(unsigned short port);
    void AcceptThread();
    void Update();

    virtual void OnAccept(clsSession<CLanServer> *session);
    virtual bool OnRecv(clsSession<CLanServer> *session);
    virtual bool OnSend(clsSession<CLanServer> *session);

    void ServerLogFile(const char *str, DWORD lastError);

  private:
    SOCKET listen_sock;
    CRITICAL_SECTION cs_list;
    std::list<clsSession<CLanServer> *> sessions;
};
