#pragma once
#include <WS2tcpip.h>
#include <iostream>
#include <strsafe.h>

#pragma comment(lib, "ws2_32")



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