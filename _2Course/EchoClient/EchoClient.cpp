// EchoClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <WS2tcpip.h>
#include <strsafe.h>

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

int main()
{
    stWSAData wsadata;


    SOCKET sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        __debugbreak();

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);

    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);

    connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    
    char buffer[101];

    while (1)
    {
        int recvSize;
        int sendSize;
        std::cin >> buffer;

        sendSize = send(sock, buffer, strlen(buffer), 0);
        if (sendSize == 0)
        {
            printf("DissConnect  IP : %d  ,Port : %d \n", ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
            break;
        }
   

        recvSize = recv(sock, buffer, 100, 0);
        if (recvSize == 0)
        {
            printf("DissConnect  IP : %d  ,Port : %d \n", ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
            break;
        }
        printf("%s\n", buffer);
        buffer[recvSize] = 0;
    }
    
}
