// EchoClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <WS2tcpip.h>
#include <iostream>
#include <strsafe.h>
#include <list>

#pragma comment(lib, "WS2_32")

struct stWSAData
{
    stWSAData()
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            LogFile("WSAStartup() %d \n", GetLastError());
    }
    ~stWSAData()
    {
        WSACleanup();
    }

    void LogFile(const char *str, DWORD lastError)
    {
        FILE *logFile;
        char filename[100];
        char buffer[100];

        StringCchPrintfA(filename, 100, "Error__%s.txt", __DATE__);

        fopen_s(&logFile, filename, "a+");
        if (logFile == nullptr)
        {
            printf("fOpenFail : GetLastError() %d\n", GetLastError());
            __debugbreak();
        }

        StringCchPrintfA(buffer, 100, str, lastError);
        fwrite(buffer, 1, strlen(buffer), logFile);

        fclose(logFile);
    }
};
class clsSession
{
  private:
    clsSession(SOCKET sock) : _sock(sock)
    {

    }

    SOCKET _sock;

    char recv_buffer[101];
    int recvSize = 0;
};

int main()
{
    stWSAData wsadata;
    int Sock_Cnt;

    SOCKADDR_IN Serveraddr;
    Serveraddr.sin_family = AF_INET;
    Serveraddr.sin_port = htons(8000);

    InetPtonW(AF_INET, L"127.0.0.1", &Serveraddr.sin_addr);


    std::cout << " Socket Cnt : ";
    std::cin >> Sock_Cnt;

    for (int i = 0; i < Sock_Cnt; i++)
    {
        SOCKET sock;
        u_long on = 1;
        sock = socket(AF_INET, SOCK_STREAM, 0);

        if (sock == INVALID_SOCKET)
            __debugbreak();

        ioctlsocket(sock, FIONBIO, &on);


    }

    std::list<clsSession> sessions;

    int ConnectRetval = connect(sock, reinterpret_cast<sockaddr *>(&Serveraddr), sizeof(Serveraddr));
    if (ConnectRetval < 0)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            __debugbreak();
    }

    while (1)
    {   

        sendSize = send(sock, buffer, strlen(buffer), 0);
        if (sendSize == 0)
        {
            printf("DissConnect  IP : %d  ,Port : %d \n", ntohl(Serveraddr.sin_addr.s_addr), ntohs(Serveraddr.sin_port));
            break;
        }

        recvSize = recv(sock, buffer, 100, 0);
        if (recvSize == 0)
        {
            printf("DissConnect  IP : %d  ,Port : %d \n", ntohl(Serveraddr.sin_addr.s_addr), ntohs(Serveraddr.sin_port));
            break;
        }
        printf("%s\n", buffer);
        buffer[recvSize] = 0;
    }
}
