

#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <list>

#pragma comment(lib, "WS2_32")

std::list<SOCKET> sock_list;

struct stWSAdata
{
    stWSAdata()
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~stWSAdata()
    {
        WSACleanup();
    }
};

struct stSession
{
};
stWSAdata wsa;
int main()
{
    int num = 0xaabbccdd;
    short port = 0xff;
    char ip[16];

    printf("ip Addr :");
    scanf_s("%s", ip, 16);
    ip[15] = 0;
    printf("port :");
    scanf_s("%d", &port, sizeof(u_short));

    printf("Connect Cnt :");
    scanf_s("%d", &num, sizeof(int));

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    InetPtonA(AF_INET, ip, &addr.sin_addr);

    int connectRetval;
    u_long on;

    on = 1;

    for (int i = 0; i < num; i++)
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (INVALID_SOCKET == sock)
            __debugbreak();
        // ioctlsocket(sock, FIONBIO, &on);
        sock_list.push_back(sock);

        // connect()
    Connect:
        connectRetval = connect(sock, (sockaddr *)&addr, sizeof(addr));
        if (connectRetval != 0)
        {
            switch (GetLastError())
            {
            case WSAECONNREFUSED:
                printf("ReConnect: %d \n", GetLastError());
                goto Connect;
                break;
            case WSAEWOULDBLOCK:
                break;
            default:
                printf("GetLastError  : %d \n", GetLastError());
            }

        }
    }
    while (1)
    {
    }
}
