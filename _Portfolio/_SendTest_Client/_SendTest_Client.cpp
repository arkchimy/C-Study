#include <iostream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <strsafe.h>

#pragma comment(lib, "ws2_32")



int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    SOCKADDR_IN addr;
    int retRecv;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(21350);
    
    wchar_t Serveraddr[16];
    ZeroMemory(Serveraddr, sizeof(wchar_t) * 16);

    printf("Port  21350 고정  Server IP :");
    int retval = wscanf_s(L"%s", Serveraddr, sizeof(Serveraddr) / sizeof(wchar_t));
    if (retval == EOF)
        __debugbreak();


    InetPtonW(AF_INET, Serveraddr, &addr.sin_addr);

    {
        SOCKET client_sock;

        client_sock = socket(AF_INET, SOCK_STREAM, 0);

        connect(client_sock, (sockaddr *)&addr, sizeof(addr));
        printf("Connect ret\n");
        char buffer[10001];
        while (1)
        {
            retRecv = recv(client_sock, buffer, 10001, 0);
            if (retRecv < 0)
            {
                printf(" %d  DisConnect %d \n ", WSAGetLastError());
                closesocket(client_sock);
                break;
            }
        }
    }
    Sleep(INFINITE);

}
