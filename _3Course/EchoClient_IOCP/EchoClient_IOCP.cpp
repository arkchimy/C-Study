#include <iostream>
#include <WS2tcpip.h>
#include <Windows.h>

#include "../../error_log.h"
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"


#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

struct WSA_DATA
{
    WSA_DATA()
    {
        WSADATA wsa;
        int retval; 
        retval = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (retval != 0)
            ERROR_FILE_LOG(L"wsaError.txt", L"WSAStartup");

    }
    ~WSA_DATA()
    {
        WSACleanup();
    }
};

int main()
{
    WSA_DATA wsa;
    
    {
        Parser parser;
    }
    

}
