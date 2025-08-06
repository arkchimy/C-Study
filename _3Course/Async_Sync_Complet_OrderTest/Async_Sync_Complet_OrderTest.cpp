#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>


#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

SOCKET listen_sock;

unsigned AcceptThread(void* arg)
{
    SOCKET client_sock;
    SOCKADDR_IN addr;
    int addrlen = sizeof(addr);

    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6000);

    client_sock = accept(listen_sock, (sockaddr *)&addr, &addrlen);
    if (client_sock == INVALID_SOCKET)
    {
        return 1;
    }

    return 0;
}

int main()
{

    WSAData wsa;
    HANDLE hPort,hAcceptThread;
    DWORD wsaStartRetval;

    wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);

    hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

    hAcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);


    
    WSACleanup();

}
