#include <iostream>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

SOCKET listen_sock;
int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        return -1;
    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8000);

    bind(listen_sock, (sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, SOMAXCONN_HINT(65535));
    

    SOCKET client;

    SOCKADDR_IN clientAddr;
    ZeroMemory(&clientAddr, sizeof(clientAddr));
    int len = sizeof(clientAddr);

    client = accept(listen_sock, (sockaddr *)&clientAddr, &len);
    
    WSACleanup();
}
